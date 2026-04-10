// ─── Display rendering: all TFT drawing functions ──────

void drawHeader() {
  tft.fillRect(0, 0, W, HDR_H, C_AMBER);
  tft.setTextColor(C_BG, C_AMBER);
#ifdef BOARD_2P8
  tft.setTextSize(1);
  tft.setCursor(4, 7);
  tft.print("OVERHEAD TRACKER");
  int locW = strlen(LOCATION_NAME) * 6;
  tft.setCursor(W - locW - 4, 7);
#else
  tft.setTextSize(2);
  tft.setCursor(8, 6);
  tft.print("OVERHEAD TRACKER");
  int locW = strlen(LOCATION_NAME) * 12;
  tft.setCursor(W - locW - 8, 6);
#endif
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
    tft.setTextColor(C_AMBER, C_BG);
    tft.setCursor(8, NAV_Y + 7);
    tft.print("TRACKING:");
    tft.setTextSize(2);
    tft.setTextColor(C_GREEN, C_BG);
    tft.setCursor(8, NAV_Y + 16);
    tft.print(trackCallsign);
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
#ifdef BOARD_2P8
    snprintf(buf, sizeof(buf), " AC %d/%d  %s  %ds  H:%d",
             flightIndex+1, flightCount, src, countdown, ESP.getFreeHeap());
#else
    snprintf(buf, sizeof(buf), "  AC %d/%d   SRC:%s   NEXT:%ds   H:%d",
             flightIndex+1, flightCount, src, countdown, ESP.getFreeHeap());
#endif
  }
#ifdef BOARD_2P8
  tft.setCursor(4, y + 4);
#else
  tft.setCursor(6, y + 6);
#endif
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
#ifdef BOARD_2P8
  tft.setCursor(W - wifiW - 4, y + 4);
#else
  tft.setCursor(W - wifiW - 6, y + 6);
#endif
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

void renderFlight(const Flight& f) {
  if (previousScreen != SCREEN_FLIGHT && previousScreen != SCREEN_TRACK) drawHeader();
  previousScreen = SCREEN_FLIGHT;

  drawNavBar();
  tft.fillRect(0, CONTENT_Y, W, CONTENT_H, C_BG);

  // Emergency squawk banner
  bool hasEmergency = strcmp(f.squawk,"7700")==0 || strcmp(f.squawk,"7600")==0 || strcmp(f.squawk,"7500")==0;
  int emergOffset = 0;
  if (hasEmergency) {
    const char* emergLabel = strcmp(f.squawk,"7700")==0 ? "EMERGENCY - MAYDAY" :
                             strcmp(f.squawk,"7600")==0 ? "EMERGENCY - NORDO"  :
                                                          "EMERGENCY - HIJACK";
#ifdef BOARD_2P8
    tft.fillRect(0, CONTENT_Y, W, 18, C_RED);
    tft.setTextColor(C_BG, C_RED);
    tft.setTextSize(1);
    int lblW = strlen(emergLabel) * 6;
    tft.setCursor((W - lblW) / 2, CONTENT_Y + 5);
    tft.print(emergLabel);
    emergOffset = 18;
#else
    tft.fillRect(0, CONTENT_Y, W, 24, C_RED);
    tft.setTextColor(C_BG, C_RED);
    tft.setTextSize(2);
    int lblW = strlen(emergLabel) * 12;
    tft.setCursor((W - lblW) / 2, CONTENT_Y + 4);
    tft.print(emergLabel);
    emergOffset = 24;
#endif
  }

  int x = 15;
  int y = CONTENT_Y + emergOffset + 4;

  // 1. PRIMARY IDENTITY
#ifdef BOARD_2P8
  int csSize = hasEmergency ? 2 : 3;
#else
  int csSize = hasEmergency ? 3 : 4;
#endif
  tft.setTextSize(csSize);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(x, y);
  tft.print(f.callsign[0] ? f.callsign : "SEARCHING");

  y += csSize * 8;
  if (!hasEmergency) {
    const Airline* al = getAirline(f.callsign);
#ifdef BOARD_2P8
    tft.setTextSize(1);
    tft.setTextColor(al ? al->color : C_DIM, C_BG);
    tft.setCursor(x, y);
    tft.print(al ? al->name : "UNKNOWN AIRLINE");
    y += 12;
#else
    tft.setTextSize(2);
    tft.setTextColor(al ? al->color : C_DIM, C_BG);
    tft.setCursor(x, y);
    tft.print(al ? al->name : "UNKNOWN AIRLINE");
    y += 20;
#endif
  } else {
    y += 8;
  }

  // 2. AIRCRAFT TYPE & REG
  tft.drawFastHLine(10, y, W - 20, C_DIMMER);
  y += 8;
  tft.setTextSize(1);
  const char* acCat = getAircraftCategory(f.type);
  tft.setTextColor(acCat ? C_AMBER : C_DIM, C_BG);
  tft.setCursor(x, y);
  tft.print(acCat ? acCat : "AIRCRAFT TYPE");
  tft.setTextColor(C_DIM, C_BG);
#ifdef BOARD_2P8
  tft.setCursor(W/2 + 10, y);
#else
  tft.setCursor(W/2 + 20, y);
#endif
  tft.print("REGISTRATION");
  y += 10;
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN, C_BG);
  tft.setCursor(x, y);
  tft.print(getAircraftTypeName(f.type));
  tft.setTextColor(C_AMBER, C_BG);
#ifdef BOARD_2P8
  tft.setCursor(W/2 + 10, y);
#else
  tft.setCursor(W/2 + 20, y);
#endif
  tft.print(f.reg[0] ? f.reg : "---");

  // 3. ROUTE SECTION
  y += 20;
  tft.drawFastHLine(10, y, W - 20, C_DIMMER);
  y += 8;
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(x, y);
  tft.print("ROUTE");
  y += 10;
  if (f.route[0]) {
    tft.setTextSize(2);
    tft.setTextColor(C_YELLOW, C_BG);
    tft.setCursor(x, y);
    tft.print(f.route);
  } else {
    tft.setTextSize(2);
    tft.setTextColor(C_DIMMER, C_BG);
    tft.setCursor(x, y);
    tft.print("NO ROUTE DATA");
  }

  // 4. DASHBOARD: PHASE | ALT | SPEED | DIST
  const int COL_W = W / 4;
#ifdef BOARD_2P8
  const int DASH_H = 58;
#else
  const int DASH_H = 75;
#endif
  int dashY = H - FOOT_H - DASH_H;
  tft.drawFastHLine(0, dashY, W, C_DIM);

  // Phase Block
  uint16_t sCol = statusColor(f.status);
  tft.fillRect(0, dashY + 1, 4, DASH_H - 1, sCol);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(8, dashY + 5);
  tft.print("PHASE");
#ifdef BOARD_2P8
  tft.setTextSize(1);
  tft.setTextColor(sCol, C_BG);
  tft.setCursor(8, dashY + 18);
  tft.print(statusLabel(f.status));
#else
  tft.setTextSize(2);
  tft.setTextColor(sCol, C_BG);
  tft.setCursor(12, dashY + 24);
  tft.print(statusLabel(f.status));
#endif

  // Squawk (below phase)
  bool emerg = strcmp(f.squawk,"7700")==0 || strcmp(f.squawk,"7600")==0 || strcmp(f.squawk,"7500")==0;
  const char* sqLabel = strcmp(f.squawk,"7700")==0 ? "MAYDAY" : strcmp(f.squawk,"7600")==0 ? "NORDO" : strcmp(f.squawk,"7500")==0 ? "HIJACK" : f.squawk;
  tft.setTextColor(emerg ? C_RED : C_DIM, C_BG);
  tft.setTextSize(1);
#ifdef BOARD_2P8
  tft.setCursor(8, dashY + 32);
#else
  tft.setCursor(12, dashY + 44);
#endif
  tft.print("SQK ");
  tft.print(sqLabel);

  // Altitude Block
  char altBuf[20];
  formatAlt(f.alt, altBuf, sizeof(altBuf));
  tft.drawFastVLine(COL_W, dashY + 5, DASH_H - 10, C_DIMMER);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(COL_W + 8, dashY + 5);
  tft.print("ALT");
#ifdef BOARD_2P8
  tft.setTextSize(1);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(COL_W + 8, dashY + 18);
  tft.print(altBuf);
#else
  tft.setTextSize(2);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(COL_W + 12, dashY + 24);
  tft.print(altBuf);
#endif
  tft.setTextSize(1);
  if (abs(f.vs) >= 50) {
    char vsBuf[16];
    if (f.vs > 0) {
      snprintf(vsBuf, sizeof(vsBuf), "+%d", f.vs);
      tft.setTextColor(C_GREEN, C_BG);
    } else {
      snprintf(vsBuf, sizeof(vsBuf), "%d", f.vs);
      tft.setTextColor(C_RED, C_BG);
    }
#ifdef BOARD_2P8
    tft.setCursor(COL_W + 8, dashY + 32);
#else
    tft.setCursor(COL_W + 12, dashY + 44);
#endif
    tft.print(vsBuf);
  } else {
    tft.setTextColor(C_AMBER, C_BG);
#ifdef BOARD_2P8
    tft.setCursor(COL_W + 8, dashY + 32);
#else
    tft.setCursor(COL_W + 12, dashY + 44);
#endif
    tft.print("LEVEL");
  }

  // Speed Block
  tft.drawFastVLine(COL_W * 2, dashY + 5, DASH_H - 10, C_DIMMER);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(COL_W * 2 + 8, dashY + 5);
  tft.print("SPD");
#ifdef BOARD_2P8
  tft.setTextSize(1);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(COL_W * 2 + 8, dashY + 18);
  if (f.speed > 0) {
    char spdBuf[16];
    snprintf(spdBuf, sizeof(spdBuf), "%dKT", f.speed);
    tft.print(spdBuf);
  } else {
    tft.print("---");
  }
#else
  tft.setTextSize(2);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(COL_W * 2 + 12, dashY + 24);
  if (f.speed > 0) {
    char spdBuf[16];
    snprintf(spdBuf, sizeof(spdBuf), "%d", f.speed);
    tft.print(spdBuf);
    tft.setTextSize(1);
    tft.print(" KT");
  } else {
    tft.print("---");
  }
#endif

  // Distance Block
  tft.drawFastVLine(COL_W * 3, dashY + 5, DASH_H - 10, C_DIMMER);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(COL_W * 3 + 8, dashY + 5);
  tft.print("DIST");
  tft.setTextColor(distanceColor(f.dist, GEOFENCE_KM), C_BG);
#ifdef BOARD_2P8
  tft.setTextSize(1);
  tft.setCursor(COL_W * 3 + 8, dashY + 18);
  if (f.dist > 0) {
    char distBuf[16];
    if (f.dist >= 10.0f) snprintf(distBuf, sizeof(distBuf), "%.0fKM", f.dist);
    else                  snprintf(distBuf, sizeof(distBuf), "%.1f", f.dist);
    tft.print(distBuf);
  } else {
    tft.print("---");
  }
#else
  tft.setTextSize(2);
  tft.setCursor(COL_W * 3 + 12, dashY + 24);
  if (f.dist > 0) {
    char distBuf[16];
    if (f.dist >= 10.0f) snprintf(distBuf, sizeof(distBuf), "%.0f", f.dist);
    else                  snprintf(distBuf, sizeof(distBuf), "%.1f", f.dist);
    tft.print(distBuf);
    tft.setTextSize(1);
    tft.print(" KM");
  } else {
    tft.print("---");
  }
#endif

  drawStatusBar();
}

void renderWeather() {
  bool fresh = (previousScreen != SCREEN_WEATHER);
  if (fresh) { drawHeader(); tft.fillRect(0, CONTENT_Y, W, CONTENT_H, C_BG); }
  previousScreen = SCREEN_WEATHER;

  drawNavBar();

  // ── Clock ──
  time_t utcNow  = time(NULL);
  bool   ntpOk   = utcNow > 1000000000UL;
  time_t localNow = (ntpOk && wxReady && wxData.utc_offset_secs != 0)
                      ? utcNow + wxData.utc_offset_secs : utcNow;
  struct tm* t   = gmtime(&localNow);

  int cy = CONTENT_Y + 12;
  char timeBuf[8];
  if (ntpOk) snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t->tm_hour, t->tm_min);
  else       strlcpy(timeBuf, "--:--", sizeof(timeBuf));

#ifdef BOARD_2P8
  tft.setTextSize(4);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor((W - 5*24) / 2, cy);
  tft.print(timeBuf);
  cy += 36;
#else
  tft.setTextSize(6);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor((W - 180) / 2, cy);
  tft.print(timeBuf);
  cy += 52;
#endif

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
#ifdef BOARD_2P8
  cy += 18;
#else
  cy += 22;
#endif

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

  char buf[32];
  int lx = 10;   // left margin
  int rx = W/2 + 10;  // right column

  // Row 1: Temperature | Feels Like
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(lx, cy);      tft.print("TEMPERATURE");
  tft.setCursor(rx, cy);      tft.print("FEELS LIKE");
  cy += 10;
  if (!fresh) {
    tft.fillRect(lx, cy, W/2 - 15, 16, C_BG);
    tft.fillRect(rx, cy, W/2 - 15, 16, C_BG);
  }
  tft.setTextSize(2);
  tft.setTextColor(C_AMBER, C_BG);
  snprintf(buf, sizeof(buf), "%.1f C", wxData.temp);
  tft.setCursor(lx, cy); tft.print(buf);
  snprintf(buf, sizeof(buf), "%.1f C", wxData.feels_like);
  tft.setCursor(rx, cy); tft.print(buf);
  cy += 20;

  tft.drawFastHLine(10, cy, W - 20, C_DIMMER); cy += 6;

  // Row 2: Condition
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

  // Row 3: Humidity | Wind
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(lx, cy);      tft.print("HUMIDITY");
  tft.setCursor(rx, cy);      tft.print("WIND");
  cy += 10;
  if (!fresh) {
    tft.fillRect(lx, cy, W/2 - 15, 16, C_BG);
    tft.fillRect(rx, cy, W/2 - 15, 16, C_BG);
  }
  tft.setTextSize(2);
  tft.setTextColor(C_AMBER, C_BG);
  snprintf(buf, sizeof(buf), "%d%%", wxData.humidity);
  tft.setCursor(lx, cy); tft.print(buf);
  snprintf(buf, sizeof(buf), "%.0f %s", wxData.wind_speed, wxData.wind_cardinal);
  tft.setCursor(rx, cy); tft.print(buf);
  cy += 20;

  tft.drawFastHLine(10, cy, W - 20, C_DIMMER); cy += 6;

  // Row 4: UV Index | Tide
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
  snprintf(buf, sizeof(buf), "%.1f", wxData.uv_index);
  tft.setCursor(lx, cy); tft.print(buf);
  if (wxData.tide_time[0]) {
    uint16_t tideCol = wxData.tide_is_high ? C_TIDE_HI : C_TIDE_LO;
    tft.setTextColor(tideCol, C_BG);
    // Compute countdown to next tide
    int tideH = (wxData.tide_time[0] - '0') * 10 + (wxData.tide_time[1] - '0');
    int tideM = (wxData.tide_time[3] - '0') * 10 + (wxData.tide_time[4] - '0');
    int tideMins = tideH * 60 + tideM;
    int nowMins  = t->tm_hour * 60 + t->tm_min;
    int diff     = tideMins - nowMins;
    if (diff < 0) diff += 24 * 60;  // tide is tomorrow
    if (diff > 0 && diff < 24 * 60) {
      int dh = diff / 60;
      int dm = diff % 60;
      if (dh > 0)
        snprintf(buf, sizeof(buf), "%c%s %dh%02dm %.1fm",
          wxData.tide_is_high ? 0x18 : 0x19,
          wxData.tide_is_high ? "HI" : "LO", dh, dm, wxData.tide_height);
      else
        snprintf(buf, sizeof(buf), "%c%s %dm %.1fm",
          wxData.tide_is_high ? 0x18 : 0x19,
          wxData.tide_is_high ? "HI" : "LO", dm, wxData.tide_height);
    } else {
      snprintf(buf, sizeof(buf), "%c%s %s %.1fm",
        wxData.tide_is_high ? 0x18 : 0x19,
        wxData.tide_is_high ? "HI" : "LO",
        wxData.tide_time, wxData.tide_height);
    }
    tft.setCursor(rx, cy); tft.print(buf);
  }

  drawStatusBar();
}

// ── Boot sequence ───────────────────────────────────────
static int bootLineY = 56;

#ifdef BOARD_2P8
  static const int BOOT_DOT_END = 140;
  static const int BOOT_RESULT_X = 142;
#else
  static const int BOOT_DOT_END = 212;
  static const int BOOT_RESULT_X = 214;
#endif

static void bootLine(const char* label, const char* result, uint16_t col, int pauseMs) {
  tft.setTextColor(C_DIMMER, C_BG);
  tft.setTextSize(1);
  tft.setCursor(10, bootLineY);
  tft.print(label);
  int dotX = 10 + strlen(label) * 6;
  while (dotX < BOOT_DOT_END) { tft.setCursor(dotX, bootLineY); tft.print("."); dotX += 6; }
  delay(pauseMs);
  tft.setTextColor(col, C_BG);
  tft.setCursor(BOOT_RESULT_X, bootLineY);
  tft.print(result);
  bootLineY += 14;
  delay(10);
}

void bootSequence() {
  tft.fillScreen(C_BG);
#ifdef BOARD_2P8
  bootLineY = 42;
#else
  bootLineY = 56;
#endif
  for (int y = 0; y < H; y += 2) {
    tft.drawFastHLine(0, y, W, C_DIMMER);
    delayMicroseconds(200);
  }
  delay(30);
  tft.fillScreen(C_BG);
  delay(20);

  tft.setTextColor(C_AMBER, C_BG);
#ifdef BOARD_2P8
  tft.setTextSize(1);
  tft.setCursor(10, 6);
  tft.print("OVERHEAD TRACKER");
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(10, 20);
  tft.print("ADS-B SURVEILLANCE  REV 3.2");
  tft.drawFastHLine(0, 33, W, C_DIM);
#else
  tft.setTextSize(2);
  tft.setCursor(10, 12);
  tft.print("OVERHEAD TRACKER");
  tft.setTextColor(C_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 34);
  tft.print("ADS-B AIRSPACE SURVEILLANCE  REV 3.2");
  tft.drawFastHLine(0, 47, W, C_DIM);
#endif
  delay(100);

  char buf[40];
  snprintf(buf, sizeof(buf), "240 MHz  DUAL CORE");
  bootLine("CPU",            buf,                    C_GREEN,  30);
  snprintf(buf, sizeof(buf), "%d KB FREE", ESP.getFreeHeap() / 1024);
  bootLine("HEAP MEMORY",    buf,                    C_GREEN,  35);
  snprintf(buf, sizeof(buf), "%d KB",      ESP.getFlashChipSize() / 1024);
  bootLine("FLASH SIZE",     buf,                    C_AMBER,  25);
  snprintf(buf, sizeof(buf), "%s", ESP.getSdkVersion());
  bootLine("ESP-IDF SDK",    buf,                    C_DIM,    20);
  bootLine("SPI BUS",        "CLK 40MHz  OK",        C_GREEN,  25);
#ifdef BOARD_2P8
  bootLine("DISPLAY",        "ST7789 320x240 16BIT", C_GREEN,  30);
#else
  bootLine("DISPLAY",        "ST7796 480x320 16BIT", C_GREEN,  30);
#endif
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  bootLine("WIFI MAC",       buf,                    C_AMBER,  30);
  bootLine("WIFI MODE",      "STA  802.11 B/G/N",    C_AMBER,  20);
#if HAS_SD
  bootLine("SD CARD",        "SEARCHING...",         C_YELLOW, 80);
#endif
  snprintf(buf, sizeof(buf), "%s:%d", PROXY_HOST, PROXY_PORT);
  bootLine("PROXY TARGET",   buf,                    C_AMBER,  25);
  snprintf(buf, sizeof(buf), "%.4f, %.4f", HOME_LAT, HOME_LON);
  bootLine("HOME COORDS",    buf,                    C_AMBER,  25);
  snprintf(buf, sizeof(buf), "%.0f KM RADIUS", GEOFENCE_KM);
  bootLine("GEOFENCE",       buf,                    C_AMBER,  20);
  bootLine("ADS-B PIPELINE", "DECODER READY",        C_GREEN,  35);
  snprintf(buf, sizeof(buf), "%d SEC AUTO-REFRESH", REFRESH_SECS);
  bootLine("POLL INTERVAL",  buf,                    C_GREEN,  25);
  tft.drawFastHLine(0, bootLineY + 4, W, C_DIM);
  int flashY = bootLineY + 8;

  tft.fillRect(8, flashY, W - 16, 24, C_GREEN);
  tft.setTextColor(C_BG, C_GREEN);
  tft.setTextSize(2);
  int textX = (W - 13*12) / 2;
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
#ifdef BOARD_2P8
    tft.setTextSize(2);
    tft.setCursor(50, 80);
    tft.print("OTA UPDATE");
    tft.setTextSize(1);
    tft.setTextColor(C_DIM, C_BG);
    tft.setCursor(40, 110);
    tft.print("Do not power off");
#else
    tft.setTextSize(3);
    tft.setCursor(100, 110);
    tft.print("OTA UPDATE");
    tft.setTextSize(2);
    tft.setTextColor(C_DIM, C_BG);
    tft.setCursor(80, 155);
    tft.print("Do not power off");
#endif
  }
#ifdef BOARD_2P8
  const int BX = 20, BY = 157, BW = 280, BH = 20;
#else
  const int BX = 40, BY = 210, BW = 400, BH = 24;
#endif
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
