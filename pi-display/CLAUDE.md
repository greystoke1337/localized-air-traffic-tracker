# Pi Display (Raspberry Pi 3B+, 3.5" TFT, 480×320)

## Deploy

```bash
scp pi-display/display.py pi@airplanes.local:/home/pi/proxy/display.py && ssh pi@airplanes.local "pm2 restart display"
```

SSH: `pi@airplanes.local`
PM2 processes: `display` (id 0), `tunnel` (id 1)

Or use the `/deploy-display` skill.

## Display Layout

`display.py` renders to `/dev/fb1` via Pygame. Three-page rotation, 15 s cycle:

- **Page 0**: Live flights (nearest 2) + weather strip + flights-today counter + histogram
- **Page 1**: Proxy stats grid (uptime, requests, cache hit, errors, clients, cached, routes, upstream) + API status dots (adsb.lol, adsb.fi, airplanes.live) + histogram + data age
- **Page 2**: Server system stats (OS uptime, CPU temp, RAM%, load avg) + device health cards (Echo/Foxtrot: online dot, fw version, heap, RSSI, uptime, age)

Title bar (24px): health dot left, page title center, page dots right.
Phase colors: CLIMBING=green, OVERHEAD=blue, CRUISING=dim.
Home location: Russell Lea, Sydney (-33.8530, 151.1410), 15 km radius.

## Key Memory Files

- `architecture.md` — display.py internals: polling intervals, rendering pipeline, error handling
