/*
  OVERHEAD TRACKER — LIVE (Redesigned UI)
  Supports two boards via BOARD_2P8 / BOARD_4P0 in config.h:
    FNK0103S   — 4.0" 480x320 ST7796 (touch + SD)
    FNK0103B_2P8 — 2.8" 320x240 ST7789 (no touch, no SD)

  Libraries needed:
    - LovyanGFX (display + touch driver)
    - ArduinoJson (install via Library Manager)
    - SD (built into Arduino ESP32 core — no install needed)
    - ArduinoOTA (built into Arduino ESP32 core — no install needed)

  File structure:
    tracker_echo.ino  — setup() + loop() + global state (this file)
    config.h                   — all #defines (layout, colours, pins, timing)
    types.h                    — Flight, WeatherData, enums
    globals.h                  — extern declarations + forward declarations
    lookup_tables.h            — airline + aircraft type arrays
    helpers.ino                — pure logic (haversine, deriveStatus, formatters, diagnostics)
    display.ino                — all TFT drawing functions
    network.ino                — HTTP fetching, JSON parsing, flight extraction
    touch.ino                  — touch calibration + input handling
    wifi_setup.ino             — WiFi config, captive portal, geocoding
    sd_config.ino              — SD card config, cache, flight logging
    serial_cmd.ino             — serial debug console (command-line diagnostics)
*/

#include "board.h"
#include "lgfx_config.h"
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#ifndef BOARD_2P8
  #include <SD.h>
#endif
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
LGFX       tft;
WebServer  setupServer(80);
DNSServer  dnsServer;

// ─── WiFi (loaded from NVS on boot; defaults used on first flash) ─
char WIFI_SSID[64] = WIFI_SSID_DEFAULT;
char WIFI_PASS[64] = WIFI_PASS_DEFAULT;

// ─── Proxy ────────────────────────────────────────────
const char* PROXY_HOST = "api.overheadtracker.com";
const int   PROXY_PORT = 443;

// ─── Location (defaults — overridden by NVS / config.txt) ─
float HOME_LAT    = 0.0f;
float HOME_LON    = 0.0f;
float GEOFENCE_KM = 10.0f;
int   ALT_FLOOR_FT = 500;
char  LOCATION_NAME[32] = "NOT SET";
char  HOME_QUERY[128]   = "";
bool  needsGeocode      = false;

// ─── SD state ─────────────────────────────────────────
#if HAS_SD
bool sdAvailable = false;
#endif

// ─── Geofence presets ─────────────────────────────────
const float GEO_PRESETS[] = {5.0f, 10.0f, 20.0f};
const int   GEO_COUNT     = 3;
int         geoIndex      = 1;

// ─── Touch ────────────────────────────────────────────
#if HAS_TOUCH
uint16_t touchCalData[8] = {0};
bool     touchReady      = false;
uint32_t lastTouchMs     = 0;
#endif

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
DynamicJsonDocument g_jsonDoc(16384);
int           flightCount  = 0;
int           flightIndex  = 0;
int           countdown    = REFRESH_SECS;
bool          isFetching   = false;
bool          usingCache   = false;
int           dataSource   = 0;  // 0=proxy, 1=direct API, 2=cache
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

// ─── Heartbeat ───────────────────────────────────────
unsigned long lastHeartbeatMs = 0;
const unsigned long HEARTBEAT_INTERVAL_MS = 60000;

// ─── Tracking mode ────────────────────────────────────
bool  trackingMode    = false;
char  trackCallsign[12] = "";
float trackProgress   = -1.0f;
char  trackTerritory[48] = "";
int   trackCountdown  = TRACK_REFRESH_SECS;

// ─── Auto-cycle (no-touch boards) ─────────────────────
#if !HAS_TOUCH
int  cyclesSinceWx = 0;
bool showingWxBriefly = false;
#endif

// ─── Setup ────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  {
    const char* reasons[] = {
      "UNKNOWN","POWERON","?","SW","PANIC","INT_WDT","TASK_WDT","WDT","DEEPSLEEP","BROWNOUT","SDIO"
    };
    esp_reset_reason_t r = esp_reset_reason();
    Serial.printf("[BOOT] Reset reason: %s (%d)\n", r < 11 ? reasons[r] : "?", (int)r);
  }
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(C_BG);
  tft.setTextWrap(false);

  bootSequence();

#if HAS_SD
  if (SD.begin(SD_CS)) {
    sdAvailable = true;
    Serial.println("SD card ready");
    loadConfig();
  } else {
    Serial.println("SD card not found — continuing without");
  }
#endif

#if HAS_TOUCH
  initTouch();
#endif

  if (!loadWiFiConfig()) {
    startCaptivePortal();  // blocks until saved; restarts device
  }

  // ── Animated WiFi connection screen ──
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, W, HDR_H, C_AMBER);
  tft.setTextColor(C_BG, C_AMBER);
#ifdef BOARD_2P8
  tft.setTextSize(1);
  tft.setCursor(4, 7);
#else
  tft.setTextSize(2);
  tft.setCursor(8, 6);
#endif
  tft.print("OVERHEAD TRACKER");

  int yBase = HDR_H + 10;
  tft.setTextColor(C_AMBER, C_BG);
#ifdef BOARD_2P8
  tft.setTextSize(1);
#else
  tft.setTextSize(2);
#endif
  tft.setCursor(16, yBase);
  tft.print("CONNECTING TO WIFI");

  tft.setTextColor(C_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(16, yBase + 24);
  tft.print("NETWORK: ");
  tft.setTextColor(C_AMBER, C_BG);
  tft.print(WIFI_SSID);

  const int BAR_X = 16;
  const int BAR_Y = yBase + 50;
  const int BAR_W = W - 32;
  const int BAR_H = 6;
  tft.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, C_DIMMER);

  const int DOT_Y   = BAR_Y + 20;
  const int DOT_SPACING = (BAR_W) / 20;
  const int STATUS_Y = BAR_Y + 44;

  tft.setTextColor(C_DIMMER, C_BG);
  tft.setTextSize(1);
  tft.setCursor(16, STATUS_Y);
  tft.print("ATTEMPTING...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  int scanPos   = 0;
  int scanDir   = 1;
  const int SCAN_W = 48;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    tft.fillRect(BAR_X + 1, BAR_Y + 1, BAR_W - 2, BAR_H - 2, C_BG);
    int segX = BAR_X + 1 + scanPos;
    int segMaxW = BAR_W - 2 - scanPos;
    int segW = min(SCAN_W, segMaxW);
    if (segW > 0) tft.fillRect(segX, BAR_Y + 1, segW, BAR_H - 2, C_AMBER);

    scanPos += scanDir * 4;
    if (scanPos + SCAN_W >= BAR_W - 2) { scanPos = BAR_W - 2 - SCAN_W; scanDir = -1; }
    if (scanPos <= 0)                   { scanPos = 0;                   scanDir =  1; }

    int dotIdx = attempts % 20;
    int dotX   = BAR_X + dotIdx * DOT_SPACING + DOT_SPACING / 2;
    if (dotIdx == 0 && attempts > 0) {
      tft.fillRect(BAR_X, DOT_Y, BAR_W, 8, C_BG);
    }
    tft.fillCircle(dotX, DOT_Y + 4, 2, C_DIM);

    char countBuf[24];
    snprintf(countBuf, sizeof(countBuf), "ATTEMPT %d / 40", attempts + 1);
    tft.fillRect(16, STATUS_Y, 200, 10, C_BG);
    tft.setTextColor(C_DIMMER, C_BG);
    tft.setTextSize(1);
    tft.setCursor(16, STATUS_Y);
    tft.print(countBuf);

    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    tft.fillRect(BAR_X + 1, BAR_Y + 1, BAR_W - 2, BAR_H - 2, C_GREEN);
    tft.fillRect(16, STATUS_Y, W - 32, 10, C_BG);
    tft.setTextColor(C_GREEN, C_BG);
    tft.setTextSize(1);
    tft.setCursor(16, STATUS_Y);
    tft.print("CONNECTED");
    for (int i = 0; i < 3; i++) {
      tft.fillRect(BAR_X + 1, BAR_Y + 1, BAR_W - 2, BAR_H - 2, C_BG);
      delay(100);
      tft.fillRect(BAR_X + 1, BAR_Y + 1, BAR_W - 2, BAR_H - 2, C_GREEN);
      delay(120);
    }
    delay(300);
    configTime(0, 0, "pool.ntp.org");
    Serial.println("NTP sync started");
    ArduinoOTA.setHostname("overhead-tracker");
    ArduinoOTA.onStart([]() {
      drawOtaProgress(0);
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      drawOtaProgress(progress * 100 / total);
    });
    ArduinoOTA.onEnd([]() {
      tft.setTextColor(C_GREEN, C_BG);
      tft.setTextSize(2);
#ifdef BOARD_2P8
      tft.setCursor(80, 190);
#else
      tft.setCursor(160, 250);
#endif
      tft.print("Restarting...");
    });
    ArduinoOTA.onError([](ota_error_t error) {
      tft.setTextColor(C_RED, C_BG);
      tft.setTextSize(2);
#ifdef BOARD_2P8
      tft.setCursor(50, 190);
#else
      tft.setCursor(120, 250);
#endif
      tft.printf("OTA Error [%u]", error);
      delay(3000);
      ESP.restart();
    });
    ArduinoOTA.begin();
    Serial.println("OTA ready — overhead-tracker.local");
  }

  // Hardware watchdog: reboot if loop() stalls for > 30 s
  const esp_task_wdt_config_t wdt_cfg = { .timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = true };
  esp_err_t wdtErr = esp_task_wdt_init(&wdt_cfg);
  if (wdtErr == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_reconfigure(&wdt_cfg);
  }
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
        renderFlight(flights[0]);
        countdown = REFRESH_SECS;
        return;
      }
    }
    // WiFi failed — show error
    tft.fillScreen(C_BG);
    tft.fillRect(0, 0, W, HDR_H, C_AMBER);
    tft.setTextColor(C_BG, C_AMBER);
#ifdef BOARD_2P8
    tft.setTextSize(1);
    tft.setCursor(4, 7);
#else
    tft.setTextSize(2);
    tft.setCursor(8, 6);
#endif
    tft.print("OVERHEAD TRACKER");
    tft.setTextColor(C_RED, C_BG);
    tft.setTextSize(2);
    tft.setCursor(16, HDR_H + 20);
    tft.print("WIFI FAILED");
    tft.setTextColor(C_DIM, C_BG);
    tft.setTextSize(1);
    tft.setCursor(16, HDR_H + 50);
    tft.print("Could not connect to: ");
    tft.setTextColor(C_AMBER, C_BG);
    tft.print(WIFI_SSID);

#if HAS_TOUCH
    int btnY = HDR_H + 74;
    tft.fillRect(16, btnY, 200, 44, C_DIMMER);
    tft.setTextColor(C_AMBER, C_DIMMER);
    tft.setTextSize(1);
    tft.setCursor(28, btnY + 12);
    tft.print("RECONFIGURE");
    tft.setCursor(28, btnY + 26);
    tft.print("Change WiFi/location");
    tft.fillRect(260, btnY, 200, 44, C_DIMMER);
    tft.setTextColor(C_AMBER, C_DIMMER);
    tft.setTextSize(1);
    tft.setCursor(272, btnY + 12);
    tft.print("RETRY");
    tft.setCursor(272, btnY + 26);
    tft.print("Reboot and try again");
    while (true) {
      if (touchReady) {
        uint16_t tx, ty;
        if (tft.getTouch(&tx, &ty)) {
          if (ty >= btnY && ty <= btnY + 44) {
            if (tx >= 16 && tx <= 216) {
              Preferences p;
              p.begin("tracker", false);
              p.remove("wifi_ssid");
              p.end();
              startCaptivePortal();
            } else if (tx >= 260 && tx <= 460) {
              ESP.restart();
            }
          }
        }
      }
      esp_task_wdt_reset();
      delay(50);
    }
#else
    tft.setTextColor(C_DIM, C_BG);
    tft.setTextSize(1);
    tft.setCursor(16, HDR_H + 70);
    tft.print("Rebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
#endif
  }

  // ── Geocode location name if needed ──
  if (needsGeocode && HOME_QUERY[0]) {
    tft.fillScreen(C_BG);
    tft.fillRect(0, 0, W, HDR_H, C_AMBER);
    tft.setTextColor(C_BG, C_AMBER);
#ifdef BOARD_2P8
    tft.setTextSize(1);
    tft.setCursor(4, 7);
#else
    tft.setTextSize(2);
    tft.setCursor(8, 6);
#endif
    tft.print("OVERHEAD TRACKER");
    tft.setTextColor(C_AMBER, C_BG);
    tft.setTextSize(2);
    tft.setCursor(16, HDR_H + 20);
    tft.print("LOCATING...");
    tft.setTextColor(C_DIM, C_BG);
    tft.setTextSize(1);
    tft.setCursor(16, HDR_H + 50);
    tft.print(HOME_QUERY);

    if (!geocodeLocation(HOME_QUERY)) {
      tft.fillScreen(C_BG);
      tft.fillRect(0, 0, W, HDR_H, C_AMBER);
      tft.setTextColor(C_BG, C_AMBER);
#ifdef BOARD_2P8
      tft.setTextSize(1);
      tft.setCursor(4, 7);
#else
      tft.setTextSize(2);
      tft.setCursor(8, 6);
#endif
      tft.print("OVERHEAD TRACKER");
      tft.setTextColor(C_RED, C_BG);
      tft.setTextSize(2);
      tft.setCursor(16, HDR_H + 20);
      tft.print("LOCATION NOT FOUND");
      tft.setTextColor(C_DIM, C_BG);
      tft.setTextSize(1);
      tft.setCursor(16, HDR_H + 50);
      tft.print(HOME_QUERY);

#if HAS_TOUCH
      int btnY2 = HDR_H + 80;
      tft.fillRect(16, btnY2, 200, 44, C_DIMMER);
      tft.setTextColor(C_AMBER, C_DIMMER);
      tft.setTextSize(1);
      tft.setCursor(28, btnY2 + 12);
      tft.print("RECONFIGURE");
      tft.setCursor(28, btnY2 + 26);
      tft.print("Change WiFi/location");
      tft.fillRect(260, btnY2, 200, 44, C_DIMMER);
      tft.setTextColor(C_AMBER, C_DIMMER);
      tft.setCursor(272, btnY2 + 12);
      tft.print("CONTINUE");
      tft.setCursor(272, btnY2 + 26);
      tft.print("Use default location");
      while (true) {
        if (touchReady) {
          uint16_t tx, ty;
          if (tft.getTouch(&tx, &ty)) {
            if (ty >= btnY2 && ty <= btnY2 + 44) {
              if (tx >= 16 && tx <= 216) {
                Preferences p; p.begin("tracker", false); p.remove("wifi_ssid"); p.end();
                startCaptivePortal();
              } else if (tx >= 260) {
                break;
              }
            }
          }
        }
        esp_task_wdt_reset();
        delay(50);
      }
#else
      tft.setTextColor(C_DIM, C_BG);
      tft.setTextSize(1);
      tft.setCursor(16, HDR_H + 70);
      tft.print("Continuing with defaults...");
      delay(3000);
#endif
    }
  }

  fetchFlights();
  esp_task_wdt_reset();
  countdown = REFRESH_SECS;
  bool wxOk = fetchWeather();
  wxCountdown = wxOk ? WX_REFRESH_SECS : 60;
}

// ─── Loop ─────────────────────────────────────────────
void loop() {
  esp_task_wdt_reset();
  unsigned long now = millis();
  ArduinoOTA.handle();
  if (Serial.available()) checkSerialCmd();

  // ── Touch polling ──
#if HAS_TOUCH
  if (touchReady) {
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty)) {
      handleTouch(tx, ty);
    }
  }
#endif

  // Periodic diagnostics (every 60 s)
  if (now - lastDiagMs >= 60000) {
    lastDiagMs = now;
    diagReport();
  }

  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    sendHeartbeat();
  }

  if (now - lastTick >= 1000) {
    lastTick = now;
    wxCountdown--;

    if (WiFi.status() != WL_CONNECTED) {
      static unsigned long lastReconnect = 0;
      if (now - lastReconnect > 10000) {
        lastReconnect = now;
        WiFi.reconnect();
      }
    }

    if ((currentScreen == SCREEN_FLIGHT || currentScreen == SCREEN_TRACK) &&
        flightCount > 0 && !isFetching) drawStatusBar();

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

    if (trackingMode) {
      trackCountdown--;
      if (trackCountdown <= 0) {
        fetchTrackStatus();
        trackCountdown = TRACK_REFRESH_SECS;
      }
    } else {
      countdown--;
      if (countdown <= 0) {
        fetchTrackStatus();
        if (!trackingMode) {
          fetchFlights();
        }
        countdown = REFRESH_SECS;
        lastCycle = millis();
      }
    }

    if (wxCountdown <= 0) {
      bool wxOk = fetchWeather();
      wxCountdown = wxOk ? WX_REFRESH_SECS : 60;
      if (currentScreen == SCREEN_WEATHER) renderWeather();
    }
  }

  // ── Flight cycling ──
#if HAS_TOUCH
  // Touch boards: auto-cycle only when multiple flights (suppressed in tracking mode)
  if (!trackingMode && flightCount > 1 && currentScreen == SCREEN_FLIGHT &&
      !isFetching && now - lastCycle >= (unsigned long)CYCLE_SECS * 1000) {
    lastCycle = now;
    int fc = flightCount;
    if (fc > 0) {
      flightIndex = (flightIndex + 1) % fc;
      renderFlight(flights[flightIndex]);
    }
  }
#else
  // No-touch boards: auto-cycle flights + weather interlude (suppressed in tracking mode)
  if (!trackingMode && currentScreen == SCREEN_FLIGHT && flightCount > 0 &&
      !isFetching && now - lastCycle >= (unsigned long)CYCLE_SECS * 1000) {
    lastCycle = now;
    int fc = flightCount;
    if (fc > 0) {
      flightIndex = (flightIndex + 1) % fc;
      if (flightIndex == 0) {
        cyclesSinceWx++;
        if (cyclesSinceWx >= 1) {
          currentScreen = SCREEN_WEATHER;
          showingWxBriefly = true;
          cyclesSinceWx = 0;
          renderWeather();
        } else {
          renderFlight(flights[flightIndex]);
        }
      } else {
        renderFlight(flights[flightIndex]);
      }
    }
  }
  // Return from weather interlude
  if (showingWxBriefly && now - lastCycle >= (unsigned long)CYCLE_SECS * 1000) {
    showingWxBriefly = false;
    lastCycle = now;
    currentScreen = SCREEN_FLIGHT;
    if (flightCount > 0) renderFlight(flights[flightIndex]);
  }
#endif
}
