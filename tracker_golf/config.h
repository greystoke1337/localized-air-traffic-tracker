#pragma once

// Display geometry
#define MATRIX_W  64
#define MATRIX_H  32

// Colors — RGB565 with G/B channels pre-swapped for this panel.
// The panel swaps G and B physical wiring, so pass color565(R, B, G) to get the right look.
// Amber (visually): R=0xFF G=0xA0 B=0x00 → swapped → color565(0xFF, 0x00, 0xA0) = 0xF814
#define C_AMBER       0xF814
// Deep blue bar (visually): R=0x00 G=0x00 B=0x40 → swapped → color565(0x00, 0x40, 0x00) = 0x0200
#define C_DEEP_BLUE   0x0200
// Light blue bar (visually): R=0x00 G=0x60 B=0x80 → swapped → color565(0x00, 0x80, 0x60) = 0x040C
#define C_LIGHT_BLUE  0x040C
// Green (visually): R=0x00 G=0xFF B=0x00 → swapped → color565(0x00, 0x00, 0xFF) = 0x001F
#define C_GREEN       0x001F
#define C_WHITE       0xFFFF
#define C_BLACK       0x0000

// Type-category palette (G/B-swapped: pass color565(R, B_vis, G_vis))
#define C_CAT_NARROW    0x07FF  // visual cyan     — narrow-body jets
#define C_CAT_WIDE      0xF81F  // visual yellow   — wide-body jets
#define C_CAT_JUMBO     0xFFE0  // visual magenta  — A380 / B747
#define C_CAT_REGIONAL  0x001F  // visual green    — regional jets
#define C_CAT_TURBOPROP 0xF800  // visual red      — turboprops

// Progress bar pixel at which callsign flips to aircraft type name (non-GA only)
#define TYPE_FLIP_PX  32

// Refresh timing
#define REFRESH_MS     30000UL
// Progress bar: advance one pixel per interval (64 pixels over 30s ≈ 468ms each)
#define PIXEL_INTERVAL (REFRESH_MS / MATRIX_W)

// Failure handling
#define MAX_FAIL_COUNT 3

// Callsign layout (built-in 6×8 font, drawn pseudo-bold with 4 offsets)
#define CALLSIGN_Y    3    // top of callsign text (post-rotation origin = top-left)
#define CHAR_W        6
#define CHAR_GAP      1

// Side bar dimensions
#define BAR_MAX_H     30   // full-bar height in pixels (rows 1–30; row 0 empty, row 31 = progress)

// Altitude bar thresholds (left column)
#define ALT_MIN_FT    300     // → 2px (near ground / altitude floor)
#define ALT_MID_FT    2000    // → 23px (3/4 of bar height)
#define ALT_MAX_FT    30000   // → BAR_MAX_H (high cruise)

// Speed bar thresholds (right column)
#define SPD_MIN_KT     50    // → 2px (slow approach / GA)
#define SPD_MAX_KT    450    // → BAR_MAX_H (jet cruise)

// Flight page text layout (TomThumb font rows)
#define ALTITUDE_Y    19   // altitude text, e.g. "35000FT"
#define ROUTE_Y       28   // route text, e.g. "SYD>MEL" (single line)

// Rotary encoder brightness control (software scaling of RGB565 pixel values, 0=off, 255=full)
// Hardware setDuty is fixed at max (2); all dimming is done in software via dim() in display.ino.
#define BRIGHTNESS_DEFAULT  77
#define BRIGHTNESS_MIN      0
#define BRIGHTNESS_MAX      255
#define BRIGHTNESS_STEP     25   // encoder ticks per step (~10 steps across full range)

// Seesaw encoder push-button pin (active-low)
#define ENCODER_BTN_PIN     24

// Type name scroll (bounce left-to-right when name is wider than display)
#define SCROLL_INTERVAL_MS  40UL  // fast redraw interval for scroll (~25fps)
#define SCROLL_SPEED_MS     200    // ms per pixel of scroll position

// OTA firmware update
#define FIRMWARE_VERSION      1          // increment on each golf-publish
#define OTA_CHECK_INTERVAL_MS 21600000UL // check for updates every 6 hours

// OTA local dev server defaults — override in secrets.h if needed
#ifndef OTA_LOCAL_HOST
#define OTA_LOCAL_HOST ""
#endif
#ifndef OTA_LOCAL_PORT
#define OTA_LOCAL_PORT 8080
#endif

// WiFi: hard-reset if the AP cannot be reached within this window
#define WIFI_CONNECT_TIMEOUT_MS 300000UL  // 5 minutes

// Weather page
#define UTC_OFFSET_HOURS    11          // AEDT (UTC+11); change to 10 for AEST
#define WEATHER_REFRESH_MS  600000UL   // re-fetch weather every 10 minutes
#define CLOCK_UPDATE_MS     1000UL     // redraw weather page every second for clock


