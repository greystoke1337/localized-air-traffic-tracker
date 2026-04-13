# Pi Display (Raspberry Pi 3B+, 3.5" TFT, 480×320)

## Deploy

```bash
scp pi-display/display.py pi@airplanes.local:/home/pi/tft_display.py && ssh -i ~/.ssh/pi_proxy pi@airplanes.local "sudo systemctl restart tft-display"
```

SSH: `pi@airplanes.local` (key: `~/.ssh/pi_proxy`)

Process manager: **systemd** (`tft-display.service`), runs as root.
Depends on `readsb.service`. Logs via `journalctl -u tft-display`.

## Display Layout

Single static page, refreshes every 2 s. Renders to `/dev/fb0` via Pygame → RGB565.
Reads local readsb files directly — **no network calls**.

Header bar (36 px): `ADS-B RECEIVER` title, cyan underline.

**Left column — HEALTH**
- TEMP (°C, colour-coded cyan/amber/red)
- CPU %
- MEMORY (used / total MB)
- UPTIME

**Left column — RECEIVER** (from `/run/readsb/stats.json` → `last1min`)
- MSGS/MIN — valid messages per minute
- SIGNAL — dBFS (amber if > −10)
- NOISE — dBFS
- STRONG — strong signal count (red if > 0)
- TRACKS — total aircraft tracked

**Right column — AIRCRAFT** (from `/run/readsb/aircraft.json`)
- TOTAL — all aircraft (how many have position)
- FURTHEST — callsign + distance (nm)
- HIGHEST — callsign + flight level
- NEAREST — callsign + distance (nm)
- EMRG — squawk 7500/7600/7700 or emergency flag (red if active, dash if clear)

Touch on `/dev/input/event2` toggles backlight (GPIO 24) with 0.5 s debounce.
