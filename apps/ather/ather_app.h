#pragma once
// ather_app.h — All Ather UI screens + menu setup/loop functions

#include "ather_tracker.h"

// ============================================================
// Ather main menu (proc 30)
// ============================================================
MENU ather_main_menu[] = {
  { "Dashboard",    31 },
  { "Last Ride",    32 },
  { "Ride History", 36 },
  { "Tracker Mode", 34 },
  { "Settings",     33 },
  { "Debug",        35 },
  { TXT_BACK,        1 },
};
int ather_main_menu_size = sizeof(ather_main_menu) / sizeof(MENU);

void ather_menu_setup() {
  cursor = 0;
  rstOverride = true;
  DISP.fillScreen(BGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setTextColor(BGCOLOR, FGCOLOR);
  DISP.setCursor(0, 0);
  DISP.println(" ATHER 450X    ");
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  drawmenu(ather_main_menu, ather_main_menu_size);
  delay(500);
}

void ather_menu_loop() {
  if (check_next_press()) {
    cursor = (cursor + 1) % ather_main_menu_size;
    drawmenu(ather_main_menu, ather_main_menu_size);
    delay(250);
    return;
  }
  if (!check_select_press()) return;
  int cmd = ather_main_menu[cursor].command;
  if (cmd == 1) { rstOverride = false; isSwitching = true; current_proc = 1; return; }
  rstOverride = false;
  isSwitching = true;
  current_proc = cmd;
}

// ============================================================
// Utility: draw a 10-char battery bar at current cursor position
// ============================================================
static void _draw_batt_bar(int soc) {
  uint16_t color = (soc > 50) ? 0x07E0 :   // green
                   (soc > 20) ? 0xFFE0 :   // yellow
                                0xF800;    // red
  DISP.setTextColor(color, BGCOLOR);
  DISP.print("[");
  int filled = soc / 10;
  for (int i = 0; i < 10; i++) DISP.print(i < filled ? "#" : "-");
  DISP.printf("] %d%%", soc);
  DISP.setTextColor(FGCOLOR, BGCOLOR);
}

// ============================================================
// Dashboard (proc 31)
// ============================================================
static void _ather_dash_draw() {
  DISP.fillScreen(BGCOLOR);
  DISP.setTextColor(BGCOLOR, FGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 0);
  DISP.println(" ATHER 450X    ");
  DISP.setTextColor(FGCOLOR, BGCOLOR);

  if (xSemaphoreTake(g_ather_mutex, pdMS_TO_TICKS(300)) == pdTRUE) {
    AtherStatus   st  = g_ather_status;
    AtherLocation loc = g_ather_location;
    AtherRide     rd  = g_ather_ride;
    bool ok = g_api_ok;
    xSemaphoreGive(g_ather_mutex);

    // Battery bar
    DISP.setCursor(0, 18);
    if (st.valid) {
      _draw_batt_bar(st.soc);
      DISP.setCursor(0, 34);
      DISP.printf("Range: %.0fkm  %s\n", st.range_km, st.ride_mode);
      DISP.printf("Charging: %s\n", st.charging ? "Yes" : "No");
    } else {
      DISP.println("Batt: ---");
    }

    // Location
    DISP.setCursor(0, 68);
    if (loc.valid) {
      DISP.printf("%.5f\n%.5f\n", loc.lat, loc.lon);
    } else {
      DISP.println("Location: ---");
    }

    // Last ride
    DISP.setCursor(0, 100);
    if (rd.valid) {
      DISP.printf("%.1fkm  avg%d", rd.distance, rd.avg_speed);
    }

    // Status line
    DISP.setCursor(0, 118);
    wl_status_t ws = WiFi.status();
    DISP.printf("WiFi:%s API:%s",
                ws == WL_CONNECTED ? "OK" : "--",
                ok ? "OK" : "DISC");
  } else {
    DISP.setCursor(0, 40);
    DISP.println("  (mutex timeout)");
  }
}

void ather_dash_setup() {
  rstOverride = false;
  _ather_dash_draw();
}

void ather_dash_loop() {
  static unsigned long last_draw = 0;
  if (millis() - last_draw > 5000UL || last_draw == 0) {
    last_draw = millis();
    _ather_dash_draw();
  }
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    isSwitching = true;
    current_proc = 30;
  }
}

// ============================================================
// Last Ride (proc 32)
// ============================================================
void ather_ride_setup() {
  rstOverride = false;
  DISP.fillScreen(BGCOLOR);
  DISP.setTextColor(BGCOLOR, FGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 0);
  DISP.println(" Last Ride     ");
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  DISP.setCursor(0, 20);

  if (xSemaphoreTake(g_ather_mutex, pdMS_TO_TICKS(300)) == pdTRUE) {
    AtherRide rd = g_ather_ride;
    xSemaphoreGive(g_ather_mutex);
    if (rd.valid) {
      DISP.printf("Distance: %.2f km\n", rd.distance);
      DISP.printf("Avg Speed: %d km/h\n", rd.avg_speed);
      DISP.printf("Max Speed: %d km/h\n", rd.max_speed);
    } else {
      DISP.println("No ride data yet.");
      DISP.println("Check API settings.");
    }
  }
  DISP.setCursor(0, 110);
  DISP.println("Any key to return");
}

void ather_ride_loop() {
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    isSwitching = true;
    current_proc = 30;
  }
}

// ============================================================
// Ride History (proc 36) — scrollable list of last 20 rides
// ============================================================
static RideRecord s_hist_buf[ATHER_MAX_RIDES];
static int        s_hist_count = 0;

void ather_history_setup() {
  cursor = 0;
  rstOverride = false;
  ather_storage_get(s_hist_buf, &s_hist_count);

  auto draw = [&]() {
    DISP.fillScreen(BGCOLOR);
    DISP.setTextColor(BGCOLOR, FGCOLOR);
    DISP.setTextSize(SMALL_TEXT);
    DISP.setCursor(0, 0);
    DISP.println(" Ride History  ");
    DISP.setTextColor(FGCOLOR, BGCOLOR);
    if (s_hist_count == 0) {
      DISP.setCursor(0, 30);
      DISP.println("No rides cached.");
      return;
    }
    // Show 5 entries per page
    int start = (cursor / 5) * 5;
    for (int i = start; i < start + 5 && i < s_hist_count; i++) {
      DISP.setCursor(0, 20 + (i - start) * 20);
      if (i == cursor) DISP.setTextColor(BGCOLOR, FGCOLOR);
      else             DISP.setTextColor(FGCOLOR, BGCOLOR);
      DISP.printf("#%02d %.1fkm avg%d max%d",
                  i + 1, s_hist_buf[i].distance,
                  s_hist_buf[i].avg_speed, s_hist_buf[i].max_speed);
    }
    DISP.setTextColor(FGCOLOR, BGCOLOR);
  };
  draw();
  // Store draw lambda — re-invoke on key press via inline loop pattern
  delay(300);
}

void ather_history_loop() {
  if (check_next_press()) {
    cursor = (cursor + 1) % max(s_hist_count, 1);
    // Redraw
    DISP.fillScreen(BGCOLOR);
    DISP.setTextColor(BGCOLOR, FGCOLOR);
    DISP.setTextSize(SMALL_TEXT);
    DISP.setCursor(0, 0);
    DISP.println(" Ride History  ");
    DISP.setTextColor(FGCOLOR, BGCOLOR);
    if (s_hist_count == 0) {
      DISP.setCursor(0, 30);
      DISP.println("No rides cached.");
    } else {
      int start = (cursor / 5) * 5;
      for (int i = start; i < start + 5 && i < s_hist_count; i++) {
        DISP.setCursor(0, 20 + (i - start) * 20);
        if (i == cursor) DISP.setTextColor(BGCOLOR, FGCOLOR);
        else             DISP.setTextColor(FGCOLOR, BGCOLOR);
        DISP.printf("#%02d %.1fkm avg%d max%d",
                    i + 1, s_hist_buf[i].distance,
                    s_hist_buf[i].avg_speed, s_hist_buf[i].max_speed);
      }
      DISP.setTextColor(FGCOLOR, BGCOLOR);
    }
    delay(250);
    return;
  }
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isKeyPressed(',') ||
      M5Cardputer.Keyboard.isKeyPressed(',')) {
    isSwitching = true;
    current_proc = 30;
  }
}

// ============================================================
// Settings (proc 33) — sequential text-input for credentials
// ============================================================
void ather_settings_setup() {
  rstOverride = false;

  // Load current values as defaults
  char email[64]   = {}, pass[64]  = {};
  char base[128]   = {}, vid[64]   = {};
  strncpy(email, s_ather_email, sizeof(email) - 1);
  strncpy(base,  s_ather_base,  sizeof(base)  - 1);
  strncpy(vid,   s_ather_vid,   sizeof(vid)   - 1);
  // Don't pre-fill password for security

  // Field 1/4 — Email
  if (!keyboard_readline("Email (1/4):", email, sizeof(email), false)) goto cancel;
  // Field 2/4 — Password
  if (!keyboard_readline("Password (2/4):", pass, sizeof(pass), true))  goto cancel;
  // Field 3/4 — API Base URL
  if (!keyboard_readline("API Base URL (3/4):", base, sizeof(base), false)) goto cancel;
  // Field 4/4 — Vehicle ID
  if (!keyboard_readline("Vehicle ID (4/4):", vid, sizeof(vid), false)) goto cancel;

  // Save and attempt login
  ather_auth_save_config(email, pass, base, vid);
  DISP.fillScreen(BGCOLOR);
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 30);
  DISP.println("Saved. Logging in...");
  if (ather_auth_login()) {
    DISP.setTextColor(0x07E0, BGCOLOR);
    DISP.println("Login OK!");
  } else {
    DISP.setTextColor(0xF800, BGCOLOR);
    DISP.println("Login failed.");
    DISP.setTextColor(FGCOLOR, BGCOLOR);
    DISP.println("Check credentials.");
  }
  delay(2000);
  isSwitching = true;
  current_proc = 30;
  return;

cancel:
  DISP.fillScreen(BGCOLOR);
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 50);
  DISP.println("  Cancelled.");
  delay(1000);
  isSwitching = true;
  current_proc = 30;
}

void ather_settings_loop() {
  // Setup handles everything; loop is a no-op (process switches away in setup)
}

// ============================================================
// Tracker Mode toggle (proc 34)
// ============================================================
static void _tracker_draw() {
  DISP.fillScreen(BGCOLOR);
  DISP.setTextColor(BGCOLOR, FGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 0);
  DISP.println(" Tracker Mode  ");
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  DISP.setCursor(0, 30);
  DISP.print("Status: ");
  if (g_tracker_enabled) {
    DISP.setTextColor(0x07E0, BGCOLOR);
    DISP.println("ENABLED");
  } else {
    DISP.setTextColor(0xF800, BGCOLOR);
    DISP.println("DISABLED");
  }
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  DISP.setCursor(0, 60);
  DISP.println("[Enter] Toggle");
  DISP.println("[Esc]   Back");
  DISP.setCursor(0, 100);
  DISP.setTextSize(1);
  DISP.println("Alert when scooter");
  DISP.println("moves >25m & not charging");
  DISP.setTextSize(SMALL_TEXT);
}

void ather_tracker_setup() {
  rstOverride = false;
  // Load persisted state
  Preferences p; p.begin("ather", true);
  g_tracker_enabled = p.getBool("tracker", false);
  p.end();
  _tracker_draw();
}

void ather_tracker_loop() {
  if (check_select_press()) {
    g_tracker_enabled = !g_tracker_enabled;
    Preferences p; p.begin("ather", false);
    p.putBool("tracker", g_tracker_enabled);
    p.end();
    ather_log("[ATHER] tracker %s", g_tracker_enabled ? "enabled" : "disabled");
    _tracker_draw();
    delay(300);
    return;
  }
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isKeyPressed(',') ||
      M5Cardputer.Keyboard.isKeyPressed(',')) {
    isSwitching = true;
    current_proc = 30;
  }
}

// ============================================================
// Debug log viewer (proc 35) — last 10 [ATHER] log lines
// ============================================================
static void _debug_draw(int scroll) {
  DISP.fillScreen(BGCOLOR);
  DISP.setTextColor(BGCOLOR, FGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 0);
  DISP.println(" Ather Debug   ");
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  DISP.setTextSize(1);
  int total = s_log_count;
  int start = min(scroll, max(0, total - 7));
  for (int i = start; i < start + 7 && i < total; i++) {
    int idx = (s_log_head - total + i + ATHER_LOG_LINES) % ATHER_LOG_LINES;
    DISP.setCursor(0, 18 + (i - start) * 16);
    DISP.println(s_log[idx]);
  }
  DISP.setTextSize(SMALL_TEXT);
}

static int s_dbg_scroll = 0;

void ather_debug_setup() {
  rstOverride = false;
  s_dbg_scroll = max(0, s_log_count - 7);
  _debug_draw(s_dbg_scroll);
}

void ather_debug_loop() {
  static unsigned long last_refresh = 0;
  if (millis() - last_refresh > 2000UL) {
    last_refresh = millis();
    _debug_draw(s_dbg_scroll);
  }
  if (check_next_press()) {
    s_dbg_scroll = min(s_dbg_scroll + 1, max(0, s_log_count - 1));
    _debug_draw(s_dbg_scroll);
    delay(200);
    return;
  }
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isKeyPressed(',') ||
      M5Cardputer.Keyboard.isKeyPressed(',')) {
    isSwitching = true;
    current_proc = 30;
  }
}

// ============================================================
// Anti-theft Alert screen (proc 37)
// ============================================================
void ather_alert_setup() {
  rstOverride = false;
  // Red background
  DISP.fillScreen(0xF800);
  DISP.setTextColor(0xFFFF, 0xF800);
  DISP.setTextSize(MEDIUM_TEXT);
  DISP.setCursor(0, 0);
  DISP.println("!! MOVING !!");
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 40);
  DISP.printf("Lat: %.5f\n", g_alert_location.lat);
  DISP.printf("Lon: %.5f\n", g_alert_location.lon);
  DISP.setCursor(0, 85);
  DISP.println("[F]     Flash horn");
  DISP.println("[S]     Stop horn");
  DISP.println("[Enter] Acknowledge");

  // Beep
  M5Cardputer.Speaker.tone(1000, 300);
  delay(400);
  M5Cardputer.Speaker.tone(1000, 300);
}

void ather_alert_loop() {
  M5Cardputer.update();
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return;

  auto ks = M5Cardputer.Keyboard.keysState();

  // F — flash horn
  for (char c : ks.word) {
    if (c == 'f' || c == 'F') { ather_api_flash_horn(); return; }
    if (c == 's' || c == 'S') { ather_api_stop_horn();  return; }
  }

  // Enter — acknowledge
  if (ks.enter) {
    g_tracker_alert = false;
    ather_api_stop_horn();
    DISP.fillScreen(BGCOLOR);
    DISP.setTextColor(FGCOLOR, BGCOLOR);
    isSwitching = true;
    current_proc = 1;
  }
}

// ============================================================
// Called from Arduino loop() — triggers alert screen if flagged
// ============================================================
void ather_tracker_check_alert() {
  if (g_tracker_alert && g_tracker_enabled && current_proc != 37) {
    isSwitching  = true;
    current_proc = 37;
  }
}
