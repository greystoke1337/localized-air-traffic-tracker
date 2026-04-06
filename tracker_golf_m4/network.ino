// network.ino — HTTPS fetch from proxy + ArduinoJson parse

static float haversine(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371.0f;
  float dlat = (lat2 - lat1) * (float)DEG_TO_RAD;
  float dlon = (lon2 - lon1) * (float)DEG_TO_RAD;
  float a = sinf(dlat/2)*sinf(dlat/2)
          + cosf(lat1*(float)DEG_TO_RAD)*cosf(lat2*(float)DEG_TO_RAD)
          * sinf(dlon/2)*sinf(dlon/2);
  return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

bool fetchFlight(Flight &out) {
  WiFiSSLClient client;
  client.setTimeout(10000);

  if (!client.connect("api.overheadtracker.com", 443)) {
    Serial.println("[NET] Connect failed");
    return false;
  }

  // Build request
  char req[256];
  snprintf(req, sizeof(req),
    "GET /flights?lat=%.4f&lon=%.4f&radius=%d&min_altitude=%d HTTP/1.1\r\n"
    "Host: api.overheadtracker.com\r\n"
    "Connection: close\r\n"
    "\r\n",
    (float)HOME_LAT, (float)HOME_LON, GEOFENCE_KM, ALT_FLOOR_FT);
  client.print(req);

  // Skip HTTP headers (read until blank line)
  unsigned long deadline = millis() + 10000UL;
  while (client.connected() && millis() < deadline) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line == "") break;
  }

  if (millis() >= deadline) {
    client.stop();
    Serial.println("[NET] Timeout reading headers");
    return false;
  }

  // Stream-parse JSON with a filter to minimise RAM usage
  JsonDocument filter;
  JsonObject acf = filter["ac"].add<JsonObject>();
  acf["flight"]   = true;
  acf["alt_baro"] = true;
  acf["gs"]       = true;
  acf["route"]    = true;
  acf["t"]        = true;
  acf["lat"]      = true;
  acf["lon"]      = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, client, DeserializationOption::Filter(filter));
  client.stop();

  if (err) {
    Serial.print("[NET] JSON error: ");
    Serial.println(err.c_str());
    return false;
  }

  JsonArray ac = doc["ac"];
  if (ac.isNull()) {
    out.valid = false;
    strncpy(out.callsign, "------", sizeof(out.callsign));
    out.origin[0] = '\0';
    out.dest[0]   = '\0';
    out.type[0]   = '\0';
    out.alt = 0; out.speed = 0; out.dist = (float)GEOFENCE_KM;
    out.callsignColor = C_AMBER;
    out.typeColor     = C_AMBER;
    return true;
  }

  // Find the closest aircraft above the altitude floor
  Flight best;
  best.valid = false;
  float bestDist = 1e9f;

  for (JsonObject aircraft : ac) {
    int alt = aircraft["alt_baro"] | 0;
    if (alt < ALT_FLOOR_FT) continue;

    float lat = aircraft["lat"] | 0.0f;
    float lon = aircraft["lon"] | 0.0f;
    float d   = haversine(HOME_LAT, HOME_LON, lat, lon);
    if (d >= bestDist) continue;

    const char *cs    = aircraft["flight"] | "------";
    const char *route = aircraft["route"]  | "";
    const char *type  = aircraft["t"]      | "";
    int speed         = aircraft["gs"]     | 0;

    // Callsign — copy and trim trailing spaces
    strncpy(best.callsign, cs, sizeof(best.callsign) - 1);
    best.callsign[sizeof(best.callsign) - 1] = '\0';
    int len = strlen(best.callsign);
    while (len > 0 && best.callsign[len - 1] == ' ') best.callsign[--len] = '\0';

    // Route — split on " > "
    best.origin[0] = '\0';
    best.dest[0]   = '\0';
    const char *sep = strstr(route, " > ");
    if (sep) {
      size_t olen = sep - route;
      if (olen >= sizeof(best.origin)) olen = sizeof(best.origin) - 1;
      strncpy(best.origin, route, olen);
      best.origin[olen] = '\0';
      strncpy(best.dest, sep + 3, sizeof(best.dest) - 1);
      best.dest[sizeof(best.dest) - 1] = '\0';
    } else if (route[0]) {
      strncpy(best.origin, route, sizeof(best.origin) - 1);
      best.origin[sizeof(best.origin) - 1] = '\0';
    }

    resolveTypeName(type, best.type, sizeof(best.type));
    best.callsignColor = getAirlineColor(cs);
    best.typeColor     = getTypeColor(type);
    best.alt   = alt;
    best.speed = speed;
    best.dist  = d;
    best.valid = true;
    bestDist   = d;
  }

  if (best.valid) {
    out = best;
    Serial.printf("[NET] %s  %s > %s  alt=%d spd=%d dist=%.1fkm\n",
      out.callsign, out.origin, out.dest, out.alt, out.speed, bestDist);
    return true;
  }

  // No aircraft above altitude floor
  out.valid = false;
  strncpy(out.callsign, "------", sizeof(out.callsign));
  out.origin[0] = '\0';
  out.dest[0]   = '\0';
  out.type[0]   = '\0';
  out.alt = 0; out.speed = 0; out.dist = (float)GEOFENCE_KM;
  out.callsignColor = C_AMBER;
  out.typeColor     = C_AMBER;
  Serial.println("[NET] No aircraft above floor");
  return true;
}

bool fetchWeather(Weather &out) {
  WiFiSSLClient client;
  client.setTimeout(10000);

  if (!client.connect("api.overheadtracker.com", 443)) {
    Serial.println("[WEATHER] Connect failed");
    return false;
  }

  char req[192];
  snprintf(req, sizeof(req),
    "GET /weather?lat=%.4f&lon=%.4f HTTP/1.1\r\n"
    "Host: api.overheadtracker.com\r\n"
    "Connection: close\r\n"
    "\r\n",
    (float)HOME_LAT, (float)HOME_LON);
  client.print(req);

  unsigned long deadline = millis() + 10000UL;
  while (client.connected() && millis() < deadline) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line == "") break;
  }

  if (millis() >= deadline) {
    client.stop();
    Serial.println("[WEATHER] Timeout reading headers");
    return false;
  }

  JsonDocument filter;
  filter["temp"]         = true;
  filter["weather_code"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, client, DeserializationOption::Filter(filter));
  client.stop();

  if (err) {
    Serial.print("[WEATHER] JSON error: ");
    Serial.println(err.c_str());
    return false;
  }

  out.tempC       = doc["temp"] | 0.0f;
  out.weatherCode = doc["weather_code"] | 0;
  out.valid       = true;
  Serial.printf("[WEATHER] %.1f°C code=%d\n", out.tempC, out.weatherCode);
  return true;
}
