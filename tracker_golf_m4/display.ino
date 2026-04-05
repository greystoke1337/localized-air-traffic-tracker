// display.ino — all rendering for the 64×32 HUB75 matrix

#include <Fonts/TomThumb.h>
#include "anim_boot.h"

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
static void drawRoute(const char *origin, const char *dest, const char *type, uint16_t typeColor = C_AMBER) {
  matrix.setFont(&TomThumb);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);

  int16_t x1, y1;
  uint16_t w, h;

  if (origin && origin[0]) {
    matrix.setTextColor(C_WHITE);
    matrix.getTextBounds(origin, 0, ROUTE_TOP_Y, &x1, &y1, &w, &h);
    matrix.setCursor((MATRIX_W - (int)w) / 2, ROUTE_TOP_Y);
    matrix.print(origin);

    if (dest && dest[0]) {
      matrix.getTextBounds(dest, 0, ROUTE_BOT_Y, &x1, &y1, &w, &h);
      matrix.setCursor((MATRIX_W - (int)w) / 2, ROUTE_BOT_Y);
      matrix.print(dest);
    }
  } else if (type && type[0]) {
    matrix.setTextColor(typeColor);
    int midY = (ROUTE_TOP_Y + ROUTE_BOT_Y) / 2;
    matrix.getTextBounds(type, 0, midY, &x1, &y1, &w, &h);
    matrix.setCursor((MATRIX_W - (int)w) / 2, midY);
    matrix.print(type);
  }

  matrix.setFont(nullptr);
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
  int ah = mapBarH(alt,   ALT_MIN_FT, ALT_MAX_FT);
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
  uint16_t csColor = showType && f.type[0] ? f.typeColor : f.callsignColor;
  drawCallsign(showType && f.type[0] ? f.type : f.callsign, scrollOffset, csColor);
  drawRoute(f.origin, f.dest, f.type, f.typeColor);
  drawDistanceBar(f.dist, f.valid);
  matrix.show();
}

// Play boot animation from PROGMEM for a given duration, looping frames.
// Does nothing if anim_boot.h has ANIM_FRAMES == 0 (placeholder).
void playBootAnimFor(uint32_t durationMs) {
  if (ANIM_FRAMES == 0) return;
  uint32_t end = millis() + durationMs;
  int f = 0;
  while (millis() < end) {
    for (int i = 0; i < MATRIX_W * MATRIX_H; i++) {
      uint16_t c = pgm_read_word(&bootAnim[f][i]);
      matrix.drawPixel(i % MATRIX_W, i / MATRIX_W, c);
    }
    matrix.show();
    delay(ANIM_DELAY_MS);
    f = (f + 1) % ANIM_FRAMES;
  }
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
