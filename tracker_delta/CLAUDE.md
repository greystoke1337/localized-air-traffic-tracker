# Delta — Waveshare ESP32-S3-Touch-LCD-3.49 (320×240, LVGL v9)

## Build & Flash

```bash
./build.sh delta           # compile + upload to COM8
./build.sh delta compile   # compile only
```

- FQBN: `esp32:esp32:esp32s3:PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,...` (see `build.sh` for exact string)
- COM8 (running)
- Serial monitor: 115200 baud

## Serial Debug Workflow

**IMPORTANT:** `arduino-cli monitor` does NOT set DTR on Windows — HWCDC drops all output.
Use PowerShell (`tools/serial_monitor.ps1`) or the Arduino IDE Serial Monitor instead.

```powershell
# Monitor (interactive, Ctrl-C to stop):
.\tools\serial_monitor.ps1 -Port COM8 -Seconds 300

# Capture log:
.\tools\serial_monitor.ps1 -Port COM8 -Seconds 120 | Tee-Object logs/delta-debug.log
```

Serial commands (type in monitor):

```
heap      query heap stats (free, max block, fragmentation)
wifi      SSID, RSSI, IP, MAC
config    dump all config constants
diag      heap + wifi + uptime combined
fetch     trigger fetchNearest() immediately
state     uptime + heap + wifi status
restart   reboot device
help      list all commands
```

Serial output format: `[uptime_s.ms][TAG] message`

Tags: `BOOT`, `WIFI`, `WX`, `NEAR`, `SRV`, `RECV`, `SYS`

### Why arduino-cli monitor fails

Delta uses `USBMode=hwcdc` (ESP32-S3 internal USB JTAG/Serial controller) with `CDCOnBoot=cdc`.
HWCDC only transmits when a USB host has the port open with DTR asserted.
`arduino-cli monitor` on Windows opens the port but does not assert DTR → device sees no host → all
`Serial.print()` calls are silently dropped. PowerShell's `[System.IO.Ports.SerialPort]` sets DTR
by default and works correctly.

## Rendering Stack

Delta uses **LVGL v9** via `lvgl_port.c` — not TFT_eSPI or LovyanGFX.

- All widget creation and LVGL object handles live in `lvgl_port.c`
- UI updates go through `lvgl_update_*()` functions defined in `lvgl_port.h`
- Do NOT call LVGL APIs directly from `.ino` files — always go through the port layer

## Network / JSON Pattern

Each fetch endpoint owns a **static `JsonDocument`** allocated once and reused:

```cpp
static JsonDocument s_weather_doc;  // reused every fetchWeather() call
```

- All **HTTPS** fetches (`fetchWeather`, `fetchNearest`, `fetchServer`) use `http.getString()` then parse from the `String` — **not** `http.getStream()`. `WiFiClientSecure::available()` returns 0 at SSL record boundaries (every 4 KB), which ArduinoJson misreads as EOF, producing `IncompleteInput` errors.
- `fetchReceiver` (plain HTTP) also uses `getString()` for consistency, plus `http.setConnectTimeout(3000)` so a missing Pi fails in 3 s instead of the default ~5 s.
- The ArduinoJson filter for `fetchNearest()` is built once via a `static bool s_filter_ready` flag and reused on every call.
- Do **not** switch HTTPS fetches back to `getStream()` — the SSL record boundary bug will return.

## Captive Portal

The portal loop is **bounded** — exits after `PORTAL_TIMEOUT_MS` (5 min) then calls `ESP.restart()`. It no longer loops indefinitely. `WebServer` and `DNSServer` are `static` locals inside `startCaptivePortal()`.

## assert() Usage

`assert()` is used for preconditions (NASA Power of Ten style):
- `assert(strlen(PROXY_HOST) > 0)` before any HTTPS fetch
- `assert(ssid != NULL && strlen(ssid) > 0)` in `saveWiFiConfig()`
- `assert(out_dist != NULL)` in `findNearest()`

Do not remove asserts — they document invariants and catch misuse early.

## Callsign Filtering

Specific callsigns can be excluded from `findNearest()` in `network.ino`. Currently filtered:

- `SSM1`, `SSM2` — calibration transponders, not real traffic

To add more, extend the `strncmp` chain in the `findNearest` loop.

## Dashboard Placeholder Behaviour

All column values initialise to `"--"` until the first successful fetch. On fetch failure the display keeps the last known good value. There is no fake hardcoded data — if a column shows real-looking values, they are live.

## File Layout

| File | Purpose |
|------|---------|
| `tracker_delta.ino` | `setup()` / `loop()`, refresh timers as static locals |
| `config.h` | Location, proxy host, all timing constants, `MAX_AC_COUNT` |
| `user_config.h` | GPIO pin assignments, LVGL/display hardware config |
| `lvgl_port.c` / `lvgl_port.h` | LVGL initialisation, all widget handles, `lvgl_update_*()` API |
| `network.ino` | `fetchWeather()`, `fetchNearest()`, `fetchReceiver()`, `fetchServer()`; static JsonDocs; callsign filter |
| `wifi_setup.ino` | `loadWiFiConfig()`, `saveWiFiConfig()`, `connectWiFi()`, `startCaptivePortal()` |
| `serial_cmd.ino` | `logTs()`, JSON debug commands, `checkSerialCmd()` |

## Key Constants (config.h)

| Constant | Default | Effect |
|----------|---------|--------|
| `HOME_LAT_DEFAULT` | -33.8614 | Home latitude for geofence centre |
| `HOME_LON_DEFAULT` | 151.1397 | Home longitude for geofence centre |
| `LOCATION_NAME` | "RUSSELL LEA" | Displayed in weather bar |
| `WEATHER_REFRESH_MS` | 300000 | Weather re-fetch interval (5 min) |
| `RECEIVER_REFRESH_MS` | 30000 | Local receiver stats interval |
| `SERVER_REFRESH_MS` | 60000 | Proxy server stats interval |
| `NEAREST_REFRESH_MS` | 10000 | Nearest aircraft interval |
| `PORTAL_TIMEOUT_MS` | 300000 | Captive portal auto-restart timeout |
| `MAX_AC_COUNT` | 100 | Max aircraft scanned in `fetchNearest()` |
