#pragma once

// ─── Feature flags ───────────────────────────────────
#define HAS_TOUCH 1
#define HAS_SD    1

// ─── SD pin ───────────────────────────────────────────
#define SD_CS 10  // CH422G EXIO4 — verify on hardware

// ─── Refresh ──────────────────────────────────────────
#define REFRESH_SECS 15
#define CYCLE_SECS   8

// ─── Screen ──────────────────────────────────────────
#define W 800
#define H 480

// ─── Layout ───────────────────────────────────────────
#define HDR_H      40
#define NAV_H      50
#define FOOT_H     28
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
#define C_TIDE_HI 0x43DF
#define C_TIDE_LO 0x07FF

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
#define NAV_BTN_W   100
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
