# Sydney Localized Air Traffic Tracker

A real-time tracker that shows **which aircraft are flying directly above any Australian suburb**, built as a single HTML file with no dependencies or backend required.

🔗 **Live:** https://greystoke1337.github.io/sydney-localized-air-traffic-tracker

---

## Features

- **Suburb geofence** — type any Australian suburb and the tracker instantly re-centres on a 5km radius around it; defaults to Russell Lea
- **Persisted location** — your last suburb is saved to `localStorage` and restored on next visit
- **Live ADS-B data** via [airplanes.live](https://airplanes.live) — free, no API key required
- **Descent profile graph** — canvas-drawn glideslope chart showing the aircraft's position relative to the ideal 3° ILS approach to SYD, normalized to always keep the aircraft in view
- **Live map** — Leaflet map with dark CartoDB tiles; shows the 5km geofence circle, suburb marker, and a line from the aircraft to your location; SYD is only pulled into the map bounds when the suburb is within 40km of the airport
- **Airline lookup** — airline name derived from the ICAO callsign prefix (e.g. QFA001 → Qantas)
- **Aircraft photo** — fetched automatically from [Planespotters.net](https://planespotters.net) by registration
- **Auto-refresh** every 30 seconds; countdown always resets cleanly after each fetch completes
- **Manual refresh** — REFRESH button triggers an immediate fetch and resets the countdown
- **Stale-request cancellation** — any in-flight API request is aborted before a new one starts, preventing stale data from overwriting fresh results
- **Keyboard navigation** — arrow keys to browse flights (disabled while typing in the suburb input)
- **NEAREST button** — jump back to the closest overhead aircraft instantly
- **Responsive** — viewport meta tag and dynamic canvas resizing keep the layout correct on mobile

---

## How It Works

1. Geocodes the entered suburb via Nominatim (OpenStreetMap) — no API key needed; searches across Australia
2. Queries airplanes.live for all aircraft within a radius derived from the geofence size (~11nm for a 5km geofence)
3. Filters precisely to aircraft within **5km** of the suburb centre
4. Sorts by distance to suburb — closest overhead first
5. Renders flight data, descent profile (relative to SYD airport as the ILS reference), map and photo

---

## Usage

Just open `index.html` in a browser. No server, no build step, no API keys needed.

> **Note:** The app makes requests to external APIs (`api.airplanes.live`, `nominatim.openstreetmap.org`, `api.planespotters.net`). These all support CORS. If you open the file over `http://` some browsers may block mixed-content requests — serve over `https://` or use a local server if you hit issues.

```bash
git clone https://github.com/greystoke1337/sydney-localized-air-traffic-tracker.git
cd sydney-localized-air-traffic-tracker
open index.html
```

---

## Data Sources

| Source | Data | Key required |
|---|---|---|
| [airplanes.live](https://api.airplanes.live) | Live ADS-B positions | No |
| [Nominatim / OpenStreetMap](https://nominatim.openstreetmap.org) | Suburb geocoding | No |
| [Planespotters.net](https://planespotters.net) | Aircraft photos | No |
| [Carto](https://carto.com) / OpenStreetMap | Map tiles | No |

---

## Roadmap

- [x] Map view of approach path
- [x] Suburb geofence — track aircraft above any location
- [x] Persist last suburb across sessions
- [x] Manual refresh button
- [x] Responsive layout / mobile support
- [ ] Sound alert on new #1 aircraft
- [ ] Airline logo display
- [ ] Historic landing log

---

## License

MIT — do whatever you want with it.
