# Overhead Tracker — Product Specification

## What it is

Real-time aircraft tracking that answers one question: "What planes are flying above me right now?" Set a location anywhere in the world, and it fetches live ADS-B data, filters to aircraft within your radius and altitude floor, and shows them sorted nearest-first.

---

## Why it exists

FlightRadar24 and similar apps show the whole world — you have to go find your location. This is location-first: you set one spot and only see aircraft directly above it. The secondary goal is a physical hardware display that needs no phone or browser.

---

## System architecture

```
┌─────────────────────┐     HTTPS      ┌────────────────────────────────┐
│  Web app            │ ──────────────▶│ api.overheadtracker.com         │
│  (GitHub Pages)     │                │ (Cloudflare Tunnel →            │
│  index.html         │                │  Raspberry Pi proxy :3000)      │
└─────────────────────┘                └──────────────┬─────────────────┘
                                                       │ HTTPS
┌─────────────────────┐     HTTP LAN   │               ▼
│  ESP32 TFT display  │ ──────────────▶│        airplanes.live ADS-B API
│  (Freenove FNK0103S)│                │
└─────────────────────┘     ◀──────────┘ cached (10s TTL)
```

**Web app (`index.html`)** — single HTML file, no build step, no framework. Deployed to GitHub Pages at `overheadtracker.com` on push to `master`.

**Raspberry Pi proxy (`server.js`)** — Node.js/Express on a Pi 3B+ at `192.168.x.x:3000`. Caches airplanes.live responses for 10 seconds so multiple clients can refresh at 15-second intervals without hitting rate limits. Exposed publicly via Cloudflare Tunnel. Managed by PM2.

**ESP32 TFT display (`.ino` firmware)** — Freenove FNK0103S, 4" 480×320 ST7796 touchscreen. Polls the local proxy over LAN, independent of the web app. Displays one flight at a time, cycling every 8 seconds.

---

## Data pipeline

1. **Geocode** — location string resolved to lat/lon via Nominatim. No API key.
2. **Fetch** — proxy queried with `lat`, `lon`, `radius` (4x the geofence in nautical miles).
3. **Filter** — reduced to aircraft within the geofence radius and above the altitude floor.
4. **Sort** — by haversine distance, closest first.
5. **Render** — flight info, map, photo, altitude bar, phase colour.
6. **Repeat** — every 15 seconds; any in-flight request is aborted before the next starts.

---

## Feature set (web app)

| Category | Features |
|---|---|
| **Location** | Worldwide search, persisted in `localStorage`, shareable via `?location=` URL param |
| **Filtering** | Geofence radius slider (2–20 km), altitude floor slider (200–5,000 ft) |
| **Flight data** | Callsign, airline name colour-coded by brand (from ICAO prefix, 46 airlines across 8 brand colours), full aircraft type name, altitude (FL or QNH ft), ground speed, heading, vertical rate |
| **Flight phase** | TAKING OFF / CLIMBING / DESCENDING / APPROACH / LANDING / OVERHEAD — 6 phases derived from altitude + vertical speed (ESP32 adds CRUISING and UNKNOWN for 8 total) |
| **Map** | Leaflet with dark CartoDB tiles, geofence circle, aircraft dot, dashed line to location, speed-scaled heading vector with chevron |
| **Photo** | Aircraft registration photo from Planespotters.net, with halftone overlay |
| **Alerts** | Emergency squawk highlighting (7700/7600/7500), optional radar ping sound on new #1 aircraft |
| **Navigation** | Arrow key browsing, NEAREST button, session flight log |
| **UX** | CRT scanline aesthetic, phase colour bleed on info block border, altitude bar, mobile-responsive |

## Feature set (ESP32 display)

| Category | Features |
|---|---|
| **Config** | Captive portal on first boot — set Wi-Fi SSID/password and location; geocodes via Nominatim and stores to NVS |
| **Display** | Structured layout: header, nav bar, flight card, 4-column dashboard (PHASE / ALT+v/rate / SPEED / DIST), footer. 480×320 TFT at 15s refresh / 8s cycle. Airline names colour-coded by brand (8 colours: red, rose, orange, gold, green, teal, sky blue, violet). Route display (departure > arrival) with city names from built-in airport lookup table. 8 flight phases (TAKEOFF / CLIMBING / CRUISING / DESCEND / APPROACH / LANDING / OVERHEAD / UNKNOWN), each with a distinct color applied to the phase dashboard column. Emergency squawk banners (7700/7600/7500) trigger a flashing red alert with compact layout. |
| **Touch** | Nav bar with three touch buttons: WX (weather screen), GEO (cycles geofence: 5 km / 10 km / 20 km), CFG (opens captive portal) |
| **Weather** | Temperature, humidity, wind speed/direction, conditions — accessed via WX button; data from Open-Meteo via Pi proxy, refreshed every 15 minutes |
| **Preview** | `tft-preview.html` — browser-based canvas simulator that mirrors firmware rendering; verify layout changes before flashing |
| **Resilience** | 3 s TCP connect timeout on proxy; falls back proxy → direct API → SD card cache (`cache.json`). WDT-safe boot — no crash if Pi is offline. Global JSON document (16 KB, allocated once) prevents heap fragmentation. Periodic heap monitoring on serial. Timestamped diagnostic logging. |
| **Debug** | Serial debug console (`serial_cmd.ino`) with 9 commands: `help`, `heap`, `state`, `wifi`, `config`, `diag`, `fetch`, `weather`, `restart`. Send via `./build.sh send <cmd>`. |

---

## External dependencies

| Service | Used for | Auth |
|---|---|---|
| airplanes.live | Live ADS-B transponder positions | None |
| Nominatim / OpenStreetMap | Location geocoding | None |
| Planespotters.net | Aircraft registration photos | None |
| CartoDB / OpenStreetMap | Map tiles | None |
| Cloudflare Tunnel | Public HTTPS ingress to Pi | Tunnel token |
| Open-Meteo | Weather data (via Pi proxy) | None |

---

## Deployment

- **Web app** — `git push` to `master`; GitHub Pages auto-deploys within ~60 seconds
- **Proxy** — SSH to `pi@piproxy.local`, edit `/home/pi/proxy/server.js`, `pm2 restart proxy`
- **ESP32** — `./build.sh` (compiles with `arduino-cli` and uploads). Preview layout changes by opening `tft-preview.html` in a browser before flashing. Debug with `./build.sh send <cmd>`. Pre-push check with `./build.sh safe`.
