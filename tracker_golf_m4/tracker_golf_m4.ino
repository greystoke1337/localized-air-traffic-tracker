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

Adafruit_seesaw encoder;
uint8_t         brightness     = BRIGHTNESS_DEFAULT;
int32_t         lastEncoderPos = 0;
bool            lastBtnState   = true;   // active-low; idle = high

Flight        currentFlight;
Weather       currentWeather;
Page          currentPage  = PAGE_FLIGHT;
bool          autoSwitched = false;   // true when page switched automatically (no flight)
int           failCount    = 0;
unsigned long lastFetchMs  = 0;
unsigned long lastPixelMs  = 0;
int           progressPixel = 0;
unsigned long lastWeatherMs = 0;
unsigned long lastClockMs   = 0;
uint32_t      ntpEpoch      = 0;
uint32_t      ntpMillis     = 0;

// Returns current local time components adjusted by UTC_OFFSET_HOURS
static void currentTime(int &hour, int &min) {
  if (!ntpEpoch) { hour = 0; min = 0; return; }
  uint32_t t = ntpEpoch + (millis() - ntpMillis) / 1000 + (uint32_t)UTC_OFFSET_HOURS * 3600UL;
  hour = (t / 3600) % 24;
  min  = (t / 60)   % 60;
}

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

  if (encoder.begin(0x36)) {
    matrix.setDuty(brightness);
    lastEncoderPos = encoder.getEncoderPosition();
    encoder.enableEncoderInterrupt();
    encoder.pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    Serial.println("[ENC] Seesaw encoder ready");
  } else {
    Serial.println("[ENC] Seesaw not found — brightness fixed");
  }

  playBootAnimFor(8000);   // animate while WiFi connects (~3-8 s)
  connectWiFi();

  // NTP time sync
  for (int i = 0; i < 5 && !ntpEpoch; i++) {
    ntpEpoch = WiFi.getTime();
    if (!ntpEpoch) delay(1000);
  }
  ntpMillis = millis();
  if (ntpEpoch) {
    int h, m;
    currentTime(h, m);
    Serial.printf("[NTP] Synced — local time %02d:%02d\n", h, m);
  } else {
    Serial.println("[NTP] Sync failed — clock will show 00:00");
  }

  playBootAnimFor(4000);   // animate while first fetches run
  currentFlight.valid  = false;
  currentWeather.valid = false;
  fetchFlight(currentFlight);
  fetchWeather(currentWeather);
  failCount = 0;

  unsigned long now = millis();
  lastFetchMs   = now;
  lastPixelMs   = now;
  lastWeatherMs = now;
  lastClockMs   = now;
  progressPixel = 0;

  drawAll(currentFlight, 0);
}

void loop() {
  unsigned long now = millis();

  // Rotary encoder brightness (setDuty range 0-2 on 120 MHz SAMD51)
  int32_t pos = encoder.getEncoderPosition();
  if (pos != lastEncoderPos) {
    int delta = pos - lastEncoderPos;
    lastEncoderPos = pos;
    brightness = (uint8_t)constrain((int)brightness + delta, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
    matrix.setDuty(brightness);
    flashBrightness(brightness);
    // Redraw whichever page is active
    if (currentPage == PAGE_WEATHER) {
      int h, m; currentTime(h, m);
      drawWeatherPage(currentWeather, h, m);
    } else {
      drawAll(currentFlight, progressPixel, progressPixel >= TYPE_FLIP_PX);
    }
  }

  // Button press — toggle page (active-low, edge-triggered)
  bool btnNow = !encoder.digitalRead(ENCODER_BTN_PIN);
  if (btnNow && !lastBtnState) {
    Serial.println("[BTN] Page toggle");
    autoSwitched = false;   // manual toggle clears auto-switch flag
    currentPage = (currentPage == PAGE_FLIGHT) ? PAGE_WEATHER : PAGE_FLIGHT;
    if (currentPage == PAGE_WEATHER) {
      int h, m; currentTime(h, m);
      drawWeatherPage(currentWeather, h, m);
    } else {
      drawAll(currentFlight, progressPixel, progressPixel >= TYPE_FLIP_PX);
    }
  }
  lastBtnState = btnNow;

  reconnectIfNeeded();

  // Weather page: update clock every second
  if (currentPage == PAGE_WEATHER && now - lastClockMs >= CLOCK_UPDATE_MS) {
    lastClockMs = now;
    int h, m; currentTime(h, m);
    drawWeatherPage(currentWeather, h, m);
  }

  // Advance flight progress bar (only relevant when on flight page)
  if (currentPage == PAGE_FLIGHT &&
      progressPixel < MATRIX_W && now - lastPixelMs >= PIXEL_INTERVAL) {
    progressPixel++;
    lastPixelMs += PIXEL_INTERVAL;
    drawAll(currentFlight, progressPixel, progressPixel >= TYPE_FLIP_PX);
  }

  // Weather refresh (every 10 minutes, regardless of current page)
  if (now - lastWeatherMs >= WEATHER_REFRESH_MS) {
    lastWeatherMs = now;
    fetchWeather(currentWeather);
    if (currentPage == PAGE_WEATHER) {
      int h, m; currentTime(h, m);
      drawWeatherPage(currentWeather, h, m);
    }
  }

  // Flight refresh cycle (every 30 seconds)
  if (now - lastFetchMs >= REFRESH_MS) {
    lastFetchMs   = now;
    lastPixelMs   = now;
    progressPixel = 0;

    if (currentPage == PAGE_FLIGHT) {
      drawAll(currentFlight, 0);  // snap back to callsign view before blocking fetch
    }

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

    // Auto page-switch logic: no flights → weather page; flight reappears → flight page
    if (!currentFlight.valid && currentPage == PAGE_FLIGHT) {
      Serial.println("[MAIN] No flights — switching to weather page");
      currentPage  = PAGE_WEATHER;
      autoSwitched = true;
    } else if (currentFlight.valid && currentPage == PAGE_WEATHER && autoSwitched) {
      Serial.println("[MAIN] Flight detected — returning to flight page");
      currentPage  = PAGE_FLIGHT;
      autoSwitched = false;
    }

    if (currentPage == PAGE_WEATHER) {
      int h, m; currentTime(h, m);
      drawWeatherPage(currentWeather, h, m);
    } else {
      drawAll(currentFlight, 0);
    }
  }
}
