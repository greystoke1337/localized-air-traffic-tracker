// Golf — Adafruit Matrix Portal M4 (64×32 HUB75 LED matrix)
// Arduino port of tracker_golf/code.py
// Display: overhead aircraft callsign + route on a public-facing LED matrix.

#include <Adafruit_Protomatter.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <Fonts/TomThumb.h>
#include "secrets.h"
#include "config.h"
#include "types.h"
#include "globals.h"
#include "lookup_tables.h"

// Matrix Portal M4 fixed pin assignments (from Adafruit Protomatter examples)
uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19, 20};

Adafruit_Protomatter matrix(
  64, 4, 1, rgbPins, 4, addrPins,
  14,   // clock
  15,   // latch
  16,   // OE
  true  // double-buffer
);

Flight        currentFlight;
int           failCount    = 0;
unsigned long lastFetchMs  = 0;
unsigned long lastPixelMs  = 0;
int           progressPixel = 0;

void setup() {
  Serial.begin(115200);

  ProtomatterStatus s = matrix.begin();
  if (s != PROTOMATTER_OK) {
    Serial.print("Protomatter error: ");
    Serial.println((int)s);
    while (1);
  }

  // 180° rotation — panel is mounted upside-down
  matrix.setRotation(2);

  drawBootStatus("Connecting...");
  connectWiFi();

  drawBootStatus("Fetching...");
  currentFlight.valid = false;
  fetchFlight(currentFlight);
  failCount = 0;

  unsigned long now = millis();
  lastFetchMs  = now;
  lastPixelMs  = now;
  progressPixel = 0;

  drawAll(currentFlight, 0);
}

void loop() {
  unsigned long now = millis();
  reconnectIfNeeded();

  // Advance progress bar one pixel at a time
  if (progressPixel < MATRIX_W && now - lastPixelMs >= PIXEL_INTERVAL) {
    progressPixel++;
    lastPixelMs += PIXEL_INTERVAL;
    drawAll(currentFlight, progressPixel, progressPixel >= TYPE_FLIP_PX);
  }

  // Full refresh cycle
  if (now - lastFetchMs >= REFRESH_MS) {
    lastFetchMs   = now;
    lastPixelMs   = now;
    progressPixel = 0;
    drawAll(currentFlight, 0);  // snap back to callsign view before blocking fetch

    bool ok = fetchFlight(currentFlight);
    if (!ok) {
      failCount++;
      Serial.print("[MAIN] Fetch fail #");
      Serial.println(failCount);
    } else {
      failCount = 0;
    }

    if (failCount >= MAX_FAIL_COUNT) {
      Serial.println("[MAIN] Too many failures — reconnecting WiFi");
      failCount = 0;
      WiFi.end();
      connectWiFi();
    }

    drawAll(currentFlight, 0);
  }
}
