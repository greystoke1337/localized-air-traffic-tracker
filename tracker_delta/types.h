#pragma once
#include "config.h"

// ─── Flight status ────────────────────────────────
enum FlightStatus {
  STATUS_UNKNOWN,
  STATUS_TAKING_OFF,
  STATUS_CLIMBING,
  STATUS_CRUISING,
  STATUS_DESCENDING,
  STATUS_APPROACH,
  STATUS_LANDING,
  STATUS_OVERHEAD,
};

// ─── Flight ───────────────────────────────────────
struct Flight {
  char         callsign[12];
  char         reg[12];
  char         type[8];
  char         route[40];
  float        lat, lon;
  int          alt;
  int          speed;
  int          vs;
  int          track;
  float        dist;
  char         squawk[6];
  char         dep[6];
  char         arr[6];
  FlightStatus status;
};

// ─── Weather ──────────────────────────────────────
struct WeatherData {
  float temp;
  float feels_like;
  int   humidity;
  char  condition[32];
  float wind_speed;
  int   wind_dir;
  char  wind_cardinal[4];
  float uv_index;
};

// ─── Proxy stats ──────────────────────────────────
struct ProxyStats {
  char  uptime[32];
  int   requests;
  float cacheHit;
  int   errors;
  int   clients;
  int   cached;
  int   routes;
  int   newRoutes;
  bool  adsbLolUp;
  bool  adsbFiUp;
  bool  airplanesLiveUp;
  bool  adsbOneUp;
};

// ─── Server status ────────────────────────────────
struct ServerStatus {
  char  osUptime[32];
  float cpuTemp;
  float ramPct;
  float load1, load5, load15;
};

// ─── Device heartbeat ─────────────────────────────
struct DeviceStatus {
  char          fw[16];
  int           heap;
  int           rssi;
  unsigned long uptimeSecs;
  unsigned long lastSeenMs;  // millis() when last seen
  bool          online;
};

// ─── Pages ────────────────────────────────────────
enum Page { PAGE_FLIGHTS = 0, PAGE_PROXY = 1, PAGE_SERVER = 2, PAGE_COUNT = 3 };
