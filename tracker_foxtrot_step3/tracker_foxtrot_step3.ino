/*
  STEP 3 DIAGNOSTIC — Waveshare ESP32-S3-Touch-LCD-4.3B
  Validates: WiFi + HTTPS fetch from actual proxy + LVGL display

  Sequence:
    1. Connect WiFi (before LVGL starts — zero lwIP threading conflicts)
    2. TLS connect to api.overheadtracker.com:443, GET /status
    3. Extract "uptime" value from JSON response
    4. Init LVGL + display
    5. Show results on screen

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

static char statusBuf[128] = "WIFI FAILED";
static char proxyBuf[128]  = "no connection";

// Extract integer value after "key": in a JSON string.
// Returns -1 if not found.
static long extractJsonLong(const String& json, const char* key) {
  String search = String("\"") + key + "\":";
  int idx = json.indexOf(search);
  if (idx < 0) return -1;
  idx += search.length();
  while (idx < (int)json.length() && (json[idx] == ' ' || json[idx] == '\t')) idx++;
  return json.substring(idx).toInt();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== STEP 3: WiFi + HTTPS proxy fetch ===");

  // ── Phase 1: Network (LVGL task not running yet) ──
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    snprintf(statusBuf, sizeof(statusBuf), "CONNECTED: %s", WiFi.localIP().toString().c_str());
    Serial.println(statusBuf);
    delay(1500);  // let DNS/DHCP settle

    WiFiClientSecure client;
    client.setInsecure();  // skip cert verification — diagnostic only

    Serial.printf("TLS connect to %s:%d ...\n", PROXY_HOST, PROXY_PORT);
    if (client.connect(PROXY_HOST, PROXY_PORT, 10000)) {
      Serial.println("TLS OK — sending request");
      client.print("GET /status HTTP/1.0\r\nHost: " PROXY_HOST "\r\nConnection: close\r\n\r\n");

      unsigned long t = millis();
      while (!client.available() && millis() - t < 10000) delay(10);

      String response = "";
      while (client.available()) response += (char)client.read();
      client.stop();
      Serial.printf("Response length: %d bytes\n", response.length());

      // Find the JSON body (after the blank line separating headers from body)
      int bodyStart = response.indexOf("\r\n\r\n");
      String body = (bodyStart >= 0) ? response.substring(bodyStart + 4) : response;
      body.trim();
      Serial.printf("Body (first 200): %.200s\n", body.c_str());

      long uptime = extractJsonLong(body, "uptime");
      long ramFree = -1;
      // "ram":{"total":...,"free":...}
      int ramIdx = body.indexOf("\"ram\":");
      if (ramIdx >= 0) {
        String ramSlice = body.substring(ramIdx);
        ramFree = extractJsonLong(ramSlice, "free");
      }

      if (uptime >= 0) {
        long days = uptime / 86400;
        long hrs  = (uptime % 86400) / 3600;
        long mins = (uptime % 3600) / 60;
        if (ramFree >= 0) {
          snprintf(proxyBuf, sizeof(proxyBuf), "UP %ldd %ldh %ldm  RAM %ldMB free",
                   days, hrs, mins, ramFree / (1024 * 1024));
        } else {
          snprintf(proxyBuf, sizeof(proxyBuf), "UP %ldd %ldh %ldm", days, hrs, mins);
        }
        Serial.printf("Proxy status: %s\n", proxyBuf);
      } else {
        Serial.println("JSON parse failed — body:");
        Serial.println(body.substring(0, 300));
        snprintf(proxyBuf, sizeof(proxyBuf), "PARSE FAILED (%d bytes)", body.length());
      }
    } else {
      Serial.println("TLS connect FAILED");
      snprintf(proxyBuf, sizeof(proxyBuf), "TLS FAILED");
    }
  } else {
    Serial.println("WiFi failed!");
  }

  // ── Phase 2: Display (LVGL task starts here) ──
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

  // WiFi status
  lv_obj_t* lbl1 = lv_label_create(lv_scr_act());
  lv_obj_set_pos(lbl1, 20, 20);
  lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_16, 0);
  bool connected = (WiFi.status() == WL_CONNECTED);
  lv_obj_set_style_text_color(lbl1, connected ? lv_color_hex(0x00cc44) : lv_color_hex(0xff3333), 0);
  lv_label_set_text(lbl1, statusBuf);

  // Proxy result
  lv_obj_t* lbl2 = lv_label_create(lv_scr_act());
  lv_obj_set_pos(lbl2, 20, 60);
  lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_16, 0);
  bool proxyOk = (strncmp(proxyBuf, "UP ", 3) == 0);
  lv_obj_set_style_text_color(lbl2, proxyOk ? lv_color_hex(0x00cc44) : lv_color_hex(0xff8800), 0);
  char dispBuf[128];
  snprintf(dispBuf, sizeof(dispBuf), "PROXY: %s", proxyBuf);
  lv_label_set_text(lbl2, dispBuf);

  // Source label
  lv_obj_t* lbl3 = lv_label_create(lv_scr_act());
  lv_obj_set_pos(lbl3, 20, 440);
  lv_obj_set_style_text_font(lbl3, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl3, lv_color_hex(0x555555), 0);
  lv_label_set_text(lbl3, "api.overheadtracker.com/status  --  step 3 / n");

  lvgl_port_unlock();
}

void loop() {
  delay(1000);
}
