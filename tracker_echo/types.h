#pragma once
#include "config.h"

// ─── Flight status ────────────────────────────────────
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

// ─── Flight struct ────────────────────────────────────
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

// ─── Screen mode ──────────────────────────────────────
enum ScreenMode { SCREEN_NONE, SCREEN_FLIGHT, SCREEN_WEATHER, SCREEN_TRACK };

// ─── Weather data ─────────────────────────────────────
struct WeatherData {
  float   temp;
  float   feels_like;
  int     humidity;
  char    condition[32];
  float   wind_speed;
  int     wind_dir;
  char    wind_cardinal[4];
  float   uv_index;
  int32_t utc_offset_secs;
  char    tide_dir[8];     // "RISING" or "FALLING" or ""
  char    tide_time[6];    // "HH:MM" or ""
  float   tide_height;
  bool    tide_is_high;    // true = next tide is HIGH
};

// ─── Airline lookup ───────────────────────────────────
struct Airline { const char* prefix; const char* name; uint16_t color; };

// ─── Aircraft type lookup ─────────────────────────────
struct AircraftType { const char* code; const char* name; const char* cat; };
