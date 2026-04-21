// ─── Serial debug console ─────────────────────────────────────────────────────

void logTs(const char *tag, const char *fmt, ...) {
  char buf[160];
  unsigned long ms = millis();
  int n = snprintf(buf, sizeof(buf), "[%lu.%03lu][%s] ", ms / 1000, ms % 1000, tag);
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
  va_end(args);
  Serial.println(buf);
}

static void cmdHelp() {
  Serial.println("{\"cmd\":\"help\",\"commands\":[\"help\",\"heap\",\"state\",\"wifi\","
                 "\"config\",\"diag\",\"fetch\",\"restart\"]}");
}

static void cmdHeap() {
  uint32_t free_bytes = ESP.getFreeHeap();
  uint32_t max_blk    = ESP.getMaxAllocHeap();
  uint32_t min_free   = ESP.getMinFreeHeap();
  int frag = (int)(100 - (max_blk * 100 / max((uint32_t)1, free_bytes)));
  Serial.printf("{\"cmd\":\"heap\",\"free\":%u,\"max_block\":%u,\"min_free\":%u,\"frag\":%d}\n",
    free_bytes, max_blk, min_free, frag);
}

static void cmdState() {
  bool connected = (WiFi.status() == WL_CONNECTED);
  Serial.printf("{\"cmd\":\"state\",\"uptime\":%lu,\"heap\":%u,\"max_block\":%u,"
                "\"wifi\":%s,\"rssi\":%d}\n",
    millis() / 1000,
    ESP.getFreeHeap(), ESP.getMaxAllocHeap(),
    connected ? "true" : "false",
    connected ? WiFi.RSSI() : 0);
}

static void cmdWifi() {
  if (WiFi.status() == WL_CONNECTED) {
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
                "\"location\":\"%s\",\"proxy_host\":\"%s\",\"proxy_port\":%d,"
                "\"weather_refresh_ms\":%lu,\"nearest_refresh_ms\":%lu,"
                "\"receiver_refresh_ms\":%lu,\"server_refresh_ms\":%lu}\n",
    HOME_LAT_DEFAULT, HOME_LON_DEFAULT, LOCATION_NAME, PROXY_HOST, PROXY_PORT,
    WEATHER_REFRESH_MS, NEAREST_REFRESH_MS, RECEIVER_REFRESH_MS, SERVER_REFRESH_MS);
}

static void cmdDiag() {
  bool connected = (WiFi.status() == WL_CONNECTED);
  Serial.printf("{\"cmd\":\"diag\",\"heap\":%d,\"maxblk\":%d,\"wifi\":%d,\"rssi\":%d,"
                "\"uptime\":%lu,\"frag\":%d}\n",
    ESP.getFreeHeap(), ESP.getMaxAllocHeap(),
    connected ? 1 : 0,
    connected ? WiFi.RSSI() : 0,
    millis() / 1000,
    (int)(100 - (ESP.getMaxAllocHeap() * 100 / max((uint32_t)1, (uint32_t)ESP.getFreeHeap()))));
}

static void cmdFetch() {
  fetchNearest();
  Serial.printf("{\"cmd\":\"fetch\",\"ok\":true,\"heap\":%u}\n", ESP.getFreeHeap());
}

void checkSerialCmd() {
  if (!Serial.available()) return;
  String input = Serial.readStringUntil('\n');
  input.trim();
  input.toLowerCase();
  if (input.length() == 0) return;

  Serial.println(">>>CMD_START<<<");

  if      (input == "help")    cmdHelp();
  else if (input == "heap")    cmdHeap();
  else if (input == "state")   cmdState();
  else if (input == "wifi")    cmdWifi();
  else if (input == "config")  cmdConfig();
  else if (input == "diag")    cmdDiag();
  else if (input == "fetch")   cmdFetch();
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
