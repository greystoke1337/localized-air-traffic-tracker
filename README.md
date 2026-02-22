# Overhead // Live Aircraft Tracker

A real-time tracker that shows **which aircraft are flying directly above any location in the world**, built as a single HTML file with no dependencies or backend required.

🔗 **Live:** https://greystoke1337.github.io/sydney-localized-air-traffic-tracker

---

## Features

### Location & Filtering
- **Worldwide location search** — type any city, suburb, or address anywhere in the world
- **Adjustable geofence radius** — slider from 2km to 20km; the map circle updates live and the next auto-refresh applies the new radius
- **Altitude floor** — slider from 200ft to 5,000ft filters out low-flying helicopters or taxiing aircraft
- **Persisted settings** — location, geofence radius, and altitude floor are saved to `localStorage` and restored on next visit
- **Share link** — the ↗ SHARE button copies a `?location=` URL to the clipboard; anyone opening it lands directly on your location

### Live Data
- **Live ADS-B data** via [airplanes.live](https://airplanes.live), routed through a self-hosted proxy at [api.overheadtracker.com](https://api.overheadtracker.com) — free, no API key required
- **Auto-refresh** every 15 seconds; countdown resets cleanly after each fetch completes
- **Manual refresh** — REFRESH button triggers an immediate fetch and resets the countdown
- **Stale-request cancellation** — any in-flight API request is aborted before a new one starts, preventing stale data from overwriting fresh results

### Flight Information
- **Airline lookup** — airline name derived from the ICAO callsign prefix (e.g. QFA001 → Qantas)
- **Full aircraft type names** — ICAO type codes translated to readable names (e.g. B789 → B787-9, A20N → A320neo); covers Boeing, Airbus, Embraer, ATR, CRJ, turboprops, business jets, military, and helicopters
- **Flight phase detection** — classifies each aircraft as LANDING, TAKING OFF, APPROACH, DESCENDING, CLIMBING, or OVERHEAD based on altitude and vertical speed
- **Emergency squawk highlighting** — 7700 (MAYDAY), 7600 (NORDO), and 7500 (HIJACK) are displayed in red with a ⚠ warning
- **Altitude display** — Flight Level notation above 10,000ft (FL350), QNH feet below

### Map
- **Live map** — Leaflet map with dark CartoDB tiles; shows the geofence circle, location marker, and a dashed line from the aircraft to your location
- **Speed-scaled heading vector** — directional arrow from the aircraft dot, scaled by ground speed (100kt = ~1km, 500kt = ~5km) with a chevron arrowhead

### Visual Design
- **Phase colour bleed** — the info block's left border and glow change colour with flight phase (red for landing, green for climbing, amber for approach)
- **Altitude bar** — a thin vertical bar on the info block fills proportionally to altitude (0–45,000ft)
- **CRT scanline effect** — a fixed 5% opacity horizontal line overlay for the dot-matrix aesthetic
- **Aircraft photo** — fetched automatically from [Planespotters.net](https://planespotters.net) by registration, displayed as a hero image with a halftone dot overlay

### Navigation & UX
- **Keyboard navigation** — arrow keys to browse flights (disabled while typing in the location input)
- **NEAREST button** — jump back to the closest overhead aircraft instantly
- **Sound alert** — optional radar ping (🔊 SND button) when a new aircraft becomes #1 overhead; synthesised via Web Audio API, no audio files required
- **Session log** — collapsible list of every unique flight seen during the current session, with timestamps and phase labels
- **Responsive / mobile-friendly** — fluid layout, 44px minimum touch targets, fluid map and photo heights, no horizontal overflow

---

## How It Works

1. Geocodes the entered location via Nominatim (OpenStreetMap) — no API key needed; works worldwide
2. Queries [api.overheadtracker.com](https://api.overheadtracker.com) (a self-hosted Raspberry Pi proxy backed by airplanes.live) for all aircraft within a radius derived from the geofence size (4× the geofence in nautical miles, to cast a wider net)
3. Filters precisely to aircraft within the geofence radius and above the altitude floor
4. Sorts by distance — closest overhead first
5. Renders flight data, map (with heading vector), altitude bar, phase colour bleed, and photo
6. Repeats every 15 seconds; slider changes are picked up on the next auto-refresh without triggering extra API calls

---

## Usage

Just open `index.html` in a browser. No server, no build step, no API keys needed.

> **Note:** The app makes requests to `api.overheadtracker.com` (flight data proxy), `nominatim.openstreetmap.org` (geocoding), and `api.planespotters.net` (photos). All are served over HTTPS and support CORS.

```bash
git clone https://github.com/greystoke1337/sydney-localized-air-traffic-tracker.git
cd sydney-localized-air-traffic-tracker
open index.html
```

---

## Data Sources

| Source | Data | Key required |
|---|---|---|
| [api.overheadtracker.com](https://api.overheadtracker.com) | Flight data proxy (Raspberry Pi + Cloudflare Tunnel) | No |
| [airplanes.live](https://api.airplanes.live) | Live ADS-B positions (via proxy) | No |
| [Nominatim / OpenStreetMap](https://nominatim.openstreetmap.org) | Location geocoding | No |
| [Planespotters.net](https://planespotters.net) | Aircraft photos | No |
| [Carto](https://carto.com) / OpenStreetMap | Map tiles | No |

---

## Roadmap

- [x] Map view of approach path
- [x] Location geofence — track aircraft above any location worldwide
- [x] Adjustable geofence radius and altitude floor sliders
- [x] Persist last location and settings across sessions
- [x] Manual refresh button
- [x] Responsive layout / mobile support
- [x] Speed-scaled heading vector on map
- [x] Flight phase detection with colour bleed
- [x] Altitude bar
- [x] Sound alert on new #1 aircraft
- [x] Session flight log
- [x] Share link via URL param
- [x] Full aircraft type name database
- [ ] Airline logo display
- [ ] Push notification when a specific flight appears overhead

---

## License

MIT — do whatever you want with it.
