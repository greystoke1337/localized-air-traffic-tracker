/*
  STEP 4 DIAGNOSTIC — Waveshare ESP32-S3-Touch-LCD-4.3B
  Validates: WiFi + HTTPS fetch of live flight data + LVGL list render

  Sequence:
    1. Connect WiFi (before LVGL starts — zero lwIP threading conflicts)
    2. TLS GET /flights?lat=...&lon=...&radius=15 from proxy
    3. Parse ac[] array — extract callsign, altitude, speed, type
    4. Init LVGL + display
    5. Render flight list on screen

  Create secrets.h in this folder (gitignored):
    #define WIFI_SSID "YourNetwork"
    #define WIFI_PASS "YourPassword"
*/

#include "esp_display_panel.hpp"
#include "lvgl_v8_port.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "secrets.h"

#ifndef WIFI_SSID
#define WIFI_SSID WIFI_SSID_DEFAULT
#endif
#ifndef WIFI_PASS
#define WIFI_PASS WIFI_PASS_DEFAULT
#endif

#define PROXY_HOST "api.overheadtracker.com"
#define PROXY_PORT 443
#define HOME_LAT   "-33.8530"
#define HOME_LON   "151.1410"
#define RADIUS     "15"
#define MAX_AC     6

struct AcEntry {
  char callsign[10];
  char reg[12];
  char type[8];
  int  alt;
  int  speed;
};

static char     statusBuf[128] = "WIFI FAILED";
static AcEntry  aircraft[MAX_AC];
static int      acCount = 0;
static int      acTotal = 0;   // "total" field from response
static char     fetchStatus[64] = "no data";

// ── JSON helpers ────────────────────────────────────────────────────────

static long extractJsonLong(const String& s, const char* key) {
  String search = String("\"") + key + "\":";
  int idx = s.indexOf(search);
  if (idx < 0) return -1;
  idx += search.length();
  while (idx < (int)s.length() && (s[idx] == ' ' || s[idx] == '\t')) idx++;
  return s.substring(idx).toInt();
}

static bool extractJsonStr(const String& s, const char* key, char* out, int outLen) {
  String search = String("\"") + key + "\":\"";
  int idx = s.indexOf(search);
  if (idx < 0) { out[0] = '\0'; return false; }
  idx += search.length();
  int end = s.indexOf('"', idx);
  if (end < 0) { out[0] = '\0'; return false; }
  s.substring(idx, end).toCharArray(out, outLen);
  // trim trailing spaces (callsigns are often padded)
  for (int i = strlen(out) - 1; i >= 0 && out[i] == ' '; i--) out[i] = '\0';
  return true;
}

// Extract up to maxEntries objects from the "ac":[...] array.
static int parseAcArray(const String& body, AcEntry* entries, int maxEntries) {
  int acStart = body.indexOf("\"ac\":[");
  if (acStart < 0) return 0;
  acStart += 6;  // skip past "ac":[

  int count = 0;
  int pos   = acStart;

  while (count < maxEntries) {
    int objStart = body.indexOf('{', pos);
    if (objStart < 0) break;

    // Find matching closing brace (depth-tracked)
    int depth = 0, objEnd = -1;
    for (int i = objStart; i < (int)body.length(); i++) {
      if      (body[i] == '{') depth++;
      else if (body[i] == '}') { if (--depth == 0) { objEnd = i; break; } }
    }
    if (objEnd < 0) break;

    String obj = body.substring(objStart, objEnd + 1);

    AcEntry& e = entries[count];
    extractJsonStr(obj, "flight", e.callsign, sizeof(e.callsign));
    extractJsonStr(obj, "r",      e.reg,      sizeof(e.reg));
    extractJsonStr(obj, "t",      e.type,     sizeof(e.type));
    long alt   = extractJsonLong(obj, "alt_baro");
    long speed = extractJsonLong(obj, "gs");
    e.alt   = (int)(alt   > 0 ? alt   : 0);
    e.speed = (int)(speed > 0 ? speed : 0);

    // Skip groundlevel blobs with nothing useful
    if (e.callsign[0] != '\0' || e.alt > 500) count++;
    pos = objEnd + 1;
  }
  return count;
}

// ── Setup ────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== STEP 4: WiFi + flights fetch + LVGL list ===");

  // ── Phase 1: Network (LVGL task not running yet) ──────────────────────
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500); Serial.print("."); attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    snprintf(statusBuf, sizeof(statusBuf), "CONNECTED: %s", WiFi.localIP().toString().c_str());
    Serial.println(statusBuf);
    delay(1500);

    WiFiClientSecure client;
    client.setInsecure();

    Serial.printf("TLS connect to %s:%d ...\n", PROXY_HOST, PROXY_PORT);
    if (client.connect(PROXY_HOST, PROXY_PORT, 10000)) {
      Serial.println("TLS OK — sending request");
      client.print(
        "GET /flights?lat=" HOME_LAT "&lon=" HOME_LON "&radius=" RADIUS
        " HTTP/1.0\r\nHost: " PROXY_HOST "\r\nConnection: close\r\n\r\n"
      );

      // Wait for first byte
      unsigned long t = millis();
      while (!client.available() && millis() - t < 12000) delay(10);

      // Read response (cap at 32 KB — well within PSRAM budget)
      String response = "";
      response.reserve(16384);
      while (client.connected() || client.available()) {
        if (client.available()) {
          response += (char)client.read();
          if ((int)response.length() >= 32768) break;
        } else {
          delay(5);
        }
      }
      client.stop();
      Serial.printf("Response: %d bytes\n", response.length());

      // Strip HTTP headers
      int bodyStart = response.indexOf("\r\n\r\n");
      String body = (bodyStart >= 0) ? response.substring(bodyStart + 4) : response;
      body.trim();
      Serial.printf("Body (first 120): %.120s\n", body.c_str());

      acTotal = (int)extractJsonLong(body, "total");
      acCount = parseAcArray(body, aircraft, MAX_AC);

      Serial.printf("total=%d  parsed=%d\n", acTotal, acCount);
      for (int i = 0; i < acCount; i++) {
        Serial.printf("  [%d] %-9s  %5d ft  %3d kts  %s  %s\n",
          i, aircraft[i].callsign, aircraft[i].alt, aircraft[i].speed,
          aircraft[i].type, aircraft[i].reg);
      }
      snprintf(fetchStatus, sizeof(fetchStatus), "OK — %d ac returned", acTotal);
    } else {
      Serial.println("TLS connect FAILED");
      snprintf(fetchStatus, sizeof(fetchStatus), "TLS FAILED");
    }
  } else {
    Serial.println("WiFi failed!");
  }

  // ── Phase 2: Display (LVGL task starts here) ──────────────────────────
  auto board = new esp_panel::board::Board();
  board->init();
  auto* rgbBus = static_cast<esp_panel::drivers::BusRGB*>(board->getLCD()->getBus());
  rgbBus->configRgbFrameBufferNumber(2);
  board->begin();
  lvgl_port_init(board->getLCD(), board->getTouch());
  Serial.println("LVGL initialized");

  lvgl_port_lock(-1);

  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

  // WiFi status row
  lv_obj_t* lblWifi = lv_label_create(lv_scr_act());
  lv_obj_set_pos(lblWifi, 20, 10);
  lv_obj_set_style_text_font(lblWifi, &lv_font_montserrat_12, 0);
  bool connected = (WiFi.status() == WL_CONNECTED);
  lv_obj_set_style_text_color(lblWifi, connected ? lv_color_hex(0x00cc44) : lv_color_hex(0xff3333), 0);
  lv_label_set_text(lblWifi, statusBuf);

  // Fetch status row
  lv_obj_t* lblFetch = lv_label_create(lv_scr_act());
  lv_obj_set_pos(lblFetch, 20, 30);
  lv_obj_set_style_text_font(lblFetch, &lv_font_montserrat_12, 0);
  bool fetchOk = (strncmp(fetchStatus, "OK", 2) == 0);
  lv_obj_set_style_text_color(lblFetch, fetchOk ? lv_color_hex(0x00cc44) : lv_color_hex(0xff8800), 0);
  char fetchLine[80];
  snprintf(fetchLine, sizeof(fetchLine), "PROXY: %s", fetchStatus);
  lv_label_set_text(lblFetch, fetchLine);

  // Divider
  lv_obj_t* div = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(div, 0, 56);
  lv_obj_set_size(div, 800, 1);
  lv_obj_set_style_bg_color(div, lv_color_hex(0x333333), 0);
  lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(div, 0, 0);
  lv_obj_set_style_pad_all(div, 0, 0);

  if (acCount == 0) {
    lv_obj_t* lbl = lv_label_create(lv_scr_act());
    lv_obj_set_pos(lbl, 20, 80);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x555555), 0);
    lv_label_set_text(lbl, fetchOk ? "NO AIRCRAFT IN RANGE" : "FETCH FAILED");
  } else {
    // Flight list rows — 6 rows, 62 px each, starting at y=65
    for (int i = 0; i < acCount; i++) {
      int y = 65 + i * 62;
      AcEntry& e = aircraft[i];

      // Callsign
      lv_obj_t* lblCs = lv_label_create(lv_scr_act());
      lv_obj_set_pos(lblCs, 20, y);
      lv_obj_set_style_text_font(lblCs, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(lblCs, lv_color_hex(0xffffff), 0);
      char csLine[32];
      snprintf(csLine, sizeof(csLine), "%s", e.callsign[0] ? e.callsign : "(no callsign)");
      lv_label_set_text(lblCs, csLine);

      // Alt + speed (right of callsign, offset x=180)
      lv_obj_t* lblAlt = lv_label_create(lv_scr_act());
      lv_obj_set_pos(lblAlt, 180, y);
      lv_obj_set_style_text_font(lblAlt, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(lblAlt, lv_color_hex(0x00aaff), 0);
      char altLine[32];
      if (e.alt > 0) snprintf(altLine, sizeof(altLine), "%d ft", e.alt);
      else           snprintf(altLine, sizeof(altLine), "--- ft");
      lv_label_set_text(lblAlt, altLine);

      lv_obj_t* lblSpd = lv_label_create(lv_scr_act());
      lv_obj_set_pos(lblSpd, 380, y);
      lv_obj_set_style_text_font(lblSpd, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(lblSpd, lv_color_hex(0x888888), 0);
      char spdLine[24];
      if (e.speed > 0) snprintf(spdLine, sizeof(spdLine), "%d kts", e.speed);
      else             snprintf(spdLine, sizeof(spdLine), "--- kts");
      lv_label_set_text(lblSpd, spdLine);

      // Type + reg (second line)
      lv_obj_t* lblType = lv_label_create(lv_scr_act());
      lv_obj_set_pos(lblType, 20, y + 24);
      lv_obj_set_style_text_font(lblType, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(lblType, lv_color_hex(0x555555), 0);
      char typeLine[32];
      snprintf(typeLine, sizeof(typeLine), "%s  %s",
               e.type[0] ? e.type : "?",
               e.reg[0]  ? e.reg  : "");
      lv_label_set_text(lblType, typeLine);

      // Row separator
      lv_obj_t* sep = lv_obj_create(lv_scr_act());
      lv_obj_set_pos(sep, 0, y + 44);
      lv_obj_set_size(sep, 800, 1);
      lv_obj_set_style_bg_color(sep, lv_color_hex(0x1a1a1a), 0);
      lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(sep, 0, 0);
      lv_obj_set_style_pad_all(sep, 0, 0);
    }
  }

  // Footer
  lv_obj_t* lblFooter = lv_label_create(lv_scr_act());
  lv_obj_set_pos(lblFooter, 20, 455);
  lv_obj_set_style_text_font(lblFooter, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lblFooter, lv_color_hex(0x444444), 0);
  char footerLine[80];
  snprintf(footerLine, sizeof(footerLine),
           "api.overheadtracker.com/flights  --  step 4 / n  --  %d ac", acTotal);
  lv_label_set_text(lblFooter, footerLine);

  lvgl_port_unlock();
}

void loop() {
  delay(1000);
}
