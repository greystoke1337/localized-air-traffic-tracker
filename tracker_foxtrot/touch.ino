// ─── Touch input — GT911 polled via LovyanGFX, manual hit-testing ──────────

// Declared in display.ino
extern bool          cfgConfirming;
extern unsigned long cfgConfirmStart;

void initTouch() {
  touchReady = true;
  Serial.println("Touch ready (GT911 via LovyanGFX).");
}

void pollTouch() {
  // CFG confirm timeout (3 s)
  if (cfgConfirming && millis() - cfgConfirmStart >= 3000) {
    cfgConfirming = false;
    drawNavBar();
  }

  lgfx::touch_point_t tp;
  if (!tft.getTouch(&tp)) return;
  if (millis() - lastTouchMs < TOUCH_DEBOUNCE_MS) return;
  lastTouchMs = millis();

  int tx = tp.x, ty = tp.y;

  // ── Nav bar ──
  if (ty >= NAV_Y && ty < NAV_Y + NAV_H) {
    if (tx >= WX_BTN_X1 && tx < WX_BTN_X1 + NAV_BTN_W) {
      if (currentScreen == SCREEN_WEATHER) {
        currentScreen = SCREEN_FLIGHT;
        if (flightCount > 0) renderFlight(flights[flightIndex]);
        else                 renderMessage("NO AIRCRAFT", "IN RANGE");
      } else {
        currentScreen = SCREEN_WEATHER;
        renderWeather();
      }
      return;
    }
    if (tx >= GEO_BTN_X1 && tx < GEO_BTN_X1 + NAV_BTN_W) {
      if (isFetching) return;
      geoIndex = (geoIndex + 1) % GEO_COUNT;
      GEOFENCE_MI = GEO_PRESETS[geoIndex];
      saveGeoIndex();
      drawNavBar();
      flightCount = 0;
      flightIndex = 0;
      fetchFlights();
      if (flightCount == 0) {
        renderMessage("NO AIRCRAFT", "IN RANGE");
      } else {
        currentScreen = SCREEN_FLIGHT;
        renderFlight(flights[flightIndex]);
      }
      countdown = REFRESH_SECS;
      lastCycle = millis();
      return;
    }
    if (tx >= CFG_BTN_X1 && tx < CFG_BTN_X1 + NAV_BTN_W) {
      if (isFetching) return;
      if (!cfgConfirming) {
        cfgConfirming   = true;
        cfgConfirmStart = millis();
        drawNavBar();
      } else {
        cfgConfirming = false;
        triggerPortal = true;
      }
      return;
    }
  }

  // ── Content area — left/right tap to cycle flights ──
  if (currentScreen == SCREEN_FLIGHT && flightCount > 1) {
    int dashY = CONTENT_Y + CONTENT_H - 130;
    if (ty >= CONTENT_Y && ty < dashY) {
      flightIndex = (tx < W / 2)
        ? (flightIndex - 1 + flightCount) % flightCount
        : (flightIndex + 1) % flightCount;
      lastCycle = millis();
      renderFlight(flights[flightIndex]);
    }
  }
}
