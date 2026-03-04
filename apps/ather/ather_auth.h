#pragma once
// ather_auth.h — Token lifecycle, config cache, HTTP helpers, log buffer

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <stdarg.h>

// ============================================================
// Log buffer  (circular, ATHER_LOG_LINES entries of ATHER_LOG_LEN chars)
// ============================================================
#define ATHER_LOG_LINES 10
#define ATHER_LOG_LEN   52

char s_log[ATHER_LOG_LINES][ATHER_LOG_LEN];
int  s_log_head  = 0;
int  s_log_count = 0;

void ather_log(const char* fmt, ...) {
  char tmp[ATHER_LOG_LEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, args);
  va_end(args);
  Serial.println(tmp);
  int slot = s_log_head % ATHER_LOG_LINES;
  strncpy(s_log[slot], tmp, ATHER_LOG_LEN - 1);
  s_log[slot][ATHER_LOG_LEN - 1] = '\0';
  s_log_head++;
  if (s_log_count < ATHER_LOG_LINES) s_log_count++;
}

// ============================================================
// Config / credential cache  (loaded from Preferences on boot)
// ============================================================
char s_ather_email[64]  = {};
char s_ather_pass[64]   = {};
char s_ather_base[128]  = {};   // e.g. "https://app.atherenergy.com"
char s_ather_vid[64]    = {};   // vehicle / scooter ID

// ============================================================
// Token cache
// ============================================================
#define ATHER_TOKEN_LEN          400
#define ATHER_TOKEN_LIFETIME_MS  (50UL * 60UL * 1000UL)   // 50 minutes

char          s_ather_access[ATHER_TOKEN_LEN]  = {};
char          s_ather_refresh[ATHER_TOKEN_LEN] = {};
unsigned long s_token_exp = 0;   // millis() when access token expires

// ============================================================
// Low-level HTTP helpers  (called from both cores — WiFi stack is thread-safe)
// ============================================================
static bool _ather_post(const char* endpoint, const char* body,
                        char* resp_buf, int resp_len,
                        const char* bearer = nullptr) {
  if (WiFi.status() != WL_CONNECTED) { ather_log("[ATHER] no wifi"); return false; }
  char url[256];
  snprintf(url, sizeof(url), "%s%s", s_ather_base, endpoint);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, url)) { ather_log("[ATHER] begin failed"); return false; }
  https.addHeader("Content-Type", "application/json");
  if (bearer && bearer[0]) https.addHeader("Authorization", String("Bearer ") + bearer);

  int code = https.POST((uint8_t*)body, strlen(body));
  if (code <= 0 || code >= 400) {
    ather_log("[ATHER] POST %s -> %d", endpoint, code);
    https.end(); return false;
  }
  String payload = https.getString();
  strncpy(resp_buf, payload.c_str(), resp_len - 1);
  resp_buf[resp_len - 1] = '\0';
  https.end();
  return true;
}

static bool _ather_get(const char* url_full, char* resp_buf, int resp_len,
                       const char* bearer) {
  if (WiFi.status() != WL_CONNECTED) { ather_log("[ATHER] no wifi"); return false; }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, url_full)) { ather_log("[ATHER] begin failed"); return false; }
  https.addHeader("Authorization", String("Bearer ") + bearer);

  int code = https.GET();
  if (code <= 0 || code >= 400) {
    ather_log("[ATHER] GET %s -> %d", url_full, code);
    https.end(); return false;
  }
  String payload = https.getString();
  strncpy(resp_buf, payload.c_str(), resp_len - 1);
  resp_buf[resp_len - 1] = '\0';
  https.end();
  return true;
}

// ============================================================
// Auth functions
// ============================================================
void ather_auth_init() {
  Preferences p;
  p.begin("ather", true);
  p.getString("email",   s_ather_email,   sizeof(s_ather_email));
  p.getString("pass",    s_ather_pass,    sizeof(s_ather_pass));
  p.getString("baseurl", s_ather_base,    sizeof(s_ather_base));
  p.getString("vid",     s_ather_vid,     sizeof(s_ather_vid));
  p.getString("access",  s_ather_access,  sizeof(s_ather_access));
  p.getString("refresh", s_ather_refresh, sizeof(s_ather_refresh));
  s_token_exp = p.getULong("exp", 0);
  p.end();
  ather_log("[ATHER] auth init ok");
}

void ather_auth_save_config(const char* email, const char* pass,
                            const char* base_url, const char* vid) {
  strncpy(s_ather_email, email,    sizeof(s_ather_email) - 1);
  strncpy(s_ather_pass,  pass,     sizeof(s_ather_pass)  - 1);
  strncpy(s_ather_base,  base_url, sizeof(s_ather_base)  - 1);
  strncpy(s_ather_vid,   vid,      sizeof(s_ather_vid)   - 1);
  Preferences p;
  p.begin("ather", false);
  p.putString("email",   email);
  p.putString("pass",    pass);
  p.putString("baseurl", base_url);
  p.putString("vid",     vid);
  p.end();
}

static bool _save_access(const char* acc) {
  strncpy(s_ather_access, acc, ATHER_TOKEN_LEN - 1);
  s_token_exp = millis() + ATHER_TOKEN_LIFETIME_MS;
  Preferences p;
  p.begin("ather", false);
  p.putString("access", s_ather_access);
  p.putULong("exp", s_token_exp);
  p.end();
  return true;
}

bool ather_auth_login() {
  ather_log("[ATHER] login...");
  char body[256];
  snprintf(body, sizeof(body),
           "{\"email\":\"%s\",\"password\":\"%s\"}", s_ather_email, s_ather_pass);
  static char resp[512];
  if (!_ather_post("/auth/login", body, resp, sizeof(resp))) return false;

  JsonDocument doc;
  if (deserializeJson(doc, resp) != DeserializationError::Ok) {
    ather_log("[ATHER] login parse err"); return false;
  }
  const char* acc = doc["access_token"];
  const char* ref = doc["refresh_token"];
  if (!acc || !ref) { ather_log("[ATHER] login: bad response"); return false; }

  strncpy(s_ather_refresh, ref, ATHER_TOKEN_LEN - 1);
  Preferences p; p.begin("ather", false); p.putString("refresh", s_ather_refresh); p.end();
  _save_access(acc);
  ather_log("[ATHER] login ok");
  return true;
}

static bool _ather_refresh() {
  ather_log("[ATHER] refreshing token...");
  char body[ATHER_TOKEN_LEN + 32];
  snprintf(body, sizeof(body), "{\"refresh_token\":\"%s\"}", s_ather_refresh);
  static char resp[512];
  if (!_ather_post("/auth/refresh", body, resp, sizeof(resp))) return false;

  JsonDocument doc;
  if (deserializeJson(doc, resp) != DeserializationError::Ok) return false;
  const char* acc = doc["access_token"];
  if (!acc) return false;
  _save_access(acc);
  ather_log("[ATHER] token refreshed");
  return true;
}

// Returns true and copies a valid Bearer token into out[maxlen].
bool ather_auth_get_token(char* out, int maxlen) {
  bool expired = (s_token_exp == 0 || millis() >= s_token_exp ||
                  s_ather_access[0] == '\0');
  if (!expired) {
    strncpy(out, s_ather_access, maxlen - 1); out[maxlen - 1] = '\0';
    return true;
  }
  if (s_ather_refresh[0] != '\0' && _ather_refresh()) {
    strncpy(out, s_ather_access, maxlen - 1); out[maxlen - 1] = '\0';
    return true;
  }
  if (s_ather_email[0] != '\0' && ather_auth_login()) {
    strncpy(out, s_ather_access, maxlen - 1); out[maxlen - 1] = '\0';
    return true;
  }
  ather_log("[ATHER] auth failed");
  return false;
}
