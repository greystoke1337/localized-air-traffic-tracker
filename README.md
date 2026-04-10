# Overhead // Live Aircraft Tracker

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/R6R61VRQHC)

![Echo display showing a Qantas flight on final approach into Sydney](https://raw.githubusercontent.com/greystoke1337/blog/main/assets/images/overhead-tracker-1.jpg)

I live under a flight path, so I'm always curious to know what aircraft is flying overhead. I first built a web page, then thought that it'd be neat to have it on a little screen, so I built a little 4 inch TFT screen with an ESP32. Then another one on a slightly bigger and nicer one, then I started to work on a 64x32 LED matrix... It's getting out of hand!
I also have an ADSB feeder that sends data to a few online services. To not overwhelm these services, I built a little server too on Railway. I have been having a ton of fun with these projects!
Live: [overheadtracker.com](https://www.overheadtracker.com/)

---

## Features

- **Live ADS-B data** from 3 community sources raced in parallel — fastest response wins, automatic fallback if one goes down
- **Flight phase detection** — LANDING, TAKING OFF, APPROACH, DESCENDING, CLIMBING, OVERHEAD — derived from altitude and vertical rate; phase colour bleeds into the UI border
- **Intercept prediction** — tells you if a flight is INBOUND (overhead in ~X min), CROSSING (closest in ~X min), DIRECTLY OVERHEAD NOW, or RECEDING
- **Emergency squawk alerts** — 7700 / 7600 / 7500 trigger a flashing red banner (MAYDAY / NORDO / HIJACK)
- **Route + airline lookup** — departure/arrival city names, airline decoded from ICAO prefix and colour-coded by brand, aircraft photo from Planespotters.net
- **Adjustable geofence** (2–20 km) and altitude floor (200–5,000 ft); settings persist across sessions
- **Demo mode** — `?demo=true&scenario=emergency` and 6 other scenarios for testing without live data

---

## How it works

Geocodes the entered location via Nominatim, then queries the proxy for aircraft within a radius 4× the geofence size. The proxy races three ADS-B APIs in parallel and returns the fastest response, caching results for 45 seconds with coordinate bucketing (nearby clients share cache entries). The web app filters down to aircraft inside the geofence and above the altitude floor, sorts by distance, and renders. Repeats every 15 seconds; any in-flight request is aborted before starting the next.

Route lookups (departure/arrival airports) fire asynchronously via adsbdb.com and update the display when they resolve. Results are cached for 30 minutes. Weather data comes from Open-Meteo, refreshed every 15 minutes.

---

## Proxy server

The Railway-hosted proxy at `api.overheadtracker.com` sits between the web app and upstream APIs.

- Races 3 ADS-B APIs in parallel (adsb.lol, adsb.fi, airplanes.live), uses the fastest response
- 45-second flight cache with coordinate bucketing
- 30-minute route cache for adsbdb.com lookups
- Weather passthrough from Open-Meteo
- Rate limiting: 100 requests/min per IP
- Health check at `/status`
- Daily flight logging with `/stats` and `/report` endpoints

Source: [`server/`](server/)

---

## ESP32 hardware displays

Standalone physical trackers that poll the proxy — no browser needed.

### Echo — Freenove FNK0103S (4.0", 480×320)

**Hardware:** Freenove FNK0103S (ESP32 + 4" ST7796 SPI touchscreen), optional 3D-printed enclosure (STL/STEP in [`tracker_echo/enclosure/`](tracker_echo/enclosure/))

**What it shows:** Callsign (colour-coded by airline), aircraft type and registration, route with city names, and a 4-column dashboard — PHASE | ALT | SPEED | DIST. Cycles through overhead flights every 8 seconds.

**Nav bar:**
- **WX** — weather screen (temperature, humidity, wind, conditions)
- **GEO** — cycles geofence radius: 5 km / 10 km / 20 km
- **CFG** — captive portal for Wi-Fi and location configuration

![Echo weather screen showing 20.4°C, partly cloudy, Monday 9 March](https://raw.githubusercontent.com/greystoke1337/blog/main/assets/images/overhead-tracker-2.jpg)

**Emergency squawk handling:** 7700 / 7600 / 7500 triggers a flashing red banner. Layout compacts automatically to prevent overlap.

**Libraries** (Arduino Library Manager): `LovyanGFX`, `ArduinoJson`, `ArduinoOTA`, `SD`

**Before flashing:** Copy `secrets.h.example` to `secrets.h` and set your default WiFi credentials. On first boot, the captive portal lets you configure WiFi and location (stored in NVS). `PROXY_HOST` in `tracker_echo.ino` defaults to `api.overheadtracker.com`.

```bash
./build.sh          # compile + auto-detect port + upload via USB
./build.sh ota      # compile + upload over Wi-Fi (OTA)
./build.sh monitor  # serial monitor
./build.sh test     # run desktop logic tests (no hardware needed)
```

**Over-the-air updates:** after the first USB flash, the device advertises itself as `overhead-tracker.local` via mDNS. Run `./build.sh ota` to compile and upload wirelessly. The TFT displays a green progress bar during the update.

### Foxtrot — Waveshare ESP32-S3-Touch-LCD-4.3 (4.3", 800×480)

**Hardware:** Waveshare ESP32-S3-Touch-LCD-4.3 (ESP32-S3 + 4.3" IPS parallel RGB display, GT911 capacitive touch, 16 MB flash, 8 MB PSRAM)

Same feature set as Echo, scaled proportionally for the larger 800×480 display. Capacitive touch (no calibration needed), CH422G I/O expander for backlight control, PSRAM for larger buffers.

```bash
arduino-cli compile --fqbn "esp32:esp32:waveshare_esp32_s3_touch_lcd_43B:PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB" tracker_foxtrot/tracker_foxtrot.ino
arduino-cli upload --fqbn "..." --port COM7 --input-dir /tmp/tracker-foxtrot-build tracker_foxtrot/tracker_foxtrot.ino
arduino-cli monitor --port COM7 --config "baudrate=115200"
```

### Golf — Adafruit Matrix Portal M4 (64×32 HUB75 LED matrix)

**Hardware:** Adafruit Matrix Portal M4 driving a 64×32 HUB75 RGB LED matrix panel.

Public-facing display designed to be read from across a room. Callsign in pseudo-bold (colour-coded by airline) for the first half of each 30-second cycle, then switches to the aircraft type name. Route shows origin and destination city. Side bars indicate relative altitude (left) and speed (right). A distance bar runs along the bottom row.

**Libraries** (Arduino Library Manager): `Adafruit Protomatter`, `Adafruit GFX Library`, `ArduinoJson`

**Configuration:** Create `tracker_golf/secrets.h` (gitignored) with your WiFi credentials and home location:

```cpp
#define WIFI_SSID     "your_ssid"
#define WIFI_PASSWORD "your_password"
#define HOME_LAT      -33.8688
#define HOME_LON      151.2093
#define GEOFENCE_KM   20
#define ALT_FLOOR_FT  1000
```

```bash
./build.sh golf         # compile + upload to COM9
./build.sh golf-compile # compile only
```

### Web firmware flasher

`flash.html` — browser-based firmware update tool (Chrome/Edge + USB cable, no dev tools needed). Open [overheadtracker.com/flash](https://overheadtracker.com/flash), plug in the device, click Flash.

### Resilience

3-tier fallback cascade: Railway proxy → direct airplanes.live API (HTTPS) → SD card cache. Proxy calls use a 3-second TCP connect timeout so the device boots cleanly even when the proxy is unreachable. Direct API failures use exponential backoff (15s → 30s → 60s → 120s).

### Preview tools

- `tft-preview.html` — browser simulator of the Echo/Foxtrot display (same pixel coordinates, colours, lookup tables). Interactive controls for every flight phase, squawk code, route length, and altitude.
- `golf-preview.html` — browser simulator of the Golf M4 LED matrix at 10× scale with LED glow effects.

---

## Pi — Raspberry Pi 3B+ (ADS-B feeder + 3.5" TFT display)

A headless Raspberry Pi (`airplanes.local`) with an RTL-SDR dongle receiving raw ADS-B signals and feeding them live to FlightAware and FlightRadar24 (`piaware` + `fr24feed`, both systemd services). It also renders a dashboard to a 3.5" TFT via framebuffer.

Two auto-rotating pages (15-second cycle): **Flights** — up to 2 closest aircraft with callsign, altitude, distance, route, phase, colour-coded. **Stats** — Railway proxy health, API status for all 3 ADS-B sources, 24-hour traffic histogram.

Source: [`pi-display/`](pi-display/)

---

## Data sources

| Source | Data | Key required |
|---|---|---|
| [api.overheadtracker.com](https://api.overheadtracker.com) | Caching proxy (races 3 ADS-B APIs, route + weather passthrough) | No |
| [adsb.lol](https://api.adsb.lol) | Live ADS-B positions (via proxy, raced) | No |
| [adsb.fi](https://opendata.adsb.fi) | Live ADS-B positions (via proxy, raced) | No |
| [airplanes.live](https://api.airplanes.live) | Live ADS-B positions (via proxy, raced) | No |
| [adsbdb.com](https://www.adsbdb.com) | Route lookups, departure/arrival airports (via proxy, cached 30 min) | No |
| [Open-Meteo](https://open-meteo.com) | Weather data (via proxy) | No |
| [Nominatim / OpenStreetMap](https://nominatim.openstreetmap.org) | Location geocoding | No |
| [Planespotters.net](https://planespotters.net) | Aircraft photos | No |
| [Carto](https://carto.com) / OpenStreetMap | Map tiles | No |

---

## Roadmap

- [ ] Push notification when a specific flight appears overhead

---

## License

MIT — do whatever you want with it.
