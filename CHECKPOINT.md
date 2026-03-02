# Project Checkpoint — 2026-03-03

## Git State

- **Branch**: `master` (single branch, no feature branches)
- **HEAD**: `45eef08` — "Untrack settings.local.json and stale .playwright-mcp log"
- **Status**: Clean working tree, fully pushed to origin
- **Live URL**: https://overheadtracker.com (GitHub Pages, auto-deploy on push)

### Recent Commits (20)

```
45eef08 Untrack settings.local.json and stale .playwright-mcp log
b2c87e6 Remove TFT preview PNGs from repo and update .gitignore
895f82c Improve TFT display spacing: show 2 flights, remove dashboard histogram
f33a082 Fix TFT ghost text, WDT crashes, and add unique phase colors
127674c Add two-page TFT display with live flights, weather, and system health
4e1773b Add airline brand color coding and fix TFT simulator bugs
f177cfc Add ADS-B API fallback chain: adsb.lol → adsb.fi → airplanes.live
b04faf2 Expand airline/airport databases for Sydney and optimize firmware display
be5ee5a Add serial debug console, fix CLEAR SKIES crash, force IPv4 in proxy, and update docs
d059146 Clean up dead code and tighten test assertion
6e9e506 Simplify ESP32 firmware and add debugging/testing tools
d1aa563 Document ESP32 heap fragmentation fix in README and SPEC
2208e9c Fix ESP32 heap fragmentation crash after ~5 min of operation
d6b0390 Fix logFlight() to use route field instead of dep/arr
466996f Vendor external libs and offload route resolution to Pi proxy
780c168 Fix ESP32 crash when Pi proxy is offline
786f058 Shorten phase labels and reduce max geofence to 20km
74a8fc7 tft preview tools
1cac921 ESP32 UX overhaul, TFT preview tool, and docs update
ec89c86 Add pi-proxy source files, secrets.h pattern, and update docs
```

---

## File Inventory

### Web App

| File | Lines | Size |
|------|-------|------|
| `index.html` | 1,848 | 123 KB |

46 JS functions. Vanilla JS + Leaflet (no framework, no build step). Key functions: `fetchFlights()`, `render()`, `initMap()`, `flightPhase()`, `detectInteresting()`, `fetchPhoto()`, `geocodeLocation()`, `fetchWeather()`.

### Pi Proxy

| File | Lines | Size |
|------|-------|------|
| `server.js` | 629 | 25 KB |
| `display.py` | 403 | 15 KB |
| `dashboard.html` | — | 9.6 KB |
| `package.json` | — | 296 B |

Dependencies: `express@^5.2.1`, `node-fetch@^3.3.2`

**server.js routes** (8 endpoints):

| Line | Route | Purpose |
|------|-------|---------|
| 247 | `POST /proxy/toggle` | Toggle proxy on/off |
| 254 | `GET /` | Dashboard HTML |
| 437 | `GET /status` | Proxy status JSON |
| 451 | `GET /flights` | Cached flight data (async) |
| 528 | `GET /stats` | Traffic statistics |
| 545 | `GET /peak` | Peak traffic data |
| 587 | `GET /weather` | Weather data (async) |

**server.js key constants**: `PORT=3000`, `CACHE_MS=10000` (10s), `ROUTE_CACHE_MS=1800000` (30min)

**display.py config**: `HOME_LAT=-33.8530`, `HOME_LON=151.1410`, `RADIUS_KM=15`, `REFRESH=10s`, `PAGE_ROTATE_SEC=15`, `WEATHER_REFRESH=300s`, renders to `/dev/fb1` via Pygame

### ESP32 Firmware

| File | Lines | Size |
|------|-------|------|
| `tracker_live_fnk0103s.ino` | 465 | 16 KB |
| `display.ino` | 500 | 17 KB |
| `network.ino` | 435 | 17 KB |
| `wifi_setup.ino` | 209 | 7.3 KB |
| `touch.ino` | 144 | 4.3 KB |
| `helpers.ino` | 110 | 3.8 KB |
| `serial_cmd.ino` | 95 | 4.2 KB |
| `sd_config.ino` | 93 | 2.9 KB |
| `globals.h` | 131 | 5.0 KB |
| `lookup_tables.h` | 57 | 3.9 KB |
| `config.h` | 60 | 3.1 KB |
| `types.h` | 52 | 1.8 KB |
| `secrets.h` | 3 | 97 B |
| **Total** | **2,354** | — |

**Key firmware constants**:
- `PROXY_HOST = "192.168.86.24"` (hardcoded), `PROXY_PORT = 3000`
- `g_jsonDoc(16384)` — global DynamicJsonDocument, reused via `.clear()`
- `WX_REFRESH_SECS = 900` (15 min), `REFRESH_SECS = 15`, `CYCLE_SECS = 8`
- Screen: 480x320, `HDR_H=28`, `NAV_H=36`, `FOOT_H=20`, `CONTENT_Y=64`, `CONTENT_H=236`
- `DIRECT_API_MIN_HEAP = 40000`, `DIRECT_API_TIMEOUT = 8000ms`
- `flights[20]` / `newFlights[20]` — max 20 flight slots
- `MAX_LOGGED = 200`
- 8 airline brand colors (AL_RED through AL_VIOLET)
- 3 geofence presets: 5, 10, 20 km

### TFT Preview

| File | Lines | Size |
|------|-------|------|
| `tft-preview.html` | 1,103 | 40 KB |

Canvas-based 480x320 simulator with CRT scanline overlay. Dark amber/orange aesthetic.

### Tests

| File | Lines | Size |
|------|-------|------|
| `test_flight_logic.c` | — | 9.0 KB |
| `test_parsing.cpp` | — | 8.8 KB |

6 JSON fixtures: `boundary_geofence.json`, `emergency_squawk.json`, `empty_response.json`, `malformed.json`, `no_altitude.json`, `normal_response.json`

### Other Files

| File | Size |
|------|------|
| `build.sh` | 15 KB |
| `PI_PROXY_SETUP.md` | 9.3 KB |
| `SPEC.md` | 6.7 KB |
| `README.md` | 6.7 KB |
| `CLAUDE.md` | 6.8 KB |
| `tools/mock-proxy.js` | 5.5 KB |
| `CNAME` | 23 B |

### .gitignore

```
logs/
.playwright-mcp/
__pycache__/
tft-preview-*.png
.claude/settings.local.json
tracker_live_fnk0103s/secrets.h
```

---

## Total Codebase Size

~6,337 lines across main source files. Largest: `index.html` (1,848), `tft-preview.html` (1,103), `server.js` (629), `display.ino` (500).
