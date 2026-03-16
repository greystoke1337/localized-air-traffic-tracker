/*
  OVERHEAD TRACKER — FOXTROT
  Waveshare ESP32-S3-Touch-LCD-4.3: 800x480 IPS, GT911 capacitive touch
  Rendering: LovyanGFX (immediate-mode, no LVGL, no FreeRTOS render task)

  Libraries needed:
    - LovyanGFX (display + touch driver)
    - ArduinoJson (install via Library Manager)
    - SD (built into Arduino ESP32 core)
    - ArduinoOTA (built into Arduino ESP32 core)
*/

#include "lgfx_config.h"
#include "ch422g.h"
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

// ─── Hardware instance ────────────────────────────────
LGFX tft;

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
bool     touchReady  = false;
uint32_t lastTouchMs = 0;

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
DynamicJsonDocument g_jsonDoc(32768);
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
unsigned long lastDiagMs = 0;

// ─── Cross-task trigger flags ─────────────────────────
volatile bool triggerPortal   = false;
volatile bool triggerGeoFetch = false;

// ─── Temp WiFi screen helpers (tft direct) ────────────
static void drawTempHeader() {
  tft.fillRect(0, 0, W, HDR_H, C_AMBER);
  tft.setFont(&lgfx::fonts::DejaVu24);
  tft.setTextColor(C_BG, C_AMBER);
  tft.setTextDatum(lgfx::middle_left);
  tft.drawString("OVERHEAD TRACKER", 16, HDR_H / 2);
  tft.setTextDatum(lgfx::top_left);
}

static void drawTempMsg(int y, const lgfx::IFont* f, uint16_t col, const char* txt) {
  tft.setFont(f);
  tft.setTextColor(col);
  tft.setTextDatum(lgfx::top_left);
  tft.drawString(txt, 20, y);
}

// ─── Load demo flights (shared by DEMO_MODE and DIAG_STEP) ──
static void loadDemoFlights() {
  strlcpy(LOCATION_NAME, "SYDNEY", sizeof(LOCATION_NAME));
  GEOFENCE_KM = 10.0f;
  memset(flights, 0, sizeof(flights));
  flightCount = 3;
  flightIndex = 0;

  strlcpy(flights[0].callsign, "QFA1",   sizeof(flights[0].callsign));
  strlcpy(flights[0].reg,      "VH-OQA", sizeof(flights[0].reg));
  strlcpy(flights[0].type,     "A380",   sizeof(flights[0].type));
  strlcpy(flights[0].route,    "SYD > LHR", sizeof(flights[0].route));
  strlcpy(flights[0].squawk,   "1234",   sizeof(flights[0].squawk));
  flights[0].alt = 35000; flights[0].speed = 485; flights[0].vs = 0;
  flights[0].dist = 3.2f; flights[0].status = STATUS_CRUISING;

  strlcpy(flights[1].callsign, "VOZ456", sizeof(flights[1].callsign));
  strlcpy(flights[1].reg,      "VH-YIA", sizeof(flights[1].reg));
  strlcpy(flights[1].type,     "B738",   sizeof(flights[1].type));
  strlcpy(flights[1].route,    "MEL > SYD", sizeof(flights[1].route));
  strlcpy(flights[1].squawk,   "3417",   sizeof(flights[1].squawk));
  flights[1].alt = 2800; flights[1].speed = 160; flights[1].vs = -1200;
  flights[1].dist = 5.7f; flights[1].status = STATUS_LANDING;

  strlcpy(flights[2].callsign, "JST621", sizeof(flights[2].callsign));
  strlcpy(flights[2].reg,      "VH-VKA", sizeof(flights[2].reg));
  strlcpy(flights[2].type,     "A320",   sizeof(flights[2].type));
  strlcpy(flights[2].route,    "BNE > SYD", sizeof(flights[2].route));
  strlcpy(flights[2].squawk,   "2501",   sizeof(flights[2].squawk));
  flights[2].alt = 12000; flights[2].speed = 340; flights[2].vs = -800;
  flights[2].dist = 8.1f; flights[2].status = STATUS_DESCENDING;
}

// ─── DIAG_STEP finish: render demo flights and stop ──
static void diagFinish(int step) {
  Serial.printf("DIAG_STEP %d complete — rendering demo flights\n", step);
  loadDemoFlights();
  dataSource = 0;
  countdown  = REFRESH_SECS;
  isFetching = false;
  const esp_task_wdt_config_t wdt_cfg = { .timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = true };
  esp_err_t wdtErr = esp_task_wdt_init(&wdt_cfg);
  if (wdtErr == ESP_ERR_INVALID_STATE) esp_task_wdt_reconfigure(&wdt_cfg);
  esp_task_wdt_add(NULL);
  initUI();
  renderFlight(flights[0]);
}

// ─── Setup ────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== FOXTROT ===");

  ch422gInit();
  ch422gResetTouch();

  tft.init();
  tft.setRotation(0);
  Serial.println("LovyanGFX initialized");

  bootSequence();

#if DEMO_MODE
  loadDemoFlights();
  dataSource = 0;
  countdown  = REFRESH_SECS;
  isFetching = false;

  const esp_task_wdt_config_t wdt_cfg = { .timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = true };
  esp_err_t wdtErr = esp_task_wdt_init(&wdt_cfg);
  if (wdtErr == ESP_ERR_INVALID_STATE) esp_task_wdt_reconfigure(&wdt_cfg);
  esp_task_wdt_add(NULL);

  initUI();
  renderFlight(flights[0]);
  lastCycle = millis();

#elif DIAG_STEP
  // Progressive hardware init for blue-tint diagnosis.
  // Each step adds one subsystem. All steps render demo flights at the end.
  // Compare display quality at each step to isolate the culprit.
  Serial.printf("DIAG_STEP = %d\n", DIAG_STEP);

#if DIAG_STEP >= 2
  Serial.println("DIAG: SPI + SD init");
  ch422gSetPin(EXIO_SD_CS, false);
  SPI.begin(12, 13, 11);
  if (SD.begin(6, SPI)) {
    sdAvailable = true;
    Serial.println("DIAG: SD card ready");
  } else {
    Serial.println("DIAG: SD card not found");
  }
#endif

#if DIAG_STEP >= 3
  Serial.println("DIAG: initTouch()");
  initTouch();
#endif

#if DIAG_STEP >= 4
  Serial.println("DIAG: WiFi.begin() — radio init only");
  if (!loadWiFiConfig()) {
    Serial.println("DIAG: No WiFi config in NVS");
  } else {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
#if DIAG_STEP == 4
    delay(2000);
    WiFi.disconnect(true);
    Serial.println("DIAG: WiFi disconnected after 2s radio init");
#else
    // Steps 5–6: full connect
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
      delay(500);
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("DIAG: WiFi connected (%d attempts)\n", attempts);
    } else {
      Serial.println("DIAG: WiFi failed to connect");
    }
#endif
  }
#endif

#if DIAG_STEP == 5
  Serial.println("DIAG: Re-initializing display after WiFi");
  tft.init();
  tft.setRotation(0);
  Serial.println("DIAG: Display re-init complete");
#endif

  diagFinish(DIAG_STEP);
  lastCycle = millis();

#if DIAG_STEP == 6
  Serial.println("DIAG: fetchFlights() — full live fetch");
  fetchFlights();
  if (flightCount > 0) renderFlight(flights[0]);
#endif

#else
  ch422gSetPin(EXIO_SD_CS, false);  // Assert real CS via expander
  SPI.begin(12, 13, 11);  // SCK, MISO, MOSI — GPIO 10 NOT touched
  if (SD.begin(6, SPI)) {
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

  // WiFi connecting screen
  tft.fillScreen(C_BG);
  drawTempHeader();
  int yBase = HDR_H + 24;
  drawTempMsg(yBase,      &lgfx::fonts::DejaVu40, C_AMBER, "CONNECTING TO WIFI");
  char netBuf[80];
  snprintf(netBuf, sizeof(netBuf), "NETWORK: %s", WIFI_SSID);
  drawTempMsg(yBase + 52, &lgfx::fonts::DejaVu24, C_DIM,   netBuf);

  // Progress bar
  int barY = yBase + 100;
  tft.drawRect(20, barY, W - 40, 14, C_DIMMER);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    int barW = (int)((W - 40) * (attempts + 1) / 40);
    tft.fillRect(20, barY, barW, 14, C_AMBER);
    char countBuf[24];
    snprintf(countBuf, sizeof(countBuf), "ATTEMPT %d / 40", attempts + 1);
    tft.fillRect(20, barY + 24, W - 40, 24, C_BG);
    drawTempMsg(barY + 24, &lgfx::fonts::DejaVu18, C_DIMMER, countBuf);
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    tft.fillRect(20, barY, W - 40, 14, C_GREEN);
    tft.fillRect(20, barY + 24, W - 40, 24, C_BG);
    drawTempMsg(barY + 24, &lgfx::fonts::DejaVu18, C_GREEN, "CONNECTED");
    delay(500);

    configTime(0, 0, "pool.ntp.org");
    Serial.println("NTP sync started");

    ArduinoOTA.setHostname("overhead-foxtrot");
    ArduinoOTA.onStart([]() { drawOtaProgress(0); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      drawOtaProgress(progress * 100 / total);
    });
    ArduinoOTA.onEnd([]() {
      tft.setFont(&lgfx::fonts::DejaVu40);
      tft.setTextColor(C_GREEN);
      tft.setTextDatum(lgfx::middle_center);
      tft.drawString("Restarting...", W / 2, H / 2);
      tft.setTextDatum(lgfx::top_left);
    });
    ArduinoOTA.onError([](ota_error_t error) {
      tft.setFont(&lgfx::fonts::DejaVu40);
      tft.setTextColor(C_RED);
      tft.setTextDatum(lgfx::middle_center);
      char buf[32]; snprintf(buf, sizeof(buf), "OTA Error [%u]", error);
      tft.drawString(buf, W / 2, H / 2);
      tft.setTextDatum(lgfx::top_left);
      delay(3000);
      ESP.restart();
    });
    ArduinoOTA.begin();
    Serial.println("OTA ready — overhead-foxtrot.local");
  }

  const esp_task_wdt_config_t wdt_cfg = { .timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = true };
  esp_err_t wdtErr = esp_task_wdt_init(&wdt_cfg);
  if (wdtErr == ESP_ERR_INVALID_STATE) esp_task_wdt_reconfigure(&wdt_cfg);
  esp_task_wdt_add(NULL);

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
        initUI();
        renderFlight(flights[0]);
        countdown = REFRESH_SECS;
        return;
      }
    }

    // WiFi failed — show error with touch buttons
    static volatile bool wifiRetryFlag    = false;
    static volatile bool wifiReconfigFlag = false;

    tft.fillScreen(C_BG);
    drawTempHeader();
    drawTempMsg(HDR_H + 24, &lgfx::fonts::DejaVu40, C_RED,   "WIFI FAILED");
    char ssidBuf[80];
    snprintf(ssidBuf, sizeof(ssidBuf), "Could not connect to: %s", WIFI_SSID);
    drawTempMsg(HDR_H + 76, &lgfx::fonts::DejaVu24, C_DIM, ssidBuf);

    int btnY = HDR_H + 130;
    // RECONFIGURE button
    tft.fillRect(20, btnY, 340, 70, C_DIMMER);
    tft.setFont(&lgfx::fonts::DejaVu24);
    tft.setTextColor(C_AMBER, C_DIMMER);
    tft.setTextDatum(lgfx::middle_center);
    tft.drawString("RECONFIGURE", 20 + 170, btnY + 35);
    // RETRY button
    tft.fillRect(440, btnY, 340, 70, C_DIMMER);
    tft.drawString("RETRY", 440 + 170, btnY + 35);
    tft.setTextDatum(lgfx::top_left);

    while (true) {
      lgfx::touch_point_t tp;
      if (tft.getTouch(&tp) && millis() - lastTouchMs > TOUCH_DEBOUNCE_MS) {
        lastTouchMs = millis();
        if (tp.y >= btnY && tp.y < btnY + 70) {
          if (tp.x >= 20 && tp.x < 360)  wifiReconfigFlag = true;
          if (tp.x >= 440 && tp.x < 780) wifiRetryFlag    = true;
        }
      }
      if (wifiRetryFlag) ESP.restart();
      if (wifiReconfigFlag) {
        Preferences p; p.begin("tracker", false); p.remove("wifi_ssid"); p.end();
        startCaptivePortal();
      }
      esp_task_wdt_reset();
      delay(50);
    }
  }

  // Geocode location name if needed
  if (needsGeocode && HOME_QUERY[0]) {
    tft.fillScreen(C_BG);
    drawTempHeader();
    drawTempMsg(HDR_H + 24, &lgfx::fonts::DejaVu40, C_AMBER, "LOCATING...");
    drawTempMsg(HDR_H + 76, &lgfx::fonts::DejaVu24, C_DIM,   HOME_QUERY);

    if (!geocodeLocation(HOME_QUERY)) {
      static volatile bool geoReconfigFlag = false;
      static volatile bool geoContinueFlag = false;

      tft.fillScreen(C_BG);
      drawTempHeader();
      drawTempMsg(HDR_H + 24, &lgfx::fonts::DejaVu40, C_RED, "LOCATION NOT FOUND");

      int btnY2 = HDR_H + 130;
      tft.fillRect(20,  btnY2, 340, 70, C_DIMMER);
      tft.fillRect(440, btnY2, 340, 70, C_DIMMER);
      tft.setFont(&lgfx::fonts::DejaVu24);
      tft.setTextColor(C_AMBER);
      tft.setTextDatum(lgfx::middle_center);
      tft.drawString("RECONFIGURE", 20 + 170,  btnY2 + 35);
      tft.drawString("CONTINUE",    440 + 170, btnY2 + 35);
      tft.setTextDatum(lgfx::top_left);

      while (true) {
        lgfx::touch_point_t tp;
        if (tft.getTouch(&tp) && millis() - lastTouchMs > TOUCH_DEBOUNCE_MS) {
          lastTouchMs = millis();
          if (tp.y >= btnY2 && tp.y < btnY2 + 70) {
            if (tp.x >= 20  && tp.x < 360) geoReconfigFlag = true;
            if (tp.x >= 440 && tp.x < 780) geoContinueFlag = true;
          }
        }
        if (geoContinueFlag) break;
        if (geoReconfigFlag) {
          Preferences p; p.begin("tracker", false); p.remove("wifi_ssid"); p.end();
          startCaptivePortal();
        }
        esp_task_wdt_reset();
        delay(50);
      }
    }
  }

  initUI();
  fetchFlights();
  if (flightCount == 0) {
    if (wxReady) renderWeather();
    else         renderMessage("CLEAR SKIES", "NO AC IN RANGE");
  } else { currentScreen = SCREEN_FLIGHT; renderFlight(flights[flightIndex]); }

  countdown = REFRESH_SECS;
  fetchWeather();
  wxCountdown = WX_REFRESH_SECS;
#endif
}

// ─── Loop ─────────────────────────────────────────────
void loop() {
  esp_task_wdt_reset();
  unsigned long now = millis();

#if DEMO_MODE || (DIAG_STEP > 0 && DIAG_STEP < 6)
  if (flightCount > 1 && currentScreen == SCREEN_FLIGHT &&
      now - lastCycle >= (unsigned long)CYCLE_SECS * 1000) {
    lastCycle = now;
    flightIndex = (flightIndex + 1) % flightCount;
    renderFlight(flights[flightIndex]);
  }

  if (now - lastTick >= 1000) {
    lastTick = now;
    countdown--;
    if (countdown <= 0) countdown = REFRESH_SECS;
    if (currentScreen == SCREEN_FLIGHT && flightCount > 0) drawStatusBar();
  }

#else
  ArduinoOTA.handle();
  if (Serial.available()) checkSerialCmd();
  pollTouch();

  if (triggerPortal) {
    triggerPortal = false;
    startCaptivePortal();
  }

  if (triggerGeoFetch) {
    triggerGeoFetch = false;
    fetchFlights();
    if (flightCount == 0) {
      if (wxReady) renderWeather();
      else         renderMessage("CLEAR SKIES", "NO AC IN RANGE");
    } else { currentScreen = SCREEN_FLIGHT; renderFlight(flights[flightIndex]); }
    countdown = REFRESH_SECS;
    lastCycle  = millis();
  }

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

    if (countdown <= 0) {
      fetchFlights();
      if (flightCount == 0) {
        if (wxReady) renderWeather();
        else         renderMessage("CLEAR SKIES", "NO AC IN RANGE");
      } else { currentScreen = SCREEN_FLIGHT; renderFlight(flights[flightIndex]); }
      countdown = REFRESH_SECS;
      lastCycle  = millis();
    }

    if (wxCountdown <= 0) {
      bool wxOk = fetchWeather();
      wxCountdown = wxOk ? WX_REFRESH_SECS : 60;
      if (currentScreen == SCREEN_WEATHER) renderWeather();
      else if (wxOk && flightCount == 0) renderWeather();
    }
  }

  if (flightCount > 1 && currentScreen == SCREEN_FLIGHT &&
      !isFetching && now - lastCycle >= (unsigned long)CYCLE_SECS * 1000) {
    lastCycle = now;
    flightIndex = (flightIndex + 1) % flightCount;
    renderFlight(flights[flightIndex]);
  }
#endif
}
