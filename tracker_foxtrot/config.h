#pragma once

// ─── Firmware version ────────────────────────────────
#define FW_VERSION "1.0.0"

// ─── Feature flags ───────────────────────────────────
#define HAS_TOUCH  1
#define HAS_SD     1
#define DEMO_MODE  0
#define ASYNC_FETCH 0

// ─── Blue-tint diagnostic (0 = off, 1–6 = progressive live init) ──────────
// 0: normal DEMO_MODE / live mode
// 1: live mode, no hardware init (SPI/touch/WiFi all skipped) — renders demo flights
// 2: + SPI/SD init
// 3: + initTouch()
// 4: + WiFi.begin() then immediate disconnect (radio init only, no connection)
// 5: + full WiFi connect, then tft.init()+setRotation(0) reinit before render
// 6: + fetchFlights() (full live mode, no tft reinit — baseline for comparison)
#define DIAG_STEP  0

// ─── SD ──────────────────────────────────────────────
// Real SD_CS is on CH422G EXIO4 (I2C expander), NOT a direct GPIO.
// GPIO 10 is part of the RGB display bus (blue B7) — never use it for SD.

// ─── Refresh ──────────────────────────────────────────
#define REFRESH_SECS 15
#define CYCLE_SECS   8

// ─── Screen ──────────────────────────────────────────
#define W 800
#define H 480

// ─── Layout ───────────────────────────────────────────
#define HDR_H      52
#define NAV_H      56
#define FOOT_H     32
#define CONTENT_Y  (HDR_H + NAV_H)
#define CONTENT_H  (H - HDR_H - NAV_H - FOOT_H)

// ─── Colours (RGB565) ─────────────────────────────────
#define C_BG      0x0820
#define C_AMBER   0xFD00
#define C_DIM     0x7940
#define C_DIMMER  0x3900
#define C_GREEN   0x07E0
#define C_RED     0xF800
#define C_CYAN    0x07FF
#define C_YELLOW  0xFFE0
#define C_ORANGE  0xFC60
#define C_GOLD    0xFE68
// ─── Airline brand colours (RGB565) ─────────────────
#define AL_RED     0xF904
#define AL_ROSE    0xFA31
#define AL_ORANGE  0xFBA0
#define AL_GOLD    0xFE80
#define AL_GREEN   0x26E8
#define AL_TEAL    0x0677
#define AL_SKY     0x455F
#define AL_VIOLET  0xA33F

// ─── Touch ────────────────────────────────────────────
#define TOUCH_DEBOUNCE_MS  350
#define NAV_Y       HDR_H
#define NAV_BTN_W   120
#define NAV_BTN_H   (NAV_H - 4)
#define NAV_BTN_GAP 6
#define CFG_BTN_X1  (W - NAV_BTN_W)
#define GEO_BTN_X1  (CFG_BTN_X1 - NAV_BTN_GAP - NAV_BTN_W)
#define WX_BTN_X1   (GEO_BTN_X1 - NAV_BTN_GAP - NAV_BTN_W)

// ─── Direct API robustness ────────────────────────────
#define DIRECT_API_MIN_HEAP  40000
#define DIRECT_API_TIMEOUT   8000

// ─── Session log ──────────────────────────────────────
#define MAX_LOGGED   200
#define MAX_UNKNOWNS 80
