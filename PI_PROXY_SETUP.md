# Pi Proxy Setup — Overhead Tracker

Complete record of how the Raspberry Pi proxy was configured.
Written: February 2026 — Updated: February 2026

---

## Hardware

- **Device:** Raspberry Pi 3B+
- **OS:** Raspberry Pi OS Lite 64-bit (headless)
- **Local IP:** 192.168.86.24
- **Hostname:** piproxy

---

## What it does

Acts as a caching proxy between the flight tracker clients and airplanes.live.
Caches each unique query for 10 seconds so multiple devices (web app + ESP32)
can refresh freely without hitting API rate limits (HTTP 429).

```
Browser / ESP32  →  Pi proxy (:3000)  →  airplanes.live
                         ↑
                  Cloudflare Tunnel
                  api.overheadtracker.com
                  dashboard.overheadtracker.com
```

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

See [`pi-proxy/server.js`](pi-proxy/server.js).

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

Create `pi-proxy/.env` on the Pi with:

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

## Cloudflare Tunnel

### Account
- Cloudflare account linked to **overheadtracker.com**
- Tunnel name: **overhead-tracker**
- Tunnel ID: `REDACTED`
- Public URLs:
  - **https://api.overheadtracker.com** — proxy API
  - **https://dashboard.overheadtracker.com** — dashboard UI

### Config file
**File:** `/home/pi/.cloudflared/config.yml`

```yaml
tunnel: REDACTED
credentials-file: /home/pi/.cloudflared/REDACTED.json

ingress:
  - hostname: api.overheadtracker.com
    service: http://localhost:3000
  - hostname: dashboard.overheadtracker.com
    service: http://localhost:3000
  - service: http_status:404
```

### DNS records
Added automatically by:
```bash
cloudflared tunnel route dns overhead-tracker api.overheadtracker.com
cloudflared tunnel route dns overhead-tracker dashboard.overheadtracker.com
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

See [`pi-proxy/display.py`](pi-proxy/display.py).

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

## PM2 Services

| Name    | Command                                        | Purpose               |
|---------|------------------------------------------------|-----------------------|
| proxy   | `node /home/pi/proxy/server.js`               | Flight data proxy     |
| tunnel  | `cloudflared tunnel run overhead-tracker`     | Cloudflare Tunnel     |
| display | `python3 /home/pi/proxy/display.py`           | 3.5" TFT dashboard    |

### Setup commands
```bash
pm2 start server.js --name proxy
pm2 start "cloudflared tunnel run overhead-tracker" --name tunnel
pm2 start "python3 /home/pi/proxy/display.py" --name display
pm2 save
pm2 startup  # then run the printed sudo command
```

### Useful PM2 commands
```bash
pm2 status              # check all 3 services are online
pm2 logs proxy          # see [FETCH] and [CACHE HIT] activity
pm2 logs tunnel         # check tunnel connection status
pm2 logs display        # check display rendering errors
pm2 restart proxy       # restart proxy after code changes
pm2 restart tunnel      # restart if tunnel drops
pm2 restart display     # restart after display.py changes
```

---

## Undervoltage Watchdog

The Pi 3B+ can experience undervoltage with insufficient power supplies, causing
services to hang without crashing (PM2 doesn't detect this). A watchdog script
monitors for this and restarts services when needed.

**File:** `/home/pi/proxy/watchdog.sh` — see [`pi-proxy/watchdog.sh`](pi-proxy/watchdog.sh).

### How it works

1. Runs every 60s via cron
2. Reads `vcgencmd get_throttled` for undervoltage flags
3. If undervoltage detected, checks if the proxy responds at `localhost:3000/status`
4. If proxy is unresponsive for 2 consecutive checks (2 min), restarts all PM2 services
5. 5-minute cooldown between restarts to prevent restart storms
6. If PM2 itself is hung, falls back to `pm2 kill` + `pm2 resurrect`

### Setup

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
| CNAME | `api`       | `<your-tunnel-id>.cfargotunnel.com` | On    |
| CNAME | `dashboard` | `<your-tunnel-id>.cfargotunnel.com` | On    |

---

## Client Configuration

### Web app (index.html)
```
https://api.overheadtracker.com/flights?lat=LAT&lon=LON&radius=RADIUS
https://api.overheadtracker.com/weather?lat=LAT&lon=LON
```

### ESP32 (.ino)
```
http://192.168.86.24:3000/flights?lat=LAT&lon=LON&radius=RADIUS
http://192.168.86.24:3000/weather?lat=LAT&lon=LON
```

---

## If the Pi reboots

Both `proxy` and `tunnel` start automatically via PM2.
Peak hour data is restored from `~/proxy/data/peak.json` automatically.
No manual intervention needed.

```bash
pm2 status  # verify both are online after reboot
```

---

## If something breaks

| Symptom | Check |
|---|---|
| `Cannot GET /` on dashboard | Add root route to server.js, restart proxy |
| dashboard.overheadtracker.com not loading | Check Cloudflare tunnel config has dashboard hostname in ingress |
| Stats all zero after restart | Normal — peak.json restores hourly data but session counts reset |
| Website shows fetch error | `pm2 status` — is proxy online? |
| api.overheadtracker.com unreachable | `pm2 logs tunnel` — is tunnel connected? |
| ESP32 shows HTTP ERR | Is Pi powered on? ping 192.168.86.24. ESP32 will fall back to direct API automatically after 3 s. |
| 429 errors from airplanes.live | Cache may have been disabled — check server.js |
| Tunnel restart count very high | `pm2 logs tunnel --lines 20` — check for auth or memory errors |
| Services unresponsive after power issue | `tail -20 ~/proxy/logs/watchdog.log` — check watchdog is in crontab |

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
