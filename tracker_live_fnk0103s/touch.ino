// ─── Touch calibration & input handling ────────────────

bool loadTouchCal() {
  Preferences p;
  p.begin("tracker", true);
  if (!p.isKey("tcal")) { p.end(); return false; }
  p.getBytes("tcal", touchCalData, sizeof(touchCalData));
  p.end();
  return true;
}

void saveTouchCal() {
  Preferences p;
  p.begin("tracker", false);
  p.putBytes("tcal", touchCalData, sizeof(touchCalData));
  p.end();
}

void initTouch() {
  if (loadTouchCal()) {
    tft.setTouch(touchCalData);
    touchReady = true;
    Serial.println("Touch cal loaded.");
    return;
  }
  // First boot — run on-screen calibration routine
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, W, HDR_H, C_AMBER);
  tft.setTextColor(C_BG, C_AMBER);
  tft.setTextSize(2);
  tft.setCursor(8, 6);
  tft.print("OVERHEAD TRACKER");
  tft.setTextColor(C_AMBER, C_BG);
  tft.setTextSize(2);
  tft.setCursor(16, H/2 - 24);
  tft.print("TOUCH CALIBRATION");
  tft.setTextColor(C_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(16, H/2 + 4);
  tft.print("Tap each corner cross when it appears");
  delay(1200);
  tft.calibrateTouch(touchCalData, C_AMBER, C_BG, 15);
  saveTouchCal();
  touchReady = true;
  Serial.println("Touch calibrated and saved.");
}

void handleTouch(uint16_t tx, uint16_t ty) {
  uint32_t now = millis();
  if (now - lastTouchMs < TOUCH_DEBOUNCE_MS) return;
  lastTouchMs = now;

  // ── Nav bar buttons ──
  if (ty >= NAV_Y && ty < NAV_Y + NAV_H) {

    // WX button
    if (tx >= WX_BTN_X1 && tx < WX_BTN_X1 + NAV_BTN_W) {
      tft.fillRect(WX_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, C_AMBER);
      tft.setTextColor(C_BG, C_AMBER);
      tft.setTextSize(2);
      tft.setCursor(WX_BTN_X1 + (NAV_BTN_W - 24) / 2, NAV_Y + 10);
      tft.print("WX");
      delay(100);
      if (currentScreen == SCREEN_WEATHER) {
        currentScreen = SCREEN_FLIGHT;
        if (flightCount > 0) renderFlight(flights[flightIndex]);
        else renderMessage("NO AIRCRAFT", "IN RANGE");
      } else {
        currentScreen = SCREEN_WEATHER;
        renderWeather();
      }
      return;
    }

    // GEO button
    if (tx >= GEO_BTN_X1 && tx < GEO_BTN_X1 + NAV_BTN_W) {
      tft.fillRect(GEO_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, C_AMBER);
      tft.setTextColor(C_BG, C_AMBER);
      tft.setTextSize(2);
      tft.setCursor(GEO_BTN_X1 + 8, NAV_Y + 10);
      tft.print(geoIndex == 0 ? "5km" : geoIndex == 1 ? "10km" : "20km");
      delay(100);
      geoIndex = (geoIndex + 1) % GEO_COUNT;
      GEOFENCE_KM = GEO_PRESETS[geoIndex];
      saveGeoIndex();
      Serial.printf("Geofence: %.0f km\n", GEOFENCE_KM);
      drawNavBar();
      if (!isFetching) {
        flightCount = 0;
        flightIndex = 0;
        fetchFlights();
        countdown = REFRESH_SECS;
        lastCycle  = millis();
      }
      return;
    }

    // CFG button (two-tap confirmation — reboot is destructive)
    if (tx >= CFG_BTN_X1 && tx < CFG_BTN_X1 + NAV_BTN_W) {
      if (isFetching) return;
      tft.fillRect(CFG_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, C_RED);
      tft.setTextColor(C_BG, C_RED);
      tft.setTextSize(1);
      tft.setCursor(CFG_BTN_X1 + 5, NAV_Y + 6);
      tft.print("REBOOT?");
      tft.setCursor(CFG_BTN_X1 + 2, NAV_Y + 20);
      tft.print("TAP AGAIN");
      uint32_t confirmDeadline = millis() + 3000;
      bool confirmed = false;
      while (millis() < confirmDeadline) {
        uint16_t cx, cy;
        if (tft.getTouch(&cx, &cy)) {
          if (cx >= CFG_BTN_X1 && cy >= NAV_Y && cy < NAV_Y + NAV_H) {
            confirmed = true;
            break;
          } else {
            break;
          }
        }
        delay(30);
      }
      if (confirmed) {
        startCaptivePortal();
      } else {
        drawNavBar();
      }
      return;
    }
    return;
  }

  // ── Content area: tap left/right to cycle flights ──
  if (ty >= CONTENT_Y && ty < (H - FOOT_H) &&
      currentScreen == SCREEN_FLIGHT && flightCount > 1) {
    if (tx < W / 2) {
      flightIndex = (flightIndex - 1 + flightCount) % flightCount;
    } else {
      flightIndex = (flightIndex + 1) % flightCount;
    }
    lastCycle = millis();
    renderFlight(flights[flightIndex]);
    return;
  }
}
