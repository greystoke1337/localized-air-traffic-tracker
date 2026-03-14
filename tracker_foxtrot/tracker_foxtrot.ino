/*
  OVERHEAD TRACKER — FOXTROT
  Waveshare ESP32-S3-Touch-LCD-4.3B: 800x480 IPS, GT911 capacitive touch, CH422G backlight

  Libraries needed:
    - LovyanGFX (display + touch driver)
    - ArduinoJson (install via Library Manager)
    - SD (built into Arduino ESP32 core)
    - ArduinoOTA (built into Arduino ESP32 core)
*/

#include <Wire.h>
#include "lgfx_config.h"
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

// ─── CH422G I/O expander — controls backlight and touch reset ──
void ch422g_init() {
  Wire.begin(8, 9);
  delay(10);
  Wire.beginTransmission(0x24);
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.beginTransmission(0x38);
  Wire.write(0x06);  // EXIO1=TP_RST high, EXIO2=LCD_BL on
  Wire.endTransmission();
  delay(50);
}

// ─── Hardware instances ───────────────────────────────
LGFX       tft;
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

// ─── Setup ────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== FOXTROT ===");

  // CH422G must init before display (backlight + touch reset)
  ch422g_init();

  tft.init();
  tft.setRotation(0);  // Waveshare 4.3B is natively landscape
  tft.fillScreen(C_BG);
  tft.setTextWrap(false);

  bootSequence();

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

  // ── Animated WiFi connection screen ──
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, W, HDR_H, C_AMBER);
  tft.setTextColor(C_BG, C_AMBER);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("OVERHEAD TRACKER");

  int yBase = HDR_H + 14;
  tft.setTextColor(C_AMBER, C_BG);
  tft.setTextSize(2);
  tft.setCursor(20, yBase);
  tft.print("CONNECTING TO WIFI");

  tft.setTextColor(C_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(20, yBase + 28);
  tft.print("NETWORK: ");
  tft.setTextColor(C_AMBER, C_BG);
  tft.print(WIFI_SSID);

  const int BAR_X = 20;
  const int BAR_Y = yBase + 56;
  const int BAR_W = W - 40;
  const int BAR_H = 8;
  tft.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, C_DIMMER);

  const int DOT_Y   = BAR_Y + 24;
  const int DOT_SPACING = (BAR_W) / 20;
  const int STATUS_Y = BAR_Y + 52;

  tft.setTextColor(C_DIMMER, C_BG);
  tft.setTextSize(1);
  tft.setCursor(20, STATUS_Y);
  tft.print("ATTEMPTING...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;
  int scanPos   = 0;
  int scanDir   = 1;
  const int SCAN_W = 60;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    tft.fillRect(BAR_X + 1, BAR_Y + 1, BAR_W - 2, BAR_H - 2, C_BG);
    int segX = BAR_X + 1 + scanPos;
    int segMaxW = BAR_W - 2 - scanPos;
    int segW = min(SCAN_W, segMaxW);
    if (segW > 0) tft.fillRect(segX, BAR_Y + 1, segW, BAR_H - 2, C_AMBER);

    scanPos += scanDir * 6;
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
    tft.fillRect(20, STATUS_Y, 240, 10, C_BG);
    tft.setTextColor(C_DIMMER, C_BG);
    tft.setTextSize(1);
    tft.setCursor(20, STATUS_Y);
    tft.print(countBuf);

    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    tft.fillRect(BAR_X + 1, BAR_Y + 1, BAR_W - 2, BAR_H - 2, C_GREEN);
    tft.fillRect(20, STATUS_Y, W - 40, 10, C_BG);
    tft.setTextColor(C_GREEN, C_BG);
    tft.setTextSize(1);
    tft.setCursor(20, STATUS_Y);
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
    ArduinoOTA.setHostname("overhead-foxtrot");
    ArduinoOTA.onStart([]() {
      drawOtaProgress(0);
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      drawOtaProgress(progress * 100 / total);
    });
    ArduinoOTA.onEnd([]() {
      tft.setTextColor(C_GREEN, C_BG);
      tft.setTextSize(2);
      tft.setCursor(280, 360);
      tft.print("Restarting...");
    });
    ArduinoOTA.onError([](ota_error_t error) {
      tft.setTextColor(C_RED, C_BG);
      tft.setTextSize(2);
      tft.setCursor(240, 360);
      tft.printf("OTA Error [%u]", error);
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
    // WiFi failed — show error with touch buttons
    tft.fillScreen(C_BG);
    tft.fillRect(0, 0, W, HDR_H, C_AMBER);
    tft.setTextColor(C_BG, C_AMBER);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("OVERHEAD TRACKER");
    tft.setTextColor(C_RED, C_BG);
    tft.setTextSize(2);
    tft.setCursor(20, HDR_H + 24);
    tft.print("WIFI FAILED");
    tft.setTextColor(C_DIM, C_BG);
    tft.setTextSize(1);
    tft.setCursor(20, HDR_H + 56);
    tft.print("Could not connect to: ");
    tft.setTextColor(C_AMBER, C_BG);
    tft.print(WIFI_SSID);

    int btnY = HDR_H + 90;
    tft.fillRect(20, btnY, 320, 56, C_DIMMER);
    tft.setTextColor(C_AMBER, C_DIMMER);
    tft.setTextSize(1);
    tft.setCursor(36, btnY + 14);
    tft.print("RECONFIGURE");
    tft.setCursor(36, btnY + 32);
    tft.print("Change WiFi/location");
    tft.fillRect(460, btnY, 320, 56, C_DIMMER);
    tft.setTextColor(C_AMBER, C_DIMMER);
    tft.setTextSize(1);
    tft.setCursor(476, btnY + 14);
    tft.print("RETRY");
    tft.setCursor(476, btnY + 32);
    tft.print("Reboot and try again");
    while (true) {
      if (touchReady) {
        lgfx::touch_point_t tp;
        if (tft.getTouch(&tp)) {
          if (tp.y >= btnY && tp.y <= btnY + 56) {
            if (tp.x >= 20 && tp.x <= 340) {
              Preferences p;
              p.begin("tracker", false);
              p.remove("wifi_ssid");
              p.end();
              startCaptivePortal();
            } else if (tp.x >= 460 && tp.x <= 780) {
              ESP.restart();
            }
          }
        }
      }
      esp_task_wdt_reset();
      delay(50);
    }
  }

  // ── Geocode location name if needed ──
  if (needsGeocode && HOME_QUERY[0]) {
    tft.fillScreen(C_BG);
    tft.fillRect(0, 0, W, HDR_H, C_AMBER);
    tft.setTextColor(C_BG, C_AMBER);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("OVERHEAD TRACKER");
    tft.setTextColor(C_AMBER, C_BG);
    tft.setTextSize(2);
    tft.setCursor(20, HDR_H + 24);
    tft.print("LOCATING...");
    tft.setTextColor(C_DIM, C_BG);
    tft.setTextSize(1);
    tft.setCursor(20, HDR_H + 56);
    tft.print(HOME_QUERY);

    if (!geocodeLocation(HOME_QUERY)) {
      tft.fillScreen(C_BG);
      tft.fillRect(0, 0, W, HDR_H, C_AMBER);
      tft.setTextColor(C_BG, C_AMBER);
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.print("OVERHEAD TRACKER");
      tft.setTextColor(C_RED, C_BG);
      tft.setTextSize(2);
      tft.setCursor(20, HDR_H + 24);
      tft.print("LOCATION NOT FOUND");
      tft.setTextColor(C_DIM, C_BG);
      tft.setTextSize(1);
      tft.setCursor(20, HDR_H + 56);
      tft.print(HOME_QUERY);

      int btnY2 = HDR_H + 100;
      tft.fillRect(20, btnY2, 320, 56, C_DIMMER);
      tft.setTextColor(C_AMBER, C_DIMMER);
      tft.setTextSize(1);
      tft.setCursor(36, btnY2 + 14);
      tft.print("RECONFIGURE");
      tft.setCursor(36, btnY2 + 32);
      tft.print("Change WiFi/location");
      tft.fillRect(460, btnY2, 320, 56, C_DIMMER);
      tft.setTextColor(C_AMBER, C_DIMMER);
      tft.setCursor(476, btnY2 + 14);
      tft.print("CONTINUE");
      tft.setCursor(476, btnY2 + 32);
      tft.print("Use default location");
      while (true) {
        if (touchReady) {
          lgfx::touch_point_t tp;
          if (tft.getTouch(&tp)) {
            if (tp.y >= btnY2 && tp.y <= btnY2 + 56) {
              if (tp.x >= 20 && tp.x <= 340) {
                Preferences p; p.begin("tracker", false); p.remove("wifi_ssid"); p.end();
                startCaptivePortal();
              } else if (tp.x >= 460) {
                break;
              }
            }
          }
        }
        esp_task_wdt_reset();
        delay(50);
      }
    }
  }

  fetchFlights();
  esp_task_wdt_reset();
  countdown = REFRESH_SECS;
  fetchWeather();
  wxCountdown = WX_REFRESH_SECS;
}

// ─── Loop ─────────────────────────────────────────────
void loop() {
  esp_task_wdt_reset();
  unsigned long now = millis();
  ArduinoOTA.handle();
  if (Serial.available()) checkSerialCmd();

  // ── Touch polling ──
  if (touchReady) {
    lgfx::touch_point_t tp;
    if (tft.getTouch(&tp)) {
      handleTouch(tp.x, tp.y);
    }
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
      countdown = REFRESH_SECS;
      lastCycle = millis();
    }

    if (wxCountdown <= 0) {
      bool wxOk = fetchWeather();
      wxCountdown = wxOk ? WX_REFRESH_SECS : 60;
      if (currentScreen == SCREEN_WEATHER) renderWeather();
    }
  }

  // ── Flight cycling (auto-cycle when multiple flights) ──
  if (flightCount > 1 && currentScreen == SCREEN_FLIGHT &&
      !isFetching && now - lastCycle >= (unsigned long)CYCLE_SECS * 1000) {
    lastCycle = now;
    int fc = flightCount;
    if (fc > 0) {
      flightIndex = (flightIndex + 1) % fc;
      renderFlight(flights[flightIndex]);
    }
  }
}
