#pragma once
#include "lgfx_config.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config.h"
#include "types.h"

extern LGFX tft;

// ─── Hardware ─────────────────────────────────────────
extern WebServer  setupServer;
extern DNSServer  dnsServer;

// ─── WiFi ─────────────────────────────────────────────
extern char WIFI_SSID[64];
extern char WIFI_PASS[64];

// ─── Proxy ────────────────────────────────────────────
extern const char* PROXY_HOST;
extern const int   PROXY_PORT;

// ─── Location ─────────────────────────────────────────
extern float HOME_LAT;
extern float HOME_LON;
extern float GEOFENCE_MI;
extern int   ALT_FLOOR_FT;
extern char  LOCATION_NAME[32];
extern char  HOME_QUERY[128];
extern bool  needsGeocode;

// ─── Geofence presets ─────────────────────────────────
extern const float GEO_PRESETS[];
extern const int   GEO_COUNT;
extern int         geoIndex;

// ─── SD state ─────────────────────────────────────────
extern bool sdAvailable;

// ─── Touch ────────────────────────────────────────────
extern bool     touchReady;
extern uint32_t lastTouchMs;

// ─── Screen ───────────────────────────────────────────
extern ScreenMode currentScreen;
extern ScreenMode previousScreen;

// ─── Weather ──────────────────────────────────────────
extern WeatherData wxData;
extern bool        wxReady;
extern int         wxCountdown;
extern int         lastMinute;

// ─── Flights ──────────────────────────────────────────
extern Flight flights[20];
extern Flight newFlights[20];
extern DynamicJsonDocument g_jsonDoc;
extern int           flightCount;
extern int           flightIndex;
extern int           countdown;
extern bool          isFetching;
extern bool          usingCache;
extern int           dataSource;
extern unsigned long lastTick;
extern unsigned long lastCycle;
extern unsigned long lastFetchOk;
extern time_t        cacheTimestamp;

// ─── Direct API robustness ────────────────────────────
extern int           directApiFailCount;
extern unsigned long directApiNextRetryMs;

// ─── Proxy failover ─────────────────────────────────
extern int           proxyFailCount;
extern unsigned long proxySkipUntilMs;

// ─── Session log ──────────────────────────────────────
extern char loggedCallsigns[MAX_LOGGED][12];
extern int  loggedCount;

// ─── Unknown tracking ────────────────────────────────
extern char loggedUnknowns[MAX_UNKNOWNS][6];
extern int  loggedUnknownCount;

// ─── Diagnostics ──────────────────────────────────────
extern unsigned long lastDiagMs;

// ─── Cross-task trigger flags ─────────────────────────
extern volatile bool triggerPortal;
extern volatile bool triggerGeoFetch;

// ─── Async fetch (FreeRTOS background task) ──────────
#if ASYNC_FETCH
#include <freertos/semphr.h>
extern volatile bool     fetchDone;
extern volatile int      fetchResultCount;
extern volatile int      fetchResultSource;
extern volatile bool     fetchResultCache;
extern volatile bool     wxFetchPending;
extern volatile bool     wxFetchDone;
extern volatile bool     wxFetchResultOk;
extern SemaphoreHandle_t fetchSemaphore;
extern TaskHandle_t      fetchTaskHandle;
#endif

// ─── Forward declarations ─────────────────────────────
// helpers.ino
bool wifiOk();
void logTs(const char* tag, const char* fmt, ...);
bool alreadyLogged(const char* cs);
const Airline* getAirline(const char* cs);
const char* getAircraftTypeName(const char* code);
const char* getAircraftCategory(const char* code);
FlightStatus deriveStatus(int alt, int vs, float dist);
const char* statusLabel(FlightStatus s);
uint16_t statusColor(FlightStatus s);
uint16_t distanceColor(float dist_mi, float max_mi);
void toUpperStr(char* s);
const char* dataSourceLabel();
void sortFlightsByDist(Flight* f, int count);
float haversineMi(float lat1, float lon1, float lat2, float lon2);
int apiRadiusNm();
void formatAlt(int alt, char* buf, int len);
void diagReport();

// display.ino
void initUI();
void drawHeader();
void drawNavBar();
void drawStatusBar();
void renderFlight(const Flight& f);
void renderWeather();
void renderMessage(const char* line1, const char* line2 = nullptr);
void bootSequence();
void drawOtaProgress(int pct);
void redrawDashNumbers(float alt, float dist, int spd, int vs);
// network.ino
String fetchFromProxy();
int fetchAndParseDirectAPI();
int parsePayload(String& payload);
int extractFlights(DynamicJsonDocument& doc);
void fetchFlights();
bool fetchWeather();
void sendHeartbeat();
#if ASYNC_FETCH
void fetchFlightsWork();
void fetchTaskFunc(void* param);
#endif

// touch.ino
void initTouch();
void pollTouch();

// wifi_setup.ino
bool loadWiFiConfig();
void saveWiFiConfig(const char* ssid, const char* pass, const char* query);
void saveGeoIndex();
bool geocodeLocation(const char* query);
void startCaptivePortal();

// serial_cmd.ino
void checkSerialCmd();

// sd_config.ino
void loadConfig();
void writeCache(const String& payload);
String readCache();
void logFlight(const Flight& f);
void logUnknown(const char* type, const char* code, const char* context);
