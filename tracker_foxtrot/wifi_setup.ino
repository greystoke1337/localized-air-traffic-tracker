// ─── WiFi config, captive portal, geocoding ────────────

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
  geoIndex = p.getInt("gfence_idx", 1);
  if (geoIndex < 0 || geoIndex >= GEO_COUNT) geoIndex = 1;
  GEOFENCE_MI = GEO_PRESETS[geoIndex];
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

void saveGeoIndex() {
  Preferences p;
  p.begin("tracker", false);
  p.putInt("gfence_idx", geoIndex);
  p.end();
}

// ─── Geocoding (Nominatim) ────────────────────────────
bool geocodeLocation(const char* query) {
  char encoded[192];
  int j = 0;
  for (int i = 0; query[i] && j < (int)sizeof(encoded) - 1; i++) {
    if (query[i] == ' ') encoded[j++] = '+';
    else                  encoded[j++] = query[i];
  }
  encoded[j] = '\0';

  char url[320];
  snprintf(url, sizeof(url),
    "https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=1",
    encoded);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("User-Agent", "OverheadTracker/1.0");
  http.setTimeout(8000);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("Geocode HTTP error: %d\n", code);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, body) != DeserializationError::Ok || !doc.is<JsonArray>()) {
    Serial.println("Geocode JSON parse error");
    return false;
  }
  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() == 0) {
    Serial.println("Geocode: no results");
    return false;
  }
  HOME_LAT = arr[0]["lat"].as<const char*>() ? String(arr[0]["lat"].as<const char*>()).toFloat() : 0.0f;
  HOME_LON = arr[0]["lon"].as<const char*>() ? String(arr[0]["lon"].as<const char*>()).toFloat() : 0.0f;

  String dispName = arr[0]["display_name"].as<String>();
  int comma = dispName.indexOf(',');
  String shortName = (comma > 0 && comma <= 30) ? dispName.substring(0, comma) : dispName.substring(0, 30);
  shortName.toUpperCase();
  strlcpy(LOCATION_NAME, shortName.c_str(), sizeof(LOCATION_NAME));

  Serial.printf("Geocoded: %s → %.4f, %.4f\n", LOCATION_NAME, HOME_LAT, HOME_LON);

  Preferences p;
  p.begin("tracker", false);
  p.putFloat("home_lat",  HOME_LAT);
  p.putFloat("home_lon",  HOME_LON);
  p.putString("home_name", LOCATION_NAME);
  p.remove("home_query");
  p.end();
  HOME_QUERY[0] = '\0';
  needsGeocode = false;
  return true;
}

// ─── Captive portal ───────────────────────────────────

static void handleSetupRoot() {
  const char* locDefault = HOME_QUERY[0] ? HOME_QUERY : LOCATION_NAME;
  static char page[2048];
  snprintf(page, sizeof(page),
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>OVERHEAD SETUP</title>"
    "<style>"
    "*{box-sizing:border-box}"
    "body{background:#041010;color:#fd0;font-family:monospace;padding:16px;max-width:480px;margin:auto}"
    "h2{font-size:1em;letter-spacing:3px;margin:0 0 20px;padding-bottom:8px;border-bottom:1px solid #3900}"
    "b{display:block;font-size:.8em;letter-spacing:1px;margin:16px 0 4px;color:#fd0}"
    "input{display:block;width:100%%;background:#0d1f1f;border:1px solid #fd0;"
    "color:#fd0;padding:10px;font-family:monospace;font-size:1em;margin-bottom:2px}"
    "button{display:block;width:100%%;margin-top:20px;padding:14px;background:#fd0;"
    "color:#041010;border:none;font-family:monospace;font-size:1em;font-weight:bold;letter-spacing:2px}"
    "</style></head><body>"
    "<h2>OVERHEAD TRACKER &mdash; SETUP</h2>"
    "<form method='POST' action='/save'>"
    "<b>WI-FI NETWORK</b>"
    "<input name='ssid' placeholder='Network name' value='%s' required>"
    "<b>WI-FI PASSWORD</b>"
    "<input name='pass' type='password' placeholder='Password'>"
    "<b>LOCATION</b>"
    "<input name='query' placeholder='e.g. Russell Lea, Sydney Airport' value='%s'>"
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
    "<html><body style='background:#041010;color:#fd0;"
    "font-family:monospace;padding:20px'>"
    "<h2 style='color:#07e0'>SAVED</h2>"
    "<p>REBOOTING NOW...</p></body></html>");
  delay(1500);
  ESP.restart();
}

void startCaptivePortal() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, W, HDR_H, C_AMBER);
  tft.setFont(&lgfx::fonts::DejaVu24);
  tft.setTextColor(C_BG, C_AMBER);
  tft.setTextDatum(lgfx::middle_left);
  tft.drawString("OVERHEAD TRACKER", 16, HDR_H / 2);
  tft.setTextDatum(lgfx::top_left);

  tft.setFont(&lgfx::fonts::DejaVu40);
  tft.setTextColor(C_AMBER);
  tft.drawString("SETUP MODE", 20, HDR_H + 24);

  const char* instrLines[] = {
    "ON YOUR PHONE:",
    "1. CONNECT TO WI-FI:  OVERHEAD-SETUP",
    "2. OPEN ANY BROWSER",
    "   (PAGE OPENS AUTOMATICALLY)",
    "   OR: 192.168.4.1",
  };
  tft.setFont(&lgfx::fonts::DejaVu18);
  tft.setTextColor(C_DIM);
  int sy = HDR_H + 80;
  for (int i = 0; i < 5; i++) {
    tft.drawString(instrLines[i], 20, sy);
    sy += 28;
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP("OVERHEAD-SETUP");
  delay(100);

  dnsServer.start(53, "*", WiFi.softAPIP());

  setupServer.on("/", HTTP_GET, handleSetupRoot);
  setupServer.on("/save", HTTP_POST, handleSetupSave);
  setupServer.onNotFound([]() {
    setupServer.sendHeader("Location", "http://192.168.4.1/");
    setupServer.send(302, "text/plain", "");
  });
  setupServer.begin();
  Serial.println("Captive portal active — AP: OVERHEAD-SETUP");

  while (true) {
    dnsServer.processNextRequest();
    setupServer.handleClient();
    esp_task_wdt_reset();
  }
}
