// ─── Display: all drawing for Delta 640×172 ─────────
// Built-in 6×8 font: size 1 = 6×8 px, size 2 = 12×16 px, size 3 = 18×24 px

// ─── Low-level helpers ────────────────────────────

// Fill a rect then print left-aligned text inside it (top-left anchor)
static void dlbl(int x, int y, int w, int h, uint16_t bg, uint16_t fg, uint8_t sz, const char* txt) {
  gfx->fillRect(x, y, w, h, bg);
  gfx->setTextSize(sz);
  gfx->setTextColor(fg);
  int cy = y + (h - sz * 8) / 2;
  gfx->setCursor(x + 2, cy);
  gfx->print(txt);
}

// Right-aligned text in a rect
static void dlbl_r(int x, int y, int w, int h, uint16_t bg, uint16_t fg, uint8_t sz, const char* txt) {
  gfx->fillRect(x, y, w, h, bg);
  int tw = strlen(txt) * sz * 6;
  int cx = x + w - tw - 2;
  if (cx < x) cx = x;
  int cy = y + (h - sz * 8) / 2;
  gfx->setTextSize(sz);
  gfx->setTextColor(fg);
  gfx->setCursor(cx, cy);
  gfx->print(txt);
}

// Center-aligned text in a rect
static void dlbl_c(int x, int y, int w, int h, uint16_t bg, uint16_t fg, uint8_t sz, const char* txt) {
  gfx->fillRect(x, y, w, h, bg);
  int tw = strlen(txt) * sz * 6;
  int cx = x + (w - tw) / 2;
  if (cx < x) cx = x;
  int cy = y + (h - sz * 8) / 2;
  gfx->setTextSize(sz);
  gfx->setTextColor(fg);
  gfx->setCursor(cx, cy);
  gfx->print(txt);
}

// Small colored dot (status indicator)
static void drawDot(int cx, int cy, uint16_t col) {
  gfx->fillCircle(cx, cy, 4, col);
}

// ─── Title bar ────────────────────────────────────
// y=0, h=HDR_H (18px)
// [dot] OVERHEAD TRACKER    [○●○] [N TODAY]
static void drawTitleBar(bool wifi, Page page) {
  gfx->fillRect(0, 0, W, HDR_H, C_BG);
  gfx->drawFastHLine(0, HDR_H - 1, W, C_DIMMER);

  // WiFi dot
  drawDot(8, HDR_H / 2, wifi ? C_GREEN : C_RED);

  // Title
  gfx->setTextSize(SZ_SM);
  gfx->setTextColor(C_ACCENT);
  gfx->setCursor(18, (HDR_H - 8) / 2);
  gfx->print("OVERHEAD TRACKER");

  // Page dots (3 dots)
  int dotX = W - 60;
  for (int i = 0; i < PAGE_COUNT; i++) {
    gfx->fillCircle(dotX + i * 10, HDR_H / 2, 3, i == (int)page ? C_ACCENT : C_DIM);
  }

  // Today count
  if (todayCount > 0) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d TODAY", todayCount);
    gfx->setTextSize(SZ_SM);
    gfx->setTextColor(C_DIM);
    int tw = strlen(buf) * 6;
    gfx->setCursor(W - tw - 4, (HDR_H - 8) / 2);
    gfx->print(buf);
  }
}

// ─── Data age strip (bottom of histogram row) ─────
static void drawDataAge(unsigned long dataMs) {
  if (dataMs == 0) return;
  unsigned long age = (millis() - dataMs) / 1000;
  char buf[24];
  if (age < 60)       snprintf(buf, sizeof(buf), "%lus AGO", age);
  else                snprintf(buf, sizeof(buf), "%lum AGO", age / 60);
  uint16_t col = age < 20 ? C_DIM : age < 60 ? C_AMBER : C_RED;
  gfx->setTextSize(SZ_SM);
  gfx->setTextColor(col);
  int tw = strlen(buf) * 6;
  gfx->setCursor(W - tw - 4, HIST_Y + (HIST_H - 8) / 2);
  gfx->print(buf);
}

// ─── Histogram strip ──────────────────────────────
// Draws traffic-by-hour bar chart in bottom HIST_H pixels
static void drawHistogram(unsigned long dataMs) {
  gfx->fillRect(0, HIST_Y, W, HIST_H, C_BG);
  gfx->drawFastHLine(0, HIST_Y, W, C_DIMMER);

  if (peakHours[0] == 0 && peakHours[12] == 0) {
    drawDataAge(dataMs);
    return;
  }

  uint8_t maxVal = 1;
  for (int i = 0; i < 24; i++) if (peakHours[i] > maxVal) maxVal = peakHours[i];

  int barW = (W - 80) / 24;  // leave 80px right for data age
  int barAreaH = HIST_H - 4;

  // Current hour highlight
  struct tm t; time_t now = time(nullptr);
  localtime_r(&now, &t);
  int curHour = t.tm_hour;

  for (int i = 0; i < 24; i++) {
    int bh = ((int)peakHours[i] * barAreaH) / maxVal;
    if (bh < 1 && peakHours[i] > 0) bh = 1;
    int bx = 4 + i * barW;
    int by = HIST_Y + HIST_H - 2 - bh;
    uint16_t col = (i == curHour) ? C_ACCENT : C_DIM;
    gfx->fillRect(bx, by, max(barW - 1, 1), bh, col);
  }

  drawDataAge(dataMs);
}

// ─── Page 0: Flights + Weather ────────────────────
// Content area: y=HDR_H..HIST_Y = 18..144 = 126px
// Row layout:
//   y=18, h=36: flight 1 primary (callsign, phase, alt, dist)
//   y=54, h=20: flight 1 route
//   y=74, h=36: flight 2 primary
//   y=110, h=16: flight 2 route (small)
//   y=126, h=18: weather

static void drawFlightRow(int y, int h, const Flight& f, bool altRow) {
  uint16_t bg = altRow ? C_ROW_BG : C_BG;
  gfx->fillRect(0, y, W, h, bg);

  // Callsign (white, large)
  gfx->setTextSize(SZ_MD);
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(6, y + (h - 16) / 2);
  gfx->print(f.callsign[0] ? f.callsign : "------");

  // Phase (colored)
  const char* lbl = statusLabel(f.status);
  uint16_t    col = statusColor(f.status);
  int phaseX = 6 + 8 * 12 + 4;  // after 8 callsign chars
  gfx->setTextSize(SZ_SM);
  gfx->setTextColor(col);
  gfx->setCursor(phaseX, y + (h - 8) / 2);
  gfx->print(lbl);

  // Altitude
  char altBuf[12];
  formatAlt(f.alt, altBuf, sizeof(altBuf));
  int altX = phaseX + 8 * 6 + 8;
  gfx->setTextColor(C_DIM);
  gfx->setCursor(altX, y + (h - 8) / 2);
  gfx->print(altBuf);

  // Distance
  char distBuf[12];
  snprintf(distBuf, sizeof(distBuf), "%.1fmi", f.dist);
  int distX = altX + 10 * 6 + 8;
  gfx->setTextColor(C_ACCENT);
  gfx->setCursor(distX, y + (h - 8) / 2);
  gfx->print(distBuf);

  // Type (right-aligned)
  if (f.type[0]) {
    gfx->setTextColor(C_DIMMER + 0x2082);  // slightly visible
    gfx->setTextSize(SZ_SM);
    int tw = strlen(f.type) * 6;
    gfx->setCursor(W - tw - 6, y + (h - 8) / 2);
    gfx->print(f.type);
  }
}

static void drawRouteRow(int y, int h, const Flight& f, bool altRow) {
  uint16_t bg = altRow ? C_ROW_BG : C_BG;
  gfx->fillRect(0, y, W, h, bg);
  if (f.route[0]) {
    gfx->setTextSize(SZ_SM);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(6, y + (h - 8) / 2);
    gfx->print(f.route);
  }
}

static void drawPageFlights() {
  int cy = HDR_H;
  bool wifi = (WiFi.status() == WL_CONNECTED);
  drawTitleBar(wifi, PAGE_FLIGHTS);

  if (flightCount == 0) {
    // Clear skies
    gfx->fillRect(0, cy, W, HIST_Y - cy, C_BG);
    dlbl_c(0, cy, W, 50, C_BG, C_DIM, SZ_MD, "CLEAR SKIES");

    // Still show weather below
    int wy = cy + 50;
    if (wxReady) {
      gfx->fillRect(0, wy, W, HIST_Y - wy, C_BG);
      char wxBuf[64];
      snprintf(wxBuf, sizeof(wxBuf), "%.0fC  %s  %s %.0f km/h",
        wxData.temp, wxData.condition, wxData.wind_cardinal, wxData.wind_speed);
      gfx->setTextSize(SZ_SM);
      gfx->setTextColor(C_ACCENT);
      gfx->setCursor(6, wy + 10);
      gfx->print(wxBuf);
    }
  } else {
    // Row heights: 2 flight rows at 34px each + route row + weather
    // With 1 or 2 flights, fill proportionally
    bool twoFlights = flightCount >= 2;
    int f1PrimaryH = 34;
    int f1RouteH   = 18;
    int f2PrimaryH = twoFlights ? 34 : 0;
    int f2RouteH   = twoFlights ? 16 : 0;
    int wxH        = HIST_Y - cy - f1PrimaryH - f1RouteH - f2PrimaryH - f2RouteH;
    if (wxH < 12) wxH = 12;

    int y0 = cy;
    drawFlightRow(y0, f1PrimaryH, flights[0], false);
    y0 += f1PrimaryH;
    drawRouteRow(y0, f1RouteH, flights[0], false);
    y0 += f1RouteH;

    if (twoFlights) {
      drawFlightRow(y0, f2PrimaryH, flights[1], true);
      y0 += f2PrimaryH;
      drawRouteRow(y0, f2RouteH, flights[1], true);
      y0 += f2RouteH;
    }

    // Weather strip
    gfx->fillRect(0, y0, W, wxH, C_BG);
    gfx->drawFastHLine(0, y0, W, C_DIMMER);
    if (wxReady) {
      char wxBuf[72];
      int32_t tempC = (int32_t)wxData.temp;
      snprintf(wxBuf, sizeof(wxBuf), "%+ldC  %s  %s %.0f",
        (long)tempC, wxData.condition, wxData.wind_cardinal, wxData.wind_speed);
      gfx->setTextSize(SZ_SM);
      gfx->setTextColor(C_ACCENT);
      gfx->setCursor(6, y0 + (wxH - 8) / 2);
      gfx->print(wxBuf);
    }
  }

  drawHistogram(lastFlightDataMs);
}

// ─── Page 1: Proxy Dashboard ──────────────────────
// y=18: stats 2×4 grid (each cell ~160×26)
// y=70: ADS-B sources
// y=96: divider
// y=100: histogram

static void drawStatCell(int x, int y, int w, int h, const char* label, const char* val, uint16_t valCol) {
  gfx->fillRect(x, y, w, h, C_BG);
  gfx->drawRect(x, y, w, h, C_DIMMER);
  // label small top-left
  gfx->setTextSize(SZ_SM);
  gfx->setTextColor(C_DIM);
  gfx->setCursor(x + 3, y + 2);
  gfx->print(label);
  // value larger bottom
  gfx->setTextSize(SZ_MD);
  gfx->setTextColor(valCol);
  gfx->setCursor(x + 3, y + h - 18);
  gfx->print(val);
}

static void drawPageProxy() {
  bool wifi = (WiFi.status() == WL_CONNECTED);
  drawTitleBar(wifi, PAGE_PROXY);

  int cy = HDR_H;
  gfx->fillRect(0, cy, W, HIST_Y - cy, C_BG);

  if (!proxyStatsReady) {
    dlbl_c(0, cy, W, HIST_Y - cy, C_BG, C_DIM, SZ_MD, "LOADING...");
    drawHistogram(lastStatsMs);
    return;
  }

  // 4 cells per row, 2 rows, each cell W/4 × 28
  int cellW = W / 4;
  int cellH = 28;
  char buf[20];

  // Row 1
  drawStatCell(0*cellW, cy,      cellW, cellH, "UPTIME",  proxyStats.uptime, C_WHITE);

  snprintf(buf, sizeof(buf), "%d", proxyStats.requests);
  drawStatCell(1*cellW, cy,      cellW, cellH, "REQUESTS", buf, C_WHITE);

  snprintf(buf, sizeof(buf), "%.0f%%", proxyStats.cacheHit * 100.0f);
  uint16_t hitCol = proxyStats.cacheHit > 0.7f ? C_GREEN : proxyStats.cacheHit > 0.4f ? C_AMBER : C_RED;
  drawStatCell(2*cellW, cy,      cellW, cellH, "CACHE HIT", buf, hitCol);

  snprintf(buf, sizeof(buf), "%d", proxyStats.errors);
  uint16_t errCol = proxyStats.errors == 0 ? C_GREEN : proxyStats.errors < 10 ? C_AMBER : C_RED;
  drawStatCell(3*cellW, cy,      cellW, cellH, "ERRORS", buf, errCol);

  // Row 2
  int cy2 = cy + cellH;
  snprintf(buf, sizeof(buf), "%d", proxyStats.clients);
  drawStatCell(0*cellW, cy2, cellW, cellH, "CLIENTS", buf, C_WHITE);

  snprintf(buf, sizeof(buf), "%d", proxyStats.cached);
  drawStatCell(1*cellW, cy2, cellW, cellH, "CACHED", buf, C_WHITE);

  snprintf(buf, sizeof(buf), "%d", proxyStats.routes);
  drawStatCell(2*cellW, cy2, cellW, cellH, "ROUTES", buf, C_WHITE);

  snprintf(buf, sizeof(buf), "%d", proxyStats.newRoutes);
  uint16_t newCol = proxyStats.newRoutes > 0 ? C_GREEN : C_DIM;
  drawStatCell(3*cellW, cy2, cellW, cellH, "NEW TODAY", buf, newCol);

  // ADS-B sources row
  int srcY = cy + 2 * cellH + 2;
  int srcH = HIST_Y - srcY;
  gfx->fillRect(0, srcY, W, srcH, C_BG);
  gfx->drawFastHLine(0, srcY, W, C_DIMMER);

  const char* srcNames[] = {"adsb.lol", "adsb.fi", "airplanes.live", "adsb-one"};
  bool       srcUp[]     = {proxyStats.adsbLolUp, proxyStats.adsbFiUp,
                             proxyStats.airplanesLiveUp, proxyStats.adsbOneUp};
  int srcSpacing = W / 4;
  for (int i = 0; i < 4; i++) {
    int sx = i * srcSpacing + 8;
    int sy = srcY + srcH / 2;
    gfx->fillCircle(sx + 4, sy, 4, srcUp[i] ? C_GREEN : C_RED);
    gfx->setTextSize(SZ_SM);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(sx + 12, sy - 4);
    gfx->print(srcNames[i]);
  }

  drawHistogram(lastStatsMs);
}

// ─── Page 2: Server + Devices ─────────────────────
// y=18: server row (h=28)
// y=46: echo row  (h=32)
// y=78: foxtrot row (h=32)
// y=110: gap + histogram at HIST_Y=144

static void drawDeviceRow(int y, int h, const char* name, const DeviceStatus& dev, bool altRow) {
  uint16_t bg = altRow ? C_ROW_BG : C_BG;
  gfx->fillRect(0, y, W, h, bg);

  // Online dot
  bool fresh = dev.online && (millis() - dev.lastSeenMs < 90000);
  drawDot(8, y + h / 2, fresh ? C_GREEN : C_RED);

  // Name
  gfx->setTextSize(SZ_SM);
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(18, y + (h - 8) / 2);
  gfx->print(name);

  if (!dev.online) {
    gfx->setTextColor(C_RED);
    gfx->setCursor(80, y + (h - 8) / 2);
    gfx->print("OFFLINE");
    return;
  }

  // fw
  gfx->setTextColor(C_DIM);
  gfx->setCursor(80, y + (h - 8) / 2);
  gfx->print("fw ");
  gfx->setTextColor(C_WHITE);
  gfx->print(dev.fw);

  // heap
  char buf[16];
  snprintf(buf, sizeof(buf), "%dkB", dev.heap / 1024);
  gfx->setTextColor(C_DIM);
  gfx->setCursor(200, y + (h - 8) / 2);
  gfx->print(buf);

  // RSSI
  snprintf(buf, sizeof(buf), "%ddBm", dev.rssi);
  uint16_t rssiCol = dev.rssi > -65 ? C_GREEN : dev.rssi > -80 ? C_AMBER : C_RED;
  gfx->setTextColor(rssiCol);
  gfx->setCursor(280, y + (h - 8) / 2);
  gfx->print(buf);

  // Uptime
  unsigned long u = dev.uptimeSecs;
  if (u < 3600)       snprintf(buf, sizeof(buf), "%lum", (unsigned long)(u / 60));
  else if (u < 86400) snprintf(buf, sizeof(buf), "%luh", (unsigned long)(u / 3600));
  else                snprintf(buf, sizeof(buf), "%lud", (unsigned long)(u / 86400));
  gfx->setTextColor(C_DIM);
  gfx->setCursor(360, y + (h - 8) / 2);
  gfx->print(buf);

  // Last seen
  unsigned long age = (millis() - dev.lastSeenMs) / 1000;
  if (age < 60)       snprintf(buf, sizeof(buf), "%lus", age);
  else                snprintf(buf, sizeof(buf), "%lum", age / 60);
  uint16_t ageCol = age < 90 ? C_DIM : age < 300 ? C_AMBER : C_RED;
  gfx->setTextColor(ageCol);
  int tw = strlen(buf) * 6;
  gfx->setCursor(W - tw - 6, y + (h - 8) / 2);
  gfx->print(buf);
}

static void drawPageServer() {
  bool wifi = (WiFi.status() == WL_CONNECTED);
  drawTitleBar(wifi, PAGE_SERVER);

  int cy = HDR_H;
  gfx->fillRect(0, cy, W, HIST_Y - cy, C_BG);

  // Server row
  int srvH = 26;
  gfx->fillRect(0, cy, W, srvH, C_BG);
  gfx->drawFastHLine(0, cy, W, C_DIMMER);
  if (serverReady) {
    char buf[80];
    snprintf(buf, sizeof(buf), "SRV  up:%s  cpu:%.0fC  ram:%.0f%%  load:%.1f",
      serverStatus.osUptime, serverStatus.cpuTemp,
      serverStatus.ramPct, serverStatus.load1);
    gfx->setTextSize(SZ_SM);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(6, cy + (srvH - 8) / 2);
    gfx->print(buf);
  } else {
    gfx->setTextSize(SZ_SM);
    gfx->setTextColor(C_DIM);
    gfx->setCursor(6, cy + (srvH - 8) / 2);
    gfx->print("SERVER — NO DATA");
  }

  // Device rows
  int devH = 32;
  drawDeviceRow(cy + srvH,          devH, "ECHO",    echoStatus,    false);
  drawDeviceRow(cy + srvH + devH,   devH, "FOXTROT", foxtrotStatus, true);
  drawDeviceRow(cy + srvH + devH*2, devH, "DELTA",   {FW_VERSION, (int)ESP.getFreeHeap(),
    WiFi.RSSI(), millis() / 1000, millis(), wifi}, false);

  drawHistogram(lastServerMs);
}

// ─── Master page dispatcher ───────────────────────
void drawPage(Page page) {
  switch (page) {
    case PAGE_FLIGHTS: drawPageFlights(); break;
    case PAGE_PROXY:   drawPageProxy();   break;
    case PAGE_SERVER:  drawPageServer();  break;
    default: break;
  }
}
