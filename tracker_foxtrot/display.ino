// ─── Display rendering: immediate-mode LovyanGFX (800x480) ─────────────────

// ─── Font aliases ────────────────────────────────────
#define FONT_XS  (&lgfx::fonts::DejaVu18)
#define FONT_SM  (&lgfx::fonts::DejaVu24)
#define FONT_MD  (&lgfx::fonts::DejaVu40)
#define FONT_LG  (&lgfx::fonts::DejaVu40)
#define FONT_XL  (&lgfx::fonts::FreeMonoBold24pt7b)
#define FONT_XXL (&lgfx::fonts::DejaVu56)
#define FONT_3XL (&lgfx::fonts::DejaVu72)
#define FONT_MDS (&lgfx::fonts::FreeSansBold18pt7b)

// ─── CFG button confirmation state ──────────────────
static bool          cfgConfirming    = false;
static unsigned long cfgConfirmStart  = 0;

// ─── Drawing helpers ─────────────────────────────────

static void dlbl(int x, int y, const lgfx::IFont* f, uint16_t col, const char* txt) {
  tft.setFont(f);
  tft.setTextColor(col);
  tft.setTextDatum(lgfx::top_left);
  tft.drawString(txt, x, y);
}

static void dlbl_r(int x, int y, const lgfx::IFont* f, uint16_t col, const char* txt) {
  tft.setFont(f);
  tft.setTextColor(col);
  tft.setTextDatum(lgfx::top_right);
  tft.drawString(txt, x, y);
  tft.setTextDatum(lgfx::top_left);
}

// Clip-based atomic fill+draw (left-aligned)
static void dlbl_fill(int x, int y, int w, int h, const lgfx::IFont* f, uint16_t col, uint16_t bg, const char* txt) {
  tft.setClipRect(x, y, w, h);
  tft.fillRect(x, y, w, h, bg);
  tft.setFont(f);
  tft.setTextColor(col);
  tft.setTextDatum(lgfx::top_left);
  tft.drawString(txt, x, y);
  tft.clearClipRect();
}

// Clip-based atomic fill+draw (right-aligned)
static void dlbl_fill_r(int x, int y, int w, int h, const lgfx::IFont* f, uint16_t col, uint16_t bg, const char* txt) {
  tft.setClipRect(x, y, w, h);
  tft.fillRect(x, y, w, h, bg);
  tft.setFont(f);
  tft.setTextColor(col);
  tft.setTextDatum(lgfx::top_right);
  tft.drawString(txt, x + w, y);
  tft.setTextDatum(lgfx::top_left);
  tft.clearClipRect();
}

// Clip-wrapped button
static void drawBtn(int x, int y, int w, int h, uint16_t bg, const lgfx::IFont* f, uint16_t txtCol, const char* txt) {
  tft.setClipRect(x, y, w, h);
  tft.fillRect(x, y, w, h, bg);
  tft.setFont(f);
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
  dlbl(10, 12, FONT_SM, C_BG, "OVERHEAD TRACKER");
  dlbl_r(W - 10, 12, FONT_SM, C_BG, LOCATION_NAME);
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
    dlbl(10, NAV_Y + 15, FONT_SM, C_DIM, buf);
  }

  uint16_t wxBg  = (currentScreen == SCREEN_WEATHER) ? C_CYAN   : C_DIMMER;
  uint16_t wxTxt = (currentScreen == SCREEN_WEATHER) ? C_BG     : C_AMBER;
  tft.clearClipRect();

  drawBtn(WX_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, wxBg, FONT_SM, wxTxt, "WX");

  const char* geoLabels[] = {"5mi", "10mi", "20mi"};
  drawBtn(GEO_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, C_DIMMER, FONT_SM, C_AMBER, geoLabels[geoIndex]);

  if (!cfgConfirming) {
    uint16_t cfgBg = isFetching ? C_RED : C_DIMMER;
    drawBtn(CFG_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, cfgBg, FONT_SM, C_AMBER, isFetching ? "..." : "CFG");
  } else {
    drawBtn(CFG_BTN_X1, NAV_Y + 2, NAV_BTN_W, NAV_BTN_H, C_RED, FONT_SM, C_AMBER, "TAP!");
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
  dlbl(8, H - FOOT_H + 6, FONT_XS, C_DIM, buf);
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
  dlbl(20, my, FONT_SM, C_AMBER, line1);
  if (line2 && line2[0]) dlbl(20, my + 34, FONT_XS, C_DIM, line2);
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
    tft.setFont(FONT_SM);
    tft.setTextColor(C_BG, C_RED);
    tft.setTextDatum(lgfx::middle_center);
    tft.drawString(emergLabel, W / 2, CY + 22);
    tft.setTextDatum(lgfx::top_left);
  }

  // ── Callsign ──
  dlbl(20, CY + yOff + 4, FONT_XXL, C_AMBER, f.callsign[0] ? f.callsign : "SEARCHING");

  // ── Airline ──
  int alY = CY + yOff + 72;
  if (!hasEmergency) {
    const Airline* al = getAirline(f.callsign);
    dlbl(20, alY, FONT_SM, al ? al->color : C_DIM, al ? al->name : "UNKNOWN AIRLINE");
  }

  // ── Divider + Type/Reg ──
  int divY = alY + (hasEmergency ? 6 : 34);
  tft.drawFastHLine(14, divY, W - 28, C_DIMMER);
  const char* acCat = getAircraftCategory(f.type);
  dlbl(20, divY + 6, FONT_XS, acCat ? C_AMBER : C_DIM, acCat ? acCat : "AIRCRAFT TYPE");
  dlbl(20, divY + 28, FONT_SM, C_CYAN, getAircraftTypeName(f.type));
  dlbl(W / 2 + 20, divY + 6, FONT_XS, C_DIM, "REGISTRATION");
  dlbl(W / 2 + 20, divY + 28, FONT_SM, C_AMBER, f.reg[0] ? f.reg : "---");

  // ── Route ──
  int routeDivY = divY + 56;
  int dashY     = CY + CONTENT_H - 90;
  tft.drawFastHLine(14, routeDivY, W - 28, C_DIMMER);
  dlbl(20, routeDivY + 13, FONT_XS, C_DIM, "ROUTE");
  if (f.route[0])
    dlbl(20, routeDivY + 36, FONT_MD, C_YELLOW, f.route);
  else
    dlbl(20, routeDivY + 36, FONT_MD, C_DIMMER, "NO ROUTE DATA");

  // ── Dashboard ──
  int COL_W = W / 4;
  tft.drawFastHLine(0, dashY, W, C_DIM);

  uint16_t sCol = statusColor(f.status);
  tft.fillRect(0, dashY + 1, 5, 89, sCol);

  dlbl(10, dashY + 12, FONT_XS, C_DIM, "PHASE");
  dlbl(14, dashY + 30, FONT_SM, sCol, statusLabel(f.status));

  const char* sqLabel = strcmp(f.squawk,"7700")==0 ? "MAYDAY" :
                        strcmp(f.squawk,"7600")==0 ? "NORDO"  :
                        strcmp(f.squawk,"7500")==0 ? "HIJACK" : f.squawk;
  char sqBuf[24];
  snprintf(sqBuf, sizeof(sqBuf), "SQK %s", sqLabel);
  dlbl(14, dashY + 56, FONT_XS, hasEmergency ? C_RED : C_DIM, sqBuf);

  tft.fillRect(COL_W, dashY + 4, 1, 78, C_DIMMER);
  dlbl(COL_W + 10, dashY + 13, FONT_XS, C_DIM, "ALT");

  char altBuf[20];
  formatAlt(f.alt, altBuf, sizeof(altBuf));
  dlbl(COL_W + 10, dashY + 29, FONT_SM, C_AMBER, altBuf);

  if (abs(f.vs) >= 50) {
    char vsBuf[24];
    if (f.vs > 0) snprintf(vsBuf, sizeof(vsBuf), "+%d FPM", f.vs);
    else          snprintf(vsBuf, sizeof(vsBuf), "%d FPM",  f.vs);
    dlbl(COL_W + 10, dashY + 55, FONT_XS, f.vs > 0 ? C_GREEN : C_RED, vsBuf);
  } else {
    dlbl(COL_W + 10, dashY + 55, FONT_XS, C_AMBER, "LEVEL");
  }

  tft.fillRect(COL_W * 2, dashY + 4, 1, 78, C_DIMMER);
  dlbl(COL_W * 2 + 10, dashY + 14, FONT_XS, C_DIM, "SPD");
  if (f.speed > 0) {
    char spdBuf[16];
    snprintf(spdBuf, sizeof(spdBuf), "%d KT", f.speed);
    dlbl(COL_W * 2 + 10, dashY + 32, FONT_MD, C_AMBER, spdBuf);
  } else {
    dlbl(COL_W * 2 + 10, dashY + 32, FONT_MD, C_AMBER, "---");
  }

  tft.fillRect(COL_W * 3, dashY + 4, 1, 78, C_DIMMER);
  dlbl(COL_W * 3 + 10, dashY + 14, FONT_XS, C_DIM, "DIST");
  if (f.dist > 0) {
    uint16_t dCol = distanceColor(f.dist, GEOFENCE_MI);
    char distBuf[16];
    snprintf(distBuf, sizeof(distBuf), "%.1f MI", f.dist);
    dlbl(COL_W * 3 + 10, dashY + 32, FONT_MD, dCol, distBuf);
  } else {
    dlbl(COL_W * 3 + 10, dashY + 32, FONT_MD, C_AMBER, "---");
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
  tft.setFont(FONT_3XL);
  tft.setTextColor(C_AMBER);
  tft.setTextDatum(lgfx::top_center);
  tft.drawString(timeBuf, W / 2, CY + 6);
  tft.setTextDatum(lgfx::top_left);

  if (ntpOk) {
    const char* dayNames[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    const char* monNames[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
    char dateBuf[20];
    snprintf(dateBuf, sizeof(dateBuf), "%s %d %s", dayNames[t->tm_wday], t->tm_mday, monNames[t->tm_mon]);
    tft.setFont(FONT_SM);
    tft.setTextColor(C_DIM);
    tft.setTextDatum(lgfx::top_center);
    tft.drawString(dateBuf, W / 2, CY + 82);
    tft.setTextDatum(lgfx::top_left);
  }

  int cy = CY + 108;
  tft.drawFastHLine(14, cy, W - 28, C_DIMMER);
  cy += 8;

  if (!wxReady) {
    dlbl(20, cy + 14, FONT_SM, C_DIMMER, "WEATHER LOADING...");
    drawStatusBar();
    return;
  }

  int lx = 15, rx = W / 2 + 15;

  char buf[32];

  dlbl(lx, cy, FONT_XS, C_DIM, "TEMPERATURE");
  dlbl(rx, cy, FONT_XS, C_DIM, "REAL FEEL");
  cy += 20;
  snprintf(buf, sizeof(buf), "%.0f F", wxData.temp * 9.0f / 5.0f + 32.0f);
  dlbl(lx, cy, FONT_SM, C_AMBER, buf);
  snprintf(buf, sizeof(buf), "%.0f F", wxData.feels_like * 9.0f / 5.0f + 32.0f);
  dlbl(rx, cy, FONT_SM, C_CYAN, buf);
  cy += 26;
  tft.drawFastHLine(14, cy, W - 28, C_DIMMER);
  cy += 4;

  dlbl(lx, cy, FONT_XS, C_DIM, "CONDITIONS");
  dlbl(rx, cy, FONT_XS, C_DIM, "PRECIPITATION");
  cy += 20;
  dlbl(lx, cy, FONT_SM, C_YELLOW, wxData.condition);
  uint16_t precipCol = wxData.precipitation_mm < 0.1f ? C_GREEN :
                       wxData.precipitation_mm < 2.5f ? C_YELLOW :
                       wxData.precipitation_mm < 7.5f ? C_AMBER  : C_RED;
  snprintf(buf, sizeof(buf), "%.2f IN", wxData.precipitation_mm * 0.0393701f);
  dlbl(rx, cy, FONT_SM, precipCol, buf);
  cy += 26;
  tft.drawFastHLine(14, cy, W - 28, C_DIMMER);
  cy += 4;

  dlbl(lx, cy, FONT_XS, C_DIM, "HUMIDITY");
  dlbl(rx, cy, FONT_XS, C_DIM, "WIND");
  cy += 20;
  snprintf(buf, sizeof(buf), "%d%%", wxData.humidity);
  dlbl(lx, cy, FONT_SM, C_AMBER, buf);
  snprintf(buf, sizeof(buf), "%.0f MPH %s", wxData.wind_speed * 0.621371f, wxData.wind_cardinal);
  dlbl(rx, cy, FONT_SM, C_AMBER, buf);
  cy += 26;
  tft.drawFastHLine(14, cy, W - 28, C_DIMMER);
  cy += 4;

  dlbl(lx, cy, FONT_XS, C_DIM, "UV INDEX");
  dlbl(rx, cy, FONT_XS, C_DIM, "VISIBILITY");
  cy += 20;

  uint16_t uvCol = wxData.uv_index < 3.0f ? C_GREEN :
                   wxData.uv_index < 6.0f ? C_YELLOW :
                   wxData.uv_index < 8.0f ? C_AMBER  : C_RED;
  snprintf(buf, sizeof(buf), "%.1f", wxData.uv_index);
  dlbl(lx, cy, FONT_SM, uvCol, buf);

  float visMi = wxData.visibility_km * 0.621371f;
  uint16_t visCol = visMi >= 6.0f ? C_GREEN :
                    visMi >= 3.0f ? C_YELLOW :
                    visMi >= 1.2f ? C_AMBER  : C_RED;
  snprintf(buf, sizeof(buf), "%.0f MI", visMi);
  dlbl(rx, cy, FONT_SM, visCol, buf);
  cy += 26;

  snprintf(buf, sizeof(buf), "RISE %s", wxData.sunrise);
  dlbl(lx, cy, FONT_XS, C_DIM, buf);
  snprintf(buf, sizeof(buf), "SET %s", wxData.sunset);
  dlbl(rx, cy, FONT_XS, C_DIM, buf);

  drawStatusBar();
}

// ─── Boot sequence ──────────────────────────────────
void bootSequence() {
  tft.fillScreen(C_BG);

  tft.setFont(FONT_XL);
  tft.setTextColor(C_AMBER);
  tft.setTextDatum(lgfx::middle_center);
  tft.drawString("OVERHEAD", W / 2, H / 2 - 50);
  tft.drawString("TRACKER", W / 2, H / 2 + 10);

  tft.setFont(FONT_SM);
  tft.setTextColor(C_DIM);
  tft.drawString("FOXTROT", W / 2, H / 2 + 60);
  tft.setFont(FONT_XS);
  tft.setTextColor(C_DIMMER);
  tft.drawString("v" FW_VERSION, W / 2, H / 2 + 85);
  tft.setTextDatum(lgfx::top_left);

  int barX = W / 4, barY = H / 2 + 110, barW = W / 2, barH = 6;
  tft.drawRect(barX, barY, barW, barH, C_DIMMER);
  for (int i = 0; i <= 10; i++) {
    tft.fillRect(barX + 1, barY + 1, (barW - 2) * i / 10, barH - 2, C_AMBER);
    delay(40);
  }

  delay(200);
}

// ─── OTA progress ───────────────────────────────────
void drawOtaProgress(int pct) {
  static bool otaInit = false;
  if (!otaInit) {
    otaInit = true;
    tft.fillScreen(C_BG);
    tft.setFont(FONT_MD);
    tft.setTextColor(C_AMBER);
    tft.setTextDatum(lgfx::middle_center);
    tft.drawString("OTA UPDATE", W / 2, 150);
    tft.setFont(FONT_SM);
    tft.setTextColor(C_DIM);
    tft.drawString("Do not power off", W / 2, 200);
    tft.setTextDatum(lgfx::top_left);
    tft.drawRect(60, 300, 680, 28, C_DIMMER);
  }
  int barW = (int)(680 * pct / 100);
  tft.fillRect(60, 300, barW, 28, C_GREEN);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  tft.setFont(FONT_SM);
  tft.setTextColor(C_GREEN);
  tft.setTextDatum(lgfx::middle_center);
  tft.drawString(buf, W / 2, 346);
  tft.setTextDatum(lgfx::top_left);
}
