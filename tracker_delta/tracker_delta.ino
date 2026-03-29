/*
  SPOTTER — DELTA
  Waveshare ESP32-S3-Touch-LCD-3.49 (Case A): 640×172 IPS QSPI, AXS15231B capacitive touch
  Rendering: Arduino_GFX (immediate-mode, no LVGL)

  Libraries needed (install via Library Manager):
    - Arduino_GFX (by Moon On Our Nation) — display driver
    - ArduinoJson
*/

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <math.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_task_wdt.h>
#include "secrets.h"
#include "config.h"
#include "types.h"

// ─── Display ──────────────────────────────────────
Arduino_DataBus* bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);

// rotation=1 → landscape 640×172; IPS=false; w=172, h=640
Arduino_GFX* gfx = new Arduino_AXS15231B(bus, LCD_RST, 1, false, 172, 640);

// ─── WiFi ─────────────────────────────────────────
char WIFI_SSID[64] = WIFI_SSID_DEFAULT;
char WIFI_PASS[64] = WIFI_PASS_DEFAULT;

// ─── Location ─────────────────────────────────────
float HOME_LAT    = -33.8530f;
float HOME_LON    = 151.1410f;
float GEOFENCE_MI = DEFAULT_GEOFENCE_MI;
char  LOCATION_NAME[32] = "NOT SET";
char  HOME_QUERY[128]   = "";
bool  needsGeocode      = false;

// ─── Flights ──────────────────────────────────────
Flight        flights[20];
Flight        newFlights[20];
DynamicJsonDocument g_jsonDoc(24576);
int           flightCount      = 0;
int           todayCount       = 0;
bool          isFetching       = false;
int           dataSource       = 0;
unsigned long lastFlightFetchMs = 0;
unsigned long lastFlightDataMs  = 0;

// ─── Weather ──────────────────────────────────────
WeatherData   wxData;
bool          wxReady      = false;
unsigned long lastWeatherMs = 0;

// ─── Proxy stats ──────────────────────────────────
ProxyStats    proxyStats;
bool          proxyStatsReady = false;
uint8_t       peakHours[24]  = {};
unsigned long lastStatsMs    = 0;

// ─── Server + devices ─────────────────────────────
ServerStatus  serverStatus;
bool          serverReady   = false;
DeviceStatus  echoStatus;
DeviceStatus  foxtrotStatus;
unsigned long lastServerMs  = 0;

// ─── Page / UI ────────────────────────────────────
Page          currentPage      = PAGE_FLIGHTS;
unsigned long lastPageChangeMs = 0;
unsigned long lastTouchMs      = 0;
bool          needsRedraw      = true;

// ─── Direct API robustness ────────────────────────
int           directApiFailCount   = 0;
unsigned long directApiNextRetryMs = 0;

// ─── Session log ──────────────────────────────────
char loggedCallsigns[MAX_LOGGED][12];
int  loggedCount = 0;

// ─── Heartbeat ────────────────────────────────────
unsigned long lastHeartbeatMs = 0;

// ─── Servers ──────────────────────────────────────
WebServer setupServer(80);
DNSServer dnsServer;

// ─── Demo flights (DEMO_MODE or pre-WiFi) ─────────
static void loadDemoFlights() {
  strlcpy(LOCATION_NAME, "SYDNEY", sizeof(LOCATION_NAME));
  GEOFENCE_MI = 15.0f;
  memset(flights, 0, sizeof(flights));
  flightCount = 2;

  strlcpy(flights[0].callsign, "QFA421",   sizeof(flights[0].callsign));
  strlcpy(flights[0].reg,      "VH-OGH",   sizeof(flights[0].reg));
  strlcpy(flights[0].type,     "B738",     sizeof(flights[0].type));
  strlcpy(flights[0].route,    "SYD > BNE", sizeof(flights[0].route));
  flights[0].alt = 35000; flights[0].speed = 480; flights[0].vs = 0;
  flights[0].dist = 6.2f; flights[0].status = STATUS_CRUISING;
  flights[0].dep[0] = 0; flights[0].arr[0] = 0;

  strlcpy(flights[1].callsign, "JST530",   sizeof(flights[1].callsign));
  strlcpy(flights[1].reg,      "VH-VFU",   sizeof(flights[1].reg));
  strlcpy(flights[1].type,     "A320",     sizeof(flights[1].type));
  strlcpy(flights[1].route,    "MEL > SYD", sizeof(flights[1].route));
  flights[1].alt = 8200; flights[1].speed = 290; flights[1].vs = 1500;
  flights[1].dist = 9.4f; flights[1].status = STATUS_CLIMBING;
  flights[1].dep[0] = 0; flights[1].arr[0] = 0;

  wxReady = true;
  wxData.temp = 22.0f; wxData.wind_speed = 14.0f; wxData.wind_dir = 315;
  strlcpy(wxData.condition,    "Partly Cloudy", sizeof(wxData.condition));
  strlcpy(wxData.wind_cardinal, "NW",           sizeof(wxData.wind_cardinal));
}

// ─── Backlight ────────────────────────────────────
static void blOn()  { digitalWrite(LCD_BL, HIGH); }
static void blOff() { digitalWrite(LCD_BL, LOW);  }

// ─── Touch (AXS15231B I2C protocol) ───────────────
TwoWire touchWire(0);

static bool readTouch(int16_t* tx, int16_t* ty) {
  static const uint8_t cmd[11] = {0xb5,0xab,0xa5,0x5a,0x00,0x00,0x00,0x08,0x00,0x00,0x00};
  uint8_t data[8] = {};

  touchWire.beginTransmission(TOUCH_ADDR);
  touchWire.write(cmd, sizeof(cmd));
  if (touchWire.endTransmission(false) != 0) return false;

  touchWire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)8);
  for (int i = 0; i < 8 && touchWire.available(); i++) data[i] = touchWire.read();

  if (data[1] == 0) return false;
  int16_t raw_x = ((data[2] & 0x0F) << 8) | data[3];
  int16_t raw_y = ((data[4] & 0x0F) << 8) | data[5];
  // rotation=1 (landscape): swap and mirror to match screen coords
  *tx = raw_y;
  *ty = 171 - raw_x;
  return true;
}

// ─── Session flight log ───────────────────────────
static void logFlight(const Flight& f) {
  if (!f.callsign[0] || loggedCount >= MAX_LOGGED) return;
  for (int i = 0; i < loggedCount; i++)
    if (strcmp(loggedCallsigns[i], f.callsign) == 0) return;
  strlcpy(loggedCallsigns[loggedCount++], f.callsign, 12);
  todayCount++;
}

// ─── Daily counter reset ──────────────────────────
static int lastDay = -1;
static void checkDayRollover() {
  struct tm t; time_t now = time(nullptr);
  localtime_r(&now, &t);
  if (t.tm_mday != lastDay && t.tm_year > 70) {
    lastDay = t.tm_mday;
    todayCount = 0; loggedCount = 0;
  }
}

// ─── WiFi connect ─────────────────────────────────
static bool connectWiFi() {
  gfx->fillScreen(C_BG);
  gfx->setTextColor(C_AMBER);
  gfx->setTextSize(SZ_MD);
  gfx->setCursor(8, HDR_H + 4);
  gfx->print("CONNECTING TO ");
  gfx->print(WIFI_SSID);
  gfx->print("...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(200);
  return WiFi.status() == WL_CONNECTED;
}

// ─── setup() ──────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Backlight
  pinMode(LCD_BL, OUTPUT);
  blOff();

  // Display init
  gfx->begin();
  gfx->fillScreen(C_BG);
  blOn();

  // Touch I2C
  touchWire.begin(TOUCH_SDA, TOUCH_SCL, 400000);

  // Watchdog
  esp_task_wdt_config_t wdt_cfg = {
    .timeout_ms = WDT_TIMEOUT_SEC * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_reconfigure(&wdt_cfg);
  esp_task_wdt_add(NULL);

#if DEMO_MODE
  loadDemoFlights();
  needsRedraw = true;
  return;
#endif

  // Load WiFi config from NVS
  if (!loadWiFiConfig()) {
    startCaptivePortal();  // never returns
  }

  // Geocode if needed
  if (connectWiFi()) {
    if (needsGeocode && HOME_QUERY[0]) geocodeLocation(HOME_QUERY);
    configTime(0, 0, "pool.ntp.org");
  } else {
    gfx->setCursor(8, HDR_H + 44);
    gfx->setTextColor(C_RED);
    gfx->print("WIFI FAILED — CHECK SETTINGS");
    delay(3000);
  }

  needsRedraw = true;
}

// ─── loop() ───────────────────────────────────────
void loop() {
  esp_task_wdt_reset();

#if DEMO_MODE
  unsigned long now = millis();
  if (needsRedraw || now - lastPageChangeMs > (unsigned long)PAGE_ROTATE_SECS * 1000) {
    if (!needsRedraw) { currentPage = (Page)((currentPage + 1) % PAGE_COUNT); lastPageChangeMs = now; }
    drawPage(currentPage);
    needsRedraw = false;
  }
  // Touch to advance page
  int16_t tx, ty;
  if (readTouch(&tx, &ty) && now - lastTouchMs > 500) {
    lastTouchMs = now;
    currentPage = (Page)((currentPage + 1) % PAGE_COUNT);
    lastPageChangeMs = now;
    needsRedraw = true;
  }
  delay(50);
  return;
#endif

  unsigned long now = millis();
  bool wifi = (WiFi.status() == WL_CONNECTED);

  // WiFi reconnect
  if (!wifi) {
    static unsigned long lastReconnectMs = 0;
    if (now - lastReconnectMs > 30000) {
      lastReconnectMs = now;
      WiFi.reconnect();
    }
  }

  // Fetch flights + stats + peak every REFRESH_SECS
  if (wifi && (lastFlightFetchMs == 0 || now - lastFlightFetchMs > (unsigned long)REFRESH_SECS * 1000)) {
    lastFlightFetchMs = now;
    fetchFlights();
    fetchStats();
    checkDayRollover();
    needsRedraw = true;
  }

  // Fetch weather every WEATHER_SECS
  if (wifi && (lastWeatherMs == 0 || now - lastWeatherMs > (unsigned long)WEATHER_SECS * 1000)) {
    lastWeatherMs = now;
    fetchWeather();
    needsRedraw = true;
  }

  // Fetch server + devices every SERVER_SECS
  if (wifi && (lastServerMs == 0 || now - lastServerMs > (unsigned long)SERVER_SECS * 1000)) {
    lastServerMs = now;
    fetchServerStatus();
    fetchDeviceStatus("echo",    &echoStatus);
    fetchDeviceStatus("foxtrot", &foxtrotStatus);
    needsRedraw = true;
  }

  // Heartbeat every 60 s
  if (wifi && (lastHeartbeatMs == 0 || now - lastHeartbeatMs > 60000)) {
    lastHeartbeatMs = now;
    sendHeartbeat();
  }

  // Auto-rotate page
  if (now - lastPageChangeMs > (unsigned long)PAGE_ROTATE_SECS * 1000) {
    currentPage = (Page)((currentPage + 1) % PAGE_COUNT);
    lastPageChangeMs = now;
    needsRedraw = true;
  }

  // Touch to advance page manually
  int16_t tx, ty;
  if (readTouch(&tx, &ty) && now - lastTouchMs > 500) {
    lastTouchMs = now;
    currentPage = (Page)((currentPage + 1) % PAGE_COUNT);
    lastPageChangeMs = now;
    needsRedraw = true;
  }

  // Redraw
  if (needsRedraw) {
    drawPage(currentPage);
    needsRedraw = false;
  }

  delay(100);
}
