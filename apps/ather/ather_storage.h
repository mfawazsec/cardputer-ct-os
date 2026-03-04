#pragma once
// ather_storage.h — Local ride history cache (last 20 rides, Preferences/NVS blob)

#include "ather_api.h"

#define ATHER_MAX_RIDES 20

struct RideRecord {
  uint32_t ts;         // unix-ish timestamp (millis-based, not real unix)
  float    distance;   // km
  uint16_t avg_speed;  // km/h
  uint16_t max_speed;  // km/h
};  // 12 bytes × 20 = 240 bytes total

RideRecord s_ride_buf[ATHER_MAX_RIDES] = {};
int        s_ride_count = 0;

void ather_storage_init() {
  Preferences p;
  p.begin("rides", true);
  size_t stored = p.getBytesLength("data");
  if (stored == sizeof(s_ride_buf)) {
    p.getBytes("data", s_ride_buf, sizeof(s_ride_buf));
    s_ride_count = 0;
    for (int i = 0; i < ATHER_MAX_RIDES; i++) {
      if (s_ride_buf[i].ts > 0) s_ride_count++;
      else break;
    }
  }
  p.end();
  ather_log("[ATHER] storage: %d rides loaded", s_ride_count);
}

// Prepend newest ride; drop oldest if > ATHER_MAX_RIDES.
void ather_storage_push(const RideRecord* r) {
  memmove(&s_ride_buf[1], &s_ride_buf[0],
          sizeof(RideRecord) * (ATHER_MAX_RIDES - 1));
  s_ride_buf[0] = *r;
  if (s_ride_count < ATHER_MAX_RIDES) s_ride_count++;

  Preferences p;
  p.begin("rides", false);
  p.putBytes("data", s_ride_buf, sizeof(s_ride_buf));
  p.end();
}

// Copy ride array and count out to caller.
void ather_storage_get(RideRecord* out, int* count) {
  memcpy(out, s_ride_buf, sizeof(s_ride_buf));
  *count = s_ride_count;
}
