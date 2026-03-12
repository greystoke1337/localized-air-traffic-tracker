#pragma once
#include "board.h"

// ─── Feature flags ───────────────────────────────────
#ifdef BOARD_2P8
  #define HAS_TOUCH 0
  #define HAS_SD    0
#else
  #define HAS_TOUCH 1
  #define HAS_SD    1
#endif

// ─── SD pin ───────────────────────────────────────────
#if HAS_SD
  #define SD_CS 5
#endif

// ─── Refresh ──────────────────────────────────────────
#define REFRESH_SECS 15
#define CYCLE_SECS   8

// ─── Screen ──────────────────────────────────────────
#ifdef BOARD_2P8
  #define W 320
  #define H 240
#else
  #define W 480
  #define H 320
#endif

// ─── Layout ───────────────────────────────────────────
#ifdef BOARD_2P8
  #define HDR_H      22
  #define NAV_H       0   // no nav bar (no touch)
  #define FOOT_H     16
#else
  #define HDR_H      28
  #define NAV_H      36
  #define FOOT_H     20
#endif
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
#define C_ORANGE  0xFC60   // descending (aligned with web app #ff8844)
#define C_GOLD    0xFE68   // approach   (aligned with web app #ffaa00)

// ─── Airline brand colours (RGB565) ─────────────────
#define AL_RED     0xF904   // #FF2222  Qantas, JAL, British
#define AL_ROSE    0xFA31   // #FF4488  Virgin, Qatar, Malaysia
#define AL_ORANGE  0xFBA0   // #FF7700  Jetstar, Air India
#define AL_GOLD    0xFE80   // #FFD000  Emirates, Singapore, Lufthansa
#define AL_GREEN   0x26E8   // #22DD44  Rex, Cathay, Eva Air
#define AL_TEAL    0x0677   // #00CCBB  Air NZ, Fiji
#define AL_SKY     0x455F   // #44AAFF  Korean, ANA, KLM, United
#define AL_VIOLET  0xA33F   // #AA66FF  Thai, Hawaiian

// ─── Touch ────────────────────────────────────────────
#if HAS_TOUCH
  #define TOUCH_DEBOUNCE_MS  350
  #define NAV_Y       HDR_H
  #define NAV_BTN_W   70
  #define NAV_BTN_H   (NAV_H - 4)
  #define NAV_BTN_GAP 4
  #define CFG_BTN_X1  (W - NAV_BTN_W)
  #define GEO_BTN_X1  (CFG_BTN_X1 - NAV_BTN_GAP - NAV_BTN_W)
  #define WX_BTN_X1   (GEO_BTN_X1 - NAV_BTN_GAP - NAV_BTN_W)
#endif

// ─── Direct API robustness ────────────────────────────
#define DIRECT_API_MIN_HEAP  40000  // 40 KB minimum free heap for TLS
#define DIRECT_API_TIMEOUT   8000   // reduced from 12 s (WDT is 30 s)

// ─── Session log ──────────────────────────────────────
#define MAX_LOGGED   200
#define MAX_UNKNOWNS 80
