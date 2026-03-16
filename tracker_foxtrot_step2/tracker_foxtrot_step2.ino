/*
  STEP 2 DIAGNOSTIC — Waveshare ESP32-S3-Touch-LCD-4.3B
  Validates: WiFi connection + single HTTP fetch + LVGL display

  Sequence:
    1. Connect WiFi (before LVGL starts — zero lwIP threading conflicts)
    2. GET http://api.ipify.org?format=text  (returns public IP as plain text)
    3. Init LVGL + display
    4. Show results on screen

  Create secrets.h in this folder (gitignored):
    #define WIFI_SSID "YourNetwork"
    #define WIFI_PASS "YourPassword"
*/

#include "esp_display_panel.hpp"
#include "lvgl_v8_port.h"
#include <WiFi.h>
#include "secrets.h"

#ifndef WIFI_SSID
#define WIFI_SSID WIFI_SSID_DEFAULT
#endif
#ifndef WIFI_PASS
#define WIFI_PASS WIFI_PASS_DEFAULT
#endif

static char statusBuf[128] = "WIFI FAILED";
static char resultBuf[64]  = "no connection";

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== STEP 2: WiFi + HTTP fetch ===");

  // ── Phase 1: Network (LVGL task not running yet — no threading conflicts) ──
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
    delay(1500);  // let DNS/DHCP settle after WL_CONNECTED

    IPAddress dnsTest;
    int dnsOk = WiFi.hostByName("api.ipify.org", dnsTest);
    Serial.printf("DNS: %s -> %s\n", dnsOk ? "OK" : "FAIL", dnsTest.toString().c_str());

    WiFiClient client;
    Serial.println("TCP connect...");
    if (client.connect("api.ipify.org", 80, 8000)) {
      Serial.println("TCP OK — sending request");
      client.print("GET /?format=text HTTP/1.0\r\nHost: api.ipify.org\r\nConnection: close\r\n\r\n");
      unsigned long t = millis();
      while (!client.available() && millis() - t < 8000) delay(10);
      String response = "";
      while (client.available()) response += (char)client.read();
      response.trim();
      int lastNl = response.lastIndexOf('\n');
      String ip = (lastNl >= 0) ? response.substring(lastNl + 1) : response;
      ip.trim();
      strlcpy(resultBuf, ip.c_str(), sizeof(resultBuf));
      client.stop();
    } else {
      Serial.println("TCP FAILED");
      snprintf(resultBuf, sizeof(resultBuf), "TCP FAILED");
    }
    Serial.printf("Result: %s\n", resultBuf);
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

  // Fetched result
  lv_obj_t* lbl2 = lv_label_create(lv_scr_act());
  lv_obj_set_pos(lbl2, 20, 60);
  lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl2, lv_color_hex(0xffffff), 0);
  char dispBuf[80];
  snprintf(dispBuf, sizeof(dispBuf), "PUBLIC IP: %s", resultBuf);
  lv_label_set_text(lbl2, dispBuf);

  // Source label
  lv_obj_t* lbl3 = lv_label_create(lv_scr_act());
  lv_obj_set_pos(lbl3, 20, 440);
  lv_obj_set_style_text_font(lbl3, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lbl3, lv_color_hex(0x555555), 0);
  lv_label_set_text(lbl3, "api.ipify.org  --  step 2 / n");

  lvgl_port_unlock();
}

void loop() {
  delay(1000);
}
