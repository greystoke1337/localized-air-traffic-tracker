/*
  OVERHEAD TRACKER — FOXTROT
  Waveshare ESP32-S3-Touch-LCD-4.3B: 800x480 IPS, GT911 capacitive touch, CH422G backlight

  Libraries needed:
    - ESP32_Display_Panel (display + touch driver)
    - LVGL v8.4.x (GUI framework)
    - ArduinoJson (install via Library Manager)
    - SD (built into Arduino ESP32 core)
    - ArduinoOTA (built into Arduino ESP32 core)
*/

#include "esp_display_panel.hpp"
#include "lvgl_v8_port.h"
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <math.h>
#include <Preferences.h>
#include <time.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include "secrets.h"
#include "config.h"
#include "types.h"
#include "globals.h"

// ─── Hardware instances ───────────────────────────────
WebServer  setupServer(80);
DNSServer  dnsServer;

// ─── WiFi ─────────────────────────────────────────────
char WIFI_SSID[64] = WIFI_SSID_DEFAULT;
char WIFI_PASS[64] = WIFI_PASS_DEFAULT;

// ─── Proxy ────────────────────────────────────────────
const char* PROXY_HOST = "api.overheadtracker.com";
const int   PROXY_PORT = 443;

// ─── Location ─────────────────────────────────────────
float HOME_LAT    = 0.0f;
float HOME_LON    = 0.0f;
float GEOFENCE_KM = 10.0f;
int   ALT_FLOOR_FT = 500;
char  LOCATION_NAME[32] = "NOT SET";
char  HOME_QUERY[128]   = "";
bool  needsGeocode      = false;

// ─── SD state ─────────────────────────────────────────
bool sdAvailable = false;

// ─── Geofence presets ─────────────────────────────────
const float GEO_PRESETS[] = {5.0f, 10.0f, 20.0f};
const int   GEO_COUNT     = 3;
int         geoIndex      = 1;

// ─── Touch ────────────────────────────────────────────
bool     touchReady      = false;
uint32_t lastTouchMs     = 0;

// ─── Screen mode ──────────────────────────────────────
ScreenMode currentScreen  = SCREEN_NONE;
ScreenMode previousScreen = SCREEN_NONE;

// ─── Weather ──────────────────────────────────────────
WeatherData wxData;
bool        wxReady         = false;
int         wxCountdown     = 0;
int         lastMinute      = -1;
const int   WX_REFRESH_SECS = 900;

// ─── Flights ──────────────────────────────────────────
Flight flights[20];
Flight newFlights[20];
DynamicJsonDocument g_jsonDoc(32768);  // 32 KB — PSRAM available
int           flightCount  = 0;
int           flightIndex  = 0;
int           countdown    = REFRESH_SECS;
bool          isFetching   = false;
bool          usingCache   = false;
int           dataSource   = 0;
unsigned long lastTick     = 0;
unsigned long lastCycle    = 0;
time_t        cacheTimestamp = 0;

// ─── Direct API robustness ────────────────────────────
int           directApiFailCount   = 0;
unsigned long directApiNextRetryMs = 0;

// ─── Session log ──────────────────────────────────────
char loggedCallsigns[MAX_LOGGED][12];
int  loggedCount = 0;

// ─── Unknown tracking ────────────────────────────────
char loggedUnknowns[MAX_UNKNOWNS][6];
int  loggedUnknownCount = 0;

// ─── Diagnostics ──────────────────────────────────────
unsigned long lastDiagMs   = 0;

// ─── Cross-task trigger flags ─────────────────────────
volatile bool triggerPortal   = false;
volatile bool triggerGeoFetch = false;

// ─── Network task (core 0, avoids lwIP TCPIP core assertion) ──────────
// fetchFlights/fetchWeather must not run on core 1 alongside the LVGL task
// because LVGL preempts the loop task mid-HTTP-teardown and triggers:
//   sys_untimeout: Required to lock TCPIP core functionality!
static SemaphoreHandle_t g_netDone = nullptr;
static bool g_wxResult = false;

static void netTaskFlight(void*) {
    fetchFlights();
    xSemaphoreGive(g_netDone);
    vTaskDelete(NULL);
}
static void netTaskWeather(void*) {
    g_wxResult = fetchWeather();
    xSemaphoreGive(g_netDone);
    vTaskDelete(NULL);
}
static void runFetchFlights() {
    xTaskCreatePinnedToCore(netTaskFlight, "net", 8192, NULL, 5, NULL, 0);
    xSemaphoreTake(g_netDone, pdMS_TO_TICKS(28000));
    esp_task_wdt_reset();
}
static bool runFetchWeather() {
    xTaskCreatePinnedToCore(netTaskWeather, "net", 8192, NULL, 5, NULL, 0);
    xSemaphoreTake(g_netDone, pdMS_TO_TICKS(28000));
    esp_task_wdt_reset();
    return g_wxResult;
}

// ─── LVGL helper: create a temporary full-screen container ──
static lv_obj_t* mkTempScreen() {
  lv_obj_t* scr = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(scr);
  lv_obj_set_size(scr, W, H);
  lv_obj_set_pos(scr, 0, 0);
  lv_obj_set_style_bg_color(scr, lvc(C_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

static lv_obj_t* mkTempHeader(lv_obj_t* par) {
  lv_obj_t* hdr = lv_obj_create(par);
  lv_obj_remove_style_all(hdr);
  lv_obj_set_size(hdr, W, HDR_H);
  lv_obj_set_pos(hdr, 0, 0);
  lv_obj_set_style_bg_color(hdr, lvc(C_AMBER), 0);
  lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* lbl = lv_label_create(hdr);
  lv_obj_set_pos(lbl, 10, 10);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl, lvc(C_BG), 0);
  lv_label_set_text(lbl, "OVERHEAD TRACKER");
  return hdr;
}

// ─── Setup ────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== FOXTROT ===");
  g_netDone = xSemaphoreCreateBinary();

  // Init display hardware via ESP32_Display_Panel (handles CH422G internally)
  auto board = new esp_panel::board::Board();
  board->init();
  static_cast<esp_panel::drivers::BusRGB*>(board->getLCD()->getBus())->configRgbFrameBufferNumber(2);
  board->begin();

  // Init LVGL port (creates LVGL task, sets up flush + touch)
  lvgl_port_init(board->getLCD(), board->getTouch());
  Serial.println("LVGL port initialized");

  // Set screen background
  lvgl_port_lock(-1);
  lv_obj_set_style_bg_color(lv_scr_act(), lvc(C_BG), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
  lvgl_port_unlock();

  bootSequence();

#if DEMO_MODE
  // ── Demo mode: fake data, no WiFi ──
  strlcpy(LOCATION_NAME, "SYDNEY", sizeof(LOCATION_NAME));
  GEOFENCE_KM = 10.0f;

  // Populate fake flights
  memset(flights, 0, sizeof(flights));
  flightCount = 3;
  flightIndex = 0;

  strlcpy(flights[0].callsign, "QFA1", sizeof(flights[0].callsign));
  strlcpy(flights[0].reg,      "VH-OQA", sizeof(flights[0].reg));
  strlcpy(flights[0].type,     "A380", sizeof(flights[0].type));
  strlcpy(flights[0].route,    "SYD > LHR", sizeof(flights[0].route));
  strlcpy(flights[0].squawk,   "1234", sizeof(flights[0].squawk));
  flights[0].alt    = 35000;
  flights[0].speed  = 485;
  flights[0].vs     = 0;
  flights[0].dist   = 3.2f;
  flights[0].status = STATUS_CRUISING;

  strlcpy(flights[1].callsign, "VOZ456", sizeof(flights[1].callsign));
  strlcpy(flights[1].reg,      "VH-YIA", sizeof(flights[1].reg));
  strlcpy(flights[1].type,     "B738", sizeof(flights[1].type));
  strlcpy(flights[1].route,    "MEL > SYD", sizeof(flights[1].route));
  strlcpy(flights[1].squawk,   "3417", sizeof(flights[1].squawk));
  flights[1].alt    = 2800;
  flights[1].speed  = 160;
  flights[1].vs     = -1200;
  flights[1].dist   = 5.7f;
  flights[1].status = STATUS_LANDING;

  strlcpy(flights[2].callsign, "JST621", sizeof(flights[2].callsign));
  strlcpy(flights[2].reg,      "VH-VKA", sizeof(flights[2].reg));
  strlcpy(flights[2].type,     "A320", sizeof(flights[2].type));
  strlcpy(flights[2].route,    "BNE > SYD", sizeof(flights[2].route));
  strlcpy(flights[2].squawk,   "2501", sizeof(flights[2].squawk));
  flights[2].alt    = 12000;
  flights[2].speed  = 340;
  flights[2].vs     = -800;
  flights[2].dist   = 8.1f;
  flights[2].status = STATUS_DESCENDING;

  dataSource = 0;
  countdown  = REFRESH_SECS;
  isFetching = false;

  // Hardware watchdog
  const esp_task_wdt_config_t wdt_cfg = { .timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = true };
  esp_err_t wdtErr = esp_task_wdt_init(&wdt_cfg);
  if (wdtErr == ESP_ERR_INVALID_STATE) esp_task_wdt_reconfigure(&wdt_cfg);
  esp_task_wdt_add(NULL);

  lvgl_port_lock(-1);
  initUI();
  renderFlight(flights[0]);
  lvgl_port_unlock();
  lastCycle = millis();

#else
  // SD card on custom SPI pins
  SPI.begin(12, 13, 11);  // SCK, MISO, MOSI
  if (SD.begin(SD_CS, SPI)) {
    sdAvailable = true;
    Serial.println("SD card ready");
    loadConfig();
  } else {
    Serial.println("SD card not found — continuing without");
  }

  initTouch();

  if (!loadWiFiConfig()) {
    startCaptivePortal();
  }

  // ── WiFi connection screen ──
  lvgl_port_lock(-1);
  lv_obj_t* wifiScr = mkTempScreen();
  mkTempHeader(wifiScr);

  int yBase = HDR_H + 14;
  lv_obj_t* connLbl = lv_label_create(wifiScr);
  lv_obj_set_pos(connLbl, 20, yBase);
  lv_obj_set_style_text_font(connLbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(connLbl, lvc(C_AMBER), 0);
  lv_label_set_text(connLbl, "CONNECTING TO WIFI");

  lv_obj_t* netLbl = lv_label_create(wifiScr);
  lv_obj_set_pos(netLbl, 20, yBase + 28);
  lv_obj_set_style_text_font(netLbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(netLbl, lvc(C_DIM), 0);
  char netBuf[80];
  snprintf(netBuf, sizeof(netBuf), "NETWORK: %s", WIFI_SSID);
  lv_label_set_text(netLbl, netBuf);

  lv_obj_t* wifiBar = lv_bar_create(wifiScr);
  lv_obj_set_pos(wifiBar, 20, yBase + 56);
  lv_obj_set_size(wifiBar, W - 40, 8);
  lv_obj_set_style_bg_color(wifiBar, lvc(C_DIMMER), LV_PART_MAIN);
  lv_obj_set_style_bg_color(wifiBar, lvc(C_AMBER), LV_PART_INDICATOR);
  lv_bar_set_range(wifiBar, 0, 40);
  lv_bar_set_value(wifiBar, 0, LV_ANIM_OFF);

  lv_obj_t* attLbl = lv_label_create(wifiScr);
  lv_obj_set_pos(attLbl, 20, yBase + 80);
  lv_obj_set_style_text_font(attLbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(attLbl, lvc(C_DIMMER), 0);
  lv_label_set_text(attLbl, "ATTEMPTING...");
  lvgl_port_unlock();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    lvgl_port_lock(-1);
    lv_bar_set_value(wifiBar, attempts + 1, LV_ANIM_ON);
    char countBuf[24];
    snprintf(countBuf, sizeof(countBuf), "ATTEMPT %d / 40", attempts + 1);
    lv_label_set_text(attLbl, countBuf);
    lvgl_port_unlock();
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    lvgl_port_lock(-1);
    lv_obj_set_style_bg_color(wifiBar, lvc(C_GREEN), LV_PART_INDICATOR);
    lv_bar_set_value(wifiBar, 40, LV_ANIM_OFF);
    lv_obj_set_style_text_color(attLbl, lvc(C_GREEN), 0);
    lv_label_set_text(attLbl, "CONNECTED");
    lvgl_port_unlock();
    delay(500);

    configTime(0, 0, "pool.ntp.org");
    Serial.println("NTP sync started");

    ArduinoOTA.setHostname("overhead-foxtrot");
    ArduinoOTA.onStart([]() {
      lvgl_port_lock(-1);
      drawOtaProgress(0);
      lvgl_port_unlock();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      lvgl_port_lock(-1);
      drawOtaProgress(progress * 100 / total);
      lvgl_port_unlock();
    });
    ArduinoOTA.onEnd([]() {
      lvgl_port_lock(-1);
      lv_obj_t* lbl = lv_label_create(lv_scr_act());
      lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(lbl, lvc(C_GREEN), 0);
      lv_label_set_text(lbl, "Restarting...");
      lv_obj_center(lbl);
      lvgl_port_unlock();
    });
    ArduinoOTA.onError([](ota_error_t error) {
      lvgl_port_lock(-1);
      lv_obj_t* lbl = lv_label_create(lv_scr_act());
      lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(lbl, lvc(C_RED), 0);
      char buf[32];
      snprintf(buf, sizeof(buf), "OTA Error [%u]", error);
      lv_label_set_text(lbl, buf);
      lv_obj_center(lbl);
      lvgl_port_unlock();
      delay(3000);
      ESP.restart();
    });
    ArduinoOTA.begin();
    Serial.println("OTA ready — overhead-foxtrot.local");
  }

  // Hardware watchdog: 30 s
  const esp_task_wdt_config_t wdt_cfg = { .timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = true };
  esp_err_t wdtErr = esp_task_wdt_init(&wdt_cfg);
  if (wdtErr == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_reconfigure(&wdt_cfg);
  }
  esp_task_wdt_add(NULL);

  // Clean up WiFi screen
  lvgl_port_lock(-1);
  lv_obj_del(wifiScr);
  lvgl_port_unlock();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi failed, attempting cache...");
    String cached = readCache();
    if (!cached.isEmpty()) {
      int n = parsePayload(cached);
      if (n > 0) {
        memcpy(flights, newFlights, sizeof(Flight) * n);
        flightCount = n;
        usingCache  = true;
        dataSource  = 2;
        lvgl_port_lock(-1);
        initUI();
        renderFlight(flights[0]);
        lvgl_port_unlock();
        countdown = REFRESH_SECS;
        return;
      }
    }

    // WiFi failed — show error with touch buttons
    static volatile bool wifiRetryFlag = false;
    static volatile bool wifiReconfigFlag = false;

    lvgl_port_lock(-1);
    lv_obj_t* failScr = mkTempScreen();
    mkTempHeader(failScr);

    lv_obj_t* failLbl = lv_label_create(failScr);
    lv_obj_set_pos(failLbl, 20, HDR_H + 24);
    lv_obj_set_style_text_font(failLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(failLbl, lvc(C_RED), 0);
    lv_label_set_text(failLbl, "WIFI FAILED");

    lv_obj_t* ssidLbl = lv_label_create(failScr);
    lv_obj_set_pos(ssidLbl, 20, HDR_H + 56);
    lv_obj_set_style_text_font(ssidLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ssidLbl, lvc(C_DIM), 0);
    char ssidBuf[80];
    snprintf(ssidBuf, sizeof(ssidBuf), "Could not connect to: %s", WIFI_SSID);
    lv_label_set_text(ssidLbl, ssidBuf);

    int btnY = HDR_H + 90;
    lv_obj_t* reconBtn = lv_btn_create(failScr);
    lv_obj_set_pos(reconBtn, 20, btnY);
    lv_obj_set_size(reconBtn, 320, 56);
    lv_obj_set_style_bg_color(reconBtn, lvc(C_DIMMER), 0);
    lv_obj_set_style_radius(reconBtn, 0, 0);
    lv_obj_t* reconLbl = lv_label_create(reconBtn);
    lv_obj_center(reconLbl);
    lv_obj_set_style_text_font(reconLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(reconLbl, lvc(C_AMBER), 0);
    lv_label_set_text(reconLbl, "RECONFIGURE");
    lv_obj_add_event_cb(reconBtn, [](lv_event_t* e) {
      *((volatile bool*)lv_event_get_user_data(e)) = true;
    }, LV_EVENT_CLICKED, (void*)&wifiReconfigFlag);

    lv_obj_t* retryBtn = lv_btn_create(failScr);
    lv_obj_set_pos(retryBtn, 460, btnY);
    lv_obj_set_size(retryBtn, 320, 56);
    lv_obj_set_style_bg_color(retryBtn, lvc(C_DIMMER), 0);
    lv_obj_set_style_radius(retryBtn, 0, 0);
    lv_obj_t* retryLbl = lv_label_create(retryBtn);
    lv_obj_center(retryLbl);
    lv_obj_set_style_text_font(retryLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(retryLbl, lvc(C_AMBER), 0);
    lv_label_set_text(retryLbl, "RETRY");
    lv_obj_add_event_cb(retryBtn, [](lv_event_t* e) {
      *((volatile bool*)lv_event_get_user_data(e)) = true;
    }, LV_EVENT_CLICKED, (void*)&wifiRetryFlag);
    lvgl_port_unlock();

    while (true) {
      if (wifiRetryFlag) ESP.restart();
      if (wifiReconfigFlag) {
        Preferences p;
        p.begin("tracker", false);
        p.remove("wifi_ssid");
        p.end();
        lvgl_port_lock(-1);
        lv_obj_del(failScr);
        lvgl_port_unlock();
        startCaptivePortal();
      }
      esp_task_wdt_reset();
      delay(50);
    }
  }

  // ── Geocode location name if needed ──
  if (needsGeocode && HOME_QUERY[0]) {
    lvgl_port_lock(-1);
    lv_obj_t* geoScr = mkTempScreen();
    mkTempHeader(geoScr);
    lv_obj_t* geoLbl = lv_label_create(geoScr);
    lv_obj_set_pos(geoLbl, 20, HDR_H + 24);
    lv_obj_set_style_text_font(geoLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(geoLbl, lvc(C_AMBER), 0);
    lv_label_set_text(geoLbl, "LOCATING...");
    lv_obj_t* queryLbl = lv_label_create(geoScr);
    lv_obj_set_pos(queryLbl, 20, HDR_H + 56);
    lv_obj_set_style_text_font(queryLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(queryLbl, lvc(C_DIM), 0);
    lv_label_set_text(queryLbl, HOME_QUERY);
    lvgl_port_unlock();

    if (!geocodeLocation(HOME_QUERY)) {
      static volatile bool geoReconfigFlag = false;
      static volatile bool geoContinueFlag = false;

      lvgl_port_lock(-1);
      lv_obj_clean(geoScr);
      mkTempHeader(geoScr);
      lv_obj_t* gfLbl = lv_label_create(geoScr);
      lv_obj_set_pos(gfLbl, 20, HDR_H + 24);
      lv_obj_set_style_text_font(gfLbl, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(gfLbl, lvc(C_RED), 0);
      lv_label_set_text(gfLbl, "LOCATION NOT FOUND");

      int btnY2 = HDR_H + 100;
      lv_obj_t* reconBtn2 = lv_btn_create(geoScr);
      lv_obj_set_pos(reconBtn2, 20, btnY2);
      lv_obj_set_size(reconBtn2, 320, 56);
      lv_obj_set_style_bg_color(reconBtn2, lvc(C_DIMMER), 0);
      lv_obj_set_style_radius(reconBtn2, 0, 0);
      lv_obj_t* reconLbl2 = lv_label_create(reconBtn2);
      lv_obj_center(reconLbl2);
      lv_obj_set_style_text_font(reconLbl2, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(reconLbl2, lvc(C_AMBER), 0);
      lv_label_set_text(reconLbl2, "RECONFIGURE");
      lv_obj_add_event_cb(reconBtn2, [](lv_event_t* e) {
        *((volatile bool*)lv_event_get_user_data(e)) = true;
      }, LV_EVENT_CLICKED, (void*)&geoReconfigFlag);

      lv_obj_t* contBtn = lv_btn_create(geoScr);
      lv_obj_set_pos(contBtn, 460, btnY2);
      lv_obj_set_size(contBtn, 320, 56);
      lv_obj_set_style_bg_color(contBtn, lvc(C_DIMMER), 0);
      lv_obj_set_style_radius(contBtn, 0, 0);
      lv_obj_t* contLbl = lv_label_create(contBtn);
      lv_obj_center(contLbl);
      lv_obj_set_style_text_font(contLbl, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(contLbl, lvc(C_AMBER), 0);
      lv_label_set_text(contLbl, "CONTINUE");
      lv_obj_add_event_cb(contBtn, [](lv_event_t* e) {
        *((volatile bool*)lv_event_get_user_data(e)) = true;
      }, LV_EVENT_CLICKED, (void*)&geoContinueFlag);
      lvgl_port_unlock();

      while (true) {
        if (geoContinueFlag) break;
        if (geoReconfigFlag) {
          Preferences p; p.begin("tracker", false); p.remove("wifi_ssid"); p.end();
          lvgl_port_lock(-1);
          lv_obj_del(geoScr);
          lvgl_port_unlock();
          startCaptivePortal();
        }
        esp_task_wdt_reset();
        delay(50);
      }
    }

    lvgl_port_lock(-1);
    lv_obj_del(geoScr);
    lvgl_port_unlock();
  }

  // ── Create main UI and start tracking ──
  lvgl_port_lock(-1);
  initUI();
  lvgl_port_unlock();

  runFetchFlights();
  lvgl_port_lock(-1);
  if (flightCount == 0) renderMessage("CLEAR SKIES", "NO AC IN RANGE");
  else { currentScreen = SCREEN_FLIGHT; renderFlight(flights[flightIndex]); }
  lvgl_port_unlock();

  countdown = REFRESH_SECS;
  runFetchWeather();
  wxCountdown = WX_REFRESH_SECS;
#endif
}

// ─── Loop ─────────────────────────────────────────────
void loop() {
  esp_task_wdt_reset();
  unsigned long now = millis();

#if DEMO_MODE
  // Auto-cycle flights every CYCLE_SECS
  if (flightCount > 1 && currentScreen == SCREEN_FLIGHT &&
      now - lastCycle >= (unsigned long)CYCLE_SECS * 1000) {
    lastCycle = now;
    flightIndex = (flightIndex + 1) % flightCount;
    lvgl_port_lock(-1);
    renderFlight(flights[flightIndex]);
    lvgl_port_unlock();
  }

  // Update countdown in status bar every second
  if (now - lastTick >= 1000) {
    lastTick = now;
    countdown--;
    if (countdown <= 0) countdown = REFRESH_SECS;
    if (currentScreen == SCREEN_FLIGHT && flightCount > 0) {
      lvgl_port_lock(-1);
      drawStatusBar();
      lvgl_port_unlock();
    }
  }

#else
  ArduinoOTA.handle();
  if (Serial.available()) checkSerialCmd();

  // Cross-task trigger: captive portal
  if (triggerPortal) {
    triggerPortal = false;
    startCaptivePortal();
  }

  // Cross-task trigger: geo change fetch
  if (triggerGeoFetch) {
    triggerGeoFetch = false;
    runFetchFlights();
    lvgl_port_lock(-1);
    if (flightCount == 0) renderMessage("CLEAR SKIES", "NO AC IN RANGE");
    else { currentScreen = SCREEN_FLIGHT; renderFlight(flights[flightIndex]); }
    lvgl_port_unlock();
    countdown = REFRESH_SECS;
    lastCycle = millis();
  }

  // Periodic diagnostics (every 60 s)
  if (now - lastDiagMs >= 60000) {
    lastDiagMs = now;
    diagReport();
  }

  if (now - lastTick >= 1000) {
    lastTick = now;
    countdown--;
    wxCountdown--;

    if (WiFi.status() != WL_CONNECTED) {
      static unsigned long lastReconnect = 0;
      if (now - lastReconnect > 10000) {
        lastReconnect = now;
        WiFi.reconnect();
      }
    }

    lvgl_port_lock(-1);
    if (currentScreen == SCREEN_FLIGHT && flightCount > 0 && !isFetching) drawStatusBar();

    if (currentScreen == SCREEN_WEATHER) {
      time_t utcNow   = time(NULL);
      time_t localNow = (wxReady && wxData.utc_offset_secs != 0)
                          ? utcNow + wxData.utc_offset_secs : utcNow;
      struct tm* t = gmtime(&localNow);
      int curMin = t->tm_hour * 60 + t->tm_min;
      if (curMin != lastMinute) {
        lastMinute = curMin;
        renderWeather();
      }
    }
    lvgl_port_unlock();

    if (countdown <= 0) {
      runFetchFlights();
      lvgl_port_lock(-1);
      if (flightCount == 0) renderMessage("CLEAR SKIES", "NO AC IN RANGE");
      else { currentScreen = SCREEN_FLIGHT; renderFlight(flights[flightIndex]); }
      lvgl_port_unlock();
      countdown = REFRESH_SECS;
      lastCycle = millis();
    }

    if (wxCountdown <= 0) {
      bool wxOk = runFetchWeather();
      wxCountdown = wxOk ? WX_REFRESH_SECS : 60;
      if (currentScreen == SCREEN_WEATHER) {
        lvgl_port_lock(-1);
        renderWeather();
        lvgl_port_unlock();
      }
    }
  }

  // ── Flight cycling (auto-cycle when multiple flights) ──
  if (flightCount > 1 && currentScreen == SCREEN_FLIGHT &&
      !isFetching && now - lastCycle >= (unsigned long)CYCLE_SECS * 1000) {
    lastCycle = now;
    int fc = flightCount;
    if (fc > 0) {
      flightIndex = (flightIndex + 1) % fc;
      lvgl_port_lock(-1);
      renderFlight(flights[flightIndex]);
      lvgl_port_unlock();
    }
  }
#endif
}
