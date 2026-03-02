#pragma once

// ─── SD pin ───────────────────────────────────────────
#define SD_CS 5

// ─── Refresh ──────────────────────────────────────────
#define REFRESH_SECS 15
#define CYCLE_SECS   8

// ─── Screen (landscape: 480 wide x 320 tall) ──────────
#define W 480
#define H 320

// ─── Layout ───────────────────────────────────────────
#define HDR_H      28   // amber header bar
#define NAV_H      36   // navigation/controls bar below header
#define FOOT_H     20   // dim footer bar
#define CONTENT_Y  (HDR_H + NAV_H)                // 64
#define CONTENT_H  (H - HDR_H - NAV_H - FOOT_H)  // 236px

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
#define TOUCH_DEBOUNCE_MS  350

// ─── Nav bar button zones ─────────────────────────────
#define NAV_Y       HDR_H
#define NAV_BTN_W   70
#define NAV_BTN_H   (NAV_H - 4)   // 32px tall (2px top/bottom padding)
#define NAV_BTN_GAP 4
#define CFG_BTN_X1  (W - NAV_BTN_W)                          // 410
#define GEO_BTN_X1  (CFG_BTN_X1 - NAV_BTN_GAP - NAV_BTN_W)  // 336
#define WX_BTN_X1   (GEO_BTN_X1 - NAV_BTN_GAP - NAV_BTN_W)  // 262

// ─── Direct API robustness ────────────────────────────
#define DIRECT_API_MIN_HEAP  40000  // 40 KB minimum free heap for TLS
#define DIRECT_API_TIMEOUT   8000   // reduced from 12 s (WDT is 30 s)

// ─── Session log ──────────────────────────────────────
#define MAX_LOGGED 200
