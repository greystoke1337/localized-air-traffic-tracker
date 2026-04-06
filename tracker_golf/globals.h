#pragma once
#include <Adafruit_Protomatter.h>
#include <Adafruit_seesaw.h>
#include <WiFiNINA.h>
#include "config.h"
#include "types.h"

extern Adafruit_Protomatter matrix;
extern Adafruit_seesaw      encoder;
extern uint8_t              brightness;
extern int32_t              lastEncoderPos;

enum Page { PAGE_FLIGHT, PAGE_WEATHER };

extern Flight        currentFlight;
extern Weather       currentWeather;
extern Page          currentPage;
extern bool          autoSwitched;
extern int           failCount;
extern unsigned long lastFetchMs;
extern unsigned long lastPixelMs;
extern int           progressPixel;
extern unsigned long lastWeatherMs;
extern uint32_t      ntpEpoch;
extern uint32_t      ntpMillis;

// wifi_setup.ino
void connectWiFi();
void reconnectIfNeeded();

// network.ino
bool fetchFlight(Flight &out);
bool fetchWeather(Weather &out);

// display.ino
void drawAll(const Flight &f, int px = 0);
void drawWeatherPage(const Weather &w, int hour, int min);
void drawBootStatus(const char *msg);
