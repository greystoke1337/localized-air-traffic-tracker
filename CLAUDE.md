# Claude Code Instructions

## Project Overview

**Overhead Tracker** is a real-time aircraft tracking system.
It answers: *"What planes are flying directly above me right now?"*

Three components:

| Component | Tech | Hosted at |
|-----------|------|-----------|
| Web app | Single-file HTML + vanilla JS | GitHub Pages (auto-deploy on push to `master`) |
| Proxy server | Node.js / Express | Railway (`api.overheadtracker.com`) |
| ESP32 firmware | Arduino C++ | Physical device (USB upload via `build.sh`) |
| Pi display | Python / Pygame on Raspberry Pi 3B+ | Physical device (3.5" TFT on `/dev/fb1`) |

Live URL: https://greystoke1337.github.io/localized-air-traffic-tracker/
Custom domain: https://overheadtracker.com

---

## Repository Layout

```
index.html                  # Entire web app (single file, ~120 KB)
build.sh                    # ESP32 build/upload/test/debug helper
tft-preview.html            # TFT display simulator (preview firmware UI in browser)
PI_PROXY_SETUP.md           # Raspberry Pi proxy setup guide
SPEC.md                     # Product specification and feature matrix
README.md                   # User-facing documentation
CNAME                       # GitHub Pages custom domain
pi-proxy/                   # Raspberry Pi proxy source
  server.js                 # Node.js caching proxy + dashboard (deployed to Railway)
  Procfile                  # Railway start command
  .dockerignore             # Railway build exclusions
  display.py                # Pygame TFT display (480×320, writes to /dev/fb1, runs on Pi)
  watchdog.sh               # Undervoltage watchdog (cron, restarts PM2, Pi only)
  dashboard.html            # Legacy dashboard UI (used by older server)
  package.json              # Node deps (express, node-fetch)
tracker_live_fnk0103s/      # ESP32 hardware project (multi-file Arduino sketch)
  tracker_live_fnk0103s.ino # setup() + loop() + global state
  config.h                  # #defines: layout, colours, pins, timing
  types.h                   # Flight, WeatherData, enums
  globals.h                 # extern declarations + forward declarations
  lookup_tables.h           # airline + aircraft type arrays
  helpers.ino               # pure logic (haversine, deriveStatus, formatters)
  display.ino               # all TFT drawing functions
  network.ino               # HTTP fetching, JSON parsing, flight extraction
  touch.ino                 # touch calibration + input handling
  wifi_setup.ino            # WiFi config, captive portal, geocoding
  sd_config.ino             # SD card config, cache, flight logging
  serial_cmd.ino            # serial debug console (9 commands)
  secrets.h                 # WiFi credential defaults (gitignored)
  enclosure/                # 3D-printable case files (STL/STEP)
tools/                      # Development utilities
  synthetic-data.js         # flight + weather data generator (8 scenarios, any lat/lon)
  mock-proxy.js             # mock proxy for firmware resilience testing (uses synthetic-data)
tests/                      # Desktop logic tests (no hardware needed)
  test_flight_logic.c       # C tests for flight logic
  test_parsing.cpp          # C++ tests for JSON parsing
  fixtures/                 # test data
.claude/agents/             # Specialist sub-agents
  backend-specialist.md
  technical-writer.md
  ux-designer.md
```

---

## Branch Policy

Always develop and push directly to **`master`**.
Do **not** create feature branches.
Pushes to `master` deploy automatically to GitHub Pages within ~60 seconds.

---

## External APIs Used by the Web App

| Service | Purpose | Auth |
|---------|---------|------|
| adsb.lol / adsb.fi / airplanes.live | Live ADS-B flight data (raced, first wins) | None (via proxy) |
| OpenSky / adsbdb | Route lookups (dep/arr airports) | None (via proxy) |
| Nominatim / OpenStreetMap | Location geocoding | None |
| Planespotters.net | Aircraft photos by registration | None |
| CartoDB | Dark map tiles (Leaflet) | None |

The proxy at `api.overheadtracker.com` (hosted on Railway) races all three ADS-B APIs in parallel and uses the fastest response. Results are cached for 10 s. Route lookups are non-blocking (fire-and-forget, cached for next request).

---

## Key Concepts

- **Geofence**: User-configurable radius (2–20 km) around a chosen location. Only aircraft inside the fence are shown.
- **Altitude floor**: Filters out aircraft below a configurable altitude (200–5 000 ft AGL).
- **Flight phase detection**: LANDING / TAKING OFF / APPROACH / DESCENDING / CLIMBING / CRUISING / OVERHEAD / UNKNOWN — derived from speed, altitude, and vertical rate.
- **TFT preview**: `tft-preview.html` mirrors the ESP32 display rendering in the browser. Same pixel coordinates, colors, and lookup tables. Use it to verify layout changes before flashing.
- **No build step**: `index.html` is deployed as-is; never introduce a bundler or external dependency that requires a build pipeline.
- **No framework**: The web app uses vanilla JS and the browser's built-in APIs only. Do not add React, Vue, or similar.

---

## Common Tasks

### Web app change
1. Edit `index.html`.
2. Test by opening the file in a browser (`file://` works — no server needed).
3. `git add index.html && git commit -m "..." && git push origin master`
4. Verify at the live URL after ~60 s.

### Proxy server change
1. Edit files in `pi-proxy/`.
2. Deploy to Railway: `eval "$(/opt/homebrew/bin/brew shellenv)" && cd pi-proxy && railway up` (or use the `/railway` skill).
3. Verify at `https://api.overheadtracker.com/status`.
4. Full setup details in `PI_PROXY_SETUP.md`.

### ESP32 firmware change
1. Edit the relevant file in `tracker_live_fnk0103s/` — the firmware is split by concern (see repo layout above).
2. Preview layout changes by opening `tft-preview.html` in a browser — it mirrors the firmware's rendering logic with interactive controls.
3. First flash (or if OTA is unavailable): run `./build.sh` to compile and upload via USB (requires Arduino IDE + `arduino-cli` on Windows).
4. Subsequent flashes: compile with `./build.sh compile`, then upload via Arduino IDE using the `overhead-tracker` network port (OTA over WiFi).
5. Serial monitor: `./build.sh monitor`.
6. Send a debug command: `./build.sh send <cmd>` (commands: help, heap, state, wifi, config, diag, fetch, weather, restart).
7. Pre-push safety check: `./build.sh safe` (runs desktop tests + compile with all warnings).
8. Stress test: `./build.sh proxy-host <dev-ip>` to point at dev machine, `./build.sh stress 10 COM4` for 10-min chaos test, then `./build.sh proxy-host 192.168.86.24` to restore.

### Testing with synthetic data
- **Web app demo mode**: open `index.html?demo=true` (or `?demo=true&scenario=emergency`). Uses generated flights with no API calls. Scenarios: busy, quiet, crowded, emergency, approach_rush, single, mixed.
- **Mock proxy for ESP32**: `node tools/mock-proxy.js normal 3000 --scenario crowded`. Supports all 10 modes × 8 scenarios.
- **Generate fixture JSON**: `node tools/synthetic-data.js --scenario emergency` (prints to stdout). Pre-generated fixtures live in `tests/fixtures/synthetic-flights.json`.

---

## Code Style

- **No comments** on self-evident code; add comments only when logic is non-obvious.
- **No docstrings / JSDoc** unless already present in that section.
- **Minimal abstractions**: prefer three clear lines over a premature helper function.
- **Security**: never embed API keys, credentials, or private IPs in committed code.
- **Accessibility**: maintain keyboard navigation support (arrow keys for table rows).

---

## Specialist Agents

Use these sub-agents for domain-specific tasks:

| Agent | File | When to use |
|-------|------|-------------|
| Backend Specialist | `.claude/agents/backend-specialist.md` | Pi proxy, ESP32 firmware, Cloudflare Tunnel, API integrations |
| Technical Writer | `.claude/agents/technical-writer.md` | README, PI_PROXY_SETUP.md, inline comments, documentation |
| UI/UX Designer | `.claude/agents/ux-designer.md` | UI implementation, layout, CRT aesthetic, accessibility, mobile responsiveness, TFT display UI |
