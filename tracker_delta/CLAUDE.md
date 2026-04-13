# Delta — Waveshare ESP32-S3-Touch-LCD-3.49 (320×240, LVGL v8)

## Build & Flash

```bash
./build.sh delta           # compile + upload to COM8
./build.sh delta compile   # compile only
```

- FQBN: `esp32:esp32:waveshare_esp32_s3_touch_lcd_3_49` (check `build.sh` for exact string)
- COM8 (running)
- Serial monitor: 115200 baud

## Rendering Stack

Delta uses **LVGL v8** via `lvgl_port.c` — not TFT_eSPI or LovyanGFX.

- All widget creation and LVGL object handles live in `lvgl_port.c`
- UI updates go through `lvgl_update_*()` functions defined in `lvgl_port.h`
- Do NOT call LVGL APIs directly from `.ino` files — always go through the port layer

## Network / JSON Pattern

Each fetch endpoint owns a **static `JsonDocument`** allocated once and reused:

```cpp
static JsonDocument s_weather_doc;  // reused every fetchWeather() call
```

- **Do not** add per-call `JsonDocument` + `http.getString()` pairs — this pattern was removed to eliminate heap fragmentation
- Parse directly from `http.getStream()` with `deserializeJson(doc, http.getStream())`
- The ArduinoJson filter for `fetchNearest()` is also built once via a `static bool s_filter_ready` flag

## Captive Portal

The portal loop is **bounded** — exits after `PORTAL_TIMEOUT_MS` (5 min) then calls `ESP.restart()`. It no longer loops indefinitely. `WebServer` and `DNSServer` are `static` locals inside `startCaptivePortal()`.

## assert() Usage

`assert()` is used for preconditions (NASA Power of Ten style):
- `assert(strlen(PROXY_HOST) > 0)` before any HTTPS fetch
- `assert(ssid != NULL && strlen(ssid) > 0)` in `saveWiFiConfig()`
- `assert(out_dist != NULL)` in `findNearest()`

Do not remove asserts — they document invariants and catch misuse early.

## File Layout

| File | Purpose |
|------|---------|
| `tracker_delta.ino` | `setup()` / `loop()`, refresh timers as static locals |
| `config.h` | All timing constants (`WEATHER_REFRESH_MS`, `PORTAL_TIMEOUT_MS`, `MAX_AC_COUNT`, etc.) |
| `lvgl_port.c` / `lvgl_port.h` | LVGL initialisation, all widget handles, `lvgl_update_*()` API |
| `network.ino` | `fetchWeather()`, `fetchNearest()`, `fetchReceiver()`, `fetchServer()`; static JsonDocs |
| `wifi_setup.ino` | `loadWiFiConfig()`, `saveWiFiConfig()`, `connectWiFi()`, `startCaptivePortal()` |
| `user_config.h` | `HOME_LAT_DEFAULT`, `HOME_LON_DEFAULT`, `LOCATION_NAME`, `PROXY_HOST` |

## Key Constants (config.h)

| Constant | Default | Effect |
|----------|---------|--------|
| `WEATHER_REFRESH_MS` | 600000 | Weather re-fetch interval |
| `RECEIVER_REFRESH_MS` | 30000 | Local receiver stats interval |
| `SERVER_REFRESH_MS` | 60000 | Proxy server stats interval |
| `NEAREST_REFRESH_MS` | 10000 | Nearest aircraft interval |
| `PORTAL_TIMEOUT_MS` | 300000 | Captive portal auto-restart timeout |
| `MAX_AC_COUNT` | 100 | Max aircraft scanned in `fetchNearest()` |
