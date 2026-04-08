# Golf — Adafruit Matrix Portal M4 (64x32 HUB75, Arduino C++)

## Build commands

```bash
./build.sh golf           # compile + upload to COM9 (USB, development)
./build.sh golf-compile   # compile only (error check)
./build.sh golf-publish   # compile + stage binary + increment OTA version (then: railway up)
./build.sh golf-serve     # compile + serve binary locally on :8080 (HTTP OTA for fast iteration)
```

- FQBN: `adafruit:samd:adafruit_matrixportal_m4`
- COM9 (running) / COM10 (bootloader — triggered automatically via 1200-baud touch)
- Serial monitor: 115200 baud

## Brightness

Brightness is software-scaled via the `dim()` helper in `display.ino`, which scales each RGB565 channel by `brightness/255` with a minimum of 1 per non-zero channel (so dark colors stay faintly visible). Hardware `setDuty` is fixed at max (2).

The rotary encoder is currently **disabled** — brightness is hardcoded at `BRIGHTNESS_DEFAULT` (77 = ~30%) in `config.h`. To change brightness, update that constant and reflash.

The boot animation (`playBootAnimFor`) is always drawn at full brightness so its faint grid lines remain visible regardless of the brightness setting.

## OTA firmware updates

OTA auto-checks (on boot and every 6 hours) are **disabled**. The device only self-updates when triggered manually via local or remote OTA.

**Remote publish (Railway):**
1. `./build.sh golf-publish` — compiles, copies `.bin` to `server/firmware/golf.bin`, increments `server/firmware/golf-version.txt`
2. `railway up` (from project root) — deploys server with new binary
3. Device picks up the update within 6 hours, or reboot it to trigger immediately

**Local OTA (fast dev iteration — no Railway needed):**
1. Add to `secrets.h` (gitignored):

   ```cpp
   #define OTA_LOCAL_HOST  "192.168.x.x"   // your machine's LAN IP
   #define OTA_LOCAL_PORT  8080             // optional, default 8080
   ```

2. `./build.sh golf-serve` — compiles and starts a local HTTP server; prints the exact `#define` lines to add
3. Reboot the device — it downloads from your machine instead of Railway; server reports version 9999 so it always updates while the server is running
4. Ctrl-C to stop; device falls back to Railway on next check

**Priority:** local server is tried first (if `OTA_LOCAL_HOST` defined); falls back to remote if unreachable.

**Recovery:** If OTA corrupts the sketch, the UF2 bootloader survives — recover via USB with `./build.sh golf`.

## File layout

| File | Purpose |
|------|---------|
| `tracker_golf.ino` | `setup()` / `loop()`, matrix init, WiFi connect, NTP sync, page state machine, fetch/display cycle |
| `config.h` | All constants: timing, colors, layout geometry, bar thresholds, weather/clock intervals |
| `types.h` | `Flight` struct; `Weather` struct (`tempC`, `weatherCode`, `valid`) |
| `globals.h` | `Page` enum (`PAGE_FLIGHT`, `PAGE_WEATHER`); extern declarations for all global state; `fetchWeather`/`drawWeatherPage` prototypes |
| `lookup_tables.h` | ICAO type code -> display name + category; airline prefix -> color; `resolveTypeName()`, `getTypeColor()`, `getAirlineColor()` |
| `network.ino` | HTTPS fetch from `api.overheadtracker.com`; `fetchFlight()` (ArduinoJson stream-parse, haversine filter, picks closest aircraft above altitude floor); `fetchWeather()` |
| `display.ino` | `drawCallsign()`, `drawRoute()`, `drawBars()`, `drawProgressBar()`, `drawAll()`, `drawBootStatus()`, `drawOTAStatus()`; weather page: `wmoShortName()`, `wmoToIconType()`, `drawWeatherIcon()`, `drawWeatherPage()`; icon type constants (`ICON_SUN`–`ICON_STORM`) |
| `ota.ino` | `checkOTA()` — version check + binary download over HTTPS; `applyOTA()` — NVMCTRL flash write from SRAM |
| `wifi_setup.ino` | `connectWiFi()`, `reconnectIfNeeded()` |
| `secrets.h` | WiFi SSID/password, HOME_LAT/LON, GEOFENCE_KM, ALT_FLOOR_FT — **gitignored** |

## Display layout (64x32 px)

Two pages; page switching via the rotary encoder button is currently **disabled** (encoder is not in use). Auto page-switching still works: no flights → weather page; flight reappears → flight page.

### Flight page (default)

```
Row  0:      [alt bar, col 0]  callsign or type name (6x8 bold)  [spd bar, col 63]
Rows 1-16:   side bars only
Rows 17-26:  route text (TomThumb font): origin line + destination line
             -- or type name in category color if no route known
             -- GA aircraft always show "GENERAL" / "AVIATION" here (see GA note below)
Row 31:      white distance bar (centered, full-width = overhead; narrows to 3px at geofence edge)
```

### Weather page

```
Rows 0-9:    clock "HH:MM" — pseudo-bold amber, centered (same technique as callsign)
Rows 12-21:  left side: procedural weather icon (~12x10 px)
             right side: temperature in °C (white, 6x8 font)
Row 28:      WMO condition string — TomThumb, amber, centered (e.g. "PT CLOUDY")
```

The weather page auto-activates when no flights are in the geofence, and auto-returns to the flight page when a flight reappears. Manual button presses clear the auto-switch flag and let the user stay on the chosen page until the next manual toggle.

## Refresh cycle (30 s)

- Progress bar advances 1px every ~468ms (`PIXEL_INTERVAL = REFRESH_MS / 64`).
- Pixels 0-31: callsign in airline color.
- Pixels 32-63: aircraft type name in category color; long names scroll left.
- **GA aircraft exception**: `typeColor == C_WHITE` suppresses the mid-cycle flip. The callsign stays visible for the full 30s cycle; the route area always shows "GENERAL" / "AVIATION" in amber.
- At px 64: fetch new data, reset bar.

## NTP clock

Time is synced via `WiFi.getTime()` (built into WiFiNINA) during `setup()`, up to 5 retries. The epoch + elapsed millis are combined at render time, so the clock stays accurate between reboots without re-syncing. If NTP fails, the clock shows `00:00`. UTC offset is set by `UTC_OFFSET_HOURS` in `config.h`.

## Weather fetch

- Endpoint: `GET api.overheadtracker.com/weather?lat=&lon=`
- Fetched once at startup, then every 10 minutes (`WEATHER_REFRESH_MS`).
- Response fields used: `temp` (float, °C) and `weather_code` (WMO integer).
- WMO codes mapped to five icon types (SUN, CLOUD, RAIN, SNOW, STORM) and short condition strings.
- If the fetch has not yet succeeded, the weather page shows "NO DATA".

## Hardware quirks

**Panel mounted upside-down** — always init with `rotation = 2`.

**G and B output channels are physically swapped** — use `color565(R, B_vis, G_vis)` to produce the intended visual color. For example, to display amber (R=high, G=mid, B=0), pass the G and B arguments in swapped order.

## Color constants (config.h)

| Constant | Visual color | Used for |
|----------|-------------|---------|
| `C_AMBER` | amber | default callsign, progress bar, clock, weather condition, fallback |
| `C_WHITE` | white | route origin/dest text; GA aircraft; temperature; weather icon cloud |
| `C_BLACK` | off | background |
| `C_DEEP_BLUE` | deep blue | altitude bar (left column) |
| `C_LIGHT_BLUE` | light blue | speed bar (right column); rain streaks on weather icon |
| `C_CAT_NARROW` | cyan | narrow-body jets |
| `C_CAT_WIDE` | yellow | wide-body jets |
| `C_CAT_JUMBO` | magenta | A380 / B747 |
| `C_CAT_REGIONAL` | green | regional jets |
| `C_CAT_TURBOPROP` | red | turboprops |

All values encoded with G/B swap applied.

## Side bars

- **Left column (px 0)**: altitude in deep blue. Non-linear: `ALT_MIN_FT` (300 ft) -> 2px, `ALT_MID_FT` (2,000 ft) -> 23px (75% of bar), `ALT_MAX_FT` (30,000 ft) -> 30px.
- **Right column (px 63)**: speed in light blue. Maps `SPD_MIN_KT` (100 kt) -> 2px, `SPD_MAX_KT` (450 kt) -> 30px.
- Both 0px when no valid aircraft is tracked.

## Key tuneable constants (config.h)

| Constant | Default | Effect |
|----------|---------|--------|
| `REFRESH_MS` | 30000 | Full fetch cycle duration (ms) |
| `TYPE_FLIP_PX` | 32 (`MATRIX_W / 2`) | Progress pixel at which type name replaces callsign (non-GA only) |
| `ALT_MIN_FT` / `ALT_MID_FT` / `ALT_MAX_FT` | 300 / 2000 / 30000 | Altitude bar clamp range |
| `SPD_MIN_KT` / `SPD_MAX_KT` | 100 / 450 | Speed bar clamp range |
| `UTC_OFFSET_HOURS` | 11 | Local UTC offset for NTP clock (11 = AEDT; 10 = AEST) |
| `WEATHER_REFRESH_MS` | 600000 | Weather re-fetch interval (ms) |
| `CLOCK_UPDATE_MS` | 1000 | Weather page clock redraw interval (ms) |
| `BRIGHTNESS_DEFAULT` | 77 | Software brightness (0–255); ~30% — change and reflash to adjust |
| `FIRMWARE_VERSION` | 1 | OTA version stamp — must match or be less than server value to skip update |

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
