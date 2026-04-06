# Claude Code Instructions

## Project Overview

**Overhead Tracker** is a real-time aircraft tracking system.
It answers: *"What planes are flying directly above me right now?"*

Five components:

| Component | Codename | Tech | Hosted at |
|-----------|----------|------|-----------|
| Web app | — | Single-file HTML + vanilla JS | GitHub Pages (auto-deploy on push to `master`) |
| Proxy server | — | Node.js / Express | Railway (`api.overheadtracker.com`) |
| ESP32 firmware (4.0") | **Echo** | Arduino C++ on Freenove FNK0103S | Physical device (USB via `build.sh`, COM4) |
| ESP32-S3 firmware (4.3") | **Foxtrot** | Arduino C++ on Waveshare ESP32-S3-Touch-LCD-4.3 | Physical device (USB via `arduino-cli`, COM7) |
| ESP32-S3 firmware (3.49") | **Delta** | Arduino C++ on Waveshare ESP32-S3-Touch-LCD-3.49 | Physical device (USB via `build.sh delta`, COM8) |
| Pi display | — | Python / Pygame on Raspberry Pi 3B+ | Physical device (3.5" TFT on `/dev/fb1`) |
| 64×32 LED matrix | **Golf** | Arduino C++ on Adafruit Matrix Portal M4 | Physical device (USB via `build.sh golf`, COM9) |

Live URL: https://greystoke1337.github.io/localized-air-traffic-tracker/
Custom domain: https://overheadtracker.com

---

## Repository Layout

```
index.html                  # Entire web app (single file, ~120 KB)
build.sh                    # Echo build/upload/test/debug helper (not for Foxtrot)
tft-preview.html            # TFT display simulator (preview Echo/Foxtrot/Delta firmware UI in browser)
golf-preview.html           # LED matrix simulator (preview Golf M4 display in browser)
flash.html                  # Web Serial firmware flasher (Chrome/Edge, esptool.js)
firmware/                   # Compiled Foxtrot binaries + manifest.json for flash.html
server/                     # Railway-hosted proxy (server.js, package.json)
pi-display/                 # Raspberry Pi TFT display (display.py, watchdog.sh)
tracker_echo/      # Echo — Freenove 4.0" (ESP32, SPI, 480×320, TFT_eSPI)
tracker_foxtrot/            # Foxtrot — Waveshare 4.3 (ESP32-S3, RGB, 800×480, LovyanGFX immediate-mode)
tracker_golf/            # Golf — Adafruit Matrix Portal M4 (64×32 HUB75 LED matrix, Arduino)
tools/                      # synthetic-data.js, mock-proxy.js, serial_monitor.ps1
tests/                      # Desktop logic tests (test_flight_logic.c, test_parsing.cpp)
```

Both firmware dirs share the same file split: `.ino` (setup/loop), `config.h`, `types.h`, `globals.h`, `lookup_tables.h`, `helpers.ino`, `display.ino`, `network.ino`, `touch.ino`, `wifi_setup.ino`, `sd_config.ino`, `serial_cmd.ino`, `secrets.h` (gitignored). Foxtrot adds `lgfx_config.h` and `esp_panel_board_supported_conf.h`.

---

## Branch Policy

Pushes to `master` deploy automatically to GitHub Pages within ~60 seconds.
Use feature branches and pull requests for non-trivial changes.

---

## External APIs Used by the Web App

| Service | Purpose | Auth |
|---------|---------|------|
| adsb.lol / adsb.fi / airplanes.live | Live ADS-B flight data (raced, first wins) | None (via proxy) |
| OpenSky / adsbdb | Route lookups (dep/arr airports) | None (via proxy) |
| Nominatim / OpenStreetMap | Location geocoding | None |
| Planespotters.net | Aircraft photos by registration | None |
| CartoDB | Dark map tiles (Leaflet) | None |

The proxy at `api.overheadtracker.com` (hosted on Railway) races all three ADS-B APIs in parallel and uses the fastest response. Results are cached for 5 s. Route lookups are non-blocking (fire-and-forget, cached for next request). New routes are tracked in `known-routes.json` (persistent) and surfaced via `/routes/new`, daily reports, and a nightly discovery email at 21:00 AEST.

---

## Key Concepts

- **Geofence**: User-configurable radius (2–20 km) around a chosen location. Only aircraft inside the fence are shown.
- **Altitude floor**: Filters out aircraft below a configurable altitude (200–5 000 ft AGL).
- **Flight phase detection**: LANDING / TAKING OFF / APPROACH / DESCENDING / CLIMBING / CRUISING / OVERHEAD / UNKNOWN — derived from speed, altitude, and vertical rate.
- **TFT preview**: `tft-preview.html` mirrors the ESP32 display rendering in the browser. Same pixel coordinates, colors, and lookup tables. Use it to verify layout changes before flashing.
- **Route discovery**: The proxy tracks every unique `"City > City"` route pair in `known-routes.json`. New routes are detected in real time, persisted in daily flight logs, and emailed nightly via Resend. API: `GET /routes/new?date=YYYY-MM-DD`.
- **No build step**: `index.html` is deployed as-is; never introduce a bundler or external dependency that requires a build pipeline.
- **No framework**: The web app uses vanilla JS and the browser's built-in APIs only. Do not add React, Vue, or similar.

---

## Common Tasks

### Web app change
Edit `index.html`, test via `file://`, push to `master` (auto-deploys to GitHub Pages in ~60 s).

### Testing with synthetic data
Web app: `index.html?demo=true&scenario=emergency`. Mock proxy: `node tools/mock-proxy.js normal 3000 --scenario crowded`.

For Echo, Foxtrot, Golf, server, and Pi — see the `CLAUDE.md` in each subdirectory (`tracker_echo/`, `tracker_foxtrot/`, `tracker_golf/`, `server/`, `pi-display/`).

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
| Backend Specialist | `.claude/agents/backend-specialist.md` | Railway proxy server, ESP32 firmware, API integrations |
| Technical Writer | `.claude/agents/technical-writer.md` | README, PI_PROXY_SETUP.md, inline comments, documentation |
| UI/UX Designer | `.claude/agents/ux-designer.md` | UI implementation, layout, CRT aesthetic, accessibility, mobile responsiveness, TFT display UI |

---

## Gemini CLI Workflow

Gemini CLI (`gemini`) is installed and configured with shell helpers in `~/.bashrc`. Use it to **explore and draft** (free, large context), then bring targeted requests to Claude to **execute and integrate**.

| Command | Use for |
|---------|---------|
| `golf-ask "question"` | Explore all Golf M4 firmware files |
| `server-ask "question"` | Explore server.js |
| `gask "question" file...` | Ask about specific files |
| `gdir "question" dir/` | Ask about all source files in a directory |
| `greview` | Review uncommitted diff for bugs |
| `gstaged` | Review staged changes before committing |
| `glog` | Summarise recent git history |

**Pattern:** `golf-ask "Where is brightness controlled?"` → then `"Change brightness step in display.ino:142 from 10 to 20"`
