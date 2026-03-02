# Overhead // Live Aircraft Tracker

Shows which aircraft are flying directly above any location in the world. Single HTML file, no dependencies, no build step.

Live: [overheadtracker.com](https://www.overheadtracker.com/)

---

## Features

- Worldwide location search with adjustable geofence radius (2–20 km) and altitude floor (200–5,000 ft)
- Settings persist across sessions; share any location via `?location=` URL param
- Live ADS-B data via [airplanes.live](https://airplanes.live), routed through a self-hosted proxy — no API key needed
- 15-second auto-refresh with manual override
- Flight phase detection: LANDING, TAKEOFF, APPROACH, DESCENDING, CLIMBING, OVERHEAD (ESP32 adds CRUISING and UNKNOWN)
- Airline name from ICAO callsign prefix (46 airlines, colour-coded by brand — QANTAS red, CATHAY green, EMIRATES gold, etc.); full aircraft type names (B789 → B787-9, A20N → A320neo, etc.)
- Emergency squawk highlighting — 7700 / 7600 / 7500 shown in red with a warning
- Leaflet map with geofence circle, aircraft dot, and speed-scaled heading vector
- Aircraft photo from [Planespotters.net](https://planespotters.net) by registration
- Phase colour bleed on the info block, altitude bar, CRT scanline aesthetic
- Keyboard navigation, NEAREST button, session flight log, optional radar ping sound
- Mobile-responsive

---

## How it works

Geocodes the entered location via Nominatim, then queries the Pi proxy for aircraft within a radius 4x the geofence size. Filters down to aircraft inside the geofence and above the altitude floor, sorts by distance, and renders. Repeats every 15 seconds; any in-flight request is aborted before starting the next.

---

## Usage

No server, no build step, no API keys.

```bash
git clone https://github.com/greystoke1337/localized-air-traffic-tracker.git
cd localized-air-traffic-tracker
open index.html
```

The app calls `api.overheadtracker.com` (flight data), `nominatim.openstreetmap.org` (geocoding), and `api.planespotters.net` (photos). All HTTPS, all CORS-enabled.

---

## ESP32 hardware display

Standalone physical tracker on the **Freenove FNK0103S** (ESP32 + 4" 480×320 ST7796 touchscreen). Polls the local proxy directly over LAN, no browser needed.

**Hardware:** Freenove FNK0103S, optional 3D-printed enclosure (STL/STEP in [`tracker_live_fnk0103s/enclosure/`](tracker_live_fnk0103s/enclosure/))

**What it shows:** Header bar, nav bar with touch buttons (WX / GEO / CFG), flight card (callsign, airline name colour-coded by brand, aircraft type, route), and a 4-column dashboard: PHASE | ALT (with vertical rate) | SPEED | DIST. Cycles through overhead flights every 8 seconds. Each of the 8 flight phases (TAKEOFF, CLIMBING, CRUISING, DESCEND, APPROACH, LANDING, OVERHEAD, UNKNOWN) has its own colour in the dashboard.

**Nav bar controls:**

- **WX** — weather screen showing temperature, humidity, wind, and conditions
- **GEO** — cycles geofence radius: 5 km / 10 km / 20 km
- **CFG** — launches the captive portal for Wi-Fi and location configuration

**Route display:** departure and arrival with airport city names from a built-in lookup table.

**Emergency squawk handling:** 7700 / 7600 / 7500 triggers a flashing red banner (MAYDAY, NORDO, or HIJACK). The layout compacts automatically to prevent overlap.

**Libraries** (Arduino Library Manager): `TFT_eSPI` (Freenove version), `ArduinoJson`, `ArduinoOTA`, `SD`

**Before flashing:** Copy `secrets.h.example` to `secrets.h` and set your default WiFi credentials. On first boot, the captive portal lets you configure WiFi and location (stored in NVS). `PROXY_HOST` in `tracker_live_fnk0103s.ino` must be hardcoded to your Pi's local IP — it has no NVS/SD fallback.

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

### Resilience

The firmware uses a 3-tier fallback cascade: Pi proxy → direct airplanes.live API (HTTPS) → SD card cache. Proxy calls use a 3-second TCP connect timeout so the device boots cleanly even when the Pi is powered off — no watchdog crash loop. The 16 KB JSON document is allocated once at startup and reused every cycle to prevent heap fragmentation on long-running sessions.

### Mock proxy tool

`tools/mock-proxy.js` is a zero-dependency Node.js server for testing firmware resilience without the real Pi proxy.

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

## Data sources

| Source | Data | Key required |
|---|---|---|
| [api.overheadtracker.com](https://api.overheadtracker.com) | Flight data proxy (Raspberry Pi + Cloudflare Tunnel) | No |
| [airplanes.live](https://api.airplanes.live) | Live ADS-B positions (via proxy) | No |
| [Nominatim / OpenStreetMap](https://nominatim.openstreetmap.org) | Location geocoding | No |
| [Planespotters.net](https://planespotters.net) | Aircraft photos | No |
| [Carto](https://carto.com) / OpenStreetMap | Map tiles | No |

---

## Roadmap

- [ ] Push notification when a specific flight appears overhead

---

## License

MIT — do whatever you want with it.
