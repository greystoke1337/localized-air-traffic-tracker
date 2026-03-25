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
| Pi display | — | Python / Pygame on Raspberry Pi 3B+ | Physical device (3.5" TFT on `/dev/fb1`) |

Live URL: https://greystoke1337.github.io/localized-air-traffic-tracker/
Custom domain: https://overheadtracker.com

---

## Repository Layout

```
index.html                  # Entire web app (single file, ~120 KB)
build.sh                    # Echo build/upload/test/debug helper (not for Foxtrot)
tft-preview.html            # TFT display simulator (preview firmware UI in browser)
flash.html                  # Web Serial firmware flasher (Chrome/Edge, esptool.js)
firmware/                   # Compiled Foxtrot binaries + manifest.json for flash.html
server/                     # Railway-hosted proxy (server.js, package.json)
pi-display/                 # Raspberry Pi TFT display (display.py, watchdog.sh)
tracker_live_fnk0103s/      # Echo — Freenove 4.0" (ESP32, SPI, 480×320, TFT_eSPI)
tracker_foxtrot/            # Foxtrot — Waveshare 4.3 (ESP32-S3, RGB, 800×480, LovyanGFX immediate-mode)
tools/                      # synthetic-data.js, mock-proxy.js, serial_monitor.ps1
tests/                      # Desktop logic tests (test_flight_logic.c, test_parsing.cpp)
```

Both firmware dirs share the same file split: `.ino` (setup/loop), `config.h`, `types.h`, `globals.h`, `lookup_tables.h`, `helpers.ino`, `display.ino`, `network.ino`, `touch.ino`, `wifi_setup.ino`, `sd_config.ino`, `serial_cmd.ino`, `secrets.h` (gitignored). Foxtrot adds `lgfx_config.h` and `esp_panel_board_supported_conf.h`. `lvgl_v8_port.h/.cpp` are present but stubbed out — do not restore them.

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

The proxy at `api.overheadtracker.com` (hosted on Railway) races all three ADS-B APIs in parallel and uses the fastest response. Results are cached for 10 s. Route lookups are non-blocking (fire-and-forget, cached for next request). New routes are tracked in `known-routes.json` (persistent) and surfaced via `/routes/new`, daily reports, and a nightly discovery email at 21:00 AEST.

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

### Proxy server change
Edit `server/`, deploy via `/railway` skill or `cd server && railway up`. Verify at `https://api.overheadtracker.com/status`.

### Echo firmware change (Freenove 4.0", COM4)
Edit files in `tracker_live_fnk0103s/`. Use `./build.sh` to compile+flash, or `/flash-and-log echo`. Preview layout with `tft-preview.html`. Pre-push: `./build.sh safe`.

### Foxtrot firmware change (Waveshare 4.3 non-B, COM7)
Edit files in `tracker_foxtrot/`. Use `/flash-and-log foxtrot` to compile+flash. **Do not use `build.sh`** for Foxtrot. Rendering is LovyanGFX immediate-mode (`tft.fillRect`, `tft.drawString`) — no LVGL, no lock/unlock needed. **Do not restore `lvgl_v8_port.cpp`** — it must stay stubbed or it re-introduces an I2C driver conflict that crashes on boot.

### Foxtrot demo mode
Set `#define DEMO_MODE 1` in `tracker_foxtrot/config.h` to boot with 3 fake Sydney flights, skipping all WiFi/network code.

### Package firmware for web flasher
Run `./tools/package-firmware.sh` to compile Foxtrot and copy binaries to `firmware/`. Push to `master` to deploy the update to `overheadtracker.com/flash`. Use `--skip-compile` to package the last build without recompiling.

### Testing with synthetic data
Web app: `index.html?demo=true&scenario=emergency`. Mock proxy: `node tools/mock-proxy.js normal 3000 --scenario crowded`.

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
