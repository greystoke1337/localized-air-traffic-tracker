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

// ─── Static buffer for proxy payload (avoids heap alloc) ─
static char s_proxyBuf[PROXY_BUF_SIZE];

// ─── Parse a char-array payload (proxy or cache) ─────────
static int parsePayload(const char* payload, int len) {
  StaticJsonDocument<512> filter;
  JsonObject af = filter["ac"].createNestedObject();
  af["flight"] = af["r"] = af["t"] = af["lat"] = af["lon"] =
  af["alt_baro"] = af["gs"] = af["baro_rate"] = af["track"] =
  af["squawk"] = af["route"] = af["dep"] = af["arr"] = true;
  g_jsonDoc.clear();
  esp_task_wdt_reset();
  Serial.printf("[MEM] Before JSON parse: %d free\n", ESP.getFreeHeap());
  DeserializationError err = deserializeJson(g_jsonDoc, payload, len,
                                             DeserializationOption::Filter(filter));
  Serial.printf("[MEM] After JSON parse: %d free\n", ESP.getFreeHeap());
  esp_task_wdt_reset();
  if (err) { Serial.printf("JSON parse error: %s\n", err.c_str()); return -1; }
  int result = extractFlights(g_jsonDoc);
  g_jsonDoc.clear();
  return result;
}

// ─── Fetch: fill buf from proxy, return true on success ──
static bool fetchFromProxy(char* buf, int maxLen) {
  if (!wifiOk()) { Serial.println("[PROXY] WiFi not connected"); return false; }
  unsigned long t0 = millis();
  WiFiClientSecure tcp;
  tcp.setInsecure();
  tcp.setTimeout(8000);
  esp_task_wdt_reset();
  if (!tcp.connect(PROXY_HOST, PROXY_PORT, 5000)) {
    Serial.printf("[PROXY] Connect failed (%lu ms)\n", millis() - t0);
    return false;
  }
  esp_task_wdt_reset();
  char url[160];
  snprintf(url, sizeof(url), "https://%s/flights?lat=%.4f&lon=%.4f&radius=%d",
           PROXY_HOST, HOME_LAT, HOME_LON, apiRadiusNm());
  HTTPClient http;
  http.begin(tcp, url);
  http.setTimeout(12000);
  int code = http.GET();
  esp_task_wdt_reset();
  int pos = 0;
  if (code == 200) {
    WiFiClient* stream = http.getStreamPtr();
    unsigned long deadline = millis() + 20000;
    while (millis() < deadline && (http.connected() || stream->available())
           && pos < maxLen - 1) {
      int avail = stream->available();
      if (avail > 0) {
        int n = stream->readBytes((uint8_t*)(buf + pos), min(avail, maxLen - pos - 1));
        if (n > 0) { pos += n; deadline = millis() + 20000; }
      } else { delay(5); }
      esp_task_wdt_reset();
    }
    buf[pos] = 0;
  }
  http.end();
  esp_task_wdt_reset();
  if (pos > 0) { Serial.printf("[PROXY] OK %d bytes (%lu ms)\n", pos, millis() - t0); return true; }
  Serial.printf("[PROXY] HTTP %d (%lu ms)\n", code, millis() - t0);
  return false;
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

// ─── Apply one key-value pair to aircraft accumulator ────
static void applyKV(AircraftState& ac, const char* key, const char* val) {
  if      (strcmp(key,"flight")==0)               strlcpy(ac.callsign, val, sizeof(ac.callsign));
  else if (strcmp(key,"r")==0)                    strlcpy(ac.reg,      val, sizeof(ac.reg));
  else if (strcmp(key,"t")==0)                    strlcpy(ac.type,     val, sizeof(ac.type));
  else if (strcmp(key,"squawk")==0)               strlcpy(ac.squawk,   val, sizeof(ac.squawk));
  else if (strcmp(key,"dep")==0)                  strlcpy(ac.dep,      val, sizeof(ac.dep));
  else if (strcmp(key,"arr")==0)                  strlcpy(ac.arr,      val, sizeof(ac.arr));
  else if (strcmp(key,"orig_iata")==0 && !ac.dep[0]) strlcpy(ac.dep,  val, sizeof(ac.dep));
  else if (strcmp(key,"dest_iata")==0 && !ac.arr[0]) strlcpy(ac.arr,  val, sizeof(ac.arr));
  else if (strcmp(key,"lat")==0)       ac.lat   = atof(val);
  else if (strcmp(key,"lon")==0)       ac.lon   = atof(val);
  else if (strcmp(key,"alt_baro")==0)  ac.alt   = atoi(val);
  else if (strcmp(key,"gs")==0)        ac.speed = (int)atof(val);
  else if (strcmp(key,"baro_rate")==0) ac.vs    = atoi(val);
  else if (strcmp(key,"track")==0)     ac.track = (int)atof(val);
}

// ─── Validate accumulator and push to newFlights[] ───────
static void commitAircraft(AircraftState& ac, int& newCount) {
  if (newCount >= 20) return;
  if (ac.alt < ALT_FLOOR_FT || ac.lat == 0.0f) return;
  float dist = haversineKm(HOME_LAT, HOME_LON, ac.lat, ac.lon);
  if (dist > GEOFENCE_KM) return;
  Flight& f = newFlights[newCount];
  memset(&f, 0, sizeof(Flight));
  strlcpy(f.callsign, ac.callsign, sizeof(f.callsign));
  for (int i = strlen(f.callsign)-1; i >= 0 && f.callsign[i] == ' '; i--) f.callsign[i] = 0;
  strlcpy(f.reg,    ac.reg,    sizeof(f.reg));
  strlcpy(f.type,   ac.type,   sizeof(f.type));
  strlcpy(f.squawk, ac.squawk[0] ? ac.squawk : "----", sizeof(f.squawk));
  strlcpy(f.dep, ac.dep, sizeof(f.dep));
  strlcpy(f.arr, ac.arr, sizeof(f.arr));
  if (ac.dep[0] || ac.arr[0]) {
    snprintf(f.route, sizeof(f.route), "%s > %s",
             ac.dep[0] ? ac.dep : "?", ac.arr[0] ? ac.arr : "?");
  }
  f.lat=ac.lat; f.lon=ac.lon; f.alt=ac.alt;
  f.speed=ac.speed; f.vs=ac.vs; f.track=ac.track; f.dist=dist;
  f.status = deriveStatus(ac.alt, ac.vs, dist);
  toUpperStr(f.callsign);
  toUpperStr(f.reg);
  toUpperStr(f.type);
  newCount++;
}

// ─── Try each direct API endpoint; return true on 200 ────
static bool connectDirectAPI(HTTPClient& http, WiFiClientSecure& tlsClient) {
  int radius = apiRadiusNm();
  const char* apiNames[] = { "adsb.lol", "airplanes.live" };
  char urls[2][160];
  snprintf(urls[0], sizeof(urls[0]),
           "https://api.adsb.lol/v2/point/%.4f/%.4f/%d", HOME_LAT, HOME_LON, radius);
  snprintf(urls[1], sizeof(urls[1]),
           "https://api.airplanes.live/v2/point/%.4f/%.4f/%d", HOME_LAT, HOME_LON, radius);
  for (int i = 0; i < 2; i++) {
    Serial.printf("[DIRECT] Trying %s...\n", apiNames[i]);
    http.begin(tlsClient, urls[i]);
    http.setTimeout(DIRECT_API_TIMEOUT);
    int code = http.GET();
    if (code == 200) { Serial.printf("[DIRECT] %s OK\n", apiNames[i]); return true; }
    http.end();
    Serial.printf("[DIRECT] %s failed (%d)\n", apiNames[i], code);
  }
  return false;
}

// ─── Fetch: direct API — zero-allocation stream scanner ──
int fetchAndParseDirectAPI() {
  if (!wifiOk()) { Serial.println("[DIRECT] WiFi not connected"); return -1; }
  int freeHeap = ESP.getFreeHeap();
  if (freeHeap < DIRECT_API_MIN_HEAP) {
    Serial.printf("[DIRECT] Insufficient heap (%d), skipping\n", freeHeap);
    return -1;
  }
  if (directApiFailCount > 0 && millis() < directApiNextRetryMs) {
    Serial.printf("[DIRECT] Backoff active, %lu ms remaining\n",
                  directApiNextRetryMs - millis());
    return -1;
  }
  HTTPClient http;
  WiFiClientSecure tlsClient;
  tlsClient.setCACert(LETSENCRYPT_ROOT_CA);
  tlsClient.setTimeout(DIRECT_API_TIMEOUT / 1000);
  if (!connectDirectAPI(http, tlsClient)) {
    directApiFailCount++;
    unsigned long backoffMs = min(120000UL, 15000UL * (1UL << min(directApiFailCount - 1, 3)));
    directApiNextRetryMs = millis() + backoffMs;
    Serial.printf("[DIRECT] All APIs failed, backoff %lu ms (fail #%d)\n",
                  backoffMs, directApiFailCount);
    return -1;
  }
  directApiFailCount = 0;
  Serial.printf("[MEM] Direct stream scan start: %d free\n", ESP.getFreeHeap());

  WiFiClient* s = http.getStreamPtr();
  if (!s) { Serial.println("[DIRECT] Null stream"); http.end(); return -1; }

  int newCount = 0;
  int depth = 0;
  AircraftState ac = {};
  char key[16] = "", val[32] = "";
  unsigned long deadline = millis() + DIRECT_API_TIMEOUT;
  unsigned long lastWdt  = millis();
  while (millis() < deadline) {
    if (millis() - lastWdt > 5000) { esp_task_wdt_reset(); lastWdt = millis(); }
    if (!wifiOk()) { Serial.println("[DIRECT] WiFi lost during stream"); break; }
    if (!s->available()) { delay(5); continue; }
    int c = s->read();
    if (c == -1) break;
    if (c == '"' && depth == 2) {
      readQuotedString(s, key, sizeof(key), deadline);
      while (s->available() && s->peek() != ':' && s->peek() != '"') s->read();
      if (s->peek() == ':') s->read();
      while (s->available() && (s->peek()==' '||s->peek()=='\t')) s->read();
      int nxt = s->peek();
      if (nxt == '"') {
        s->read(); readQuotedString(s, val, sizeof(val), deadline); applyKV(ac, key, val);
      } else if (nxt == '-' || (nxt >= '0' && nxt <= '9')) {
        readNumber(s, val, sizeof(val)); applyKV(ac, key, val);
      }
      key[0] = 0;
      continue;
    }
    if (c == '{') {
      depth++;
      if (depth == 2) { ac = {}; }
    } else if (c == '}') {
      if (depth == 2) commitAircraft(ac, newCount);
      if (depth > 0) depth--;
    }
  }
  http.end();
  Serial.printf("[MEM] Direct scan done: %d free, found %d\n", ESP.getFreeHeap(), newCount);
  sortFlightsByDist(newFlights, newCount);
  return newCount;
}

// ─── Parse track JSON doc into flight state ───────────────
static void parseTrackResponse(StaticJsonDocument<512>& doc) {
  if (doc["callsign"].isNull()) {
    if (trackingMode) {
      trackingMode = false;
      trackCallsign[0] = 0;
      trackProgress = -1.0f;
      trackTerritory[0] = 0;
      fetchFlights();
    }
    return;
  }
  strlcpy(trackCallsign, doc["callsign"] | "", sizeof(trackCallsign));
  trackingMode = true;
  if (doc.containsKey("error")) {
    strlcpy(trackTerritory, doc["territory"] | "", sizeof(trackTerritory));
    Serial.printf("[TRACK] %s not found\n", trackCallsign);
    return;
  }
  Flight& f = flights[0];
  memset(&f, 0, sizeof(Flight));
  strlcpy(f.callsign, doc["callsign"] | "", sizeof(f.callsign));
  strlcpy(f.reg,      doc["reg"]      | "", sizeof(f.reg));
  strlcpy(f.type,     doc["type"]     | "", sizeof(f.type));
  strlcpy(f.squawk,   doc["squawk"]   | "----", sizeof(f.squawk));
  strlcpy(f.dep,      doc["dep"]      | "", sizeof(f.dep));
  strlcpy(f.arr,      doc["arr"]      | "", sizeof(f.arr));
  strlcpy(f.route,    doc["route"]    | "", sizeof(f.route));
  f.lat   = doc["lat"]       | 0.0f;
  f.lon   = doc["lon"]       | 0.0f;
  f.alt   = doc["alt_baro"]  | 0;
  f.speed = doc["gs"]        | 0;
  f.vs    = doc["baro_rate"] | 0;
  f.track = doc["track"]     | -1;
  f.dist  = haversineKm(HOME_LAT, HOME_LON, f.lat, f.lon);
  f.status = deriveStatus(f.alt, f.vs, f.dist);
  toUpperStr(f.callsign); toUpperStr(f.reg); toUpperStr(f.type);
  flightCount = 1; flightIndex = 0;
  trackProgress = doc["progress"].isNull() ? -1.0f : (float)doc["progress"];
  strlcpy(trackTerritory, doc["territory"] | "", sizeof(trackTerritory));
  renderTrackFlight(flights[0]);
}

// ─── Fetch tracking mode status from proxy ───────────────
void fetchTrackStatus() {
  if (!wifiOk()) return;
  unsigned long t0 = millis();
  WiFiClientSecure tcp;
  tcp.setInsecure();
  tcp.setTimeout(8000);
  esp_task_wdt_reset();
  char url[80];
  snprintf(url, sizeof(url), "https://%s/track", PROXY_HOST);
  HTTPClient http;
  http.begin(tcp, url);
  http.setTimeout(8000);
  int code = http.GET();
  esp_task_wdt_reset();
  if (code != 200) {
    Serial.printf("[TRACK] HTTP %d\n", code);
    http.end();
    return;
  }
  char body[512] = {0};
  int bpos = 0;
  WiFiClient* stream = http.getStreamPtr();
  unsigned long dl = millis() + 8000;
  while (millis() < dl && (http.connected() || stream->available())
         && bpos < (int)sizeof(body) - 1) {
    int avail = stream->available();
    if (avail > 0) {
      int n = stream->readBytes((uint8_t*)(body + bpos),
                                min(avail, (int)sizeof(body) - bpos - 1));
      bpos += n;
    } else { delay(5); }
    esp_task_wdt_reset();
  }
  http.end();
  esp_task_wdt_reset();
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    Serial.println("[TRACK] JSON parse error");
    return;
  }
  parseTrackResponse(doc);
  Serial.printf("[TRACK] Done (%lu ms)\n", millis() - t0);
}

// ─── Send heartbeat to proxy ─────────────────────────────
void sendHeartbeat() {
  if (!wifiOk()) return;
  WiFiClientSecure tcp;
  tcp.setInsecure();
  tcp.setTimeout(8000);
  esp_task_wdt_reset();
  if (!tcp.connect(PROXY_HOST, PROXY_PORT, 5000)) {
    Serial.println("[HB] Connect failed");
    return;
  }
  esp_task_wdt_reset();
  char body[256];
  snprintf(body, sizeof(body),
    "{\"device\":\"echo\",\"fw\":\"%s\",\"heap\":%d,\"uptime\":%lu,"
    "\"rssi\":%d,\"flights\":%d,\"source\":%d,\"location\":\"%s\"}",
    FW_VERSION, ESP.getFreeHeap(), millis() / 1000,
    WiFi.RSSI(), flightCount, dataSource, LOCATION_NAME);
  char url[120];
  snprintf(url, sizeof(url), "https://%s/device/heartbeat", PROXY_HOST);
  HTTPClient http;
  http.begin(tcp, url);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();
  esp_task_wdt_reset();
  if (code != 200) { Serial.printf("[HB] Unexpected response: %d\n", code); }
  Serial.printf("[HB] Heartbeat sent (%d)\n", code);
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
    f.lat   = lat; f.lon = lon; f.alt = alt;
    f.speed = (int)(a["gs"]       | 0.0f);
    f.vs    = a["baro_rate"]      | 0;
    f.track = (int)(a["track"]    | -1.0f);
    f.dist  = dist;
    f.status = deriveStatus(alt, f.vs, dist);
    toUpperStr(f.callsign); toUpperStr(f.reg); toUpperStr(f.type);
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
      if (fetchFromProxy(s_proxyBuf, PROXY_BUF_SIZE)) {
        writeCache(s_proxyBuf);
        esp_task_wdt_reset();
        newCount = parsePayload(s_proxyBuf, strlen(s_proxyBuf));
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
    String cached = readCache();
    if (!cached.isEmpty()) {
      newCount = parsePayload(cached.c_str(), cached.length());
      fromCache = true;
      dataSource = 2;
      Serial.println("Using cached data.");
    } else {
      renderMessage("NO DATA", "ALL SOURCES FAILED");
      isFetching = false;
      return;
    }
  }

  if (newCount < 0) {
    renderMessage("JSON ERROR", fromCache ? "CACHE CORRUPT" : "BAD RESPONSE");
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
  if (flightCount == 0) {
    logTs("FETCH", "No flights, switching to weather");
    currentScreen = SCREEN_WEATHER;
    renderWeather();
  } else {
    currentScreen = SCREEN_FLIGHT;
    renderFlight(flights[0]);
  }
  logTs("HEAP", "Free:%d MaxBlock:%d", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

// ─── Parse weather JSON body into wxData ─────────────────
static bool parseWeatherBody(const char* body) {
  StaticJsonDocument<640> doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return false;
  wxData.temp            = doc["temp"]            | 0.0f;
  wxData.feels_like      = doc["feels_like"]      | 0.0f;
  wxData.humidity        = doc["humidity"]        | 0;
  wxData.wind_speed      = doc["wind_speed"]      | 0.0f;
  wxData.wind_dir        = doc["wind_dir"]        | 0;
  wxData.uv_index        = doc["uv_index"]        | 0.0f;
  wxData.utc_offset_secs = doc["utc_offset_secs"] | 0;
  strlcpy(wxData.condition,     doc["condition"]     | "---", sizeof(wxData.condition));
  strlcpy(wxData.wind_cardinal, doc["wind_cardinal"] | "?",   sizeof(wxData.wind_cardinal));
  strlcpy(wxData.tide_dir,      doc["tide_dir"]      | "",    sizeof(wxData.tide_dir));
  strlcpy(wxData.tide_time,     doc["tide_time"]     | "",    sizeof(wxData.tide_time));
  wxData.tide_height  = doc["tide_height"] | 0.0f;
  wxData.tide_is_high = (strcmp(doc["tide_type"] | "LOW", "HIGH") == 0);
  wxReady = true;
  Serial.printf("[WX] %.1f C  %s  UV %.1f  TIDE %s %s\n",
    wxData.temp, wxData.condition, wxData.uv_index, wxData.tide_dir, wxData.tide_time);
  return true;
}

// ─── Fetch weather from proxy (Open-Meteo) ───────────────
bool fetchWeather() {
  esp_task_wdt_reset();
  if (!wifiOk()) { Serial.println("[WX] WiFi not connected"); return false; }
  char url[160];
  snprintf(url, sizeof(url), "https://%s/weather?lat=%.4f&lon=%.4f",
           PROXY_HOST, HOME_LAT, HOME_LON);

  for (int attempt = 1; attempt <= 2; attempt++) {
    if (attempt > 1) { Serial.println("[WX] Retrying..."); delay(2000); esp_task_wdt_reset(); }
    WiFiClientSecure tcp;
    tcp.setInsecure();
    tcp.setTimeout(8000);
    esp_task_wdt_reset();
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
    char body[768] = {0};
    int bpos = 0;
    WiFiClient* stream = http.getStreamPtr();
    unsigned long dl = millis() + 10000;
    while (millis() < dl && (http.connected() || stream->available())
           && bpos < (int)sizeof(body) - 1) {
      int avail = stream->available();
      if (avail > 0) {
        int n = stream->readBytes((uint8_t*)(body + bpos),
                                  min(avail, (int)sizeof(body) - bpos - 1));
        if (n > 0) { bpos += n; dl = millis() + 10000; }
      } else { delay(5); }
      esp_task_wdt_reset();
    }
    http.end();
    esp_task_wdt_reset();
    if (parseWeatherBody(body)) return true;
    Serial.printf("[WX] JSON parse error (attempt %d/2)\n", attempt);
  }
  Serial.println("[WX] All attempts failed");
  return false;
}
