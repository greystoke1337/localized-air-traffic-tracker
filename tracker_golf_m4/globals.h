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

extern Flight        currentFlight;
extern int           failCount;
extern unsigned long lastFetchMs;
extern unsigned long lastPixelMs;
extern int           progressPixel;

// wifi_setup.ino
void connectWiFi();
void reconnectIfNeeded();

// network.ino
bool fetchFlight(Flight &out);

// display.ino
void drawAll(const Flight &f, int progressPx, bool showType = false);
void drawBootStatus(const char *msg);
