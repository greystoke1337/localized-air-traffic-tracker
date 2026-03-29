#pragma once
#include "types.h"
#include <Arduino_GFX_Library.h>
#include <WiFi.h>

// ─── Display ──────────────────────────────────────
extern Arduino_GFX* gfx;

// ─── WiFi ─────────────────────────────────────────
extern char WIFI_SSID[64];
extern char WIFI_PASS[64];

// ─── Location ─────────────────────────────────────
extern float HOME_LAT;
extern float HOME_LON;
extern float GEOFENCE_MI;
extern char  LOCATION_NAME[32];
extern char  HOME_QUERY[128];
extern bool  needsGeocode;

// ─── Flights ──────────────────────────────────────
extern Flight        flights[20];
extern Flight        newFlights[20];
extern int           flightCount;
extern int           todayCount;
extern bool          isFetching;
extern int           dataSource;
extern unsigned long lastFlightFetchMs;
extern unsigned long lastFlightDataMs;

// ─── Weather ──────────────────────────────────────
extern WeatherData   wxData;
extern bool          wxReady;
extern unsigned long lastWeatherMs;

// ─── Proxy stats ──────────────────────────────────
extern ProxyStats    proxyStats;
extern bool          proxyStatsReady;
extern uint8_t       peakHours[24];
extern unsigned long lastStatsMs;

// ─── Server + devices ─────────────────────────────
extern ServerStatus  serverStatus;
extern bool          serverReady;
extern DeviceStatus  echoStatus;
extern DeviceStatus  foxtrotStatus;
extern unsigned long lastServerMs;

// ─── Page / UI ────────────────────────────────────
extern Page          currentPage;
extern unsigned long lastPageChangeMs;
extern unsigned long lastTouchMs;
extern bool          needsRedraw;

// ─── Direct API robustness ────────────────────────
extern int           directApiFailCount;
extern unsigned long directApiNextRetryMs;

// ─── Session log ──────────────────────────────────
extern char loggedCallsigns[MAX_LOGGED][12];
extern int  loggedCount;

// ─── Heartbeat ────────────────────────────────────
extern unsigned long lastHeartbeatMs;
