// display.ino — all rendering for the 64×32 HUB75 matrix

#include <Fonts/TomThumb.h>
#include "anim_boot.h"

// Scale an RGB565 color by the global brightness (0=off, 255=full).
static inline uint16_t dim(uint16_t c) {
  if (brightness == 255 || c == 0) return c;
  uint8_t r5 = (c >> 11) & 0x1F;
  uint8_t g6 = (c >> 5)  & 0x3F;
  uint8_t b5 =  c        & 0x1F;
  uint8_t r = r5 ? max(1, (int)r5 * brightness / 255) : 0;
  uint8_t g = g6 ? max(1, (int)g6 * brightness / 255) : 0;
  uint8_t b = b5 ? max(1, (int)b5 * brightness / 255) : 0;
  return (r << 11) | (g << 5) | b;
}

// Draw the callsign pseudo-bold (4 overlapping copies at +1 offsets, matching CircuitPython technique)
// When text is wider than MATRIX_W, bounces left-to-right instead of overflowing.
static void drawCallsign(const char *cs, uint16_t color = C_AMBER) {
  if (!cs || !cs[0]) return;
  matrix.setFont(nullptr);   // built-in 6×8 font
  matrix.setTextSize(1);
  matrix.setTextWrap(false);

  int n     = strlen(cs);
  int textW = n * (CHAR_W + CHAR_GAP) - CHAR_GAP;
  int startX;

  if (textW <= MATRIX_W) {
    startX = (MATRIX_W - textW) / 2;
  } else {
    int overflow    = textW - MATRIX_W;
    uint32_t t      = millis() / SCROLL_SPEED_MS;
    uint32_t period = (uint32_t)(2 * overflow);
    uint32_t phase  = t % period;
    startX = -(int)(phase <= (uint32_t)overflow ? phase : period - phase);
  }

  // 4-offset pseudo-bold
  for (int dy = 0; dy <= 1; dy++) {
    for (int dx = 0; dx <= 1; dx++) {
      for (int i = 0; i < n; i++) {
        matrix.setCursor(startX + i * (CHAR_W + CHAR_GAP) + dx, CALLSIGN_Y + dy);
        matrix.setTextColor(dim(color));
        matrix.print(cs[i]);
      }
    }
  }
}

// Print a TomThumb string centered horizontally at a given y baseline.
static void printCenteredTT(const char *text, int y, uint16_t color) {
  if (!text || !text[0]) return;
  matrix.setFont(&TomThumb);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.setTextColor(dim(color));
  int16_t x1, y1;
  uint16_t w, h;
  matrix.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  matrix.setCursor((MATRIX_W - (int)w) / 2, y);
  matrix.print(text);
  matrix.setFont(nullptr);
}

// Draw altitude (deep blue) and speed (green) side-by-side on the same row.
// Numbers use full color; unit suffixes (FT, KT) are drawn dimmer.
static void drawAltSpeed(int alt, int speed) {
  matrix.setFont(&TomThumb);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);

  {
    char numBuf[8];
    if (alt > 0) snprintf(numBuf, sizeof(numBuf), "%d", alt);
    else         snprintf(numBuf, sizeof(numBuf), "---");
    matrix.setTextColor(dim(C_DEEP_BLUE));
    matrix.setCursor(3, ALTITUDE_Y);
    matrix.print(numBuf);
    matrix.print("FT");
  }

  {
    char numBuf[8];
    if (speed > 0) snprintf(numBuf, sizeof(numBuf), "%d", speed);
    else           snprintf(numBuf, sizeof(numBuf), "---");
    // Measure full string width for right-alignment
    char fullBuf[10];
    snprintf(fullBuf, sizeof(fullBuf), "%sKT", numBuf);
    int16_t x1, y1; uint16_t w, h;
    matrix.getTextBounds(fullBuf, 0, ALTITUDE_Y, &x1, &y1, &w, &h);
    matrix.setCursor(MATRIX_W - 2 - (int)w, ALTITUDE_Y);
    matrix.setTextColor(dim(C_LIGHT_BLUE));
    matrix.print(numBuf);
    matrix.print("KT");
  }

  matrix.setFont(nullptr);
}

// Draw route as a single centered line.
// GA: show model name (e.g. "PILATUS PC-12") in amber.
// Airline with codes: "SYD>MEL" in white.
// Airline without codes: type name in amber.
static void drawRoute(const char *depCode, const char *arrCode, const char *type, bool isGA) {
  if (isGA) {
    printCenteredTT(type && type[0] ? type : "GA", ROUTE_Y, C_AMBER);
  } else if (depCode && depCode[0] && arrCode && arrCode[0]) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%s > %s", depCode, arrCode);
    printCenteredTT(buf, ROUTE_Y, C_WHITE);
  } else if (depCode && depCode[0]) {
    printCenteredTT(depCode, ROUTE_Y, C_WHITE);
  } else if (type && type[0]) {
    printCenteredTT(type, ROUTE_Y, C_AMBER);
  }
}

// Maps altitude to a non-linear bar height (300-2000ft = 75% of bar, 2000-30000ft = 25%)
static int mapAltBarH(int alt) {
  if (alt <= 0) return 0;
  if (alt <= ALT_MIN_FT) return 2;
  if (alt >= ALT_MAX_FT) return BAR_MAX_H;

  // Split point at 23px (2px floor + 75% of 28px range = 23)
  const int BAR_MID_H = 23;
  if (alt <= ALT_MID_FT) {
    return 2 + (long)(alt - ALT_MIN_FT) * (BAR_MID_H - 2) / (ALT_MID_FT - ALT_MIN_FT);
  } else {
    return BAR_MID_H + (long)(alt - ALT_MID_FT) * (BAR_MAX_H - BAR_MID_H) / (ALT_MAX_FT - ALT_MID_FT);
  }
}

// Maps a value to a bar height in pixels (bottom-anchored, min 2px when above threshold, 0 when no data)
static int mapBarH(int val, int minVal, int maxVal) {
  if (val <= 0)      return 0;
  if (val <= minVal) return 2;
  if (val >= maxVal) return BAR_MAX_H;
  return 2 + (val - minVal) * (BAR_MAX_H - 2) / (maxVal - minVal);
}

// Dynamic side bars driven by live altitude + speed
static void drawBars(int alt, int speed) {
  int ah = mapAltBarH(alt);
  int sh = mapBarH(speed, SPD_MIN_KT, SPD_MAX_KT);
  if (ah > 0) matrix.drawFastVLine(0,           MATRIX_H - ah - 1, ah, dim(C_DEEP_BLUE));
  if (sh > 0) matrix.drawFastVLine(MATRIX_W - 1, MATRIX_H - sh - 1, sh, dim(C_LIGHT_BLUE));
}

// Centred distance bar: 64px wide when overhead, 3px at geofence edge.
static void drawDistanceBar(float dist, bool valid) {
  if (!valid) return;
  float clamped = dist < 0 ? 0 : (dist > (float)GEOFENCE_KM ? (float)GEOFENCE_KM : dist);
  int barW = (int)(3.0f + (1.0f - clamped / (float)GEOFENCE_KM) * (MATRIX_W - 3));
  int startX = (MATRIX_W - barW) / 2;
  matrix.fillRect(startX, MATRIX_H - 1, barW, 1, dim(C_WHITE));
}

// Full frame: clear → bars → callsign/type → alt+speed → route → distance bar → show
void drawAll(const Flight &f, int px) {
  matrix.fillScreen(C_BLACK);
  drawBars(f.alt, f.speed);

  bool isGA      = (f.typeColor == C_WHITE);
  bool showType  = f.valid && !isGA && px >= TYPE_FLIP_PX;

  if (showType) {
    drawCallsign(f.type, f.callsignColor);
  } else {
    drawCallsign(f.valid ? f.callsign : "------", f.callsignColor);
  }

  if (f.valid) {
    if (!isGA) drawAltSpeed(f.alt, f.speed);
    drawRoute(f.depCode, f.arrCode, f.type, isGA);
  }

  drawDistanceBar(f.dist, f.valid);
  matrix.show();
}

// Procedural synthwave perspective grid boot animation.
// Cyan floor grid recedes to an amber horizon, stars in sky above.
void playBootAnimFor(uint32_t durationMs) {
  uint32_t end = millis() + durationMs;
  float t = 0.0f;

  static const uint8_t STARS_X[] = { 4, 12, 19, 28, 37, 45, 53, 60 };
  static const uint8_t STARS_Y[] = { 2,  7,  3,  9,  1,  6,  4,  8 };

  const int VP_X    = MATRIX_W / 2;    // 32 — vanishing point x
  const int VP_Y    = 11;              // horizon row
  const int FLOOR_H = MATRIX_H - VP_Y; // 21 rows of floor

  const int8_t LANE_BX[] = { 0, 8, 16, 24, 32, 40, 48, 56, 63 };
  const int    N_LANES  = 9;
  const int    N_HLINES = 5;
  const float  SPEED    = 0.7f;

  // Faint amber glow: visual (80, 40, 0) → color565(R, B_vis, G_vis)
  const uint16_t C_GLOW = matrix.color565(80, 0, 40);

  while (millis() < end) {
    matrix.fillScreen(C_BLACK);

    // Stars in sky
    for (int i = 0; i < 8; i++)
      matrix.drawPixel(STARS_X[i], STARS_Y[i], matrix.color565(80, 80, 80));

    // Pre-horizon glow and horizon line
    matrix.drawFastHLine(0, VP_Y - 1, MATRIX_W, C_GLOW);
    matrix.drawFastHLine(0, VP_Y,     MATRIX_W, C_AMBER);

    // Vertical lane lines from vanishing point to bottom edge
    uint16_t dimCyan = matrix.color565(0, 35, 35);
    for (int i = 0; i < N_LANES; i++)
      matrix.drawLine(VP_X, VP_Y, LANE_BX[i], MATRIX_H - 1, dimCyan);

    // Scrolling horizontal lines — perspective-spaced, brightness = depth
    float offset = fmod(t * SPEED, 1.0f);
    for (int i = 0; i < N_HLINES; i++) {
      float frac = (float)i / N_HLINES + offset;
      float z = frac >= 1.0f ? frac - 1.0f : frac; // 0=horizon, 1=viewer, wraps back
      if (z < 0.02f) continue;

      // z^2 bunches lines near horizon, spreads them near viewer
      int sy = VP_Y + (int)(FLOOR_H * z * z);
      if (sy >= MATRIX_H) continue;

      int half_w = (int)(VP_X * z);
      if (half_w < 1) continue;

      uint8_t b = (uint8_t)(40 + 180 * z); // dim at horizon, bright at viewer
      uint16_t lineColor = matrix.color565(0, b, b); // cyan (G==B so swap is no-op)
      matrix.drawFastHLine(VP_X - half_w, sy, half_w * 2 + 1, lineColor);
    }

    matrix.show();
    delay(33);
    t += 0.033f;
  }
}

// Flash brightness level as a proportional fill bar (full-width = 255, narrow = dim).
// Drawn at full hardware brightness so it's always visible regardless of current level.
void flashBrightness(uint8_t level) {
  int barW = (int)((uint32_t)level * MATRIX_W / 255);
  matrix.fillScreen(C_BLACK);
  if (barW > 0) matrix.fillRect(0, MATRIX_H / 2 - 2, barW, 4, C_AMBER);
  matrix.show();
  delay(600);
}

// Flash "DONT TOUCH!!" on press — momentary test for button input
void flashDontTouch() {
  matrix.fillScreen(C_BLACK);
  matrix.setFont(nullptr);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.setTextColor(dim(C_WHITE));

  const char *line1 = "DONT";
  const char *line2 = "TOUCH!!";
  int w1 = strlen(line1) * (CHAR_W + CHAR_GAP) - CHAR_GAP;
  int w2 = strlen(line2) * (CHAR_W + CHAR_GAP) - CHAR_GAP;
  matrix.setCursor((MATRIX_W - w1) / 2, 5);
  matrix.print(line1);
  matrix.setCursor((MATRIX_W - w2) / 2, 14);
  matrix.print(line2);
  matrix.show();
  delay(1000);
}

// "OVERHEAD TRACKER" scan-reveal boot splash — white scan line sweeps top-to-bottom,
// revealing each text line as it passes, then holds for 800ms.
void drawBootSplash() {
  matrix.setFont(nullptr);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);

  const char *L1 = "OVERHEAD";
  const char *L2 = "TRACKER";
  int n1 = strlen(L1), n2 = strlen(L2);
  int startX1 = (MATRIX_W - (n1 * (CHAR_W + CHAR_GAP) - CHAR_GAP)) / 2;
  int startX2 = (MATRIX_W - (n2 * (CHAR_W + CHAR_GAP) - CHAR_GAP)) / 2;
  const int Y1 = 5, Y2 = 19;

  for (int scan = 0; scan <= MATRIX_H; scan++) {
    matrix.fillScreen(C_BLACK);

    if (scan > Y1) {
      for (int dy = 0; dy <= 1; dy++)
        for (int dx = 0; dx <= 1; dx++)
          for (int i = 0; i < n1; i++) {
            matrix.setCursor(startX1 + i * (CHAR_W + CHAR_GAP) + dx, Y1 + dy);
            matrix.setTextColor(dim(C_AMBER));
            matrix.print(L1[i]);
          }
    }

    if (scan > Y2) {
      for (int dy = 0; dy <= 1; dy++)
        for (int dx = 0; dx <= 1; dx++)
          for (int i = 0; i < n2; i++) {
            matrix.setCursor(startX2 + i * (CHAR_W + CHAR_GAP) + dx, Y2 + dy);
            matrix.setTextColor(dim(C_AMBER));
            matrix.print(L2[i]);
          }
    }

    if (scan < MATRIX_H)
      matrix.drawFastHLine(0, scan, MATRIX_W, dim(C_WHITE));

    matrix.show();
    delay(15);
  }
  delay(800);
}

// OTA status: pseudo-bold amber message centered on display (same style as callsign)
void drawOTAStatus(const char *msg) {
  if (!msg || !msg[0]) return;
  matrix.fillScreen(C_BLACK);
  matrix.setFont(nullptr);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  int n = strlen(msg);
  int w = n * (CHAR_W + CHAR_GAP) - CHAR_GAP;
  for (int dy = 0; dy <= 1; dy++)
    for (int dx = 0; dx <= 1; dx++)
      for (int i = 0; i < n; i++) {
        matrix.setCursor((MATRIX_W - w) / 2 + i * (CHAR_W + CHAR_GAP) + dx, CALLSIGN_Y + dy);
        matrix.setTextColor(dim(C_AMBER));
        matrix.print(msg[i]);
      }
  matrix.show();
}

// Boot status: clear screen and print a single centred status line in TomThumb
void drawBootStatus(const char *msg) {
  matrix.fillScreen(C_BLACK);
  matrix.setFont(&TomThumb);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.setTextColor(dim(C_AMBER));
  int16_t x1, y1;
  uint16_t w, h;
  matrix.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
  matrix.setCursor((MATRIX_W - (int)w) / 2, MATRIX_H / 2 + 2);
  matrix.print(msg);
  matrix.setFont(nullptr);
  matrix.show();
}

// ── Weather page ─────────────────────────────────────────────────────────────

// Map WMO weather code → short display string (max ~10 chars for TomThumb)
static const char* wmoShortName(int code) {
  if (code == 0 || code == 1) return "CLEAR";
  if (code <= 3)              return "PT CLOUDY";
  if (code <= 48)             return "CLOUDY";
  if (code <= 55)             return "DRIZZLE";
  if (code <= 67)             return "RAIN";
  if (code <= 77)             return "SNOW";
  if (code <= 82)             return "SHOWERS";
  if (code <= 86)             return "SNOW SHWRS";
  return "STORM";
}


// Full weather page: clock + icon + temperature + condition
void drawWeatherPage(const Weather &w, int hour, int min) {
  matrix.fillScreen(C_BLACK);

  // Clock "HH:MM" — pseudo-bold, amber, centered (same technique as callsign)
  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hour, min);
  matrix.setFont(nullptr);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  int n      = strlen(timeStr);
  int totalW = n * (CHAR_W + CHAR_GAP) - CHAR_GAP;
  int startX = (MATRIX_W - totalW) / 2;
  for (int dy = 0; dy <= 1; dy++) {
    for (int dx = 0; dx <= 1; dx++) {
      for (int i = 0; i < n; i++) {
        matrix.setCursor(startX + i * (CHAR_W + CHAR_GAP) + dx, 2 + dy);
        matrix.setTextColor(dim(C_AMBER));
        matrix.print(timeStr[i]);
      }
    }
  }

  if (w.valid) {
    // Temperature — TomThumb, white, centered at row 15
    char tempStr[6];
    snprintf(tempStr, sizeof(tempStr), "%dC", (int)roundf(w.tempC));
    printCenteredTT(tempStr, 15, C_WHITE);

    // Wind (left) and visibility (right) on same row
    char windStr[10];
    if (w.windCardinal[0])
      snprintf(windStr, sizeof(windStr), "%s %d", w.windCardinal, (int)roundf(w.windSpeedKmh));
    else
      snprintf(windStr, sizeof(windStr), "%d", (int)roundf(w.windSpeedKmh));

    char visStr[8];
    snprintf(visStr, sizeof(visStr), "%dKM", w.visibilityKm);

    matrix.setFont(&TomThumb);
    matrix.setTextSize(1);
    matrix.setTextWrap(false);
    matrix.setTextColor(dim(C_AMBER));
    matrix.setCursor(2, 21);
    matrix.print(windStr);
    int16_t x1, y1; uint16_t vw, vh;
    matrix.getTextBounds(visStr, 0, 21, &x1, &y1, &vw, &vh);
    matrix.setCursor(MATRIX_W - 2 - (int)vw, 21);
    matrix.print(visStr);
    matrix.setFont(nullptr);

    // Condition — TomThumb, amber, centered at row 27
    printCenteredTT(wmoShortName(w.weatherCode), 27, C_AMBER);
  } else {
    printCenteredTT("NO DATA", 20, C_DEEP_BLUE);
  }

  matrix.show();
}
