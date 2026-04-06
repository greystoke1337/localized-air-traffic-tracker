// wifi_setup.ino — WiFi connection management via onboard ESP32 co-processor (WiFiNINA)

void connectWiFi() {
  Serial.print("[WIFI] Connecting to ");
  Serial.println(WIFI_SSID);

  int attempts = 0;
  while (WiFi.begin(WIFI_SSID, WIFI_PASSWORD) != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (++attempts >= 30) {
      Serial.println("\n[WIFI] Retrying...");
      attempts = 0;
    }
  }
  Serial.print("\n[WIFI] Connected, IP: ");
  Serial.println(WiFi.localIP());
}

void reconnectIfNeeded() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 15000UL) return;
  lastCheck = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Lost connection — reconnecting");
    connectWiFi();
  }
}
