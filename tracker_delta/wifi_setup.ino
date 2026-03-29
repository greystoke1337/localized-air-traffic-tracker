// ─── WiFi config, captive portal, geocoding ──────────

bool loadWiFiConfig() {
  Preferences p;
  p.begin("tracker", true);
  if (!p.isKey("wifi_ssid")) { p.end(); return false; }
  strlcpy(WIFI_SSID, p.getString("wifi_ssid", "").c_str(), sizeof(WIFI_SSID));
  strlcpy(WIFI_PASS, p.getString("wifi_pass", "").c_str(), sizeof(WIFI_PASS));
  if (p.isKey("home_lat")) {
    HOME_LAT = p.getFloat("home_lat", HOME_LAT);
    HOME_LON = p.getFloat("home_lon", HOME_LON);
    String name = p.getString("home_name", "");
    if (name.length() > 0) {
      name.toUpperCase();
      strlcpy(LOCATION_NAME, name.c_str(), sizeof(LOCATION_NAME));
    }
    needsGeocode = false;
  } else if (p.isKey("home_query")) {
    strlcpy(HOME_QUERY, p.getString("home_query", "").c_str(), sizeof(HOME_QUERY));
    needsGeocode = true;
  }
  p.end();
  return true;
}

void saveWiFiConfig(const char* ssid, const char* pass, const char* query) {
  Preferences p;
  p.begin("tracker", false);
  p.putString("wifi_ssid",  ssid);
  p.putString("wifi_pass",  pass);
  p.putString("home_query", query);
  p.remove("home_lat");
  p.remove("home_lon");
  p.remove("home_name");
  p.end();
}

bool geocodeLocation(const char* query) {
  char encoded[192]; int j = 0;
  for (int i = 0; query[i] && j < (int)sizeof(encoded) - 1; i++)
    encoded[j++] = (query[i] == ' ') ? '+' : query[i];
  encoded[j] = '\0';

  char url[320];
  snprintf(url, sizeof(url),
    "https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=1", encoded);

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(8);
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("User-Agent", "SpotterDelta/1.0");
  http.setTimeout(8000);
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, body) != DeserializationError::Ok || !doc.is<JsonArray>()) return false;
  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() == 0) return false;

  HOME_LAT = String(arr[0]["lat"].as<const char*>()).toFloat();
  HOME_LON = String(arr[0]["lon"].as<const char*>()).toFloat();

  String dispName = arr[0]["display_name"].as<String>();
  int comma = dispName.indexOf(',');
  String shortName = (comma > 0 && comma <= 30) ? dispName.substring(0, comma) : dispName.substring(0, 30);
  shortName.toUpperCase();
  strlcpy(LOCATION_NAME, shortName.c_str(), sizeof(LOCATION_NAME));

  Preferences p;
  p.begin("tracker", false);
  p.putFloat("home_lat",    HOME_LAT);
  p.putFloat("home_lon",    HOME_LON);
  p.putString("home_name",  LOCATION_NAME);
  p.remove("home_query");
  p.end();
  HOME_QUERY[0] = '\0';
  needsGeocode = false;
  return true;
}

// ─── Captive portal ───────────────────────────────

static void handleSetupRoot() {
  const char* locDefault = HOME_QUERY[0] ? HOME_QUERY : LOCATION_NAME;
  static char page[2048];
  snprintf(page, sizeof(page),
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>DELTA SETUP</title>"
    "<style>*{box-sizing:border-box}"
    "body{background:#041010;color:#fd0;font-family:monospace;padding:16px;max-width:480px;margin:auto}"
    "h2{font-size:1em;letter-spacing:3px;margin:0 0 20px;padding-bottom:8px;border-bottom:1px solid #3900}"
    "b{display:block;font-size:.8em;letter-spacing:1px;margin:16px 0 4px;color:#fd0}"
    "input{display:block;width:100%%;background:#0d1f1f;border:1px solid #fd0;"
    "color:#fd0;padding:10px;font-family:monospace;font-size:1em;margin-bottom:2px}"
    "button{display:block;width:100%%;margin-top:20px;padding:14px;background:#fd0;"
    "color:#041010;border:none;font-family:monospace;font-size:1em;font-weight:bold;letter-spacing:2px}"
    "</style></head><body>"
    "<h2>DELTA &mdash; SETUP</h2>"
    "<form method='POST' action='/save'>"
    "<b>WI-FI NETWORK</b>"
    "<input name='ssid' placeholder='Network name' value='%s' required>"
    "<b>WI-FI PASSWORD</b>"
    "<input name='pass' type='password' placeholder='Password'>"
    "<b>LOCATION</b>"
    "<input name='query' placeholder='e.g. Russell Lea, Sydney' value='%s'>"
    "<button type='submit'>SAVE &amp; REBOOT</button>"
    "</form></body></html>",
    WIFI_SSID, locDefault);
  setupServer.send(200, "text/html", page);
}

static void handleSetupSave() {
  String ssid  = setupServer.arg("ssid");
  String pass  = setupServer.arg("pass");
  String query = setupServer.arg("query");
  ssid.trim(); query.trim();
  saveWiFiConfig(ssid.c_str(), pass.c_str(), query.c_str());
  setupServer.send(200, "text/html",
    "<html><body style='background:#041010;color:#fd0;font-family:monospace;padding:20px'>"
    "<h2 style='color:#07e0'>SAVED</h2><p>REBOOTING...</p></body></html>");
  delay(1500);
  ESP.restart();
}

void startCaptivePortal() {
  gfx->fillScreen(C_BG);
  gfx->setTextSize(SZ_MD);
  gfx->setTextColor(C_AMBER);
  gfx->setCursor(6, HDR_H + 4);
  gfx->print("DELTA SETUP MODE");

  gfx->setTextSize(SZ_SM);
  gfx->setTextColor(C_DIM);
  int sy = HDR_H + 30;
  const char* lines[] = {
    "CONNECT PHONE TO WI-FI:  DELTA-SETUP",
    "OPEN BROWSER TO:  192.168.4.1",
  };
  for (int i = 0; i < 2; i++) {
    gfx->setCursor(6, sy);
    gfx->print(lines[i]);
    sy += 16;
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP("DELTA-SETUP");
  delay(100);

  dnsServer.start(53, "*", WiFi.softAPIP());
  setupServer.on("/",     HTTP_GET,  handleSetupRoot);
  setupServer.on("/save", HTTP_POST, handleSetupSave);
  setupServer.onNotFound([]() {
    setupServer.sendHeader("Location", "http://192.168.4.1/");
    setupServer.send(302, "text/plain", "");
  });
  setupServer.begin();

  while (true) {
    dnsServer.processNextRequest();
    setupServer.handleClient();
    delay(10);
  }
}
