// ─── SD card operations: config, cache, flight log ─────

void loadConfig() {
  if (!sdAvailable) return;
  File f = SD.open("/config.txt", FILE_READ);
  if (!f) {
    Serial.println("No config.txt — using defaults");
    return;
  }
  Serial.println("Reading config.txt...");
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.startsWith("#") || line.length() == 0) continue;
    int eq = line.indexOf('=');
    if (eq < 0) continue;
    String key = line.substring(0, eq);
    String val = line.substring(eq + 1);
    key.trim(); val.trim();
    if      (key == "lat")       HOME_LAT     = val.toFloat();
    else if (key == "lon")       HOME_LON     = val.toFloat();
    else if (key == "geofence")  GEOFENCE_KM  = val.toFloat();
    else if (key == "alt_floor") ALT_FLOOR_FT = val.toInt();
    else if (key == "name") {
      val.toUpperCase();
      strlcpy(LOCATION_NAME, val.c_str(), sizeof(LOCATION_NAME));
    }
    Serial.printf("  %s = %s\n", key.c_str(), val.c_str());
  }
  f.close();
  Serial.println("Config loaded.");
}

void writeCache(const String& payload) {
  if (!sdAvailable) return;
  File f = SD.open("/cache.json", FILE_WRITE);
  if (!f) { Serial.println("Cache write failed"); return; }
  f.print(payload);
  f.close();
  time_t now = time(NULL);
  if (now > 1000000000) {  // only write if NTP has synced (epoch > ~2001)
    File tf = SD.open("/cache_ts.txt", FILE_WRITE);
    if (tf) { tf.print((unsigned long)now); tf.close(); }
    cacheTimestamp = now;
  }
  Serial.printf("Cache written (%d bytes)\n", payload.length());
}

String readCache() {
  if (!sdAvailable) return "";
  File f = SD.open("/cache.json", FILE_READ);
  if (!f) return "";
  String payload = f.readString();
  f.close();
  File tf = SD.open("/cache_ts.txt", FILE_READ);
  if (tf) {
    cacheTimestamp = (time_t)tf.readString().toInt();
    tf.close();
  }
  Serial.printf("Cache loaded (%d bytes, ts=%lu)\n", payload.length(), (unsigned long)cacheTimestamp);
  return payload;
}

void logFlight(const Flight& f) {
  if (!sdAvailable) return;
  if (!f.callsign[0] || alreadyLogged(f.callsign)) return;
  bool isNew = !SD.exists("/flightlog.csv");
  File file = SD.open("/flightlog.csv", FILE_APPEND);
  if (!file) return;

  if (isNew) {
    file.println("callsign,reg,type,airline,route,status,dist_km");
  }

  char row[128];
  snprintf(row, sizeof(row), "%s,%s,%s,%s,%s,%s,%.1f",
    f.callsign,
    f.reg,
    f.type,
    getAirline(f.callsign),
    f.route[0] ? f.route : "",
    statusLabel(f.status),
    f.dist
  );
  file.println(row);
  file.close();

  if (loggedCount < MAX_LOGGED) {
    strlcpy(loggedCallsigns[loggedCount++], f.callsign, 12);
  }
  Serial.printf("Logged: %s\n", f.callsign);
}
