---
name: foxtrot-ui
description: Edit the Foxtrot 4.3" display UI (layout, colors, text, screens). Use when the user says "Foxtrot UI", "display layout", "change the screen", "Foxtrot screen", or wants to modify any visual element of the 800x480 Waveshare display. Loads full context, verifies in browser, then flashes.
allowed-tools: Read, Edit, Write, Bash, mcp__plugin_playwright_playwright__browser_navigate, mcp__plugin_playwright_playwright__browser_select_option, mcp__plugin_playwright_playwright__browser_take_screenshot, mcp__plugin_playwright_playwright__browser_close
---

# Foxtrot UI Edit Skill

You already know the full Foxtrot UI. Use the reference below — do NOT read `config.h` or `display.ino` for constants you can find here. Only read source files when you need to see the exact current code around the section you're editing.

---

## Screen & Layout (config.h)

```
W=800  H=480

HDR_H=52        y=0..51      Header (amber bar)
NAV_Y=52        y=52..107    Nav bar (HDR_H, height=NAV_H=56)
CONTENT_Y=108   y=108..447   Content area (CONTENT_H=292)
FOOT_H=32       y=448..479   Status bar
```

Nav buttons (right-aligned, all NAV_BTN_W=120 × NAV_BTN_H=52, gap=6):
```
WX_BTN_X1  = 428    (800 - 3×120 - 2×6)
GEO_BTN_X1 = 554
CFG_BTN_X1 = 680
```

---

## Colors (config.h, RGB565)

**System:**
```c
C_BG     0x0820   // dark blue-gray background
C_AMBER  0xFD00   // primary accent / header text
C_DIM    0x7940   // secondary text
C_DIMMER 0x3900   // dividers, inactive buttons
C_GREEN  0x07E0   // takeoff, good status, OTA bar
C_RED    0xF800   // landing, emergency, fetch indicator
C_CYAN   0x07FF   // climbing phase, low tide
C_YELLOW 0xFFE0   // overhead phase, weather conditions
C_ORANGE 0xFC60   // descending phase
C_GOLD   0xFE68   // approach phase
C_TIDE_HI 0x43DF  // high tide label
C_TIDE_LO 0x07FF  // low tide label (= C_CYAN)
```

**Airline brands:**
```c
AL_RED    0xF904   // Qantas, JAL, British Airways, Air China
AL_ROSE   0xFA31   // Virgin, Qatar, Malaysia Airlines
AL_ORANGE 0xFBA0   // Jetstar, Air India
AL_GOLD   0xFE80   // Emirates, Etihad, Singapore, Lufthansa, Cathay (some)
AL_GREEN  0x26E8   // REX, EVA Air, Sri Lankan
AL_TEAL   0x0677   // Air NZ, Fiji, Alliance
AL_SKY    0x455F   // Korean, ANA, American, United
AL_VIOLET 0xA33F   // Thai, Hawaiian
```

**Phase → color mapping** (from `STATUS_TABLE` in helpers.ino):
```
UNKNOWN    → C_DIM
TAKING_OFF → C_GREEN
CLIMBING   → C_CYAN
CRUISING   → C_AMBER
DESCENDING → C_ORANGE
APPROACH   → C_GOLD
LANDING    → C_RED
OVERHEAD   → C_YELLOW
```

---

## Fonts (display.ino)

```c
FONT_XS  DejaVu18   // small labels, status bar text
FONT_SM  DejaVu24   // nav buttons, section labels, table values
FONT_MD  DejaVu40   // large labels
FONT_LG  DejaVu40   // callsign (= FONT_MD)
FONT_XL  DejaVu56   // weather clock only
```

Boot sequence uses `&lgfx::fonts::DejaVu12` directly (compact 16-row diagnostic table).

---

## Drawing helpers (display.ino)

```c
dlbl(x, y, font, color, text)       // left-aligned text at (x,y)
dlbl_r(x, y, font, color, text)     // right-aligned text, anchor at (x,y)
drawBtn(x, y, w, h, bg, font, txtCol, text)  // filled rect + centered text
```

Always use these helpers. Do NOT call `tft.drawString()` directly unless the helpers don't fit the use case (e.g., `top_center` datum for clock in `renderWeather`).

---

## File map

| File | Owns |
|------|------|
| `tracker_foxtrot/display.ino` | All render functions: `drawHeader()`, `drawNavBar()`, `drawStatusBar()`, `renderMessage()`, `renderFlight()`, `renderWeather()`, `bootSequence()`, `drawOtaProgress()` + font aliases + drawing helpers |
| `tracker_foxtrot/helpers.ino` | `statusColor()`, `statusLabel()`, `distanceColor()`, `getAirline()`, `getAircraftTypeName()`, `getAircraftCategory()`, `formatAlt()`, `deriveStatus()` |
| `tracker_foxtrot/config.h` | All `#define` constants: colors, layout, fonts-as-aliases, touch, feature flags |
| `tracker_foxtrot/lookup_tables.h` | `AIRLINES[]` table (prefix, name, color), `AIRCRAFT_TYPES[]` table |
| `tracker_foxtrot/globals.h` | Global state: `flightCount`, `flightIndex`, `currentScreen`, `isFetching`, `countdown`, `dataSource`, `wxReady`, `wxData` |
| `tracker_foxtrot/types.h` | `Flight` struct, `FlightStatus` enum, `ScreenId` enum |
| `tracker_foxtrot/lgfx_config.h` | Hardware bus/panel/touch config — **rarely touch this** |

---

## Key constraints

- **Never restore `lvgl_v8_port.cpp`** — it causes an I2C driver conflict that crashes on boot.
- Rendering is **immediate-mode LovyanGFX** — no LVGL, no FreeRTOS task locks needed.
- Content area is always cleared with `tft.fillRect(0, CONTENT_Y, W, CONTENT_H, C_BG)` at the start of each render function.
- Call `drawStatusBar()` at the end of every render function.
- Only `drawHeader()` is conditionally skipped (when `previousScreen` matches, to avoid flicker).

---

## Workflow

### 1. Make the edit

Read only the specific function(s) in `display.ino` or `helpers.ino` you're modifying. Use the reference above to avoid reading `config.h` for constants.

### 2. Visual verification (browser)

Start a local server and screenshot via Playwright:

```bash
cd /c/Users/maxim/localized-air-traffic-tracker
/c/python314/python.exe -m http.server 8765
```

Run the server in the background, then:
- Navigate to `http://localhost:8765/tft-preview.html`
- Select the relevant scenario from `#ctl-scenario`
- Take a screenshot with `browser_take_screenshot`
- Stop the server after

If the preview looks correct, proceed to flash. If not, fix and re-verify.

### 3. Flash to device

Use the flash-and-log skill:

```
/flash-and-log foxtrot
```

Or directly:
```bash
CLI="/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
CFG="C:/Users/maxim/.arduinoIDE/arduino-cli.yaml"
FQBN="esp32:esp32:waveshare_esp32_s3_touch_lcd_43B:PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB"
"$CLI" --config-file "$CFG" compile --fqbn "$FQBN" --build-path /tmp/tracker-foxtrot-build tracker_foxtrot/tracker_foxtrot.ino
"$CLI" --config-file "$CFG" upload --fqbn "$FQBN" --port COM7 --input-dir /tmp/tracker-foxtrot-build tracker_foxtrot/tracker_foxtrot.ino
```

### Demo mode tip

To test without live WiFi, set `#define DEMO_MODE 1` in `config.h` before flashing — boots with 3 fake Sydney flights instantly.
