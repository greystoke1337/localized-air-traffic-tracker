// ─── Serial debug console ────────────────────────────

static void cmdHelp() {
  Serial.println("{\"cmd\":\"help\",\"commands\":[\"help\",\"heap\",\"state\",\"wifi\","
                 "\"config\",\"diag\",\"fetch\",\"weather\",\"unknowns\",\"restart\"]}");
}

static void cmdHeap() {
  uint32_t free   = ESP.getFreeHeap();
  uint32_t maxBlk = ESP.getMaxAllocHeap();
  uint32_t minFree = ESP.getMinFreeHeap();
  int frag = (int)(100 - (maxBlk * 100 / max((uint32_t)1, free)));
  Serial.printf("{\"cmd\":\"heap\",\"free\":%u,\"max_block\":%u,\"min_free\":%u,\"frag\":%d}\n",
    free, maxBlk, minFree, frag);
}

static void cmdState() {
  const char* scr = currentScreen == SCREEN_WEATHER ? "weather" : "flight";
  const char* src = dataSourceLabel();
  Serial.printf("{\"cmd\":\"state\",\"screen\":\"%s\",\"flights\":%d,\"flight_idx\":%d,"
                "\"countdown\":%d,\"wx_countdown\":%d,\"data_src\":\"%s\","
                "\"fetching\":%s,\"using_cache\":%s,\"sd\":%s,\"wx_ready\":%s,"
                "\"logged\":%d,\"uptime\":%lu,\"heap\":%u,\"max_block\":%u}\n",
    scr, flightCount, flightIndex, countdown, wxCountdown, src,
    isFetching ? "true" : "false",
    usingCache  ? "true" : "false",
#if HAS_SD
    sdAvailable ? "true" : "false",
#else
    "false",
#endif
    wxReady ? "true" : "false",
    loggedCount, millis() / 1000,
    ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

static void cmdWifi() {
  if (wifiOk()) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    Serial.printf("{\"cmd\":\"wifi\",\"connected\":true,\"ssid\":\"%s\",\"rssi\":%d,"
                  "\"ip\":\"%s\",\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"channel\":%d}\n",
      WIFI_SSID, WiFi.RSSI(), WiFi.localIP().toString().c_str(),
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], WiFi.channel());
  } else {
    Serial.println("{\"cmd\":\"wifi\",\"connected\":false}");
  }
}

static void cmdConfig() {
  Serial.printf("{\"cmd\":\"config\",\"home_lat\":%.4f,\"home_lon\":%.4f,"
                "\"geofence_km\":%.1f,\"alt_floor_ft\":%d,\"location\":\"%s\","
                "\"proxy_host\":\"%s\",\"proxy_port\":%d,"
                "\"refresh_secs\":%d,\"cycle_secs\":%d}\n",
    HOME_LAT, HOME_LON, GEOFENCE_KM, ALT_FLOOR_FT, LOCATION_NAME,
    PROXY_HOST, PROXY_PORT, REFRESH_SECS, CYCLE_SECS);
}

static void cmdDiag() {
  const char* src = dataSourceLabel();
  Serial.printf("{\"cmd\":\"diag\",\"heap\":%d,\"maxblk\":%d,\"wifi\":%d,\"rssi\":%d,"
                "\"src\":\"%s\",\"flights\":%d,\"uptime\":%lu,\"frag\":%d}\n",
    ESP.getFreeHeap(), ESP.getMaxAllocHeap(),
    wifiOk() ? 1 : 0, wifiOk() ? WiFi.RSSI() : 0,
    src, flightCount, millis() / 1000,
    (int)(100 - (ESP.getMaxAllocHeap() * 100 / max((uint32_t)1, (uint32_t)ESP.getFreeHeap()))));
}

static void cmdFetch() {
  if (isFetching) {
    Serial.println("{\"cmd\":\"fetch\",\"ok\":false,\"error\":\"fetch in progress\"}");
    return;
  }
  fetchFlights();
  countdown = REFRESH_SECS;
  const char* src = dataSourceLabel();
  Serial.printf("{\"cmd\":\"fetch\",\"ok\":true,\"flights\":%d,\"source\":\"%s\",\"heap\":%u}\n",
    flightCount, src, ESP.getFreeHeap());
}

static void cmdWeather() {
  fetchWeather();
  wxCountdown = WX_REFRESH_SECS;
  Serial.printf("{\"cmd\":\"weather\",\"ok\":true,\"wx_ready\":%s,\"temp\":%.1f,"
                "\"condition\":\"%s\",\"heap\":%u}\n",
    wxReady ? "true" : "false",
    wxData.temp, wxData.condition, ESP.getFreeHeap());
}

static void cmdUnknowns() {
#if HAS_SD
  if (!sdAvailable) {
    Serial.println("{\"cmd\":\"unknowns\",\"ok\":false,\"error\":\"no SD\"}");
    return;
  }
  Serial.printf("{\"cmd\":\"unknowns\",\"count\":%d}\n", loggedUnknownCount);
  File f = SD.open("/unknowns.csv", FILE_READ);
  if (f) { while (f.available()) Serial.println(f.readStringUntil('\n')); f.close(); }
#else
  Serial.println("{\"cmd\":\"unknowns\",\"ok\":false,\"error\":\"no SD\"}");
#endif
}

void checkSerialCmd() {
  String input = Serial.readStringUntil('\n');
  input.trim();
  input.toLowerCase();
  if (input.length() == 0) return;

  Serial.println(">>>CMD_START<<<");

  if      (input == "help")     cmdHelp();
  else if (input == "heap")     cmdHeap();
  else if (input == "state")    cmdState();
  else if (input == "wifi")     cmdWifi();
  else if (input == "config")   cmdConfig();
  else if (input == "diag")     cmdDiag();
  else if (input == "fetch")    cmdFetch();
  else if (input == "weather")  cmdWeather();
  else if (input == "unknowns") cmdUnknowns();
  else if (input == "restart") {
    Serial.println("{\"cmd\":\"restart\",\"ok\":true}");
    Serial.println(">>>CMD_END<<<");
    Serial.flush();
    delay(100);
    ESP.restart();
  } else {
    Serial.printf("{\"cmd\":\"error\",\"input\":\"%s\",\"message\":\"unknown command\"}\n",
                  input.c_str());
  }

  Serial.println(">>>CMD_END<<<");
}
