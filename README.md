# Sydney Localized Air Traffic Tracker

A real-time tracker that shows **which aircraft are flying directly above any Sydney suburb**, built as a single HTML file with no dependencies or backend required.

🔗 **Live:** https://greystoke1337.github.io/sydney-localized-air-traffic-tracker

---

## Features

- **Suburb geofence** — type any NSW suburb and the tracker instantly re-centres on a 5km radius around it; defaults to Russell Lea
- **Live ADS-B data** via [airplanes.live](https://airplanes.live) — free, no API key required
- **Descent profile graph** — canvas-drawn glideslope chart showing the aircraft's position relative to the ideal 3° ILS approach to SYD, normalized to always keep the aircraft in view
- **Live map** — Leaflet map with dark CartoDB tiles; shows the 5km geofence circle, suburb marker, and a line from the aircraft to your location
- **Airline lookup** — airline name derived from the ICAO callsign prefix (e.g. QFA001 → Qantas)
- **Aircraft photo** — fetched automatically from [Planespotters.net](https://planespotters.net) by registration
- **Auto-refresh** every 30 seconds
- **Keyboard navigation** — arrow keys to browse flights (disabled while typing in the suburb input)
- **NOW button** — jump back to the closest overhead aircraft instantly

---

## How It Works

1. Geocodes the entered suburb via Nominatim (OpenStreetMap) — no API key needed
2. Queries airplanes.live for all aircraft within 10nm of the suburb centre
3. Filters precisely to aircraft within **5km** of the suburb centre
4. Sorts by distance to suburb — closest overhead first
5. Renders flight data, descent profile (relative to SYD), map and photo

---

## Usage

Just open `index.html` in a browser. No server, no build step, no API keys needed.

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
- [ ] Sound alert on new #1 aircraft
- [ ] Airline logo display
- [ ] Historic landing log

---

## License

MIT — do whatever you want with it.
