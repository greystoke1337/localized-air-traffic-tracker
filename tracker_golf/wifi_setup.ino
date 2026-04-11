// wifi_setup.ino — WiFi connection management via onboard ESP32 co-processor (WiFiNINA)

void connectWiFi() {
  Serial.print("[WIFI] Connecting to ");
  Serial.println(WIFI_SSID);

  unsigned long deadline = millis() + WIFI_CONNECT_TIMEOUT_MS;
  while (WiFi.begin(WIFI_SSID, WIFI_PASSWORD) != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (millis() >= deadline) {
      Serial.println("\n[WIFI] Timeout — rebooting");
      NVIC_SystemReset();
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
