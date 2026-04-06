#pragma once

struct Flight {
  char callsign[12];
  char origin[40];    // split from "Origin > Destination"
  char dest[40];
  char type[16];      // aircraft type display name, e.g. "737-800" — shown when no route
  int      alt;              // feet
  int      speed;            // knots
  float    dist;             // km from home
  bool     valid;
  uint16_t callsignColor;    // airline-derived
  uint16_t typeColor;        // aircraft-category-derived
};

struct Weather {
  float tempC;
  int   weatherCode;
  bool  valid;
};
