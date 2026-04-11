#pragma once
#include "board.h"

// ─── Firmware version ────────────────────────────────
#define FW_VERSION "1.0.0"

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
#define REFRESH_SECS          10
#define TRACK_REFRESH_SECS     5
#define CYCLE_SECS             8
#define WX_REFRESH_SECS      900
#define HEARTBEAT_INTERVAL_MS 60000

// ─── Network ──────────────────────────────────────────
#define PROXY_BUF_SIZE 20480

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
#define C_TIDE_HI 0x43DF   // blue  — rising toward high tide
#define C_TIDE_LO 0x07FF   // cyan  — falling toward low tide

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

// ─── Board profile (eliminates per-file #ifdef BOARD_2P8) ────────────────
#ifdef BOARD_2P8
  // Header
  #define BP_HDR_TS       1
  #define BP_HDR_X        4
  #define BP_HDR_Y        7
  // Location name
  #define BP_LOC_CW       6       // char width in pixels
  #define BP_LOC_XOFF     4
  // Status bar
  #define BP_SB_X         4
  #define BP_SB_YOFF      4
  // Emergency banner
  #define BP_EMERG_H      18
  #define BP_EMERG_TS     1
  #define BP_EMERG_CW     6
  #define BP_EMERG_YIN    5
  // Callsign
  #define BP_CS_TS        3
  #define BP_CS_TS_EMERG  2
  // Airline
  #define BP_AL_TS        1
  #define BP_AL_YINC      12
  // Registration column
  #define BP_REG_X_OFF    10      // reg col = W/2 + BP_REG_X_OFF
  // Dashboard rows
  #define BP_DASH_H       58
  #define BP_DASH_TS      1
  #define BP_DASH_VAL_Y   18      // value Y = dashY + BP_DASH_VAL_Y
  #define BP_DASH_X_OFF   8       // col cursor X = colX + BP_DASH_X_OFF
  #define BP_DASH_VS_Y    32
  // Clock
  #define BP_CLK_TS       4
  #define BP_CLK_CX_OFF   60      // (W - 5*24)/2 at 320px
  #define BP_CLK_YINC     36
  #define BP_DATE_YINC    18
  // Boot sequence
  #define BP_BOOT_LINE_Y0 42
  #define BP_BOOT_DOT_END 140
  #define BP_BOOT_RES_X   142
  #define BP_BOOT_TTL_TS  1
  #define BP_BOOT_TTL_Y   6
  #define BP_BOOT_SUB_Y   20
  #define BP_BOOT_SEP_Y   33
  // OTA progress
  #define BP_OTA_TTL_TS   2
  #define BP_OTA_TTL_X    50
  #define BP_OTA_TTL_Y    80
  #define BP_OTA_SUB_TS   1
  #define BP_OTA_SUB_X    40
  #define BP_OTA_SUB_Y    110
  #define BP_OTA_BX       20
  #define BP_OTA_BY       157
  #define BP_OTA_BW       280
  #define BP_OTA_BH       20
  #define BP_OTA_DONE_X   80
  #define BP_OTA_DONE_Y   190
  #define BP_OTA_ERR_X    50
#else  // BOARD_4P0
  // Header
  #define BP_HDR_TS       2
  #define BP_HDR_X        8
  #define BP_HDR_Y        6
  // Location name
  #define BP_LOC_CW       12
  #define BP_LOC_XOFF     8
  // Status bar
  #define BP_SB_X         6
  #define BP_SB_YOFF      6
  // Emergency banner
  #define BP_EMERG_H      24
  #define BP_EMERG_TS     2
  #define BP_EMERG_CW     12
  #define BP_EMERG_YIN    4
  // Callsign
  #define BP_CS_TS        4
  #define BP_CS_TS_EMERG  3
  // Airline
  #define BP_AL_TS        2
  #define BP_AL_YINC      20
  // Registration column
  #define BP_REG_X_OFF    20
  // Dashboard rows
  #define BP_DASH_H       75
  #define BP_DASH_TS      2
  #define BP_DASH_VAL_Y   24
  #define BP_DASH_X_OFF   12
  #define BP_DASH_VS_Y    44
  // Clock
  #define BP_CLK_TS       6
  #define BP_CLK_CX_OFF   150     // (W - 180)/2 at 480px
  #define BP_CLK_YINC     52
  #define BP_DATE_YINC    22
  // Boot sequence
  #define BP_BOOT_LINE_Y0 56
  #define BP_BOOT_DOT_END 212
  #define BP_BOOT_RES_X   214
  #define BP_BOOT_TTL_TS  2
  #define BP_BOOT_TTL_Y   12
  #define BP_BOOT_SUB_Y   34
  #define BP_BOOT_SEP_Y   47
  // OTA progress
  #define BP_OTA_TTL_TS   3
  #define BP_OTA_TTL_X    100
  #define BP_OTA_TTL_Y    110
  #define BP_OTA_SUB_TS   2
  #define BP_OTA_SUB_X    80
  #define BP_OTA_SUB_Y    155
  #define BP_OTA_BX       40
  #define BP_OTA_BY       210
  #define BP_OTA_BW       400
  #define BP_OTA_BH       24
  #define BP_OTA_DONE_X   160
  #define BP_OTA_DONE_Y   250
  #define BP_OTA_ERR_X    120
#endif

// ─── Board-specific string literals ─────────────────
#ifdef BOARD_2P8
  #define BP_BOOT_SUBTITLE "ADS-B SURVEILLANCE  REV 3.2"
  #define BP_DISPLAY_NAME  "ST7789 320x240 16BIT"
#else
  #define BP_BOOT_SUBTITLE "ADS-B AIRSPACE SURVEILLANCE  REV 3.2"
  #define BP_DISPLAY_NAME  "ST7796 480x320 16BIT"
#endif

// ─── Compile-time invariant checks ───────────────────
static_assert(MAX_LOGGED   <= 200,  "loggedCallsigns array overflow");
static_assert(MAX_UNKNOWNS <= 80,   "loggedUnknowns array overflow");
static_assert(REFRESH_SECS  > 0,   "REFRESH_SECS must be positive");
static_assert(PROXY_BUF_SIZE >= 4096, "PROXY_BUF_SIZE too small for typical payloads");
static_assert(WX_REFRESH_SECS > 0, "WX_REFRESH_SECS must be positive");
