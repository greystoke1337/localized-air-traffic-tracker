#pragma once

// ─── Firmware version ────────────────────────────
#define FW_VERSION "0.1.0"

// ─── Feature flags ───────────────────────────────
#define DEMO_MODE 0

// ─── Screen ──────────────────────────────────────
// Physical: 172×640 (portrait). Rotated 90° → 640×172 (landscape).
#define W 640
#define H 172

// ─── Layout ───────────────────────────────────────
#define HDR_H   18   // title bar
#define HIST_H  28   // histogram strip at bottom
#define HIST_Y  (H - HIST_H)
#define CONT_Y  HDR_H
#define CONT_H  (H - HDR_H - HIST_H)  // 126 px of content

// ─── GPIO ─────────────────────────────────────────
#define LCD_CS    9
#define LCD_CLK   10
#define LCD_D0    11
#define LCD_D1    12
#define LCD_D2    13
#define LCD_D3    14
#define LCD_RST   21
#define LCD_BL    8

#define TOUCH_SDA  17
#define TOUCH_SCL  18
#define TOUCH_ADDR 0x3B

// ─── Refresh intervals (seconds) ─────────────────
#define REFRESH_SECS      10   // flights + stats + histogram
#define WEATHER_SECS      300  // weather
#define SERVER_SECS       30   // server status + device heartbeats
#define PAGE_ROTATE_SECS  15   // auto-advance page

// ─── Geofence ─────────────────────────────────────
#define DEFAULT_GEOFENCE_MI 10.0f
#define ALT_FLOOR_FT        500

// ─── Proxy ────────────────────────────────────────
#define PROXY_HOST "api.overheadtracker.com"
#define PROXY_PORT 443

// ─── Colours (RGB565) ─────────────────────────────
#define C_BG      0x0821   // (15,15,25)   dark navy
#define C_ACCENT  0x059F   // (0,180,255)  cyan
#define C_GREEN   0x06E4   // (0,220,100)  green
#define C_RED     0xF840   // (255,80,80)  red
#define C_AMBER   0xFD00   // (255,160,0)  amber
#define C_DIM     0x39C4   // (56,56,66)   dim
#define C_DIMMER  0x2103   // (32,32,24)   very dim
#define C_ORANGE  0xFC60   // (255,140,0)  orange
#define C_GOLD    0xFE80   // (255,210,0)  gold
#define C_YELLOW  0xFFE0   // (255,255,0)  yellow
#define C_WHITE   0xFFFF
#define C_ROW_BG  0x1163   // (22,22,35)   row band

// ─── Font text sizes (built-in 6×8 pixel font) ───
#define SZ_SM 1   // 6×8  px
#define SZ_MD 2   // 12×16 px
#define SZ_LG 3   // 18×24 px

// ─── Watchdog ─────────────────────────────────────
#define WDT_TIMEOUT_SEC 30

// ─── Direct API robustness ────────────────────────
#define DIRECT_API_MIN_HEAP  40000
#define DIRECT_API_TIMEOUT   8000

// ─── Session log ──────────────────────────────────
#define MAX_LOGGED 100
