// ─── Display rendering: all TFT drawing functions ──────

void drawHeader() {
  uint16_t hdrColor = trackingMode ? C_GREEN : C_AMBER;
  tft.fillRect(0, 0, W, HDR_H, hdrColor);
  tft.setTextColor(C_BG, hdrColor);
  tft.setTextSize(BP_HDR_TS);
  tft.setCursor(BP_HDR_X, BP_HDR_Y);
  tft.print("OVERHEAD TRACKER");
  int locW = strlen(LOCATION_NAME) * BP_LOC_CW;
  tft.setCursor(W - locW - BP_LOC_XOFF, BP_HDR_Y);
  tft.print(LOCATION_NAME);
}

void drawNavBar() {
#if !HAS_TOUCH
  return;
#else
  tft.fillRect(0, NAV_Y, W, NAV_H, C_BG);
  tft.drawFastHLine(0, NAV_Y, W, C_DIMMER);

  if (currentScreen == SCREEN_TRACK) {
    tft.setTextSize(1);
    tft.setTextColor(C_GREEN, C_BG);
    tft.setCursor(8, NAV_Y + 14);
    tft.print("SINGLE FLIGHT TRACKING MODE");
  } else if (currentScreen == SCREEN_FLIGHT && flightCount > 1) {
    char navBuf[16];
    snprintf(navBuf, sizeof(navBuf), "< %d/%d >", flightIndex + 1, flightCount);
    tft.setTextSize(2);
    tft.setTextColor(C_DIM, C_BG);
    tft.setCursor(8, NAV_Y + 10);
    tft.print(navBuf);
  }

  // WX button
  uint16_t wxBg = (currentScreen == SCREEN_WEATHER) ? C_CYAN : C_DIMMER;
  uint16_t wxFg = (currentScreen == SCREEN_WEATHER) ? C_BG   : C_AMBER;
  tft.fillRect(WX_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, wxBg);
  tft.setTextColor(wxFg, wxBg);
  tft.setTextSize(2);
  tft.setCursor(WX_BTN_X1 + (NAV_BTN_W - 24) / 2, NAV_Y + 10);
  tft.print("WX");

  // GEO button
  const char* geoLabels[] = {"5km", "10km", "20km"};
  tft.fillRect(GEO_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, C_DIMMER);
  tft.setTextColor(C_AMBER, C_DIMMER);
  tft.setTextSize(2);
  int geoW = strlen(geoLabels[geoIndex]) * 12;
  tft.setCursor(GEO_BTN_X1 + (NAV_BTN_W - geoW) / 2, NAV_Y + 10);
  tft.print(geoLabels[geoIndex]);

  // CFG button
  uint16_t cfgBg = isFetching ? C_RED : C_DIMMER;
  const char* cfgLbl = isFetching ? "..." : "CFG";
  tft.fillRect(CFG_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, cfgBg);
  tft.setTextColor(C_AMBER, cfgBg);
  tft.setTextSize(2);
  int cfgW = strlen(cfgLbl) * 12;
  tft.setCursor(CFG_BTN_X1 + (NAV_BTN_W - cfgW) / 2, NAV_Y + 10);
  tft.print(cfgLbl);
#endif
}

void drawStatusBar() {
  int y = H - FOOT_H;
  tft.fillRect(0, y, W, FOOT_H, C_BG);
  tft.drawFastHLine(0, y, W, C_DIMMER);
  tft.setTextColor(C_DIM, C_BG);
  tft.setTextSize(1);
  char buf[80];
  if (trackingMode && !isFetching) {
    snprintf(buf, sizeof(buf), "  TRACKING: %s   NEXT:%ds   H:%d",
             trackCallsign, trackCountdown, ESP.getFreeHeap());
  } else if (isFetching) {
    snprintf(buf, sizeof(buf), "  SCANNING AIRSPACE...");
  } else if (flightCount == 0) {
    const char* src = dataSource==2 ? "CACHE" : dataSource==1 ? "DIRECT" : "PROXY";
    snprintf(buf, sizeof(buf), "  CLEAR SKIES   SRC:%s   NEXT:%ds   H:%d",
             src, countdown, ESP.getFreeHeap());
  } else if (dataSource == 2 && cacheTimestamp > 0) {
    time_t now = time(NULL);
    long ageSec = (now > cacheTimestamp) ? (long)(now - cacheTimestamp) : 0;
    if (ageSec < 3600)
      snprintf(buf, sizeof(buf), "  AC %d/%d   SRC:CACHE(%ldm%lds)   NEXT:%ds",
               flightIndex+1, flightCount, ageSec/60, ageSec%60, countdown);
    else
      snprintf(buf, sizeof(buf), "  AC %d/%d   SRC:CACHE(%ldh%ldm)   NEXT:%ds",
               flightIndex+1, flightCount, ageSec/3600, (ageSec%3600)/60, countdown);
  } else {
    const char* src = dataSource==2 ? "CACHE" : dataSource==1 ? "DIRECT" : "PROXY";
    snprintf(buf, sizeof(buf), "  AC %d/%d   SRC:%s   NEXT:%ds   H:%d",
             flightIndex+1, flightCount, src, countdown, ESP.getFreeHeap());
  }
  tft.setCursor(BP_SB_X, y + BP_SB_YOFF);
  tft.print(buf);

  // WiFi indicator (right side of status bar)
  char wifiBuf[16];
  if (wifiOk()) {
    snprintf(wifiBuf, sizeof(wifiBuf), "WiFi %d", WiFi.RSSI());
    tft.setTextColor(C_DIM, C_BG);
  } else {
    snprintf(wifiBuf, sizeof(wifiBuf), "NO WIFI");
    tft.setTextColor(C_RED, C_BG);
  }
  int wifiW = strlen(wifiBuf) * 6;
  tft.setCursor(W - wifiW - BP_SB_X, y + BP_SB_YOFF);
  tft.print(wifiBuf);
}

void renderMessage(const char* line1, const char* line2) {
  tft.fillScreen(C_BG);
  previousScreen = SCREEN_NONE;
  drawHeader();
  drawNavBar();
  tft.setTextColor(C_AMBER, C_BG);
  tft.setTextSize(2);
  tft.setCursor(16, H/2 - 16);
  tft.print(line1);
  if (line2) {
    tft.setTextColor(C_DIM, C_BG);
    tft.setTextSize(1);
    tft.setCursor(16, H/2 + 12);
    tft.print(line2);
  }
}

// ─── renderFlight sub-renderers ──────────────────────

static void renderFlightIdentity(const Flight& f, int& y, bool hasEmergency) {
  int csSize = hasEmergency ? BP_CS_TS_EMERG : BP_CS_TS;
  tft.setTextSize(csSize);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(15, y);
  tft.print(f.callsign[0] ? f.callsign : "SEARCHING");
  y += csSize * 8;
  if (!hasEmergency) {
    const Airline* al = getAirline(f.callsign);
    tft.setTextSize(BP_AL_TS);
    tft.setTextColor(al ? al->color : C_DIM, C_BG);
    tft.setCursor(15, y);
    tft.print(al ? al->name : "UNKNOWN AIRLINE");
    y += BP_AL_YINC;
  } else {
    y += 8;
  }
}

static void renderFlightInfo(const Flight& f, int& y) {
  tft.drawFastHLine(10, y, W - 20, C_DIMMER);
  y += 8;
  tft.setTextSize(1);
  const char* acCat = getAircraftCategory(f.type);
  tft.setTextColor(acCat ? C_AMBER : C_DIM, C_BG);
  tft.setCursor(15, y);
  tft.print(acCat ? acCat : "AIRCRAFT TYPE");
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(W/2 + BP_REG_X_OFF, y);
  tft.print("REGISTRATION");
  y += 10;
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN, C_BG);
  tft.setCursor(15, y);
  tft.print(getAircraftTypeName(f.type));
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(W/2 + BP_REG_X_OFF, y);
  tft.print(f.reg[0] ? f.reg : "---");
  y += 20;
  tft.drawFastHLine(10, y, W - 20, C_DIMMER);
  y += 8;
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(15, y);
  tft.print("ROUTE");
  y += 10;
  tft.setTextSize(2);
  if (f.route[0]) {
    tft.setTextColor(C_YELLOW, C_BG);
    tft.setCursor(15, y);
    tft.print(f.route);
  } else {
    tft.setTextColor(C_DIMMER, C_BG);
    tft.setCursor(15, y);
    tft.print("NO ROUTE DATA");
  }
}

static void renderDashPhase(const Flight& f, int dashY) {
  uint16_t sCol = statusColor(f.status);
  tft.fillRect(0, dashY + 1, 4, BP_DASH_H - 1, sCol);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(BP_DASH_X_OFF, dashY + 5);
  tft.print("PHASE");
  tft.setTextSize(BP_DASH_TS);
  tft.setTextColor(sCol, C_BG);
  tft.setCursor(BP_DASH_X_OFF, dashY + BP_DASH_VAL_Y);
  tft.print(statusLabel(f.status));
  bool emerg = strcmp(f.squawk,"7700")==0 || strcmp(f.squawk,"7600")==0 || strcmp(f.squawk,"7500")==0;
  const char* sqLabel = strcmp(f.squawk,"7700")==0 ? "MAYDAY" :
                        strcmp(f.squawk,"7600")==0 ? "NORDO"  :
                        strcmp(f.squawk,"7500")==0 ? "HIJACK" : f.squawk;
  tft.setTextColor(emerg ? C_RED : C_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(BP_DASH_X_OFF, dashY + BP_DASH_VS_Y);
  tft.print("SQK ");
  tft.print(sqLabel);
}

static void renderDashAlt(const Flight& f, int dashY, int colW) {
  char altBuf[20];
  formatAlt(f.alt, altBuf, sizeof(altBuf));
  tft.drawFastVLine(colW, dashY + 5, BP_DASH_H - 10, C_DIMMER);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(colW + BP_DASH_X_OFF, dashY + 5);
  tft.print("ALT");
  tft.setTextSize(BP_DASH_TS);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(colW + BP_DASH_X_OFF, dashY + BP_DASH_VAL_Y);
  tft.print(altBuf);
  tft.setTextSize(1);
  if (abs(f.vs) >= 50) {
    char vsBuf[16];
    snprintf(vsBuf, sizeof(vsBuf), f.vs > 0 ? "+%d" : "%d", f.vs);
    tft.setTextColor(f.vs > 0 ? C_GREEN : C_RED, C_BG);
    tft.setCursor(colW + BP_DASH_X_OFF, dashY + BP_DASH_VS_Y);
    tft.print(vsBuf);
  } else {
    tft.setTextColor(C_AMBER, C_BG);
    tft.setCursor(colW + BP_DASH_X_OFF, dashY + BP_DASH_VS_Y);
    tft.print("LEVEL");
  }
}

static void renderDashSpeed(const Flight& f, int dashY, int colW) {
  tft.drawFastVLine(colW * 2, dashY + 5, BP_DASH_H - 10, C_DIMMER);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(colW * 2 + BP_DASH_X_OFF, dashY + 5);
  tft.print("SPD");
  tft.setTextSize(BP_DASH_TS);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(colW * 2 + BP_DASH_X_OFF, dashY + BP_DASH_VAL_Y);
  if (f.speed > 0) {
    char spdBuf[16];
    if (BP_DASH_TS == 1) {
      snprintf(spdBuf, sizeof(spdBuf), "%dKT", f.speed);
      tft.print(spdBuf);
    } else {
      snprintf(spdBuf, sizeof(spdBuf), "%d", f.speed);
      tft.print(spdBuf);
      tft.setTextSize(1);
      tft.print(" KT");
    }
  } else { tft.print("---"); }
}

static void renderDashDist(const Flight& f, int dashY, int colW) {
  tft.drawFastVLine(colW * 3, dashY + 5, BP_DASH_H - 10, C_DIMMER);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(colW * 3 + BP_DASH_X_OFF, dashY + 5);
  tft.print("DIST");
  tft.setTextColor(distanceColor(f.dist, GEOFENCE_KM), C_BG);
  tft.setTextSize(BP_DASH_TS);
  tft.setCursor(colW * 3 + BP_DASH_X_OFF, dashY + BP_DASH_VAL_Y);
  if (f.dist > 0) {
    char distBuf[16];
    if (BP_DASH_TS == 1) {
      if (f.dist >= 10.0f) snprintf(distBuf, sizeof(distBuf), "%.0fKM", f.dist);
      else                  snprintf(distBuf, sizeof(distBuf), "%.1f", f.dist);
      tft.print(distBuf);
    } else {
      if (f.dist >= 10.0f) snprintf(distBuf, sizeof(distBuf), "%.0f", f.dist);
      else                  snprintf(distBuf, sizeof(distBuf), "%.1f", f.dist);
      tft.print(distBuf);
      tft.setTextSize(1);
      tft.print(" KM");
    }
  } else { tft.print("---"); }
}

static void renderFlightDashboard(const Flight& f) {
  const int COL_W = W / 4;
  int dashY = H - FOOT_H - BP_DASH_H;
  tft.drawFastHLine(0, dashY, W, C_DIM);
  renderDashPhase(f, dashY);
  renderDashAlt(f, dashY, COL_W);
  renderDashSpeed(f, dashY, COL_W);
  renderDashDist(f, dashY, COL_W);
}

void renderFlight(const Flight& f) {
  if (previousScreen != SCREEN_FLIGHT && previousScreen != SCREEN_TRACK) drawHeader();
  previousScreen = SCREEN_FLIGHT;
  drawNavBar();
  tft.fillRect(0, CONTENT_Y, W, CONTENT_H, C_BG);

  bool hasEmergency = strcmp(f.squawk,"7700")==0 || strcmp(f.squawk,"7600")==0 || strcmp(f.squawk,"7500")==0;
  int emergOffset = 0;
  if (hasEmergency) {
    const char* emergLabel = strcmp(f.squawk,"7700")==0 ? "EMERGENCY - MAYDAY" :
                             strcmp(f.squawk,"7600")==0 ? "EMERGENCY - NORDO"  :
                                                          "EMERGENCY - HIJACK";
    tft.fillRect(0, CONTENT_Y, W, BP_EMERG_H, C_RED);
    tft.setTextColor(C_BG, C_RED);
    tft.setTextSize(BP_EMERG_TS);
    tft.setCursor((W - (int)strlen(emergLabel) * BP_EMERG_CW) / 2, CONTENT_Y + BP_EMERG_YIN);
    tft.print(emergLabel);
    emergOffset = BP_EMERG_H;
  }
  int y = CONTENT_Y + emergOffset + 4;
  renderFlightIdentity(f, y, hasEmergency);
  renderFlightInfo(f, y);
  renderFlightDashboard(f);
  drawStatusBar();
}

// ─── renderWeather sub-renderers ─────────────────────

static void renderWeatherClock(int& cy, struct tm* t, bool fresh, bool ntpOk) {
  char timeBuf[8];
  if (ntpOk) snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t->tm_hour, t->tm_min);
  else       strlcpy(timeBuf, "--:--", sizeof(timeBuf));
  tft.setTextSize(BP_CLK_TS);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(BP_CLK_CX_OFF, cy);
  tft.print(timeBuf);
  cy += BP_CLK_YINC;
  if (ntpOk) {
    const char* dayNames[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    const char* monNames[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
    char dateBuf[20];
    snprintf(dateBuf, sizeof(dateBuf), "%s %d %s",
             dayNames[t->tm_wday], t->tm_mday, monNames[t->tm_mon]);
    int dateW = strlen(dateBuf) * 12;
    if (!fresh) tft.fillRect(0, cy, W, 16, C_BG);
    tft.setTextSize(2);
    tft.setTextColor(C_DIM, C_BG);
    tft.setCursor((W - dateW) / 2, cy);
    tft.print(dateBuf);
  }
  cy += BP_DATE_YINC;
}

static void renderWxTempRow(int lx, int rx, int& cy, bool fresh) {
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(lx, cy); tft.print("TEMPERATURE");
  tft.setCursor(rx, cy); tft.print("FEELS LIKE");
  cy += 10;
  if (!fresh) {
    tft.fillRect(lx, cy, W/2 - 15, 16, C_BG);
    tft.fillRect(rx, cy, W/2 - 15, 16, C_BG);
  }
  tft.setTextSize(2);
  tft.setTextColor(C_AMBER, C_BG);
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f C", wxData.temp);
  tft.setCursor(lx, cy); tft.print(buf);
  snprintf(buf, sizeof(buf), "%.1f C", wxData.feels_like);
  tft.setCursor(rx, cy); tft.print(buf);
  cy += 20;
  tft.drawFastHLine(10, cy, W - 20, C_DIMMER); cy += 6;
}

static void renderWxCondRow(int lx, int& cy, bool fresh) {
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(lx, cy); tft.print("CONDITIONS");
  cy += 10;
  if (!fresh) tft.fillRect(lx, cy, W - 20, 16, C_BG);
  tft.setTextSize(2);
  tft.setTextColor(C_YELLOW, C_BG);
  tft.setCursor(lx, cy); tft.print(wxData.condition);
  cy += 20;
  tft.drawFastHLine(10, cy, W - 20, C_DIMMER); cy += 6;
}

static void renderWxWindRow(int lx, int rx, int& cy, bool fresh) {
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(lx, cy); tft.print("HUMIDITY");
  tft.setCursor(rx, cy); tft.print("WIND");
  cy += 10;
  if (!fresh) {
    tft.fillRect(lx, cy, W/2 - 15, 16, C_BG);
    tft.fillRect(rx, cy, W/2 - 15, 16, C_BG);
  }
  tft.setTextSize(2);
  tft.setTextColor(C_AMBER, C_BG);
  char buf[32];
  snprintf(buf, sizeof(buf), "%d%%", wxData.humidity);
  tft.setCursor(lx, cy); tft.print(buf);
  snprintf(buf, sizeof(buf), "%.0f %s", wxData.wind_speed, wxData.wind_cardinal);
  tft.setCursor(rx, cy); tft.print(buf);
  cy += 20;
  tft.drawFastHLine(10, cy, W - 20, C_DIMMER); cy += 6;
}

static void renderWxTideRow(int lx, int rx, int cy, struct tm* t, bool fresh) {
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(lx, cy); tft.print("UV INDEX");
  if (wxData.tide_time[0]) { tft.setCursor(rx, cy); tft.print("TIDE"); }
  cy += 10;
  if (!fresh) {
    tft.fillRect(lx, cy, W/2 - 15, 16, C_BG);
    tft.fillRect(rx, cy, W/2 - 15, 16, C_BG);
  }
  uint16_t uvCol = wxData.uv_index < 3.0f ? C_GREEN :
                   wxData.uv_index < 6.0f ? C_YELLOW :
                   wxData.uv_index < 8.0f ? C_AMBER  : C_RED;
  tft.setTextSize(2);
  tft.setTextColor(uvCol, C_BG);
  char buf[64];
  snprintf(buf, sizeof(buf), "%.1f", wxData.uv_index);
  tft.setCursor(lx, cy); tft.print(buf);
  if (!wxData.tide_time[0]) return;
  uint16_t tideCol = wxData.tide_is_high ? C_TIDE_HI : C_TIDE_LO;
  tft.setTextColor(tideCol, C_BG);
  int tideH = (wxData.tide_time[0]-'0')*10 + (wxData.tide_time[1]-'0');
  int tideM = (wxData.tide_time[3]-'0')*10 + (wxData.tide_time[4]-'0');
  int diff  = tideH * 60 + tideM - (t->tm_hour * 60 + t->tm_min);
  if (diff < 0) diff += 1440;
  int dh = diff / 60, dm = diff % 60;
  char marker = wxData.tide_is_high ? 0x18 : 0x19;
  const char* lbl = wxData.tide_is_high ? "HI" : "LO";
  if (diff > 0 && diff < 1440) {
    if (dh > 0) snprintf(buf, sizeof(buf), "%c%s %dh%02dm %.1fm", marker, lbl, dh, dm, wxData.tide_height);
    else        snprintf(buf, sizeof(buf), "%c%s %dm %.1fm", marker, lbl, dm, wxData.tide_height);
  } else {
    snprintf(buf, sizeof(buf), "%c%s %s %.1fm", marker, lbl, wxData.tide_time, wxData.tide_height);
  }
  tft.setCursor(rx, cy); tft.print(buf);
}

static void renderWeatherRows(int lx, int rx, int cy, struct tm* t, bool fresh) {
  renderWxTempRow(lx, rx, cy, fresh);
  renderWxCondRow(lx, cy, fresh);
  renderWxWindRow(lx, rx, cy, fresh);
  renderWxTideRow(lx, rx, cy, t, fresh);
}

void renderWeather() {
  bool fresh = (previousScreen != SCREEN_WEATHER);
  if (fresh) { drawHeader(); tft.fillRect(0, CONTENT_Y, W, CONTENT_H, C_BG); }
  previousScreen = SCREEN_WEATHER;
  drawNavBar();
  time_t utcNow   = time(NULL);
  bool   ntpOk    = utcNow > 1000000000UL;
  time_t localNow = (ntpOk && wxReady && wxData.utc_offset_secs != 0)
                      ? utcNow + wxData.utc_offset_secs : utcNow;
  struct tm* t = gmtime(&localNow);
  int cy = CONTENT_Y + 12;
  renderWeatherClock(cy, t, fresh, ntpOk);
  tft.drawFastHLine(10, cy, W - 20, C_DIMMER);
  cy += 8;
  if (!wxReady) {
    if (!fresh) tft.fillRect(0, cy, W, CONTENT_H - (cy - CONTENT_Y), C_BG);
    tft.setTextSize(1);
    tft.setTextColor(C_DIMMER, C_BG);
    tft.setCursor(15, cy);
    tft.print("WEATHER LOADING...");
    drawStatusBar();
    return;
  }
  renderWeatherRows(10, W/2 + 10, cy, t, fresh);
  drawStatusBar();
}

// ─── Boot sequence ────────────────────────────────────
static int bootLineY = BP_BOOT_LINE_Y0;

static void bootLine(const char* label, const char* result, uint16_t col, int pauseMs) {
  tft.setTextColor(C_DIMMER, C_BG);
  tft.setTextSize(1);
  tft.setCursor(10, bootLineY);
  tft.print(label);
  int dotX = 10 + strlen(label) * 6;
  while (dotX < BP_BOOT_DOT_END) { tft.setCursor(dotX, bootLineY); tft.print("."); dotX += 6; }
  delay(pauseMs);
  tft.setTextColor(col, C_BG);
  tft.setCursor(BP_BOOT_RES_X, bootLineY);
  tft.print(result);
  bootLineY += 14;
  delay(10);
}

static void bootTitle() {
  tft.setTextColor(C_AMBER, C_BG);
  tft.setTextSize(BP_BOOT_TTL_TS);
  tft.setCursor(10, BP_BOOT_TTL_Y);
  tft.print("OVERHEAD TRACKER");
  tft.setTextColor(C_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(10, BP_BOOT_SUB_Y);
  tft.print(BP_BOOT_SUBTITLE);
  tft.drawFastHLine(0, BP_BOOT_SEP_Y, W, C_DIM);
}

void bootSequence() {
  tft.fillScreen(C_BG);
  bootLineY = BP_BOOT_LINE_Y0;
  for (int y = 0; y < H; y += 2) {
    tft.drawFastHLine(0, y, W, C_DIMMER);
    delayMicroseconds(200);
  }
  delay(30);
  tft.fillScreen(C_BG);
  delay(20);
  bootTitle();
  delay(100);

  char buf[40];
  bootLine("CPU",             "240 MHz  DUAL CORE",         C_GREEN,  30);
  snprintf(buf, sizeof(buf), "%d KB FREE", ESP.getFreeHeap() / 1024);
  bootLine("HEAP MEMORY",    buf,                            C_GREEN,  35);
  snprintf(buf, sizeof(buf), "%d KB", ESP.getFlashChipSize() / 1024);
  bootLine("FLASH SIZE",     buf,                            C_AMBER,  25);
  snprintf(buf, sizeof(buf), "%s", ESP.getSdkVersion());
  bootLine("ESP-IDF SDK",    buf,                            C_DIM,    20);
  bootLine("SPI BUS",        "CLK 40MHz  OK",                C_GREEN,  25);
  bootLine("DISPLAY",        BP_DISPLAY_NAME,                C_GREEN,  30);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  bootLine("WIFI MAC",       buf,                            C_AMBER,  30);
  bootLine("WIFI MODE",      "STA  802.11 B/G/N",            C_AMBER,  20);
#if HAS_SD
  bootLine("SD CARD",        "SEARCHING...",                  C_YELLOW, 80);
#endif
  snprintf(buf, sizeof(buf), "%s:%d", PROXY_HOST, PROXY_PORT);
  bootLine("PROXY TARGET",   buf,                            C_AMBER,  25);
  snprintf(buf, sizeof(buf), "%.4f, %.4f", HOME_LAT, HOME_LON);
  bootLine("HOME COORDS",    buf,                            C_AMBER,  25);
  snprintf(buf, sizeof(buf), "%.0f KM RADIUS", GEOFENCE_KM);
  bootLine("GEOFENCE",       buf,                            C_AMBER,  20);
  bootLine("ADS-B PIPELINE", "DECODER READY",                C_GREEN,  35);
  snprintf(buf, sizeof(buf), "%d SEC AUTO-REFRESH", REFRESH_SECS);
  bootLine("POLL INTERVAL",  buf,                            C_GREEN,  25);
  tft.drawFastHLine(0, bootLineY + 4, W, C_DIM);
  int flashY = bootLineY + 8;
  int textX  = (W - 13*12) / 2;
  tft.fillRect(8, flashY, W - 16, 24, C_GREEN);
  tft.setTextColor(C_BG, C_GREEN);
  tft.setTextSize(2);
  tft.setCursor(textX, flashY + 4);
  tft.print("SYSTEM ONLINE");
  delay(120);
  tft.fillRect(8, flashY, W - 16, 24, C_BG);
  delay(80);
  tft.fillRect(8, flashY, W - 16, 24, C_GREEN);
  tft.setTextColor(C_BG, C_GREEN);
  tft.setTextSize(2);
  tft.setCursor(textX, flashY + 4);
  tft.print("SYSTEM ONLINE");
  delay(400);
}

void drawOtaProgress(int pct) {
  static bool first = true;
  if (first) {
    first = false;
    tft.fillScreen(C_BG);
    tft.setTextColor(C_AMBER, C_BG);
    tft.setTextSize(BP_OTA_TTL_TS);
    tft.setCursor(BP_OTA_TTL_X, BP_OTA_TTL_Y);
    tft.print("OTA UPDATE");
    tft.setTextSize(BP_OTA_SUB_TS);
    tft.setTextColor(C_DIM, C_BG);
    tft.setCursor(BP_OTA_SUB_X, BP_OTA_SUB_Y);
    tft.print("Do not power off");
  }
  const int BX = BP_OTA_BX, BY = BP_OTA_BY, BW = BP_OTA_BW, BH = BP_OTA_BH;
  tft.drawRect(BX, BY, BW, BH, C_AMBER);
  tft.fillRect(BX + 1, BY + 1, (BW - 2) * pct / 100, BH - 2, C_GREEN);
}

// ─── Tracking mode: flight progress bar ─────────────
// Only compiled for 4.0" board (480x320). Sits at y=196..204, between
// route text (bottom y=192) and dashboard (y=225).
void drawProgressBar(float progress, const char* dep, const char* arr) {
#ifndef BOARD_2P8
  const int BAR_X = 47;
  const int BAR_Y = 196;
  const int BAR_W = 380;
  const int BAR_H = 8;

  // Clear the strip (includes IATA label area)
  tft.fillRect(0, BAR_Y - 2, W, BAR_H + 6, C_BG);

  // Dep IATA on left (textSize=1 = 6px/char × 8px tall)
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(15, BAR_Y);
  tft.print((dep && dep[0]) ? dep : "?");

  // Arr IATA on right
  const char* arrStr = (arr && arr[0]) ? arr : "?";
  int arrW = strlen(arrStr) * 6;
  tft.setCursor(W - 15 - arrW, BAR_Y);
  tft.print(arrStr);

  // Outer bar outline
  tft.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, C_DIMMER);

  if (progress >= 0.0f) {
    int fillW = (int)(progress * (float)(BAR_W - 2));
    if (fillW < 1) fillW = 1;
    if (fillW > BAR_W - 2) fillW = BAR_W - 2;

    // Lerp green → amber: r=0→248, g=252→160 as progress 0→1
    float t = progress;
    uint8_t r8 = (uint8_t)(248.0f * t);
    uint8_t g8 = (uint8_t)(252.0f - 92.0f * t);
    uint16_t barColor = ((r8 >> 3) << 11) | ((g8 >> 2) << 5);
    tft.fillRect(BAR_X + 1, BAR_Y + 1, fillW, BAR_H - 2, barColor);
    if (fillW < BAR_W - 2)
      tft.fillRect(BAR_X + 1 + fillW, BAR_Y + 1, BAR_W - 2 - fillW, BAR_H - 2, C_DIMMER);
  } else {
    // Unknown progress — dimmer fill
    tft.fillRect(BAR_X + 1, BAR_Y + 1, BAR_W - 2, BAR_H - 2, C_DIMMER);
  }
#endif
}

// ─── Tracking mode: territory line ──────────────────
// Sits at y=207..223, above dashboard (y=225).
void drawTerritoryLine(const char* territory) {
#ifndef BOARD_2P8
  const int TERR_Y = 207;
  tft.fillRect(0, TERR_Y, W, 18, C_BG);
  if (!territory || !territory[0]) return;

  char buf[64];
  snprintf(buf, sizeof(buf), "OVER %s", territory);
  for (int i = 0; buf[i]; i++) buf[i] = toupper((unsigned char)buf[i]);

  int charW = 12; // textSize=2
  int txtW  = strlen(buf) * charW;
  if (txtW > W - 10) {
    charW = 6; // textSize=1 fallback
    txtW  = strlen(buf) * charW;
  }
  tft.setTextSize(charW == 12 ? 2 : 1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor((W - txtW) / 2, TERR_Y);
  tft.print(buf);
#endif
}

// ─── Tracking mode: full flight + overlays ──────────
void renderTrackFlight(const Flight& f) {
  previousScreen = SCREEN_FLIGHT; // so renderFlight() skips header redraw
  renderFlight(f);
  currentScreen = SCREEN_TRACK;   // restore after renderFlight() sets SCREEN_FLIGHT
  drawNavBar();                    // redraw nav bar with TRACKING label
  drawProgressBar(trackProgress, f.dep, f.arr);
  drawTerritoryLine(trackTerritory);
}
