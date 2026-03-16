// ─── Network: all HTTP fetching and JSON parsing ────────

// ─── Root CA (ISRG Root X1 / Let's Encrypt) — shared by adsb.lol + airplanes.live ──
static const char LETSENCRYPT_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoBggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// ─── Parse a String payload (proxy or cache) ──────────
int parsePayload(String& payload) {
  StaticJsonDocument<512> filter;
  JsonObject af = filter["ac"].createNestedObject();
  af["flight"] = af["r"] = af["t"] = af["lat"] = af["lon"] =
  af["alt_baro"] = af["gs"] = af["baro_rate"] = af["track"] =
  af["squawk"] = af["route"] = af["dep"] = af["arr"] = true;
  g_jsonDoc.clear();
  esp_task_wdt_reset();
  Serial.printf("[MEM] Before JSON parse: %d free\n", ESP.getFreeHeap());
  DeserializationError err = deserializeJson(g_jsonDoc, &payload[0], payload.length(), DeserializationOption::Filter(filter));
  Serial.printf("[MEM] After JSON parse: %d free\n", ESP.getFreeHeap());
  esp_task_wdt_reset();
  if (err) {
    Serial.printf("JSON parse error: %s\n", err.c_str());
    return -1;
  }
  int result = extractFlights(g_jsonDoc);
  g_jsonDoc.clear();
  payload = String();
  return result;
}

// ─── Fetch: proxy (returns small String) ──────────────
String fetchFromProxy() {
  if (!wifiOk()) { Serial.println("[PROXY] WiFi not connected"); return ""; }
  unsigned long t0 = millis();
  WiFiClientSecure tcp;
  tcp.setInsecure();
  if (!tcp.connect(PROXY_HOST, PROXY_PORT, 5000)) {
    Serial.printf("[PROXY] Connect failed (%lu ms)\n", millis() - t0);
    return "";
  }
  esp_task_wdt_reset();
  char url[160];
  snprintf(url, sizeof(url),
    "https://%s/flights?lat=%.4f&lon=%.4f&radius=%d",
    PROXY_HOST, HOME_LAT, HOME_LON, apiRadiusNm());
  HTTPClient http;
  http.begin(tcp, url);
  http.setTimeout(12000);
  int code = http.GET();
  esp_task_wdt_reset();
  if (code == 200) {
    String p = http.getString();
    http.end();
    esp_task_wdt_reset();
    Serial.printf("[PROXY] OK %d bytes (%lu ms)\n", p.length(), millis() - t0);
    return p;
  }
  http.end();
  Serial.printf("[PROXY] HTTP %d (%lu ms)\n", code, millis() - t0);
  return "";
}

// ─── Stream helpers for direct API ─────────────────────
static bool readQuotedString(WiFiClient* s, char* buf, int maxLen, unsigned long deadline) {
  int i = 0;
  int c;
  while (millis() < deadline && (c = s->read()) != -1) {
    if (c == '"') { buf[i] = 0; return true; }
    if (c == '\\') { if (millis() >= deadline) break; s->read(); }
    if (i < maxLen - 1) buf[i++] = (char)c;
  }
  buf[i] = 0;
  return false;
}

static int readNumber(WiFiClient* s, char* buf, int maxLen) {
  int i = 0;
  int c = s->peek();
  while (c == ' ' || c == '\n' || c == '\r') { s->read(); c = s->peek(); }
  while (i < maxLen - 1) {
    c = s->peek();
    if (c == -1 || c == ',' || c == '}' || c == ']' || c == ' ') break;
    buf[i++] = (char)s->read();
  }
  buf[i] = 0;
  return i;
}

// ─── Fetch: direct API — zero-allocation stream scanner ──
int fetchAndParseDirectAPI() {
  if (!wifiOk()) {
    Serial.println("[DIRECT] WiFi not connected, skipping");
    return -1;
  }
  int freeHeap = ESP.getFreeHeap();
  Serial.printf("[DIRECT] Free heap: %d\n", freeHeap);
  if (freeHeap < DIRECT_API_MIN_HEAP) {
    Serial.println("[DIRECT] Insufficient heap for TLS, skipping");
    return -1;
  }
  if (directApiFailCount > 0 && millis() < directApiNextRetryMs) {
    Serial.printf("[DIRECT] Backoff active, %lu ms remaining\n",
                  directApiNextRetryMs - millis());
    return -1;
  }

  int radius = apiRadiusNm();
  const char* apiNames[] = { "adsb.lol", "airplanes.live" };
  char urls[2][160];
  snprintf(urls[0], sizeof(urls[0]),
    "https://api.adsb.lol/v2/point/%.4f/%.4f/%d",
    HOME_LAT, HOME_LON, radius);
  snprintf(urls[1], sizeof(urls[1]),
    "https://api.airplanes.live/v2/point/%.4f/%.4f/%d",
    HOME_LAT, HOME_LON, radius);

  HTTPClient http;
  WiFiClientSecure tlsClient;
  tlsClient.setCACert(LETSENCRYPT_ROOT_CA);
  tlsClient.setTimeout(DIRECT_API_TIMEOUT / 1000);
  bool connected = false;
  for (int i = 0; i < 2; i++) {
    Serial.printf("[DIRECT] Trying %s...\n", apiNames[i]);
    http.begin(tlsClient, urls[i]);
    http.setTimeout(DIRECT_API_TIMEOUT);
    int code = http.GET();
    if (code == 200) {
      Serial.printf("[DIRECT] %s OK\n", apiNames[i]);
      connected = true;
      break;
    }
    http.end();
    Serial.printf("[DIRECT] %s failed (%d)\n", apiNames[i], code);
  }
  if (!connected) {
    directApiFailCount++;
    unsigned long backoffMs = min(120000UL, 15000UL * (1UL << min(directApiFailCount - 1, 3)));
    directApiNextRetryMs = millis() + backoffMs;
    Serial.printf("[DIRECT] All APIs failed, backoff %lu ms (fail #%d)\n", backoffMs, directApiFailCount);
    return -1;
  }
  directApiFailCount = 0;

  Serial.printf("[MEM] Direct stream scan start: %d free\n", ESP.getFreeHeap());

  WiFiClient* s = http.getStreamPtr();
  if (!s) { Serial.println("[DIRECT] Null stream"); http.end(); return -1; }
  int newCount = 0;
  int depth = 0;
  bool inString = false;
  char key[16] = "";
  char val[32] = "";
  bool readingKey = false;
  bool readingVal = false;
  char ac_callsign[12]={}, ac_reg[12]={}, ac_type[8]={};
  char ac_dep[6]={},       ac_arr[6]={},  ac_squawk[6]={};
  char ac_route[40]={};
  float ac_lat=0, ac_lon=0;
  int   ac_alt=0, ac_speed=0, ac_vs=0, ac_track=-1;

  auto commitAircraft = [&]() {
    if (newCount >= 20) return;
    if (ac_alt < ALT_FLOOR_FT || ac_lat == 0.0f) return;
    float dist = haversineKm(HOME_LAT, HOME_LON, ac_lat, ac_lon);
    if (dist > GEOFENCE_KM) return;
    Flight& f = newFlights[newCount];
    memset(&f, 0, sizeof(Flight));
    strlcpy(f.callsign, ac_callsign, sizeof(f.callsign));
    for (int i = strlen(f.callsign)-1; i >= 0 && f.callsign[i] == ' '; i--) f.callsign[i] = 0;
    strlcpy(f.reg,    ac_reg,    sizeof(f.reg));
    strlcpy(f.type,   ac_type,   sizeof(f.type));
    strlcpy(f.squawk, ac_squawk[0] ? ac_squawk : "----", sizeof(f.squawk));
    strlcpy(f.dep, ac_dep, sizeof(f.dep));
    strlcpy(f.arr, ac_arr, sizeof(f.arr));
    if (ac_dep[0] || ac_arr[0]) {
      snprintf(f.route, sizeof(f.route), "%s > %s",
               ac_dep[0] ? ac_dep : "?", ac_arr[0] ? ac_arr : "?");
    } else {
      f.route[0] = 0;
    }
    f.lat=ac_lat; f.lon=ac_lon; f.alt=ac_alt;
    f.speed=ac_speed; f.vs=ac_vs; f.track=ac_track; f.dist=dist;
    f.status = deriveStatus(ac_alt, ac_vs, dist);
    toUpperStr(f.callsign);
    toUpperStr(f.reg);
    toUpperStr(f.type);
    newCount++;
  };
  auto applyKV = [&]() {
    if      (strcmp(key,"flight")==0)    strlcpy(ac_callsign, val, sizeof(ac_callsign));
    else if (strcmp(key,"r")==0)         strlcpy(ac_reg,      val, sizeof(ac_reg));
    else if (strcmp(key,"t")==0)         strlcpy(ac_type,     val, sizeof(ac_type));
    else if (strcmp(key,"squawk")==0)    strlcpy(ac_squawk,   val, sizeof(ac_squawk));
    else if (strcmp(key,"dep")==0)       strlcpy(ac_dep,      val, sizeof(ac_dep));
    else if (strcmp(key,"arr")==0)       strlcpy(ac_arr,      val, sizeof(ac_arr));
    else if (strcmp(key,"orig_iata")==0 && !ac_dep[0])  strlcpy(ac_dep, val, sizeof(ac_dep));
    else if (strcmp(key,"dest_iata")==0 && !ac_arr[0])  strlcpy(ac_arr, val, sizeof(ac_arr));
    else if (strcmp(key,"lat")==0)       ac_lat   = atof(val);
    else if (strcmp(key,"lon")==0)       ac_lon   = atof(val);
    else if (strcmp(key,"alt_baro")==0)  ac_alt   = atoi(val);
    else if (strcmp(key,"gs")==0)        ac_speed = (int)atof(val);
    else if (strcmp(key,"baro_rate")==0) ac_vs    = atoi(val);
    else if (strcmp(key,"track")==0)     ac_track = (int)atof(val);
    key[0] = 0; val[0] = 0;
  };

  unsigned long deadline = millis() + DIRECT_API_TIMEOUT;
  unsigned long lastWdt = millis();
  int c;
  while (millis() < deadline) {
    if (millis() - lastWdt > 5000) { esp_task_wdt_reset(); lastWdt = millis(); }
    if (!wifiOk()) { Serial.println("[DIRECT] WiFi lost during stream"); break; }
    if (!s->available()) { delay(5); continue; }
    c = s->read();
    if (c == -1) break;
    if (c == '"' && depth == 2) {
      readQuotedString(s, key, sizeof(key), deadline);
      while (s->available() && s->peek() != ':' && s->peek() != '"') s->read();
      if (s->peek() == ':') s->read();
      while (s->available() && (s->peek()==' '||s->peek()=='\t')) s->read();
      int nxt = s->peek();
      if (nxt == '"') {
        s->read();
        readQuotedString(s, val, sizeof(val), deadline);
        applyKV();
      } else if (nxt == '-' || (nxt >= '0' && nxt <= '9')) {
        readNumber(s, val, sizeof(val));
        applyKV();
      }
      key[0] = 0;
      continue;
    }

    if (c == '{') {
      depth++;
      if (depth == 2) {
        ac_callsign[0]=ac_reg[0]=ac_type[0]=0;
        ac_dep[0]=ac_arr[0]=ac_squawk[0]=ac_route[0]=0;
        ac_lat=ac_lon=0; ac_alt=ac_speed=ac_vs=0; ac_track=-1;
      }
    } else if (c == '}') {
       if (depth == 2) commitAircraft();
      if (depth > 0) depth--;
    }
  }

  http.end();
  Serial.printf("[MEM] Direct scan done: %d free, found %d\n", ESP.getFreeHeap(), newCount);
  sortFlightsByDist(newFlights, newCount);
  return newCount;
}

// ─── Extract flights from a parsed JSON doc ────────────
int extractFlights(DynamicJsonDocument& doc) {
  JsonArray ac = doc["ac"].as<JsonArray>();
  int newCount = 0;
  for (JsonObject a : ac) {
    if (newCount >= 20) break;
    float lat = a["lat"] | 0.0f;
    float lon = a["lon"] | 0.0f;
    int   alt = a["alt_baro"] | 0;
    if (alt < ALT_FLOOR_FT || lat == 0.0f) continue;
    float dist = haversineKm(HOME_LAT, HOME_LON, lat, lon);
    if (dist > GEOFENCE_KM) continue;

    Flight& f = newFlights[newCount];
    memset(&f, 0, sizeof(Flight));
    const char* cs = a["flight"] | "";
    strlcpy(f.callsign, cs, sizeof(f.callsign));
    for (int i = strlen(f.callsign)-1; i >= 0 && f.callsign[i] == ' '; i--) f.callsign[i] = 0;
    strlcpy(f.reg,    a["r"]      | "",     sizeof(f.reg));
    strlcpy(f.type,   a["t"]      | "",     sizeof(f.type));
    strlcpy(f.squawk, a["squawk"] | "----", sizeof(f.squawk));
    strlcpy(f.route,  a["route"]  | "",     sizeof(f.route));
    strlcpy(f.dep,    a["dep"]    | "",     sizeof(f.dep));
    strlcpy(f.arr,    a["arr"]    | "",     sizeof(f.arr));
    f.lat   = lat;
    f.lon = lon; f.alt = alt;
    f.speed = (int)(a["gs"]      | 0.0f);
    f.vs    = a["baro_rate"]     | 0;
    f.track = (int)(a["track"]   | -1.0f);
    f.dist  = dist;
    f.status = deriveStatus(alt, f.vs, dist);

    toUpperStr(f.callsign);
    toUpperStr(f.reg);
    toUpperStr(f.type);
    newCount++;
  }

  sortFlightsByDist(newFlights, newCount);
  return newCount;
}

// ─── Main fetch orchestrator ───────────────────────────
void fetchFlights() {
  logTs("FETCH", "Start (heap %d, maxblk %d)", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  isFetching = true;
  memset(newFlights, 0, sizeof(newFlights));

  int newCount = -1;
  bool fromCache = false;

  if (wifiOk()) {
    int maxBlock = ESP.getMaxAllocHeap();
    if (maxBlock >= 8000) {
      String payload = fetchFromProxy();
      if (!payload.isEmpty()) {
        writeCache(payload);
        esp_task_wdt_reset();
        newCount = parsePayload(payload);
        payload = String();
        dataSource = 0;
      }
    } else {
      logTs("FETCH", "WARN: heap fragmented (maxblk %d), skipping proxy", maxBlock);
    }
    if (newCount < 0) {
      logTs("FETCH", "Proxy empty/skipped, trying direct API...");
      esp_task_wdt_reset();
      newCount = fetchAndParseDirectAPI();
      if (newCount >= 0) dataSource = 1;
    }
  } else {
    logTs("FETCH", "WiFi not connected, skipping network");
  }

  if (newCount < 0) {
    logTs("FETCH", "Network failed, trying SD cache...");
    String payload = readCache();
    if (!payload.isEmpty()) {
      newCount = parsePayload(payload);
      payload = String();
      fromCache = true;
      dataSource = 2;
      Serial.println("Using cached data.");
    } else {
      isFetching = false;
      return;
    }
  }

  if (newCount < 0) {
    isFetching = false;
    return;
  }

  memcpy(flights, newFlights, sizeof(Flight) * newCount);
  flightCount = newCount;
  flightIndex = 0;
  isFetching  = false;
  usingCache  = fromCache;

  for (int i = 0; i < flightCount; i++) logFlight(flights[i]);
  esp_task_wdt_reset();
  logTs("HEAP", "Free:%d MaxBlock:%d", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

// ─── Fetch weather from Pi proxy (Open-Meteo) ─────────
bool fetchWeather() {
  esp_task_wdt_reset();
  if (!wifiOk()) { Serial.println("[WX] WiFi not connected"); return false; }
  char url[160];
  snprintf(url, sizeof(url),
    "https://%s/weather?lat=%.4f&lon=%.4f",
    PROXY_HOST, HOME_LAT, HOME_LON);

  for (int attempt = 1; attempt <= 2; attempt++) {
    if (attempt > 1) {
      Serial.println("[WX] Retrying...");
      delay(2000);
      esp_task_wdt_reset();
    }
    WiFiClientSecure tcp;
    tcp.setInsecure();
    if (!tcp.connect(PROXY_HOST, PROXY_PORT, 5000)) {
      Serial.printf("[WX] Connect failed (attempt %d/2)\n", attempt);
      continue;
    }
    esp_task_wdt_reset();
    HTTPClient http;
    http.begin(tcp, url);
    http.setTimeout(5000);
    int code = http.GET();
    esp_task_wdt_reset();
    if (code != 200) {
      Serial.printf("[WX] Fetch failed (%d) (attempt %d/2)\n", code, attempt);
      http.end();
      continue;
    }
    String body = http.getString();
    http.end();
    esp_task_wdt_reset();
    StaticJsonDocument<640> doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
      Serial.printf("[WX] JSON parse error (attempt %d/2)\n", attempt);
      continue;
    }
    wxData.temp            = doc["temp"]            | 0.0f;
    wxData.feels_like      = doc["feels_like"]      | 0.0f;
    wxData.humidity        = doc["humidity"]        | 0;
    wxData.wind_speed      = doc["wind_speed"]      | 0.0f;
    wxData.wind_dir        = doc["wind_dir"]        | 0;
    wxData.uv_index        = doc["uv_index"]        | 0.0f;
    wxData.utc_offset_secs = doc["utc_offset_secs"] | 0;
    const char* cond = doc["condition"] | "---";
    strlcpy(wxData.condition, cond, sizeof(wxData.condition));
    const char* wc = doc["wind_cardinal"] | "?";
    strlcpy(wxData.wind_cardinal, wc, sizeof(wxData.wind_cardinal));
    const char* td = doc["tide_dir"] | "";
    strlcpy(wxData.tide_dir, td, sizeof(wxData.tide_dir));
    const char* tt = doc["tide_time"] | "";
    strlcpy(wxData.tide_time, tt, sizeof(wxData.tide_time));
    wxData.tide_height  = doc["tide_height"] | 0.0f;
    wxData.tide_is_high = (strcmp(doc["tide_type"] | "LOW", "HIGH") == 0);
    wxReady = true;
    Serial.printf("[WX] %.1f C  %s  UV %.1f  TIDE %s %s\n",
      wxData.temp, wxData.condition, wxData.uv_index, wxData.tide_dir, wxData.tide_time);
    return true;
  }
  Serial.println("[WX] All attempts failed");
  return false;
}
