#pragma once
// ather_api.h — Shared data structs + HTTP API calls

#include "ather_auth.h"

// ============================================================
// Shared data structures
// ============================================================
struct AtherStatus {
  int   soc;            // battery %
  float range_km;
  bool  charging;
  char  ride_mode[16];
  bool  valid;
};

struct AtherLocation {
  float         lat;
  float         lon;
  unsigned long ts;
  bool          valid;
};

struct AtherRide {
  unsigned long ts;
  float         distance;   // km
  int           avg_speed;  // km/h
  int           max_speed;  // km/h
  bool          valid;
};

// ============================================================
// Global state  (written by Core-0 task, read by Core-1 UI)
// Protected by g_ather_mutex for struct updates.
// ============================================================
AtherStatus   g_ather_status   = {0, 0.0f, false, "unknown", false};
AtherLocation g_ather_location = {0.0f, 0.0f, 0, false};
AtherRide     g_ather_ride     = {0, 0.0f, 0, 0, false};
volatile bool g_api_ok         = false;
char          g_api_error[64]  = "Not connected";

SemaphoreHandle_t g_ather_mutex = NULL;

// Rate limiter: enforce >= 10 s between any two API calls
static unsigned long s_last_api_ms = 0;
static bool _rate_ok() {
  unsigned long now = millis();
  if (now - s_last_api_ms < 10000UL) { ather_log("[ATHER] rate limited"); return false; }
  s_last_api_ms = now;
  return true;
}

void ather_api_init() {
  g_ather_mutex = xSemaphoreCreateMutex();
  ather_log("[ATHER] api init");
}

// ============================================================
// API fetch functions  (all blocking — call from Core-0 task)
// ============================================================
bool ather_api_fetch_status() {
  if (!_rate_ok()) return false;
  ather_log("[ATHER] polling vehicle status");
  char token[ATHER_TOKEN_LEN];
  if (!ather_auth_get_token(token, sizeof(token))) return false;

  char url[256];
  snprintf(url, sizeof(url), "%s/vehicle/%s/status", s_ather_base, s_ather_vid);
  static char resp[512];
  if (!_ather_get(url, resp, sizeof(resp), token)) {
    g_api_ok = false;
    strncpy(g_api_error, "Status fetch failed", sizeof(g_api_error));
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, resp) != DeserializationError::Ok) return false;

  if (xSemaphoreTake(g_ather_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
    g_ather_status.soc      = doc["soc"]      | 0;
    g_ather_status.range_km = doc["range_km"] | 0.0f;
    g_ather_status.charging = doc["charging"] | false;
    const char* mode = doc["ride_mode"];
    strncpy(g_ather_status.ride_mode, mode ? mode : "unknown", 15);
    g_ather_status.valid = true;
    g_api_ok = true;
    strncpy(g_api_error, "OK", sizeof(g_api_error));
    xSemaphoreGive(g_ather_mutex);
  }
  ather_log("[ATHER] soc=%d range=%.1fkm charging=%d",
            g_ather_status.soc, g_ather_status.range_km, (int)g_ather_status.charging);
  return true;
}

bool ather_api_fetch_location() {
  if (!_rate_ok()) return false;
  ather_log("[ATHER] polling location");
  char token[ATHER_TOKEN_LEN];
  if (!ather_auth_get_token(token, sizeof(token))) return false;

  char url[256];
  snprintf(url, sizeof(url), "%s/vehicle/%s/location", s_ather_base, s_ather_vid);
  static char resp[512];
  if (!_ather_get(url, resp, sizeof(resp), token)) return false;

  JsonDocument doc;
  if (deserializeJson(doc, resp) != DeserializationError::Ok) return false;

  if (xSemaphoreTake(g_ather_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
    g_ather_location.lat   = doc["lat"]       | 0.0f;
    g_ather_location.lon   = doc["lon"]       | 0.0f;
    g_ather_location.ts    = doc["timestamp"] | 0UL;
    g_ather_location.valid = true;
    xSemaphoreGive(g_ather_mutex);
  }
  return true;
}

bool ather_api_fetch_ride() {
  if (!_rate_ok()) return false;
  ather_log("[ATHER] polling latest ride");
  char token[ATHER_TOKEN_LEN];
  if (!ather_auth_get_token(token, sizeof(token))) return false;

  char url[256];
  snprintf(url, sizeof(url), "%s/rides/latest", s_ather_base);
  static char resp[512];
  if (!_ather_get(url, resp, sizeof(resp), token)) return false;

  JsonDocument doc;
  if (deserializeJson(doc, resp) != DeserializationError::Ok) return false;

  if (xSemaphoreTake(g_ather_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
    g_ather_ride.distance  = doc["distance"]  | 0.0f;
    g_ather_ride.avg_speed = doc["avg_speed"] | 0;
    g_ather_ride.max_speed = doc["max_speed"] | 0;
    g_ather_ride.ts        = millis();
    g_ather_ride.valid     = true;
    xSemaphoreGive(g_ather_mutex);
  }
  return true;
}

bool ather_api_flash_horn() {
  ather_log("[ATHER] flash horn");
  char token[ATHER_TOKEN_LEN];
  if (!ather_auth_get_token(token, sizeof(token))) return false;
  char url[256];
  snprintf(url, sizeof(url), "%s/vehicle/%s/horn", s_ather_base, s_ather_vid);

  WiFiClientSecure client; client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, url)) return false;
  https.addHeader("Authorization", String("Bearer ") + token);
  https.addHeader("Content-Type", "application/json");
  int code = https.POST((uint8_t*)"{}", 2);
  https.end();
  return code == 200 || code == 204;
}

bool ather_api_stop_horn() {
  ather_log("[ATHER] stop horn");
  char token[ATHER_TOKEN_LEN];
  if (!ather_auth_get_token(token, sizeof(token))) return false;
  char url[256];
  snprintf(url, sizeof(url), "%s/vehicle/%s/horn/stop", s_ather_base, s_ather_vid);

  WiFiClientSecure client; client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, url)) return false;
  https.addHeader("Authorization", String("Bearer ") + token);
  https.addHeader("Content-Type", "application/json");
  int code = https.POST((uint8_t*)"{}", 2);
  https.end();
  return code == 200 || code == 204;
}
