#pragma once
// wifi_config.h — Wi-Fi Configuration module for M5-ctOS
// Provides: keyboard_readline(), wifi_autoconnect(), wificfg_setup/loop()

#include <WiFi.h>
#include <Preferences.h>

// ============================================================
// Shared keyboard text-input helper (used by Ather Settings too)
// Fills buf with typed text. Returns true on Enter, false on Esc.
// mask=true renders '*' instead of characters (passwords).
// ============================================================
bool keyboard_readline(const char* prompt, char* buf, int maxlen, bool mask = false) {
  int len = (int)strnlen(buf, maxlen);

  auto redraw = [&]() {
    DISP.fillScreen(BGCOLOR);
    DISP.setTextColor(FGCOLOR, BGCOLOR);
    DISP.setTextSize(SMALL_TEXT);
    DISP.setCursor(0, 0);
    DISP.println(prompt);
    // input area — inverted colours
    DISP.setTextColor(BGCOLOR, FGCOLOR);
    char vis[65] = {};
    if (mask) {
      memset(vis, '*', min(len, 64));
    } else {
      strncpy(vis, buf, 64);
    }
    DISP.print(vis);
    DISP.print('_');
    DISP.setTextColor(FGCOLOR, BGCOLOR);
  };

  redraw();

  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      auto ks = M5Cardputer.Keyboard.keysState();
      if (ks.enter) { delay(100); return true; }
      if (ks.del && len > 0) { buf[--len] = '\0'; redraw(); }
      for (char c : ks.word) {
        if (c >= 0x20 && c < 0x7F && len < maxlen - 1) {
          buf[len++] = c;
          buf[len]   = '\0';
          redraw();
        }
      }
    }
    if (M5Cardputer.Keyboard.isKeyPressed(',')) { delay(100); return false; }
  }
}

// ============================================================
// Auto-connect on boot — non-blocking (WiFi connects in background)
// ============================================================
void wifi_autoconnect() {
  Preferences p;
  p.begin("wifi", true);
  char ssid[64] = {}, pass[64] = {};
  p.getString("ssid", ssid, sizeof(ssid));
  p.getString("pass", pass, sizeof(pass));
  p.end();
  if (strlen(ssid) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    Serial.printf("[WiFi] auto-connect to %s\n", ssid);
  }
}

// ============================================================
// Wi-Fi Config menu (proc 20)
// ============================================================
MENU wificfg_menu[] = {
  { "Connect",    1 },
  { "Status",     2 },
  { "Disconnect", 3 },
  { TXT_BACK,     5 },
};
int wificfg_menu_size = sizeof(wificfg_menu) / sizeof(MENU);

static void wificfg_draw_status() {
  DISP.fillScreen(BGCOLOR);
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  DISP.setTextSize(SMALL_TEXT);
  DISP.setCursor(0, 0);
  DISP.setTextColor(BGCOLOR, FGCOLOR);
  DISP.println(" Wi-Fi Status  ");
  DISP.setTextColor(FGCOLOR, BGCOLOR);
  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    DISP.printf("SSID: %s\n",  WiFi.SSID().c_str());
    DISP.printf("IP:   %s\n",  WiFi.localIP().toString().c_str());
    DISP.printf("RSSI: %d dBm\n", WiFi.RSSI());
    DISP.println("Status: CONNECTED");
  } else {
    DISP.println("Status: NOT CONNECTED");
    switch (st) {
      case WL_IDLE_STATUS:    DISP.println("(idle)"); break;
      case WL_NO_SSID_AVAIL:  DISP.println("(SSID not found)"); break;
      case WL_CONNECT_FAILED: DISP.println("(auth failed)"); break;
      case WL_DISCONNECTED:   DISP.println("(disconnected)"); break;
      default: DISP.printf("(code %d)\n", (int)st); break;
    }
  }
  DISP.println("\nAny key to return");
}

void wificfg_setup() {
  cursor = 0;
  rstOverride = true;
  drawmenu(wificfg_menu, wificfg_menu_size);
  delay(500);
}

void wificfg_loop() {
  if (check_next_press()) {
    cursor = (cursor + 1) % wificfg_menu_size;
    drawmenu(wificfg_menu, wificfg_menu_size);
    delay(250);
    return;
  }
  if (!check_select_press()) return;

  int opt = wificfg_menu[cursor].command;

  // Back
  if (opt == 5) {
    rstOverride = false;
    isSwitching = true;
    current_proc = 1;
    return;
  }

  // Status
  if (opt == 2) {
    wificfg_draw_status();
    while (true) {
      M5Cardputer.update();
      if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) break;
    }
    delay(200);
    drawmenu(wificfg_menu, wificfg_menu_size);
    return;
  }

  // Disconnect + forget creds
  if (opt == 3) {
    WiFi.disconnect();
    Preferences p;
    p.begin("wifi", false);
    p.putString("ssid", "");
    p.putString("pass", "");
    p.end();
    DISP.fillScreen(BGCOLOR);
    DISP.setTextColor(FGCOLOR, BGCOLOR);
    DISP.setTextSize(SMALL_TEXT);
    DISP.setCursor(0, 40);
    DISP.println("  Disconnected.");
    DISP.println("  Credentials cleared.");
    delay(1500);
    drawmenu(wificfg_menu, wificfg_menu_size);
    return;
  }

  // Connect
  if (opt == 1) {
    char ssid[64] = {}, pass[64] = {};
    // Pre-fill SSID from Preferences
    { Preferences p; p.begin("wifi", true); p.getString("ssid", ssid, sizeof(ssid)); p.end(); }

    if (!keyboard_readline("SSID:", ssid, sizeof(ssid), false)) {
      drawmenu(wificfg_menu, wificfg_menu_size);
      return;
    }
    delay(100);
    if (!keyboard_readline("Password:", pass, sizeof(pass), true)) {
      drawmenu(wificfg_menu, wificfg_menu_size);
      return;
    }

    // Save to Preferences
    Preferences p;
    p.begin("wifi", false);
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.end();

    // Connect with live feedback
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    DISP.fillScreen(BGCOLOR);
    DISP.setTextColor(FGCOLOR, BGCOLOR);
    DISP.setTextSize(SMALL_TEXT);
    DISP.setCursor(0, 0);
    DISP.printf("Connecting to\n%s\n", ssid);

    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
      DISP.print(".");
      M5Cardputer.update();
      delay(500);
    }

    DISP.fillScreen(BGCOLOR);
    DISP.setCursor(0, 0);
    if (WiFi.status() == WL_CONNECTED) {
      DISP.setTextColor(0x07E0, BGCOLOR); // green
      DISP.println("Connected!");
      DISP.setTextColor(FGCOLOR, BGCOLOR);
      DISP.println(WiFi.localIP().toString());
    } else {
      DISP.setTextColor(0xF800, BGCOLOR); // red
      DISP.println("Failed.");
      DISP.setTextColor(FGCOLOR, BGCOLOR);
      DISP.println("Check credentials.");
    }
    delay(2000);
    drawmenu(wificfg_menu, wificfg_menu_size);
  }
}
