// display.ino — all rendering for the 64×32 HUB75 matrix

#include <Fonts/TomThumb.h>
#include "anim_boot.h"

// Weather icon type constants
#define ICON_SUN   0
#define ICON_CLOUD 1
#define ICON_RAIN  2
#define ICON_SNOW  3
#define ICON_STORM 4

// Draw the callsign pseudo-bold (4 overlapping copies at +1 offsets, matching CircuitPython technique)
static void drawCallsign(const char *cs, int scrollOffset = 0, uint16_t color = C_AMBER) {
  if (!cs || !cs[0]) return;
  matrix.setFont(nullptr);   // built-in 6×8 font
  matrix.setTextSize(1);
  matrix.setTextWrap(false);

  int n      = strlen(cs);
  int totalW = n * (CHAR_W + CHAR_GAP) - CHAR_GAP;
  int startX = (totalW > MATRIX_W) ? -scrollOffset : (MATRIX_W - totalW) / 2;

  // 4-offset pseudo-bold
  for (int dy = 0; dy <= 1; dy++) {
    for (int dx = 0; dx <= 1; dx++) {
      for (int i = 0; i < n; i++) {
        // setCursor positions baseline; for built-in font, baseline = y + 7
        matrix.setCursor(startX + i * (CHAR_W + CHAR_GAP) + dx, CALLSIGN_Y + dy);
        matrix.setTextColor(color);
        matrix.print(cs[i]);
      }
    }
  }
}

// Draw route as two centred lines using TomThumb font.
// If no route, show aircraft type centred between the two lines.
static void drawRoute(const char *origin, const char *dest, const char *type, uint16_t typeColor = C_AMBER, uint16_t routeColor = C_WHITE) {
  matrix.setFont(&TomThumb);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);

  int16_t x1, y1;
  uint16_t w, h;

  auto printCentered = [&](const char *text, int y) {
    matrix.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
    matrix.setCursor((MATRIX_W - (int)w) / 2, y);
    matrix.print(text);
  };

  if (typeColor == C_WHITE) {
    // GA aircraft: rego already in callsign area; label the category here
    matrix.setTextColor(routeColor);
    printCentered("GENERAL",  ROUTE_TOP_Y);
    printCentered("AVIATION", ROUTE_BOT_Y);
  } else if (origin && origin[0]) {
    matrix.setTextColor(routeColor);
    printCentered(origin, ROUTE_TOP_Y);
    if (dest && dest[0]) printCentered(dest, ROUTE_BOT_Y);
  } else if (type && type[0]) {
    matrix.setTextColor(typeColor);
    printCentered(type, (ROUTE_TOP_Y + ROUTE_BOT_Y) / 2);
  }

  matrix.setFont(nullptr);
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
  if (ah > 0) matrix.drawFastVLine(0,           MATRIX_H - ah - 1, ah, C_DEEP_BLUE);
  if (sh > 0) matrix.drawFastVLine(MATRIX_W - 1, MATRIX_H - sh - 1, sh, C_LIGHT_BLUE);
}

// Centred distance bar: 64px wide when overhead, 3px at geofence edge.
static void drawDistanceBar(float dist, bool valid) {
  if (!valid) return;
  float clamped = dist < 0 ? 0 : (dist > (float)GEOFENCE_KM ? (float)GEOFENCE_KM : dist);
  int barW = (int)(3.0f + (1.0f - clamped / (float)GEOFENCE_KM) * (MATRIX_W - 3));
  int startX = (MATRIX_W - barW) / 2;
  matrix.fillRect(startX, MATRIX_H - 1, barW, 1, C_AMBER);
}

// Full frame: clear → bars → callsign → route → progress → show
void drawAll(const Flight &f, int progressPx, bool showType) {
  matrix.fillScreen(C_BLACK);
  drawBars(f.alt, f.speed);

  int scrollOffset = 0;
  if (showType && f.type[0]) {
    int totalW = strlen(f.type) * (CHAR_W + CHAR_GAP) - CHAR_GAP;
    if (totalW > MATRIX_W) {
      int maxScroll = totalW - MATRIX_W + 2;
      int t      = progressPx - TYPE_FLIP_PX;
      int period = 2 * maxScroll;
      int mod    = t % period;
      scrollOffset = (mod <= maxScroll) ? mod : (period - mod);
    }
  }
  bool isGA = (f.typeColor == C_WHITE);
  bool flipToType = showType && f.type[0] && !isGA;
  uint16_t csColor = flipToType ? f.typeColor : f.callsignColor;
  drawCallsign(flipToType ? f.type : f.callsign, scrollOffset, csColor);
  uint16_t routeColor = isGA ? C_AMBER : C_WHITE;
  drawRoute(f.origin, f.dest, f.type, f.typeColor, routeColor);
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

// Flash brightness level as 3 filled blocks — one per level (0/1/2)
void flashBrightness(uint8_t level) {
  const int BLK_W = 12, BLK_H = 10;
  const int GAP   = 4;
  const int totalW = 3 * BLK_W + 2 * GAP;
  const int startX = (MATRIX_W - totalW) / 2;
  const int startY = (MATRIX_H - BLK_H) / 2;

  matrix.fillScreen(C_BLACK);
  for (int i = 0; i < 3; i++) {
    uint16_t color = (i <= (int)level) ? C_AMBER : C_DEEP_BLUE;
    matrix.fillRect(startX + i * (BLK_W + GAP), startY, BLK_W, BLK_H, color);
  }
  matrix.show();
  delay(800);
}

// Flash "DONT TOUCH!!" on press — momentary test for button input
void flashDontTouch() {
  matrix.fillScreen(C_BLACK);
  matrix.setFont(nullptr);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.setTextColor(C_WHITE);

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

// Boot status: clear screen and print a single centred status line in TomThumb
void drawBootStatus(const char *msg) {
  matrix.fillScreen(C_BLACK);
  matrix.setFont(&TomThumb);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.setTextColor(C_AMBER);
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

// Map WMO code → icon type constant
static int wmoToIconType(int code) {
  if (code == 0 || code == 1)       return ICON_SUN;
  if (code >= 95)                   return ICON_STORM;
  if (code >= 85 || (code >= 71 && code <= 77)) return ICON_SNOW;
  if (code >= 51)                   return ICON_RAIN;
  return ICON_CLOUD;
}

// Draw a procedural weather icon at pixel (x, y), occupying a ~12×10 region.
// G/B channels are swapped on this panel — all colors already account for that.
static void drawWeatherIcon(int x, int y, int iconType) {
  // Cloud blob shared by CLOUD/RAIN/SNOW/STORM — white, top of the icon
  auto drawCloud = [&](int cx, int cy) {
    // Two bumps + base rectangle
    matrix.fillCircle(cx - 2, cy,     2, C_WHITE);
    matrix.fillCircle(cx + 2, cy - 1, 2, C_WHITE);
    matrix.fillRect(cx - 4, cy + 1, 9, 2, C_WHITE);
  };

  switch (iconType) {
    case ICON_SUN: {
      // Circle body (radius 3) + 8 single-pixel rays
      matrix.drawCircle(x + 5, y + 4, 3, C_AMBER);
      matrix.drawPixel(x + 5, y,      C_AMBER); // top
      matrix.drawPixel(x + 5, y + 8,  C_AMBER); // bottom
      matrix.drawPixel(x,     y + 4,  C_AMBER); // left
      matrix.drawPixel(x + 10, y + 4, C_AMBER); // right
      matrix.drawPixel(x + 1,  y + 1, C_AMBER); // diagonals
      matrix.drawPixel(x + 9,  y + 1, C_AMBER);
      matrix.drawPixel(x + 1,  y + 7, C_AMBER);
      matrix.drawPixel(x + 9,  y + 7, C_AMBER);
      break;
    }
    case ICON_CLOUD: {
      drawCloud(x + 6, y + 4);
      break;
    }
    case ICON_RAIN: {
      drawCloud(x + 6, y + 2);
      // Three angled rain streaks below
      uint16_t rc = C_LIGHT_BLUE;
      matrix.drawPixel(x + 2, y + 6, rc);
      matrix.drawPixel(x + 2, y + 8, rc);
      matrix.drawPixel(x + 5, y + 7, rc);
      matrix.drawPixel(x + 5, y + 9, rc);
      matrix.drawPixel(x + 8, y + 6, rc);
      matrix.drawPixel(x + 8, y + 8, rc);
      break;
    }
    case ICON_SNOW: {
      drawCloud(x + 6, y + 2);
      // Six snow dots in two rows
      matrix.drawPixel(x + 2, y + 6, C_WHITE);
      matrix.drawPixel(x + 5, y + 7, C_WHITE);
      matrix.drawPixel(x + 8, y + 6, C_WHITE);
      matrix.drawPixel(x + 2, y + 9, C_WHITE);
      matrix.drawPixel(x + 5, y + 9, C_WHITE);
      matrix.drawPixel(x + 8, y + 9, C_WHITE);
      break;
    }
    case ICON_STORM: {
      drawCloud(x + 6, y + 2);
      // Lightning bolt: zigzag in amber
      matrix.drawLine(x + 7, y + 5, x + 5, y + 7, C_AMBER);
      matrix.drawLine(x + 5, y + 7, x + 7, y + 7, C_AMBER);
      matrix.drawLine(x + 7, y + 7, x + 5, y + 9, C_AMBER);
      break;
    }
  }
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
        matrix.setTextColor(C_AMBER);
        matrix.print(timeStr[i]);
      }
    }
  }

  if (w.valid) {
    // Weather icon — 12×10 region, left side, rows 12-21
    drawWeatherIcon(2, 12, wmoToIconType(w.weatherCode));

    // Temperature — right of icon, vertically centred alongside it
    char tempStr[8];
    snprintf(tempStr, sizeof(tempStr), "%d\xb0" "C", (int)roundf(w.tempC));
    int tW = strlen(tempStr) * (CHAR_W + CHAR_GAP) - CHAR_GAP;
    matrix.setCursor(16 + (48 - tW) / 2, 15);
    matrix.setTextColor(C_WHITE);
    matrix.print(tempStr);

    // Condition — TomThumb, centred, bottom row
    matrix.setFont(&TomThumb);
    matrix.setTextColor(C_AMBER);
    const char *cond = wmoShortName(w.weatherCode);
    int16_t bx, by;
    uint16_t bw, bh;
    matrix.getTextBounds(cond, 0, 28, &bx, &by, &bw, &bh);
    matrix.setCursor((MATRIX_W - (int)bw) / 2, 28);
    matrix.print(cond);
    matrix.setFont(nullptr);
  } else {
    // Weather not yet fetched — show placeholder
    matrix.setFont(&TomThumb);
    matrix.setTextColor(C_DEEP_BLUE);
    matrix.setCursor(18, 20);
    matrix.print("NO DATA");
    matrix.setFont(nullptr);
  }

  matrix.show();
}
