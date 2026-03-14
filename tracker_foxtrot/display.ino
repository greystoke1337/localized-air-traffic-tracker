// ─── Display rendering: all TFT drawing functions (800x480) ──

void drawHeader() {
  tft.fillRect(0, 0, W, HDR_H, C_AMBER);
  tft.setTextColor(C_BG, C_AMBER);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("OVERHEAD TRACKER");
  int locW = strlen(LOCATION_NAME) * 12;
  tft.setCursor(W - locW - 10, 10);
  tft.print(LOCATION_NAME);
}

void drawNavBar() {
  tft.fillRect(0, NAV_Y, W, NAV_H, C_BG);
  tft.drawFastHLine(0, NAV_Y, W, C_DIMMER);

  if (currentScreen == SCREEN_FLIGHT && flightCount > 1) {
    char navBuf[16];
    snprintf(navBuf, sizeof(navBuf), "< %d/%d >", flightIndex + 1, flightCount);
    tft.setTextSize(2);
    tft.setTextColor(C_DIM, C_BG);
    tft.setCursor(10, NAV_Y + 14);
    tft.print(navBuf);
  }

  // WX button
  uint16_t wxBg = (currentScreen == SCREEN_WEATHER) ? C_CYAN : C_DIMMER;
  uint16_t wxFg = (currentScreen == SCREEN_WEATHER) ? C_BG   : C_AMBER;
  tft.fillRect(WX_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, wxBg);
  tft.setTextColor(wxFg, wxBg);
  tft.setTextSize(2);
  tft.setCursor(WX_BTN_X1 + (NAV_BTN_W - 24) / 2, NAV_Y + 14);
  tft.print("WX");

  // GEO button
  const char* geoLabels[] = {"5km", "10km", "20km"};
  tft.fillRect(GEO_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, C_DIMMER);
  tft.setTextColor(C_AMBER, C_DIMMER);
  tft.setTextSize(2);
  int geoW = strlen(geoLabels[geoIndex]) * 12;
  tft.setCursor(GEO_BTN_X1 + (NAV_BTN_W - geoW) / 2, NAV_Y + 14);
  tft.print(geoLabels[geoIndex]);

  // CFG button
  uint16_t cfgBg = isFetching ? C_RED : C_DIMMER;
  const char* cfgLbl = isFetching ? "..." : "CFG";
  tft.fillRect(CFG_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, cfgBg);
  tft.setTextColor(C_AMBER, cfgBg);
  tft.setTextSize(2);
  int cfgW = strlen(cfgLbl) * 12;
  tft.setCursor(CFG_BTN_X1 + (NAV_BTN_W - cfgW) / 2, NAV_Y + 14);
  tft.print(cfgLbl);
}

void drawStatusBar() {
  int y = H - FOOT_H;
  tft.fillRect(0, y, W, FOOT_H, C_BG);
  tft.drawFastHLine(0, y, W, C_DIMMER);
  tft.setTextColor(C_DIM, C_BG);
  tft.setTextSize(1);
  char buf[80];
  if (isFetching) {
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
  tft.setCursor(8, y + 8);
  tft.print(buf);
}

void renderMessage(const char* line1, const char* line2) {
  tft.fillScreen(C_BG);
  previousScreen = SCREEN_NONE;
  drawHeader();
  drawNavBar();
  tft.setTextColor(C_AMBER, C_BG);
  tft.setTextSize(2);
  tft.setCursor(20, H/2 - 20);
  tft.print(line1);
  if (line2) {
    tft.setTextColor(C_DIM, C_BG);
    tft.setTextSize(1);
    tft.setCursor(20, H/2 + 16);
    tft.print(line2);
  }
}

void renderFlight(const Flight& f) {
  if (previousScreen != SCREEN_FLIGHT) drawHeader();
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
    tft.fillRect(0, CONTENT_Y, W, 32, C_RED);
    tft.setTextColor(C_BG, C_RED);
    tft.setTextSize(2);
    int lblW = strlen(emergLabel) * 12;
    tft.setCursor((W - lblW) / 2, CONTENT_Y + 6);
    tft.print(emergLabel);
    emergOffset = 32;
  }

  int x = 20;
  int y = CONTENT_Y + emergOffset + 6;

  // 1. PRIMARY IDENTITY
  int csSize = hasEmergency ? 4 : 5;
  tft.setTextSize(csSize);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(x, y);
  tft.print(f.callsign[0] ? f.callsign : "SEARCHING");

  y += csSize * 8;
  if (!hasEmergency) {
    const Airline* al = getAirline(f.callsign);
    tft.setTextSize(2);
    tft.setTextColor(al ? al->color : C_DIM, C_BG);
    tft.setCursor(x, y);
    tft.print(al ? al->name : "UNKNOWN AIRLINE");
    y += 24;
  } else {
    y += 10;
  }

  // 2. AIRCRAFT TYPE & REG
  tft.drawFastHLine(14, y, W - 28, C_DIMMER);
  y += 10;
  tft.setTextSize(1);
  const char* acCat = getAircraftCategory(f.type);
  tft.setTextColor(acCat ? C_AMBER : C_DIM, C_BG);
  tft.setCursor(x, y);
  tft.print(acCat ? acCat : "AIRCRAFT TYPE");
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(W/2 + 20, y);
  tft.print("REGISTRATION");
  y += 12;
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN, C_BG);
  tft.setCursor(x, y);
  tft.print(getAircraftTypeName(f.type));
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(W/2 + 20, y);
  tft.print(f.reg[0] ? f.reg : "---");

  // 3. ROUTE SECTION
  y += 24;
  tft.drawFastHLine(14, y, W - 28, C_DIMMER);
  y += 10;
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(x, y);
  tft.print("ROUTE");
  y += 12;
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
  const int DASH_H = 100;
  int dashY = H - FOOT_H - DASH_H;
  tft.drawFastHLine(0, dashY, W, C_DIM);

  // Phase Block
  uint16_t sCol = statusColor(f.status);
  tft.fillRect(0, dashY + 1, 5, DASH_H - 1, sCol);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(10, dashY + 6);
  tft.print("PHASE");
  tft.setTextSize(2);
  tft.setTextColor(sCol, C_BG);
  tft.setCursor(14, dashY + 28);
  tft.print(statusLabel(f.status));

  // Squawk
  bool emerg = strcmp(f.squawk,"7700")==0 || strcmp(f.squawk,"7600")==0 || strcmp(f.squawk,"7500")==0;
  const char* sqLabel = strcmp(f.squawk,"7700")==0 ? "MAYDAY" : strcmp(f.squawk,"7600")==0 ? "NORDO" : strcmp(f.squawk,"7500")==0 ? "HIJACK" : f.squawk;
  tft.setTextColor(emerg ? C_RED : C_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(14, dashY + 52);
  tft.print("SQK ");
  tft.print(sqLabel);

  // Altitude Block
  char altBuf[20];
  formatAlt(f.alt, altBuf, sizeof(altBuf));
  tft.drawFastVLine(COL_W, dashY + 6, DASH_H - 12, C_DIMMER);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(COL_W + 10, dashY + 6);
  tft.print("ALT");
  tft.setTextSize(2);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(COL_W + 14, dashY + 28);
  tft.print(altBuf);
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
    tft.setCursor(COL_W + 14, dashY + 52);
    tft.print(vsBuf);
  } else {
    tft.setTextColor(C_AMBER, C_BG);
    tft.setCursor(COL_W + 14, dashY + 52);
    tft.print("LEVEL");
  }

  // Speed Block
  tft.drawFastVLine(COL_W * 2, dashY + 6, DASH_H - 12, C_DIMMER);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(COL_W * 2 + 10, dashY + 6);
  tft.print("SPD");
  tft.setTextSize(2);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor(COL_W * 2 + 14, dashY + 28);
  if (f.speed > 0) {
    char spdBuf[16];
    snprintf(spdBuf, sizeof(spdBuf), "%d", f.speed);
    tft.print(spdBuf);
    tft.setTextSize(1);
    tft.print(" KT");
  } else {
    tft.print("---");
  }

  // Distance Block
  tft.drawFastVLine(COL_W * 3, dashY + 6, DASH_H - 12, C_DIMMER);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(COL_W * 3 + 10, dashY + 6);
  tft.print("DIST");
  tft.setTextColor(distanceColor(f.dist, GEOFENCE_KM), C_BG);
  tft.setTextSize(2);
  tft.setCursor(COL_W * 3 + 14, dashY + 28);
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

  int cy = CONTENT_Y + 16;
  char timeBuf[8];
  if (ntpOk) snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t->tm_hour, t->tm_min);
  else       strlcpy(timeBuf, "--:--", sizeof(timeBuf));

  tft.setTextSize(8);
  tft.setTextColor(C_AMBER, C_BG);
  tft.setCursor((W - 5*48) / 2, cy);
  tft.print(timeBuf);
  cy += 68;

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
  cy += 26;

  tft.drawFastHLine(14, cy, W - 28, C_DIMMER);
  cy += 10;

  if (!wxReady) {
    if (!fresh) tft.fillRect(0, cy, W, CONTENT_H - (cy - CONTENT_Y), C_BG);
    tft.setTextSize(1);
    tft.setTextColor(C_DIMMER, C_BG);
    tft.setCursor(20, cy);
    tft.print("WEATHER LOADING...");
    drawStatusBar();
    return;
  }

  char buf[32];
  int lx = 15;
  int rx = W/2 + 15;

  // Row 1: Temperature | Feels Like
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(lx, cy);      tft.print("TEMPERATURE");
  tft.setCursor(rx, cy);      tft.print("FEELS LIKE");
  cy += 12;
  if (!fresh) {
    tft.fillRect(lx, cy, W/2 - 20, 16, C_BG);
    tft.fillRect(rx, cy, W/2 - 20, 16, C_BG);
  }
  tft.setTextSize(2);
  tft.setTextColor(C_AMBER, C_BG);
  snprintf(buf, sizeof(buf), "%.1f C", wxData.temp);
  tft.setCursor(lx, cy); tft.print(buf);
  snprintf(buf, sizeof(buf), "%.1f C", wxData.feels_like);
  tft.setCursor(rx, cy); tft.print(buf);
  cy += 24;

  tft.drawFastHLine(14, cy, W - 28, C_DIMMER); cy += 8;

  // Row 2: Condition
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(lx, cy); tft.print("CONDITIONS");
  cy += 12;
  if (!fresh) tft.fillRect(lx, cy, W - 30, 16, C_BG);
  tft.setTextSize(2);
  tft.setTextColor(C_YELLOW, C_BG);
  tft.setCursor(lx, cy); tft.print(wxData.condition);
  cy += 24;

  tft.drawFastHLine(14, cy, W - 28, C_DIMMER); cy += 8;

  // Row 3: Humidity | Wind
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(lx, cy);      tft.print("HUMIDITY");
  tft.setCursor(rx, cy);      tft.print("WIND");
  cy += 12;
  if (!fresh) {
    tft.fillRect(lx, cy, W/2 - 20, 16, C_BG);
    tft.fillRect(rx, cy, W/2 - 20, 16, C_BG);
  }
  tft.setTextSize(2);
  tft.setTextColor(C_AMBER, C_BG);
  snprintf(buf, sizeof(buf), "%d%%", wxData.humidity);
  tft.setCursor(lx, cy); tft.print(buf);
  snprintf(buf, sizeof(buf), "%.0f %s", wxData.wind_speed, wxData.wind_cardinal);
  tft.setCursor(rx, cy); tft.print(buf);
  cy += 24;

  tft.drawFastHLine(14, cy, W - 28, C_DIMMER); cy += 8;

  // Row 4: UV Index | Tide
  tft.setTextSize(1);
  tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(lx, cy); tft.print("UV INDEX");
  if (wxData.tide_time[0]) { tft.setCursor(rx, cy); tft.print("TIDE"); }
  cy += 12;
  if (!fresh) {
    tft.fillRect(lx, cy, W/2 - 20, 16, C_BG);
    tft.fillRect(rx, cy, W/2 - 20, 16, C_BG);
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
    int tideH = (wxData.tide_time[0] - '0') * 10 + (wxData.tide_time[1] - '0');
    int tideM = (wxData.tide_time[3] - '0') * 10 + (wxData.tide_time[4] - '0');
    int tideMins = tideH * 60 + tideM;
    int nowMins  = t->tm_hour * 60 + t->tm_min;
    int diff     = tideMins - nowMins;
    if (diff < 0) diff += 24 * 60;
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
static int bootLineY = 80;
static const int BOOT_DOT_END  = 340;
static const int BOOT_RESULT_X = 344;

static void bootLine(const char* label, const char* result, uint16_t col, int pauseMs) {
  tft.setTextColor(C_DIMMER, C_BG);
  tft.setTextSize(1);
  tft.setCursor(14, bootLineY);
  tft.print(label);
  int dotX = 14 + strlen(label) * 6;
  while (dotX < BOOT_DOT_END) { tft.setCursor(dotX, bootLineY); tft.print("."); dotX += 6; }
  delay(pauseMs);
  tft.setTextColor(col, C_BG);
  tft.setCursor(BOOT_RESULT_X, bootLineY);
  tft.print(result);
  bootLineY += 18;
  delay(10);
}

void bootSequence() {
  tft.fillScreen(C_BG);
  bootLineY = 80;
  for (int y = 0; y < H; y += 2) {
    tft.drawFastHLine(0, y, W, C_DIMMER);
    delayMicroseconds(200);
  }
  delay(30);
  tft.fillScreen(C_BG);
  delay(20);

  tft.setTextColor(C_AMBER, C_BG);
  tft.setTextSize(2);
  tft.setCursor(14, 16);
  tft.print("OVERHEAD TRACKER");
  tft.setTextColor(C_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(14, 44);
  tft.print("ADS-B AIRSPACE SURVEILLANCE  REV 4.0  FOXTROT");
  tft.drawFastHLine(0, 62, W, C_DIM);
  delay(100);

  char buf[48];
  snprintf(buf, sizeof(buf), "240 MHz  DUAL CORE  ESP32-S3");
  bootLine("CPU",            buf,                    C_GREEN,  30);
  snprintf(buf, sizeof(buf), "%d KB FREE", ESP.getFreeHeap() / 1024);
  bootLine("HEAP MEMORY",    buf,                    C_GREEN,  35);
  snprintf(buf, sizeof(buf), "%d KB",      ESP.getFlashChipSize() / 1024);
  bootLine("FLASH SIZE",     buf,                    C_AMBER,  25);
  snprintf(buf, sizeof(buf), "%d KB PSRAM", ESP.getPsramSize() / 1024);
  bootLine("EXT MEMORY",     buf,                    C_GREEN,  25);
  snprintf(buf, sizeof(buf), "%s", ESP.getSdkVersion());
  bootLine("ESP-IDF SDK",    buf,                    C_DIM,    20);
  bootLine("RGB BUS",        "16-BIT PARALLEL  OK",  C_GREEN,  25);
  bootLine("DISPLAY",        "ST7262 800x480 IPS",   C_GREEN,  30);
  bootLine("TOUCH",          "GT911 CAPACITIVE",     C_GREEN,  25);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  bootLine("WIFI MAC",       buf,                    C_AMBER,  30);
  bootLine("WIFI MODE",      "STA  802.11 B/G/N",    C_AMBER,  20);
  bootLine("SD CARD",        "SEARCHING...",          C_YELLOW, 80);
  snprintf(buf, sizeof(buf), "%s:%d", PROXY_HOST, PROXY_PORT);
  bootLine("PROXY TARGET",   buf,                    C_AMBER,  25);
  snprintf(buf, sizeof(buf), "%.4f, %.4f", HOME_LAT, HOME_LON);
  bootLine("HOME COORDS",    buf,                    C_AMBER,  25);
  snprintf(buf, sizeof(buf), "%.0f KM RADIUS", GEOFENCE_KM);
  bootLine("GEOFENCE",       buf,                    C_AMBER,  20);
  bootLine("ADS-B PIPELINE", "DECODER READY",        C_GREEN,  35);
  snprintf(buf, sizeof(buf), "%d SEC AUTO-REFRESH", REFRESH_SECS);
  bootLine("POLL INTERVAL",  buf,                    C_GREEN,  25);
  tft.drawFastHLine(0, bootLineY + 6, W, C_DIM);
  int flashY = bootLineY + 12;

  tft.fillRect(10, flashY, W - 20, 30, C_GREEN);
  tft.setTextColor(C_BG, C_GREEN);
  tft.setTextSize(2);
  int textX = (W - 13*12) / 2;
  tft.setCursor(textX, flashY + 5);
  tft.print("SYSTEM ONLINE");
  delay(120);
  tft.fillRect(10, flashY, W - 20, 30, C_BG);
  delay(80);
  tft.fillRect(10, flashY, W - 20, 30, C_GREEN);
  tft.setTextColor(C_BG, C_GREEN);
  tft.setTextSize(2);
  tft.setCursor(textX, flashY + 5);
  tft.print("SYSTEM ONLINE");
  delay(400);
}

void drawOtaProgress(int pct) {
  static bool first = true;
  if (first) {
    first = false;
    tft.fillScreen(C_BG);
    tft.setTextColor(C_AMBER, C_BG);
    tft.setTextSize(3);
    tft.setCursor(240, 150);
    tft.print("OTA UPDATE");
    tft.setTextSize(2);
    tft.setTextColor(C_DIM, C_BG);
    tft.setCursor(200, 210);
    tft.print("Do not power off");
  }
  const int BX = 60, BY = 300, BW = 680, BH = 28;
  tft.drawRect(BX, BY, BW, BH, C_AMBER);
  tft.fillRect(BX + 1, BY + 1, (BW - 2) * pct / 100, BH - 2, C_GREEN);
}
