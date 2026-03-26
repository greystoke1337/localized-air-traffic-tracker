// ─── Display rendering: immediate-mode LovyanGFX (800x480) ─────────────────

// ─── CFG button confirmation state ──────────────────
static bool          cfgConfirming    = false;
static unsigned long cfgConfirmStart  = 0;

// ─── Drawing helpers ─────────────────────────────────

static void dlbl(int x, int y, float sz, uint16_t col, const char* txt) {
  tft.setFont(GLCD_FONT);
  tft.setTextSize(sz);
  tft.setTextColor(col);
  tft.setTextDatum(lgfx::top_left);
  tft.drawString(txt, x, y);
}

static void dlbl_r(int x, int y, float sz, uint16_t col, const char* txt) {
  tft.setFont(GLCD_FONT);
  tft.setTextSize(sz);
  tft.setTextColor(col);
  tft.setTextDatum(lgfx::top_right);
  tft.drawString(txt, x, y);
  tft.setTextDatum(lgfx::top_left);
}

// Clipped left-aligned draw (text truncated to w×h box)
static void dlbl_clip(int x, int y, int w, int h, float sz, uint16_t col, const char* txt) {
  tft.setClipRect(x, y, w, h);
  tft.setFont(GLCD_FONT);
  tft.setTextSize(sz);
  tft.setTextColor(col);
  tft.setTextDatum(lgfx::top_left);
  tft.drawString(txt, x, y);
  tft.clearClipRect();
}

// Clip-based atomic fill+draw (left-aligned)
static void dlbl_fill(int x, int y, int w, int h, float sz, uint16_t col, uint16_t bg, const char* txt) {
  tft.setClipRect(x, y, w, h);
  tft.fillRect(x, y, w, h, bg);
  tft.setFont(GLCD_FONT);
  tft.setTextSize(sz);
  tft.setTextColor(col);
  tft.setTextDatum(lgfx::top_left);
  tft.drawString(txt, x, y);
  tft.clearClipRect();
}

// Clip-based atomic fill+draw (right-aligned)
static void dlbl_fill_r(int x, int y, int w, int h, float sz, uint16_t col, uint16_t bg, const char* txt) {
  tft.setClipRect(x, y, w, h);
  tft.fillRect(x, y, w, h, bg);
  tft.setFont(GLCD_FONT);
  tft.setTextSize(sz);
  tft.setTextColor(col);
  tft.setTextDatum(lgfx::top_right);
  tft.drawString(txt, x + w, y);
  tft.setTextDatum(lgfx::top_left);
  tft.clearClipRect();
}

// Clip-wrapped button
static void drawBtn(int x, int y, int w, int h, uint16_t bg, float sz, uint16_t txtCol, const char* txt) {
  tft.setClipRect(x, y, w, h);
  tft.fillRect(x, y, w, h, bg);
  tft.setFont(GLCD_FONT);
  tft.setTextSize(sz);
  tft.setTextColor(txtCol, bg);
  tft.setTextDatum(lgfx::middle_center);
  tft.drawString(txt, x + w / 2, y + h / 2);
  tft.setTextDatum(lgfx::top_left);
  tft.clearClipRect();
}

// ─── drawHeader ─────────────────────────────────────
void drawHeader() {
  tft.setClipRect(0, 0, W, HDR_H);
  tft.fillRect(0, 0, W, HDR_H, C_AMBER);
  // Title takes left half, location takes right half — clipped to prevent overlap
  int titleW = 7 * (int)(6 * SZ_SM) + 10;  // "SPOTTER" width + margin
  dlbl(10, 14, SZ_SM, C_BG, "SPOTTER");
  dlbl_clip(titleW + 10, 14, W - titleW - 20, (int)(8 * SZ_SM), SZ_SM, C_BG, "");
  // Right-align location, clipped to remaining space
  tft.setClipRect(titleW, 0, W - titleW, HDR_H);
  dlbl_r(W - 10, 14, SZ_SM, C_BG, LOCATION_NAME);
  tft.clearClipRect();
}

// ─── drawNavBar ─────────────────────────────────────
void drawNavBar() {
  tft.setClipRect(0, NAV_Y, W, NAV_H);
  tft.fillRect(0, NAV_Y, W, NAV_H, C_BG);
  tft.drawFastHLine(0, NAV_Y, W, C_DIMMER);

  if (currentScreen == SCREEN_FLIGHT && flightCount > 1) {
    char buf[16];
    snprintf(buf, sizeof(buf), "< %d/%d >", flightIndex + 1, flightCount);
    dlbl(10, NAV_Y + 16, SZ_SM, C_DIM, buf);
  }

  uint16_t wxBg  = (currentScreen == SCREEN_WEATHER) ? C_CYAN   : C_DIMMER;
  uint16_t wxTxt = (currentScreen == SCREEN_WEATHER) ? C_BG     : C_AMBER;
  tft.clearClipRect();

  drawBtn(WX_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, wxBg, SZ_SM, wxTxt, "WX");

  const char* geoLabels[] = {"5mi", "10mi", "20mi"};
  drawBtn(GEO_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, C_DIMMER, SZ_SM, C_AMBER, geoLabels[geoIndex]);

  if (!cfgConfirming) {
    uint16_t cfgBg = isFetching ? C_RED : C_DIMMER;
    drawBtn(CFG_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, cfgBg, SZ_SM, C_AMBER, isFetching ? "..." : "CFG");
  } else {
    drawBtn(CFG_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, C_RED, SZ_SM, C_AMBER, "TAP!");
  }
}

// ─── drawStatusBar ──────────────────────────────────
void drawStatusBar() {
  tft.setClipRect(0, H - FOOT_H, W, FOOT_H);
  tft.fillRect(0, H - FOOT_H, W, FOOT_H, C_BG);
  tft.drawFastHLine(0, H - FOOT_H, W, C_DIMMER);

  char buf[80];
  const char* src = dataSource == 2 ? "CACHE" : dataSource == 1 ? "DIRECT" : "PROXY";
  if (flightCount == 0) {
    snprintf(buf, sizeof(buf), "  CLEAR SKIES   SRC:%s   NEXT:%ds", src, countdown);
  } else {
    snprintf(buf, sizeof(buf), "  AC %d/%d   SRC:%s   NEXT:%ds",
             flightIndex + 1, flightCount, src, countdown);
  }
  dlbl(8, H - FOOT_H + 8, SZ_XS, C_DIM, buf);

  // WiFi indicator (right side of status bar)
  if (wifiOk()) {
    char wifiBuf[16];
    snprintf(wifiBuf, sizeof(wifiBuf), "WiFi %d", WiFi.RSSI());
    dlbl_r(W - 10, H - FOOT_H + 8, SZ_XS, C_DIM, wifiBuf);
  } else {
    dlbl_r(W - 10, H - FOOT_H + 8, SZ_XS, C_RED, "NO WIFI");
  }
  tft.clearClipRect();
}

// ─── initUI ─────────────────────────────────────────
void initUI() {
  tft.fillScreen(C_BG);
  drawHeader();
  drawNavBar();
  drawStatusBar();
}

// ─── renderMessage ──────────────────────────────────
void renderMessage(const char* line1, const char* line2) {
  currentScreen  = SCREEN_NONE;
  previousScreen = SCREEN_NONE;
  tft.fillRect(0, CONTENT_Y, W, CONTENT_H, C_BG);
  drawHeader();
  drawNavBar();
  int my = CONTENT_Y + CONTENT_H / 2 - 22;
  dlbl(20, my, SZ_SM, C_AMBER, line1);
  if (line2 && line2[0]) dlbl(20, my + 34, SZ_XS, C_DIM, line2);
  drawStatusBar();
}

// ─── renderFlight (full redraw every call) ──────────
void renderFlight(const Flight& f) {
  currentScreen = SCREEN_FLIGHT;
  if (previousScreen != SCREEN_FLIGHT) drawHeader();
  previousScreen = SCREEN_FLIGHT;
  drawNavBar();

  const int CY = CONTENT_Y;

  // Clear content area
  tft.fillRect(0, CY, W, CONTENT_H, C_BG);

  // Emergency state
  bool hasEmergency = strcmp(f.squawk, "7700") == 0 ||
                      strcmp(f.squawk, "7600") == 0 ||
                      strcmp(f.squawk, "7500") == 0;
  int yOff = hasEmergency ? 44 : 0;

  // ── Emergency banner ──
  if (hasEmergency) {
    const char* emergLabel = strcmp(f.squawk, "7700") == 0 ? "EMERGENCY - MAYDAY" :
                             strcmp(f.squawk, "7600") == 0 ? "EMERGENCY - NORDO"  :
                                                             "EMERGENCY - HIJACK";
    tft.fillRect(0, CY, W, 44, C_RED);
    tft.setFont(GLCD_FONT);
    tft.setTextSize(SZ_SM);
    tft.setTextColor(C_BG, C_RED);
    tft.setTextDatum(lgfx::middle_center);
    tft.drawString(emergLabel, W / 2, CY + 22);
    tft.setTextDatum(lgfx::top_left);
  }

  // ── Callsign ──
  dlbl(20, CY + yOff + 4, SZ_XXL, C_AMBER, f.callsign[0] ? f.callsign : "SEARCHING");

  // ── Airline ──
  int alY = CY + yOff + 72;
  if (!hasEmergency) {
    const Airline* al = getAirline(f.callsign);
    dlbl(20, alY, SZ_SM, al ? al->color : C_DIM, al ? al->name : "UNKNOWN AIRLINE");
  }

  // ── Divider + Type/Reg ──
  int divY = alY + (hasEmergency ? 6 : 34);
  tft.drawFastHLine(14, divY, W - 28, C_DIMMER);
  const char* acCat = getAircraftCategory(f.type);
  dlbl(20, divY + 6, SZ_XS, acCat ? C_AMBER : C_DIM, acCat ? acCat : "AIRCRAFT TYPE");
  // Clip aircraft type to left half
  dlbl_clip(20, divY + 28, W / 2 - 30, (int)(8 * SZ_SM), SZ_SM, C_CYAN, getAircraftTypeName(f.type));
  dlbl(W / 2 + 20, divY + 6, SZ_XS, C_DIM, "REGISTRATION");
  dlbl(W / 2 + 20, divY + 28, SZ_SM, C_AMBER, f.reg[0] ? f.reg : "---");

  // ── Route ──
  int routeDivY = divY + 56;
  int dashY     = CY + CONTENT_H - 90;

  // Only show route if it fits above dashboard
  int routeBottomMax = dashY - 4;
  if (routeDivY + 13 < routeBottomMax) {
    tft.drawFastHLine(14, routeDivY, W - 28, C_DIMMER);
    dlbl(20, routeDivY + 13, SZ_XS, C_DIM, "ROUTE");
    if (routeDivY + 36 + (int)(8 * SZ_RT) < routeBottomMax) {
      // Clip route text to screen width
      if (f.route[0])
        dlbl_clip(20, routeDivY + 36, W - 40, (int)(8 * SZ_RT), SZ_RT, C_YELLOW, f.route);
      else
        dlbl(20, routeDivY + 36, SZ_RT, C_DIMMER, "NO ROUTE DATA");
    }
  }

  // ── Dashboard ──
  int COL_W = W / 4;
  int lblY  = dashY + 12;   // all column labels at same Y
  int valY  = dashY + 30;   // all column values at same Y
  int subY  = dashY + 56;   // all sub-rows at same Y

  tft.drawFastHLine(0, dashY, W, C_DIM);

  uint16_t sCol = statusColor(f.status);
  tft.fillRect(0, dashY + 1, 5, 89, sCol);

  dlbl(10, lblY, SZ_XS, C_DIM, "PHASE");
  dlbl(14, valY, SZ_SM, sCol, statusLabel(f.status));

  const char* sqLabel = strcmp(f.squawk,"7700")==0 ? "MAYDAY" :
                        strcmp(f.squawk,"7600")==0 ? "NORDO"  :
                        strcmp(f.squawk,"7500")==0 ? "HIJACK" : f.squawk;
  char sqBuf[24];
  snprintf(sqBuf, sizeof(sqBuf), "SQK %s", sqLabel);
  dlbl(14, subY, SZ_XS, hasEmergency ? C_RED : C_DIM, sqBuf);

  tft.fillRect(COL_W, dashY + 4, 1, 78, C_DIMMER);
  dlbl(COL_W + 10, lblY, SZ_XS, C_DIM, "ALT");

  char altBuf[20];
  formatAlt(f.alt, altBuf, sizeof(altBuf));
  dlbl(COL_W + 10, valY, SZ_SM, C_AMBER, altBuf);

  if (abs(f.vs) >= 50) {
    char vsBuf[24];
    if (f.vs > 0) snprintf(vsBuf, sizeof(vsBuf), "+%d FPM", f.vs);
    else          snprintf(vsBuf, sizeof(vsBuf), "%d FPM",  f.vs);
    dlbl(COL_W + 10, subY, SZ_XS, f.vs > 0 ? C_GREEN : C_RED, vsBuf);
  } else {
    dlbl(COL_W + 10, subY, SZ_XS, C_AMBER, "LEVEL");
  }

  tft.fillRect(COL_W * 2, dashY + 4, 1, 78, C_DIMMER);
  dlbl(COL_W * 2 + 10, lblY, SZ_XS, C_DIM, "SPD");
  if (f.speed > 0) {
    char spdNum[10];
    snprintf(spdNum, sizeof(spdNum), "%d", f.speed);
    int sx = COL_W * 2 + 10;
    dlbl(sx, valY, SZ_DASH, C_AMBER, spdNum);
    int numW = strlen(spdNum) * (int)(6 * SZ_DASH) + 4;
    dlbl(sx + numW, valY + 10, SZ_XS, C_DIM, "KT");
  } else {
    dlbl(COL_W * 2 + 10, valY, SZ_DASH, C_AMBER, "---");
  }

  tft.fillRect(COL_W * 3, dashY + 4, 1, 78, C_DIMMER);
  dlbl(COL_W * 3 + 10, lblY, SZ_XS, C_DIM, "DIST");
  if (f.dist > 0) {
    uint16_t dCol = distanceColor(f.dist, GEOFENCE_MI);
    char distNum[10];
    snprintf(distNum, sizeof(distNum), "%.1f", f.dist);
    int dx = COL_W * 3 + 10;
    dlbl(dx, valY, SZ_DASH, dCol, distNum);
    int numW = strlen(distNum) * (int)(6 * SZ_DASH) + 4;
    dlbl(dx + numW, valY + 10, SZ_XS, C_DIM, "MI");
  } else {
    dlbl(COL_W * 3 + 10, valY, SZ_DASH, C_AMBER, "---");
  }

  drawStatusBar();
}

// ─── renderWeather ──────────────────────────────────
void renderWeather() {
  currentScreen = SCREEN_WEATHER;
  tft.fillRect(0, CONTENT_Y, W, CONTENT_H, C_BG);
  if (previousScreen != SCREEN_WEATHER) drawHeader();
  previousScreen = SCREEN_WEATHER;
  drawNavBar();

  const int CY = CONTENT_Y;

  time_t utcNow = time(NULL);
  bool   ntpOk  = utcNow > 1000000000UL;
  time_t localNow = (ntpOk && wxReady && wxData.utc_offset_secs != 0)
                    ? utcNow + wxData.utc_offset_secs : utcNow;
  struct tm* t = gmtime(&localNow);

  char timeBuf[8];
  if (ntpOk) snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t->tm_hour, t->tm_min);
  else       strlcpy(timeBuf, "--:--", sizeof(timeBuf));
  tft.setFont(GLCD_FONT);
  tft.setTextSize(SZ_3XL);
  tft.setTextColor(C_AMBER);
  tft.setTextDatum(lgfx::top_center);
  tft.drawString(timeBuf, W / 2, CY + 6);
  tft.setTextDatum(lgfx::top_left);

  if (ntpOk) {
    const char* dayNames[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    const char* monNames[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
    char dateBuf[20];
    snprintf(dateBuf, sizeof(dateBuf), "%s %d %s", dayNames[t->tm_wday], t->tm_mday, monNames[t->tm_mon]);
    tft.setFont(GLCD_FONT);
    tft.setTextSize(SZ_SM);
    tft.setTextColor(C_DIM);
    tft.setTextDatum(lgfx::top_center);
    tft.drawString(dateBuf, W / 2, CY + 82);
    tft.setTextDatum(lgfx::top_left);
  }

  int cy = CY + 112;
  tft.drawFastHLine(14, cy, W - 28, C_DIMMER);
  cy += 8;

  if (!wxReady) {
    dlbl(20, cy + 14, SZ_SM, C_DIMMER, "WEATHER LOADING...");
    drawStatusBar();
    return;
  }

  int lx = 15, rx = W / 2 + 15;
  int halfW = W / 2 - 30;  // max width per column for clipping

  char buf[32];

  dlbl(lx, cy, SZ_XS, C_DIM, "TEMPERATURE");
  dlbl(rx, cy, SZ_XS, C_DIM, "REAL FEEL");
  cy += 20;
  snprintf(buf, sizeof(buf), "%.0f F", wxData.temp * 9.0f / 5.0f + 32.0f);
  dlbl(lx, cy, SZ_SM, C_AMBER, buf);
  snprintf(buf, sizeof(buf), "%.0f F", wxData.feels_like * 9.0f / 5.0f + 32.0f);
  dlbl(rx, cy, SZ_SM, C_CYAN, buf);
  cy += 26;
  tft.drawFastHLine(14, cy, W - 28, C_DIMMER);
  cy += 4;

  dlbl(lx, cy, SZ_XS, C_DIM, "CONDITIONS");
  dlbl(rx, cy, SZ_XS, C_DIM, "PRECIPITATION");
  cy += 20;
  // Clip conditions to left half to prevent bleed into precipitation
  dlbl_clip(lx, cy, halfW, (int)(8 * SZ_SM), SZ_SM, C_YELLOW, wxData.condition);
  uint16_t precipCol = wxData.precipitation_mm < 0.1f ? C_GREEN :
                       wxData.precipitation_mm < 2.5f ? C_YELLOW :
                       wxData.precipitation_mm < 7.5f ? C_AMBER  : C_RED;
  snprintf(buf, sizeof(buf), "%.2f IN", wxData.precipitation_mm * 0.0393701f);
  dlbl(rx, cy, SZ_SM, precipCol, buf);
  cy += 26;
  tft.drawFastHLine(14, cy, W - 28, C_DIMMER);
  cy += 4;

  dlbl(lx, cy, SZ_XS, C_DIM, "HUMIDITY");
  dlbl(rx, cy, SZ_XS, C_DIM, "WIND");
  cy += 20;
  snprintf(buf, sizeof(buf), "%d%%", wxData.humidity);
  dlbl(lx, cy, SZ_SM, C_AMBER, buf);
  snprintf(buf, sizeof(buf), "%.0f MPH %s", wxData.wind_speed * 0.621371f, wxData.wind_cardinal);
  dlbl(rx, cy, SZ_SM, C_AMBER, buf);
  cy += 26;
  tft.drawFastHLine(14, cy, W - 28, C_DIMMER);
  cy += 4;

  dlbl(lx, cy, SZ_XS, C_DIM, "UV INDEX");
  dlbl(rx, cy, SZ_XS, C_DIM, "VISIBILITY");
  cy += 20;

  uint16_t uvCol = wxData.uv_index < 3.0f ? C_GREEN :
                   wxData.uv_index < 6.0f ? C_YELLOW :
                   wxData.uv_index < 8.0f ? C_AMBER  : C_RED;
  snprintf(buf, sizeof(buf), "%.1f", wxData.uv_index);
  dlbl(lx, cy, SZ_SM, uvCol, buf);

  float visMi = wxData.visibility_km * 0.621371f;
  uint16_t visCol = visMi >= 6.0f ? C_GREEN :
                    visMi >= 3.0f ? C_YELLOW :
                    visMi >= 1.2f ? C_AMBER  : C_RED;
  snprintf(buf, sizeof(buf), "%.0f MI", visMi);
  dlbl(rx, cy, SZ_SM, visCol, buf);
  cy += 26;

  snprintf(buf, sizeof(buf), "RISE %s", wxData.sunrise);
  dlbl(lx, cy, SZ_XS, C_DIM, buf);
  snprintf(buf, sizeof(buf), "SET %s", wxData.sunset);
  dlbl(rx, cy, SZ_XS, C_DIM, buf);

  drawStatusBar();
}

// ─── Boot helpers ───────────────────────────────────

// Typewriter: draws one char at a time with a scramble effect
static void typewriter(const char* txt, int x, int y, float sz, uint16_t col, int charDelayMs) {
  const char scrambleChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%&*";
  int len = strlen(txt);
  int charW = (int)(6 * sz);
  int charH = (int)(8 * sz);

  tft.setFont(GLCD_FONT);
  tft.setTextSize(sz);
  tft.setTextDatum(lgfx::top_left);

  // Calculate starting X for center alignment
  int totalW = len * charW;
  int sx = x - totalW / 2;

  char single[2] = {0, 0};
  for (int i = 0; i < len; i++) {
    if (txt[i] == ' ') {
      delay(charDelayMs / 2);
      continue;
    }
    // 3 scramble frames
    for (int s = 0; s < 3; s++) {
      single[0] = scrambleChars[random(0, sizeof(scrambleChars) - 1)];
      tft.setTextColor(C_DIMMER);
      tft.fillRect(sx + i * charW, y, charW, charH, C_BG);
      tft.drawString(single, sx + i * charW, y);
      delay(charDelayMs / 4);
    }
    // Real character
    single[0] = txt[i];
    tft.setTextColor(col);
    tft.fillRect(sx + i * charW, y, charW, charH, C_BG);
    tft.drawString(single, sx + i * charW, y);
    delay(charDelayMs);
  }
}

// Expanding radar rings from center
static void radarSweep(int cx, int cy, int maxR, uint16_t col) {
  for (int r = 10; r <= maxR; r += 30) {
    tft.drawCircle(cx, cy, r, col);
    delay(25);
  }
  delay(100);
  for (int r = 10; r <= maxR; r += 30) {
    tft.drawCircle(cx, cy, r, C_BG);
  }
}

// ─── Boot sequence ──────────────────────────────────
void bootSequence() {
  tft.fillScreen(C_BG);

  // Scanline wipe from top
  for (int y = 0; y < H; y += 4) {
    tft.drawFastHLine(0, y, W, C_DIMMER);
    delay(1);
  }
  delay(80);
  for (int y = 0; y < H; y += 4) {
    tft.drawFastHLine(0, y, W, C_BG);
  }

  // Radar rings
  radarSweep(W / 2, H / 2 - 20, 160, C_DIMMER);

  // Crosshair
  int cx = W / 2, cy = H / 2 - 20;
  tft.drawFastHLine(cx - 60, cy, 120, C_DIMMER);
  tft.drawFastVLine(cx, cy - 40, 80, C_DIMMER);
  delay(150);
  tft.drawFastHLine(cx - 60, cy, 120, C_BG);
  tft.drawFastVLine(cx, cy - 40, 80, C_BG);

  // Typewriter title
  tft.setFont(GLCD_FONT);
  typewriter("SPOTTER", W / 2, H / 2 - 36, SZ_XL, C_AMBER, 60);

  // Glitch flash: briefly invert then restore
  delay(80);
  tft.invertDisplay(true);
  delay(40);
  tft.invertDisplay(false);

  // Subtitle fade-in (3 distinct brightness steps)
  const uint16_t fadeSteps[] = {C_DIMMER, C_DIM, C_AMBER};
  tft.setFont(GLCD_FONT);
  tft.setTextDatum(lgfx::top_center);
  for (int i = 0; i < 3; i++) {
    tft.setTextSize(SZ_SM);
    tft.setTextColor(fadeSteps[i]);
    tft.drawString("FOXTROT", W / 2, H / 2 + 52);
    delay(80);
  }
  // Settle subtitle to final DIM color
  tft.setTextSize(SZ_SM);
  tft.setTextColor(C_DIM);
  tft.drawString("FOXTROT", W / 2, H / 2 + 52);

  tft.setTextSize(SZ_XS);
  tft.setTextColor(C_DIMMER);
  tft.drawString("v" FW_VERSION, W / 2, H / 2 + 84);
  tft.setTextDatum(lgfx::top_left);

  // Progress bar with segmented fill
  int barX = W / 4, barY = H / 2 + 114, barW = W / 2, barH = 6;
  tft.drawRect(barX, barY, barW, barH, C_DIMMER);
  int segments = 20;
  int segW = (barW - 2) / segments;
  for (int i = 0; i < segments; i++) {
    tft.fillRect(barX + 1 + i * segW, barY + 1, segW - 1, barH - 2, C_AMBER);
    delay(20);
  }

  delay(150);
}

// ─── OTA progress ───────────────────────────────────
void drawOtaProgress(int pct) {
  static bool otaInit = false;
  if (!otaInit) {
    otaInit = true;
    tft.fillScreen(C_BG);
    tft.setFont(GLCD_FONT);
    tft.setTextSize(SZ_MD);
    tft.setTextColor(C_AMBER);
    tft.setTextDatum(lgfx::middle_center);
    tft.drawString("OTA UPDATE", W / 2, 150);
    tft.setFont(GLCD_FONT);
    tft.setTextSize(SZ_SM);
    tft.setTextColor(C_DIM);
    tft.drawString("Do not power off", W / 2, 200);
    tft.setTextDatum(lgfx::top_left);
    tft.drawRect(60, 300, 680, 28, C_DIMMER);
  }
  int barW = (int)(680 * pct / 100);
  tft.fillRect(60, 300, barW, 28, C_GREEN);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  tft.setFont(GLCD_FONT);
  tft.setTextSize(SZ_SM);
  tft.setTextColor(C_GREEN);
  tft.setTextDatum(lgfx::middle_center);
  tft.drawString(buf, W / 2, 346);
  tft.setTextDatum(lgfx::top_left);
}
