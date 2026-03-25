# Overhead // Live Aircraft Tracker

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/R6R61VRQHC)

Shows which aircraft are flying directly above any location in the world. Single HTML file, no dependencies, no build step.

Live: [overheadtracker.com](https://www.overheadtracker.com/)

---

## Features

- Worldwide location search with adjustable geofence radius (2–20 km) and altitude floor (200–5,000 ft)
- Settings persist across sessions; share any location via `?location=` URL param
- Live ADS-B data from 3 sources raced in parallel (adsb.lol, adsb.fi, airplanes.live), routed through a caching proxy at `api.overheadtracker.com`
- 15-second auto-refresh with manual override
- Flight phase detection: LANDING, TAKING OFF, APPROACH, DESCENDING, CLIMBING, CRUISING, OVERHEAD
- Intercept prediction: INBOUND (overhead in ~X min), CROSSING (closest in ~X min), DIRECTLY OVERHEAD NOW, RECEDING
- Interesting flight detection with badges: military (callsign prefix or type), private/charter, bizjet, high speed (>520 kt), very high altitude (>43,000 ft), anonymous (no callsign)
- Session flight log recording all flights seen with callsign, type, phase, airline, altitude, speed, distance, timestamp
- Statistics panel: total seen, currently in range, busiest airline, most common type, highest altitude, fastest speed, closest approach, interesting count
- Route display with departure/arrival airports and city names via adsbdb.com (non-blocking lookup, cached)
- Weather strip showing temperature, conditions, wind, humidity, UV index (Open-Meteo via proxy, refreshed every 15 min)
- Airline name from ICAO callsign prefix (colour-coded by brand); full aircraft type names (B789 → B787-9, A20N → A320neo, etc.)
- Emergency squawk highlighting: 7700 / 7600 / 7500 shown in red with a warning
- Leaflet map with geofence circle, aircraft dot, and speed-scaled heading vector
- Aircraft photo from [Planespotters.net](https://planespotters.net) by registration
- Altitude bar alongside the info block
- Phase colour bleed: flight phase colour tints the info block border and glow
- Unit toggle (KM/MI) for distances; altitude, speed, and vertical rate use standard aviation units (FT, KT, FPM)
- CRT scanline aesthetic
- Keyboard navigation, NEAREST button, optional radar ping sound
- Mobile-responsive
- Demo mode: `?demo=true&scenario=...` with 7 scenarios (busy, quiet, crowded, emergency, approach_rush, single, mixed)

---

## How it works

Geocodes the entered location via Nominatim, then queries the proxy for aircraft within a radius 4x the geofence size. The proxy races three ADS-B APIs in parallel and returns the fastest response, caching results for 45 seconds with coordinate bucketing. The web app filters down to aircraft inside the geofence and above the altitude floor, sorts by distance, and renders. Repeats every 15 seconds; any in-flight request is aborted before starting the next.

Route lookups (departure/arrival airports) fire asynchronously via adsbdb.com and update the display when they resolve. Results are cached for 30 minutes. Weather data comes from Open-Meteo, refreshed every 15 minutes.

---

## Usage

The web app is a single HTML file with no build step and no API keys. It relies on the public proxy at `api.overheadtracker.com` for flight and weather data, so you don't need to host any backend yourself.

```bash
git clone https://github.com/greystoke1337/localized-air-traffic-tracker.git
cd localized-air-traffic-tracker
open index.html
```

The app calls `api.overheadtracker.com` (flight + weather data), `nominatim.openstreetmap.org` (geocoding), and `api.planespotters.net` (photos). All HTTPS, all CORS-enabled.

---

## Proxy server

The Railway-hosted proxy at `api.overheadtracker.com` sits between the web app and upstream APIs.

- Races 3 ADS-B APIs in parallel (adsb.lol, adsb.fi, airplanes.live), uses the fastest response
- 45-second flight cache with coordinate bucketing (nearby clients share cache entries)
- 30-minute route cache for adsbdb.com lookups
- Weather passthrough from Open-Meteo
- Rate limiting: 100 requests/min per IP
- Health check at `/status`
- Daily flight logging with `/stats` and `/report` endpoints

Source: [`server/`](server/)

---

## ESP32 hardware displays

Two standalone physical trackers that poll the proxy — no browser needed.

### Echo — Freenove FNK0103S (4.0", 480×320)

**Hardware:** Freenove FNK0103S (ESP32 + 4" ST7796 SPI touchscreen), optional 3D-printed enclosure (STL/STEP in [`tracker_live_fnk0103s/enclosure/`](tracker_live_fnk0103s/enclosure/))

**What it shows:** Header bar, nav bar with touch buttons (WX / GEO / CFG), flight card (callsign, airline name colour-coded by brand, aircraft type, route), and a 4-column dashboard: PHASE | ALT (with vertical rate) | SPEED | DIST. Cycles through overhead flights every 8 seconds. Each of the 8 flight phases (TAKEOFF, CLIMBING, CRUISING, DESCEND, APPROACH, LANDING, OVERHEAD, UNKNOWN) has its own colour in the dashboard.

**Nav bar controls:**

- **WX** — weather screen showing temperature, humidity, wind, and conditions
- **GEO** — cycles geofence radius: 5 km / 10 km / 20 km
- **CFG** — launches the captive portal for Wi-Fi and location configuration

**Route display:** departure and arrival with airport city names from a built-in lookup table.

**Emergency squawk handling:** 7700 / 7600 / 7500 triggers a flashing red banner (MAYDAY, NORDO, or HIJACK). The layout compacts automatically to prevent overlap.

**Libraries** (Arduino Library Manager): `LovyanGFX`, `ArduinoJson`, `ArduinoOTA`, `SD`

**Before flashing:** Copy `secrets.h.example` to `secrets.h` and set your default WiFi credentials. On first boot, the captive portal lets you configure WiFi and location (stored in NVS). `PROXY_HOST` in `tracker_live_fnk0103s.ino` defaults to `api.overheadtracker.com`.

```bash
./build.sh                  # compile + auto-detect port + upload via USB
./build.sh compile          # compile only
./build.sh upload           # upload last build via USB
./build.sh ota              # compile + upload over Wi-Fi (OTA)
./build.sh monitor          # serial monitor
./build.sh send <cmd>       # send debug command, print JSON response
./build.sh validate         # compile with all warnings + safety checks
./build.sh test             # run desktop logic tests (no hardware needed)
./build.sh safe             # test + validate (full pre-push check)
```

**Over-the-air updates:** after the first USB flash, the device advertises itself as `overhead-tracker.local` on the local network via mDNS. Run `./build.sh ota` (or press **Ctrl+Shift+B** in VS Code) to compile and upload wirelessly. The TFT displays a green progress bar during the update.

### Foxtrot — Waveshare ESP32-S3-Touch-LCD-4.3 (4.3", 800×480)

**Hardware:** Waveshare ESP32-S3-Touch-LCD-4.3 (ESP32-S3 + 4.3" ST7262 IPS parallel RGB display, GT911 capacitive touch, 16 MB flash, 8 MB PSRAM)

Same feature set as Echo, scaled proportionally for the larger 800×480 display. Key hardware differences: capacitive touch (no calibration needed), CH422G I/O expander for backlight control, PSRAM for larger buffers.

**Libraries** (Arduino Library Manager): `LovyanGFX`, `ArduinoJson`, `ArduinoOTA`

```bash
arduino-cli compile --fqbn "esp32:esp32:waveshare_esp32_s3_touch_lcd_43B:PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB" tracker_foxtrot/tracker_foxtrot.ino
arduino-cli upload --fqbn "..." --port COM7 --input-dir /tmp/tracker-foxtrot-build tracker_foxtrot/tracker_foxtrot.ino
arduino-cli monitor --port COM7 --config "baudrate=115200"
```

### Web firmware flasher

`flash.html` is a browser-based firmware update tool. End users can flash new Foxtrot firmware with **zero development tools installed** — just Chrome/Edge and a USB cable.

Open [overheadtracker.com/flash](https://overheadtracker.com/flash), plug in the device, click Connect, click Flash. Uses the Web Serial API and [esptool-js](https://github.com/nicknisi/esptool-js) to write firmware directly from the browser.

**Developer workflow** (after making firmware changes):

```bash
./tools/package-firmware.sh    # compile + copy binaries to firmware/
git push                       # auto-deploys to GitHub Pages
```

The packaging script compiles Foxtrot, copies the 4 binary files (bootloader, partitions, boot_app0, app) to `firmware/`, and updates `firmware/manifest.json` with the version from `config.h`.

**Driver notes:** Windows 10/11 auto-installs the CH343 USB driver. macOS requires a one-time driver install from WCH (instructions on the flash page). Linux works out of the box.

### Resilience

The firmware uses a 3-tier fallback cascade: Railway proxy → direct airplanes.live API (HTTPS) → SD card cache. Proxy calls use a 3-second TCP connect timeout so the device boots cleanly even when the proxy is unreachable — no watchdog crash loop. The 16 KB JSON document is allocated once at startup and reused every cycle to prevent heap fragmentation on long-running sessions.

### Mock proxy tool

`tools/mock-proxy.js` is a zero-dependency Node.js server for testing firmware resilience without the real proxy.

```bash
node tools/mock-proxy.js [mode] [port]
```

| Mode | Behavior |
|------|----------|
| `normal` | Valid flight + weather JSON (default) |
| `timeout` | Accepts TCP, never responds |
| `error503` | Returns 503 Service Unavailable |
| `error502` | Returns 502 Bad Gateway |
| `corrupt` | Returns 200 with broken JSON |
| `partial` | Returns 200, drops connection mid-body |
| `slow` | Waits 4 seconds before valid response |

### TFT preview tool

`tft-preview.html` is a browser-based simulator of the ESP32 display. Open it to preview layout changes before flashing.

It mirrors the firmware's rendering logic: same pixel coordinates, same RGB565 colour palette, same lookup tables (airlines, aircraft types, airports). Interactive controls let you test every combination of flight phase, squawk code, route length, and altitude.

```bash
open tft-preview.html   # or just double-click
```

---

## Pi display — Raspberry Pi 3B+ (3.5" TFT, 480×320)

A headless Raspberry Pi display that renders to a 3.5" TFT via framebuffer.

**Hardware:** Raspberry Pi 3B+ with 3.5" TFT on `/dev/fb1`

**What it shows:** Two auto-rotating pages (15-second cycle):
- **Flights page** — up to 2 closest flights with callsign, altitude, distance, route, and phase (colour-coded). Daily flight count shown in the header.
- **Stats page** — Railway proxy health (uptime, requests, cache hit rate, errors, clients), API status for all 3 ADS-B sources, and a 24-hour traffic histogram with peak-hour highlighting.

**Weather strip** on the flights page: temperature, conditions, wind speed and direction.

**Timing:** 10-second flight refresh, 5-minute weather refresh.

**CRT-style colour scheme** with phase colour-coding matching the web app and ESP32 displays.

**Configuration:** set `PROXY_URL` environment variable to point at a different proxy instance. Edit `HOME_LAT`, `HOME_LON`, and `RADIUS_KM` at the top of `display.py` for your location.

Source: [`pi-display/`](pi-display/)

---

## Data sources

| Source | Data | Key required |
|---|---|---|
| [api.overheadtracker.com](https://api.overheadtracker.com) | Caching proxy (races 3 ADS-B APIs, route + weather passthrough) | No |
| [adsb.lol](https://api.adsb.lol) | Live ADS-B positions (via proxy, raced) | No |
| [adsb.fi](https://opendata.adsb.fi) | Live ADS-B positions (via proxy, raced) | No |
| [airplanes.live](https://api.airplanes.live) | Live ADS-B positions (via proxy, raced) | No |
| [adsbdb.com](https://www.adsbdb.com) | Route lookups, departure/arrival airports (via proxy, cached 30 min) | No |
| [Open-Meteo](https://open-meteo.com) | Weather data (via proxy) | No |
| [Nominatim / OpenStreetMap](https://nominatim.openstreetmap.org) | Location geocoding | No |
| [Planespotters.net](https://planespotters.net) | Aircraft photos | No |
| [Carto](https://carto.com) / OpenStreetMap | Map tiles | No |

---

## Roadmap

- [ ] Push notification when a specific flight appears overhead

---

## License

MIT — do whatever you want with it.
