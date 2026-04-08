#pragma once

struct Flight {
  char callsign[12];
  char depCode[6];    // IATA departure code, e.g. "SYD"
  char arrCode[6];    // IATA arrival code, e.g. "MEL"
  char type[16];      // aircraft type display name, e.g. "737-800" — shown when no route or for GA model
  int      alt;              // feet
  int      speed;            // knots
  float    dist;             // km from home
  bool     valid;
  uint16_t callsignColor;    // airline-derived
  uint16_t typeColor;        // aircraft-category-derived (C_WHITE = GA)
};

struct Weather {
  float tempC;
  int   weatherCode;
  float windSpeedKmh;
  int   visibilityKm;
  char  windCardinal[4];
  int   utcOffsetSec;
  bool  valid;
};
