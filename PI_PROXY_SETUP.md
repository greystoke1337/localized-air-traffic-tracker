# Proxy Setup — Overhead Tracker

Complete record of how the proxy server is configured.
Written: February 2026 — Updated: March 2026

---

## Architecture

The proxy server runs on **Railway** (managed hosting, ~$5/mo).
The Raspberry Pi 3B+ remains in use solely for the 3.5" TFT display.

```
Browser / ESP32  →  Railway proxy (:8080)  →  adsb.lol    ┐
                         ↑                  →  adsb.fi     ├─ raced (first wins)
                  api.overheadtracker.com   →  airplanes.live ┘
                  (CNAME → overhead-tracker-proxy-production.up.railway.app)
```

The proxy fires all three upstream ADS-B APIs simultaneously and uses whichever
responds first (~0.8 s typical). Route lookups (OpenSky, adsbdb) run in the
background and are cached for 30 minutes.

### Why Railway?

The proxy originally ran on the Pi via Cloudflare Tunnel, but the Pi suffered
from undervoltage crashes, SD card corruption, and required manual SSH deploys.
Railway provides managed hosting, auto-deploys, persistent volumes, and better uptime.

---

## Railway Hosting

- **Project:** resourceful-integrity
- **Service:** overhead-tracker-proxy
- **Railway domain:** overhead-tracker-proxy-production.up.railway.app
- **Custom domain:** api.overheadtracker.com
- **Volume:** /data (persistent storage for route cache + flight reports)
- **Node version:** 22 (auto-detected by Railpack)
- **Start command:** `node server.js` (from Procfile)

### Environment Variables (set in Railway dashboard or CLI)

| Variable | Purpose |
|----------|---------|
| `PORT` | Set by Railway (8080) |
| `ADMIN_TOKEN` | Required for /proxy/toggle and /report/send |
| `HOME_LAT` | Home location latitude |
| `HOME_LON` | Home location longitude |
| `ROUTE_CACHE_FILE` | `/data/route-cache.json` |
| `REPORTS_DIR` | `/data/reports` |
| `SMTP_HOST` | For daily email reports |
| `SMTP_PORT` | SMTP port (default 587) |
| `SMTP_USER` | SMTP username |
| `SMTP_PASS` | SMTP password |
| `REPORT_FROM` | Email sender address |
| `REPORT_TO` | Email recipient address |

### Deploying

```bash
eval "$(/opt/homebrew/bin/brew shellenv)"
cd server
railway up
```

Or use the `/railway` or `/deploy-server` Claude skills.

### Managing

```bash
eval "$(/opt/homebrew/bin/brew shellenv)"
railway service logs --service overhead-tracker-proxy    # view logs
railway service redeploy --service overhead-tracker-proxy --yes  # restart
railway variables                                         # view env vars
railway variables set KEY=VALUE                           # set env var
```

---

## Raspberry Pi (display only)

- **Device:** Raspberry Pi 3B+
- **OS:** Raspberry Pi OS Lite 64-bit (headless)
- **Local IP:** 192.168.86.24
- **Hostname:** piproxy
- **Role:** Runs `display.py` only (3.5" TFT dashboard)

---

## SD Card & OS Setup

Flashed via **Raspberry Pi Imager** on Windows with these settings:

- **Device:** Raspberry Pi 3
- **OS:** Raspberry Pi OS Lite (64-bit)
- **Hostname:** `piproxy`
- **SSH:** enabled, password authentication
- **Username:** `pi`
- **WiFi SSID:** REDACTED
- **WiFi country:** AU

---

## SSH Access

From any machine on the local network:

```bash
ssh pi@piproxy.local
# or
ssh pi@192.168.86.24
```

**Recommended:** Use **VS Code with the Remote SSH extension** for editing files.
Connect to `pi@piproxy.local`, then open `/home/pi/proxy` as the workspace.
Much easier than nano or scp.

---

## Software Installed

### Node.js 20

```bash
sudo apt update && sudo apt upgrade -y
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt install -y nodejs
```

### Proxy server dependencies

```bash
mkdir ~/proxy && cd ~/proxy
npm init -y
npm install express node-fetch
```

### PM2 (process manager — keeps services alive across reboots)

```bash
sudo npm install -g pm2
```

### cloudflared (Cloudflare Tunnel client)

```bash
curl -L https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-arm64 -o cloudflared
chmod +x cloudflared
sudo mv cloudflared /usr/local/bin/
```

---

## File Structure

```
/home/pi/proxy/
├── server.js          # Main proxy server
├── dashboard.html     # Browser dashboard UI
├── display.py         # 3.5" TFT dashboard (framebuffer)
├── watchdog.sh        # Undervoltage watchdog (cron, restarts PM2)
├── package.json
├── data/
│   └── peak.json      # Persisted hourly traffic counts (auto-created)
└── logs/
    └── watchdog.log   # Watchdog activity log (auto-created)
```

---

## Proxy Server

**File:** `/home/pi/proxy/server.js`

See [`server/server.js`](server/server.js).

---

## Dashboard

**File:** `/home/pi/proxy/dashboard.html`

Served at `dashboard.overheadtracker.com` (and at `/` on port 3000).

Features:
- Live proxy status (online/offline indicator)
- Uptime, total requests, cache hits/misses, errors, unique clients
- Cache hit rate with progress bar
- Peak hour bar chart (24h) — amber = current hour, green = all-time peak
- Refresh button and stop proxy button (with confirmation dialog)
- Auto-refreshes every 10 seconds

---

## API Endpoints

| Endpoint     | Description |
|--------------|-------------|
| `GET /`      | Serves dashboard HTML |
| `GET /flights?lat=&lon=&radius=` | Proxied + cached flight data from airplanes.live (10 s cache) |
| `GET /weather?lat=&lon=` | Current weather from Open-Meteo (10 min cache) |
| `GET /stats` | JSON — uptime, request counts, cache stats, peak hour array |
| `GET /peak`  | JSON — full 24-hour breakdown with labels and percentages |
| `GET /report?date=YYYY-MM-DD` | Daily flight report (HTML or JSON with `&format=json`) |
| `POST /proxy/toggle` | Toggle proxy on/off — requires `ADMIN_TOKEN` (see below) |
| `POST /report/send` | Manually trigger daily email report — requires `ADMIN_TOKEN` |

### Environment Variables (`.env`)

Create `server/.env` locally (or set in Railway dashboard) with:

```env
ADMIN_TOKEN=your-secret-token    # Required for /proxy/toggle and /report/send
HOME_LAT=-33.8530                # Home location for flight distance calculations
HOME_LON=151.1410
SMTP_HOST=smtp.example.com       # Optional — for daily email reports
SMTP_PORT=587
SMTP_USER=user@example.com
SMTP_PASS=password
REPORT_FROM=Overhead Tracker <user@example.com>
REPORT_TO=you@example.com
```

Admin endpoints return 403 when `ADMIN_TOKEN` is not configured. To use the toggle:

```bash
curl -X POST -H "Authorization: Bearer your-secret-token" http://192.168.86.24:3000/proxy/toggle
```

### Graceful Shutdown

The server handles SIGTERM/SIGINT by saving the route cache and flight log before exiting. PM2 sends SIGTERM on restart, so data is preserved across `pm2 restart proxy`.

---

## Cloudflare Tunnel (RETIRED)

The Cloudflare Tunnel previously connected the Pi proxy to the internet.
It has been replaced by Railway hosting. The tunnel service (`cloudflared`)
and PM2 `tunnel` process should be stopped on the Pi.

To clean up:

```bash
ssh piproxy "pm2 stop tunnel && pm2 delete tunnel && pm2 save"
```

---

## 3.5" TFT Display

**Hardware:** Generic MPI3501 clone (ILI9486 SPI, 480×320, resistive touch)
**Driver:** `tft35a` dtoverlay (installed via goodtft/LCD-show `LCD35-show`)

### How it works

`display.py` uses pygame with `SDL_VIDEODRIVER=offscreen` to render in memory,
then converts the surface to RGB565 and writes directly to `/dev/fb1`.
No X server or desktop environment needed.

Dependencies: `python3-pygame`, `python3-numpy`, `requests`

See [`pi-display/display.py`](pi-display/display.py).

### Relevant boot config (`/boot/firmware/config.txt`)

```
dtparam=spi=on
dtoverlay=tft35a:rotate=270
```

### Troubleshooting

| Symptom | Check |
|---|---|
| Black screen after reboot | `ls /dev/fb1` — if missing, driver didn't load; check config.txt |
| `pm2 logs display` shows errors | Check for numpy/pygame import errors |
| Display shows "proxy unreachable" | proxy PM2 service is down; `pm2 restart proxy` |

---

## PM2 Services (Pi)

The Pi now only runs the TFT display. The proxy runs on Railway.

| Name    | Command                                        | Purpose               |
|---------|------------------------------------------------|-----------------------|
| display | `python3 /home/pi/proxy/display.py`           | 3.5" TFT dashboard    |

### Setup

```bash
pm2 start "python3 /home/pi/proxy/display.py" --name display
pm2 save
pm2 startup  # then run the printed sudo command
```

### Useful commands

```bash
pm2 status              # check display is online
pm2 logs display        # check display rendering errors
pm2 restart display     # restart after display.py changes
```

---

## Undervoltage Watchdog

The Pi 3B+ can experience undervoltage with insufficient power supplies, causing
services to hang without crashing (PM2 doesn't detect this). A watchdog script
monitors for this and restarts services when needed.

**File:** `/home/pi/proxy/watchdog.sh` — see [`pi-display/watchdog.sh`](pi-display/watchdog.sh).

### Behavior

1. Runs every 60s via cron
2. Reads `vcgencmd get_throttled` for undervoltage flags
3. If undervoltage detected, checks if the proxy responds at `localhost:3000/status`
4. If proxy is unresponsive for 2 consecutive checks (2 min), restarts all PM2 services
5. 5-minute cooldown between restarts to prevent restart storms
6. If PM2 itself is hung, falls back to `pm2 kill` + `pm2 resurrect`

### Watchdog setup

```bash
chmod +x /home/pi/proxy/watchdog.sh
crontab -e
# Add this line:
# * * * * * /home/pi/proxy/watchdog.sh
```

### Checking watchdog activity

```bash
tail -20 /home/pi/proxy/logs/watchdog.log
```

---

## Domain — overheadtracker.com

Registered through Cloudflare Registrar.

### DNS records

| Type  | Name        | Value                           | Proxy |
|-------|-------------|---------------------------------|-------|
| CNAME | `@`         | `greystoke1337.github.io`      | On    |
| CNAME | `www`       | `greystoke1337.github.io`      | On    |
| CNAME | `api`       | `overhead-tracker-proxy-production.up.railway.app` | **Off** (grey cloud) |

**Important:** The `api` CNAME must have Cloudflare proxy **OFF** (grey cloud / DNS only).
Railway handles its own TLS via Let's Encrypt and will fail with 502 if Cloudflare proxies the request.

---

## Client Configuration

### Web app (index.html)

```
https://api.overheadtracker.com/flights?lat=LAT&lon=LON&radius=RADIUS
https://api.overheadtracker.com/weather?lat=LAT&lon=LON
```

### ESP32 (.ino)

```
https://api.overheadtracker.com/flights?lat=LAT&lon=LON&radius=RADIUS
https://api.overheadtracker.com/weather?lat=LAT&lon=LON
```

---

## If the Pi reboots

The display process starts automatically via PM2.
The proxy runs on Railway and is unaffected by Pi reboots.

```bash
ssh piproxy "pm2 status"  # verify display is online after reboot
```

---

## If something breaks

| Symptom | Check |
|---|---|
| api.overheadtracker.com returns 502 | Check Railway: `railway service logs --service overhead-tracker-proxy`. Verify Cloudflare proxy is OFF (grey cloud) on the `api` CNAME. |
| Website shows fetch error | Check Railway status: `curl https://api.overheadtracker.com/status` |
| ESP32 shows HTTP ERR | Check Railway is up. ESP32 will fall back to direct API automatically after 3 s. |
| 429 errors from airplanes.live | Cache may have been disabled — check server.js |
| Stats all zero after restart | Normal — peak data restores but session counts reset |
| TFT display blank | SSH into Pi: `pm2 logs display` — check for errors |
| Display shows "proxy unreachable" | Railway proxy is down — check `railway service status` |

---

## Testing Without the Pi

Use the mock proxy tool to simulate failure modes:

```bash
node tools/mock-proxy.js timeout    # hangs — tests ESP32 connect timeout
node tools/mock-proxy.js error503   # returns 503 — tests fallback to direct API
node tools/mock-proxy.js normal     # valid responses — tests happy path
```

Set `PROXY_HOST` in the firmware to your dev machine's IP, flash, and monitor serial output.

The Pi proxy also has a built-in toggle (requires `ADMIN_TOKEN` in `.env`):

```bash
curl -X POST -H "Authorization: Bearer $ADMIN_TOKEN" http://192.168.86.24:3000/proxy/toggle
```

This makes it return 503 until toggled back — useful for testing fallback without reflashing.

---

## Features Planned / In Progress

- [ ] Aircraft of the day (fastest, highest, rarest type — resets midnight)
- [ ] Filtered `/rare` endpoint (military, bizjets, uncommon types)
- [ ] Traffic heatmap (position accumulation over time → JSON for web overlay)
