// ─── Network: HTTP fetching and JSON parsing ─────────

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

// ─── Generic HTTPS GET to proxy ───────────────────
static String proxyGet(const char* path) {
  if (WiFi.status() != WL_CONNECTED) return "";
  char url[256];
  snprintf(url, sizeof(url), "https://%s%s", PROXY_HOST, path);
  WiFiClientSecure tcp;
  tcp.setInsecure();
  tcp.setHandshakeTimeout(8);
  if (!tcp.connect(PROXY_HOST, PROXY_PORT, 5000)) return "";
  HTTPClient http;
  http.begin(tcp, url);
  http.setTimeout(8000);
  int code = http.GET();
  String body = (code == 200) ? http.getString() : "";
  http.end();
  return body;
}

// ─── Helpers ──────────────────────────────────────
bool wifiOk() { return WiFi.status() == WL_CONNECTED; }

// ─── Extract flights from JSON doc ────────────────
static int extractFlights(DynamicJsonDocument& doc) {
  JsonArray ac = doc["ac"].as<JsonArray>();
  int n = 0;
  for (JsonObject a : ac) {
    if (n >= 20) break;
    float lat = a["lat"] | 0.0f;
    float lon = a["lon"] | 0.0f;
    int   alt = a["alt_baro"] | 0;
    if (alt < ALT_FLOOR_FT || lat == 0.0f) continue;
    float dist = haversineMi(HOME_LAT, HOME_LON, lat, lon);
    if (dist > GEOFENCE_MI) continue;

    Flight& f = newFlights[n];
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
    f.speed = (int)(a["gs"] | 0.0f);
    f.vs    = a["baro_rate"] | 0;
    f.track = (int)(a["track"] | -1.0f);
    f.dist  = dist;
    f.status = deriveStatus(alt, f.vs, dist);
    toUpperStr(f.callsign);
    toUpperStr(f.reg);
    toUpperStr(f.type);
    n++;
  }
  sortFlightsByDist(newFlights, n);
  return n;
}

// ─── fetchFlights ─────────────────────────────────
void fetchFlights() {
  esp_task_wdt_delete(NULL);
  isFetching = true;
  memset(newFlights, 0, sizeof(newFlights));

  char path[128];
  snprintf(path, sizeof(path), "/flights?lat=%.4f&lon=%.4f&radius=%d",
    HOME_LAT, HOME_LON, apiRadiusNm());
  String payload = proxyGet(path);

  int newCount = -1;
  if (!payload.isEmpty()) {
    StaticJsonDocument<512> filter;
    JsonObject af = filter["ac"].createNestedObject();
    af["flight"] = af["r"] = af["t"] = af["lat"] = af["lon"] =
    af["alt_baro"] = af["gs"] = af["baro_rate"] = af["track"] =
    af["squawk"] = af["route"] = af["dep"] = af["arr"] = true;
    g_jsonDoc.clear();
    DeserializationError err = deserializeJson(g_jsonDoc, &payload[0], payload.length(),
                                               DeserializationOption::Filter(filter));
    if (!err) {
      newCount = extractFlights(g_jsonDoc);
      g_jsonDoc.clear();
    }
    payload = String();
    dataSource = 0;
  }

  if (newCount >= 0) {
    memcpy(flights, newFlights, sizeof(Flight) * newCount);
    flightCount = newCount;
    for (int i = 0; i < flightCount; i++) logFlight(flights[i]);
    lastFlightDataMs = millis();
  }

  isFetching = false;
  esp_task_wdt_add(NULL);
  esp_task_wdt_reset();
}

// ─── fetchStats (proxy stats + peak histogram) ────
void fetchStats() {
  esp_task_wdt_delete(NULL);

  // /stats
  String body = proxyGet("/stats");
  if (!body.isEmpty()) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
      strlcpy(proxyStats.uptime, doc["uptime"] | "---", sizeof(proxyStats.uptime));
      proxyStats.requests  = doc["requests"]  | 0;
      proxyStats.cacheHit  = doc["cacheHit"]  | 0.0f;
      proxyStats.errors    = doc["errors"]    | 0;
      proxyStats.clients   = doc["clients"]   | 0;
      proxyStats.cached    = doc["cached"]    | 0;
      proxyStats.routes    = doc["routes"]    | 0;
      proxyStats.newRoutes = doc["newRoutes"] | 0;

      // ADS-B source status flags (proxy reports these)
      proxyStats.adsbLolUp        = doc["adsbLolUp"]        | false;
      proxyStats.adsbFiUp         = doc["adsbFiUp"]         | false;
      proxyStats.airplanesLiveUp  = doc["airplanesLiveUp"]  | false;
      proxyStats.adsbOneUp        = doc["adsbOneUp"]        | false;
      proxyStatsReady = true;
    }
  }

  // /peak
  body = proxyGet("/peak");
  if (!body.isEmpty()) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
      JsonArray arr = doc["peak"].as<JsonArray>();
      for (int i = 0; i < 24 && i < (int)arr.size(); i++) peakHours[i] = arr[i] | 0;
    }
  }

  lastStatsMs = millis();
  esp_task_wdt_add(NULL);
  esp_task_wdt_reset();
}

// ─── fetchWeather ─────────────────────────────────
void fetchWeather() {
  esp_task_wdt_delete(NULL);
  char path[80];
  snprintf(path, sizeof(path), "/weather?lat=%.4f&lon=%.4f", HOME_LAT, HOME_LON);
  String body = proxyGet(path);
  if (!body.isEmpty()) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
      wxData.temp       = doc["temp"]       | 0.0f;
      wxData.feels_like = doc["feels_like"] | 0.0f;
      wxData.humidity   = doc["humidity"]   | 0;
      wxData.wind_speed = doc["wind_speed"] | 0.0f;
      wxData.wind_dir   = doc["wind_dir"]   | 0;
      wxData.uv_index   = doc["uv_index"]   | 0.0f;
      strlcpy(wxData.condition,    doc["condition"]    | "---", sizeof(wxData.condition));
      strlcpy(wxData.wind_cardinal,doc["wind_cardinal"]| "?",   sizeof(wxData.wind_cardinal));
      wxReady = true;
    }
  }
  esp_task_wdt_add(NULL);
  esp_task_wdt_reset();
}

// ─── fetchServerStatus ────────────────────────────
void fetchServerStatus() {
  esp_task_wdt_delete(NULL);
  String body = proxyGet("/status");
  if (!body.isEmpty()) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
      strlcpy(serverStatus.osUptime, doc["uptime"] | "---", sizeof(serverStatus.osUptime));
      serverStatus.cpuTemp = doc["cpuTemp"] | 0.0f;
      serverStatus.ramPct  = doc["ram"]     | 0.0f;
      serverStatus.load1   = doc["load1"]   | 0.0f;
      serverStatus.load5   = doc["load5"]   | 0.0f;
      serverStatus.load15  = doc["load15"]  | 0.0f;
      serverReady = true;
    }
  }
  esp_task_wdt_add(NULL);
  esp_task_wdt_reset();
}

// ─── fetchDeviceStatus ────────────────────────────
void fetchDeviceStatus(const char* codename, DeviceStatus* dev) {
  esp_task_wdt_delete(NULL);
  char path[48];
  snprintf(path, sizeof(path), "/device/%s/status", codename);
  String body = proxyGet(path);
  if (!body.isEmpty()) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
      strlcpy(dev->fw, doc["fw"] | "---", sizeof(dev->fw));
      dev->heap        = doc["heap"]   | 0;
      dev->rssi        = doc["rssi"]   | 0;
      dev->uptimeSecs  = doc["uptime"] | 0UL;
      dev->lastSeenMs  = millis();
      dev->online      = true;
    }
  } else {
    if (dev->online && millis() - dev->lastSeenMs > 90000) dev->online = false;
  }
  esp_task_wdt_add(NULL);
  esp_task_wdt_reset();
}

// ─── sendHeartbeat ────────────────────────────────
void sendHeartbeat() {
  esp_task_wdt_delete(NULL);
  if (!wifiOk()) { esp_task_wdt_add(NULL); esp_task_wdt_reset(); return; }
  char body[256];
  snprintf(body, sizeof(body),
    "{\"device\":\"delta\",\"fw\":\"%s\",\"heap\":%d,\"uptime\":%lu,"
    "\"rssi\":%d,\"flights\":%d,\"source\":%d,\"location\":\"%s\"}",
    FW_VERSION, ESP.getFreeHeap(), millis() / 1000,
    WiFi.RSSI(), flightCount, dataSource, LOCATION_NAME);

  char url[80];
  snprintf(url, sizeof(url), "https://%s/device/heartbeat", PROXY_HOST);
  WiFiClientSecure tcp;
  tcp.setInsecure();
  tcp.setHandshakeTimeout(5);
  if (!tcp.connect(PROXY_HOST, PROXY_PORT, 3000)) {
    esp_task_wdt_add(NULL); esp_task_wdt_reset(); return;
  }
  HTTPClient http;
  http.begin(tcp, url);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");
  http.POST(body);
  http.end();
  esp_task_wdt_add(NULL);
  esp_task_wdt_reset();
}
