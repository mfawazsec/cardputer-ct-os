#pragma once
// ather_tracker.h — FreeRTOS background polling task + movement detection

#include "ather_storage.h"
#include <math.h>

// ============================================================
// Tracker state  (written by Core-0 task, read by Core-1 UI)
// ============================================================
volatile bool g_tracker_alert   = false;
bool          g_tracker_enabled = false;
AtherLocation g_alert_location  = {};

// ============================================================
// Haversine distance (metres) between two lat/lon points
// ============================================================
static float _haversine(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371000.0f;
  const float TO_RAD = 0.01745329252f;
  float dlat = (lat2 - lat1) * TO_RAD;
  float dlon = (lon2 - lon1) * TO_RAD;
  float a = sinf(dlat / 2) * sinf(dlat / 2) +
            cosf(lat1 * TO_RAD) * cosf(lat2 * TO_RAD) *
            sinf(dlon / 2) * sinf(dlon / 2);
  return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

// ============================================================
// Background polling task (Core 0)
// Cycle: fetch_status → 10 s → fetch_location → 10 s → fetch_ride → 10 s
// Total ≈ 30 s per cycle (plus HTTP latency)
// ============================================================
static void _polling_task(void* /*param*/) {
  float prev_lat = 0.0f, prev_lon = 0.0f;
  bool  had_prev = false;

  while (true) {
    // Wait for WiFi
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    // 1. Vehicle status
    ather_api_fetch_status();
    vTaskDelay(pdMS_TO_TICKS(10000));

    // 2. Location + movement check
    float snap_lat = g_ather_location.lat;
    float snap_lon = g_ather_location.lon;
    bool  snap_valid = g_ather_location.valid;

    ather_api_fetch_location();
    vTaskDelay(pdMS_TO_TICKS(10000));

    if (g_tracker_enabled && snap_valid && g_ather_location.valid &&
        !g_ather_status.charging) {
      float dist = _haversine(snap_lat, snap_lon,
                              g_ather_location.lat, g_ather_location.lon);
      if (dist > 25.0f) {
        ather_log("[ATHER] movement detected: %.1fm", dist);
        if (xSemaphoreTake(g_ather_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
          g_alert_location = g_ather_location;
          xSemaphoreGive(g_ather_mutex);
        }
        g_tracker_alert = true;
      }
    }

    // 3. Latest ride
    bool had_ride = g_ather_ride.valid;
    unsigned long prev_ts = g_ather_ride.ts;
    ather_api_fetch_ride();
    vTaskDelay(pdMS_TO_TICKS(10000));

    // Push to local cache only if this looks like a new ride
    if (g_ather_ride.valid && (!had_ride || g_ather_ride.ts != prev_ts)) {
      RideRecord r = {
        (uint32_t)(millis() / 1000),
        g_ather_ride.distance,
        (uint16_t)g_ather_ride.avg_speed,
        (uint16_t)g_ather_ride.max_speed
      };
      ather_storage_push(&r);
    }

    had_prev = true;
    (void)had_prev;
  }
}

// ============================================================
// Start the polling task on Core 0
// ============================================================
void ather_tracker_start() {
  xTaskCreatePinnedToCore(_polling_task, "atherPoll", 8192,
                          nullptr, 1, nullptr, 0);
  ather_log("[ATHER] tracker task started");
}

// ============================================================
// Called from Arduino loop() (Core 1) — triggers alert screen if needed
// ============================================================
void ather_tracker_check_alert();   // forward decl; defined in ather_app.h
