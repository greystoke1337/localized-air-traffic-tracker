# Sydney Localized Air Traffic Tracker

A real-time flight arrival tracker for **Sydney Kingsford Smith Airport (SYD)**, built as a single HTML file with no dependencies or backend required.

🔗 **Live:** https://greystoke1337.github.io/sydney-localized-air-traffic-tracker

---

## Features

- **Live ADS-B data** via [airplanes.live](https://airplanes.live) — free, no API key required
- **Descent profile graph** — canvas-drawn glideslope chart showing the aircraft's position relative to the ideal 3° ILS approach, normalized to always keep the aircraft in view
- **Live map** — Leaflet map with dark CartoDB tiles showing the selected aircraft's position relative to SYD, with a dashed approach line
- **Airline lookup** — airline name derived from the ICAO callsign prefix (e.g. QFA001 → Qantas)
- **Aircraft photo** — fetched automatically from [Planespotters.net](https://planespotters.net) by registration
- **Filtered for SYD only** — excludes Bankstown Airport (BWU) traffic
- **Auto-refresh** every 30 seconds
- **Keyboard navigation** — arrow keys to browse flights
- **NOW button** — jump back to the nearest arrival instantly

---

## How It Works

1. Queries airplanes.live for all aircraft within 50nm of Sydney Airport (SYD)
2. Filters for descending aircraft (`baro_rate < 0`) not near Bankstown (BWU)
3. Sorts by distance to SYD — closest first
4. Renders flight data, descent profile, map and photo

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
| [Planespotters.net](https://planespotters.net) | Aircraft photos | No |
| [Carto](https://carto.com) / OpenStreetMap | Map tiles | No |

---

## Roadmap

- [x] Map view of approach path
- [ ] Sound alert on new #1 aircraft
- [ ] Airline logo display
- [ ] Historic landing log

---

## License

MIT — do whatever you want with it.
