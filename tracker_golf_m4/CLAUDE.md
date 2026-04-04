# Golf — Adafruit Matrix Portal M4 (64x32 HUB75, Arduino C++)

## Build commands

```bash
./build.sh golf           # compile + upload to COM9
./build.sh golf-compile   # compile only
```

- FQBN: `adafruit:samd:adafruit_matrixportal_m4`
- COM9 (running) / COM10 (bootloader — triggered automatically via 1200-baud touch)
- Serial monitor: 115200 baud

## File layout

| File | Purpose |
|------|---------|
| `tracker_golf_m4.ino` | `setup()` / `loop()`, matrix init, WiFi connect, fetch/display cycle |
| `config.h` | All constants: timing, colors, layout geometry, bar thresholds |
| `types.h` | `Flight` struct (callsign, origin, dest, type, alt, speed, valid, callsignColor, typeColor) |
| `globals.h` | Extern declarations for `matrix` and global state |
| `lookup_tables.h` | ICAO type code -> display name + category; airline prefix -> color; `resolveTypeName()`, `getTypeColor()`, `getAirlineColor()` |
| `network.ino` | HTTPS fetch from `api.overheadtracker.com`; ArduinoJson stream-parse; haversine filter; picks closest aircraft above altitude floor |
| `display.ino` | `drawCallsign()`, `drawRoute()`, `drawBars()`, `drawProgressBar()`, `drawAll()`, `drawBootStatus()` |
| `wifi_setup.ino` | `connectWiFi()`, `reconnectIfNeeded()` |
| `secrets.h` | WiFi SSID/password, HOME_LAT/LON, GEOFENCE_KM, ALT_FLOOR_FT — **gitignored** |

## Display layout (64x32 px)

```
Row  0:      [alt bar, col 0]  callsign or type name (6x8 bold)  [spd bar, col 63]
Rows 1-16:   side bars only
Rows 17-26:  route text (TomThumb font): origin line + destination line
             -- or type name in category color if no route known
Row 31:      amber progress bar (fills left-to-right over 30s)
```

## Refresh cycle (30 s)

- Progress bar advances 1px every ~468ms (`PIXEL_INTERVAL = REFRESH_MS / 64`).
- Pixels 0-31: callsign in airline color.
- Pixels 32-63: aircraft type name in category color; long names scroll left.
- At px 64: fetch new data, reset bar.

## Hardware quirks

**Panel mounted upside-down** — always init with `rotation = 2`.

**G and B output channels are physically swapped** — use `color565(R, B_vis, G_vis)` to produce the intended visual color. For example, to display amber (R=high, G=mid, B=0), pass the G and B arguments in swapped order.

## Color constants (config.h)

| Constant | Visual color | Used for |
|----------|-------------|---------|
| `C_AMBER` | amber | default callsign, progress bar, fallback |
| `C_WHITE` | white | route origin/dest text; GA aircraft |
| `C_BLACK` | off | background |
| `C_DEEP_BLUE` | deep blue | altitude bar (left column) |
| `C_LIGHT_BLUE` | light blue | speed bar (right column) |
| `C_CAT_NARROW` | cyan | narrow-body jets |
| `C_CAT_WIDE` | yellow | wide-body jets |
| `C_CAT_JUMBO` | magenta | A380 / B747 |
| `C_CAT_REGIONAL` | green | regional jets |
| `C_CAT_TURBOPROP` | red | turboprops |

All values encoded with G/B swap applied.

## Side bars

- **Left column (px 0)**: altitude in deep blue. Maps `ALT_MIN_FT` (500 ft) -> 2px, `ALT_MAX_FT` (30,000 ft) -> 30px.
- **Right column (px 63)**: speed in light blue. Maps `SPD_MIN_KT` (100 kt) -> 2px, `SPD_MAX_KT` (450 kt) -> 30px.
- Both 0px when no valid aircraft is tracked.

## Key tuneable constants (config.h)

| Constant | Default | Effect |
|----------|---------|--------|
| `REFRESH_MS` | 30000 | Full fetch cycle duration (ms) |
| `TYPE_FLIP_PX` | 32 (`MATRIX_W / 2`) | Progress pixel at which type name replaces callsign |
| `ALT_MIN_FT` / `ALT_MAX_FT` | 500 / 30000 | Altitude bar clamp range |
| `SPD_MIN_KT` / `SPD_MAX_KT` | 100 / 450 | Speed bar clamp range |

## Config (secrets.h — gitignored)

No template file — create manually:

```cpp
#define WIFI_SSID     "your_ssid"
#define WIFI_PASSWORD "your_password"
#define HOME_LAT      -33.8688
#define HOME_LON      151.2093
#define GEOFENCE_KM   20
#define ALT_FLOOR_FT  1000
```
