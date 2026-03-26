# Spotter — User Guide

Spotter is a real-time aircraft tracker. It shows you every plane flying over your location right now — airline, route, altitude, speed, and distance — all on a 4.3" touchscreen display.

---

## What's in the Box

- **Spotter device** — 4.3" IPS touchscreen (800x480), ESP32-S3 microcontroller
- **USB-C cable** — for power (any USB adapter, 5V 1A minimum)

That's it. No subscription, no app, no account needed.

---

## Quick Start

Setup takes about 5 minutes. You'll need your phone or laptop and your home WiFi password.

### Step 1: Plug in

Connect the USB-C cable to the device and to any USB power source (wall adapter, power bank, computer USB port). The screen will light up with a boot animation.

### Step 2: Connect to the setup WiFi

On your phone or laptop, open WiFi settings and look for a network called **SPOTTER-SETUP**. Connect to it (no password needed).

### Step 3: Open the setup page

A setup page should open automatically in your browser. If it doesn't, open any browser and go to **192.168.4.1**.

### Step 4: Enter your WiFi details

- **WI-FI NETWORK** — Type your home WiFi network name exactly as it appears
- **WI-FI PASSWORD** — Enter your WiFi password

### Step 5: Enter your location

Type your address, neighborhood, or city. Examples:
- `Wicker Park, Chicago`
- `123 Main Street, Brooklyn`
- `Sydney Airport`

The device will look up the exact coordinates automatically.

### Step 6: Save and go

Tap **SAVE & REBOOT**. The device will restart, connect to your WiFi, and start tracking aircraft within a few seconds.

---

## Understanding the Display

### Header bar (top)

The amber bar at the top shows **SPOTTER** on the left and your **location name** on the right.

### Flight card (main area)

When aircraft are nearby, the display shows:

| Field | Description |
|-------|-------------|
| **Callsign** | Large amber text, e.g., `UAL1218` (United flight 1218) |
| **Airline** | Full airline name with brand color, e.g., `UNITED AIRLINES` |
| **Aircraft type** | What kind of plane, e.g., `BOEING 737-800` |
| **Registration** | Tail number, e.g., `N37510` |
| **Route** | Where it's coming from and going to, e.g., `CHICAGO O'HARE > LOS ANGELES` |

### Dashboard (bottom of flight card)

Four columns of live data:

| Column | What it shows |
|--------|---------------|
| **PHASE** | What the plane is doing — see below |
| **ALT** | Altitude in feet above sea level |
| **SPD** | Ground speed in knots |
| **DIST** | Distance from you in miles |

### Flight phases

| Phase | Color | Meaning |
|-------|-------|---------|
| LANDING | Red | Final approach, about to touch down |
| TAKING OFF | Orange | Just departed, climbing out |
| APPROACH | Orange | Heading toward a nearby airport |
| DESCENDING | Yellow | Losing altitude |
| CLIMBING | Green | Gaining altitude |
| CRUISING | Dim | Steady altitude, passing through |
| OVERHEAD | Yellow | Directly above your location |

### Status bar (very bottom)

Shows: **AC 1/3** (viewing flight 1 of 3), **SRC:PROXY** (data source), **NEXT:12s** (seconds until next refresh).

**CLEAR SKIES** means no aircraft are currently in your tracking range — totally normal during quiet periods.

### Emergency alerts

If a plane declares an emergency (squawk code 7700, 7600, or 7500), a red banner appears at the top of the flight card with the emergency type: MAYDAY, NORDO (no radio), or HIJACK. This is rare.

---

## Touch Controls

### Browse flights

When multiple aircraft are in range, **tap the left half** of the screen to go to the previous flight, or **tap the right half** to go to the next one. Flights also auto-cycle every 8 seconds.

### Navigation buttons (middle bar)

Three buttons appear in the middle bar on the right side:

| Button | What it does |
|--------|--------------|
| **WX** | Toggle weather overlay — shows temperature, wind, visibility, pressure for your location |
| **5mi / 10mi / 20mi** | Tap to cycle tracking range. Larger range = more aircraft but farther away. Recommend **10mi** for most locations, **20mi** near major airports |
| **CFG** | Enter setup mode to change WiFi or location. **Requires double-tap** — first tap turns red with "TAP!", second tap within 3 seconds confirms |

---

## Changing Your Settings

### Change WiFi or location

1. **Double-tap the CFG button** (tap once — it turns red — tap again within 3 seconds)
2. The device enters setup mode and creates the **SPOTTER-SETUP** WiFi network
3. Connect to it on your phone and follow the same steps as initial setup
4. Your new settings are saved permanently

### Change tracking range

Just tap the range button (shows **5mi**, **10mi**, or **20mi**) to cycle through presets. This takes effect immediately — no restart needed. Your choice is remembered across power cycles.

### Advanced: SD card configuration

For advanced users, you can place a file called `config.txt` on a micro SD card with these settings:

```
lat=41.9028
lon=-87.6773
geofence=20
alt_floor=500
name=CHICAGO
```

Insert the SD card before powering on. SD card settings override WiFi setup values.

---

## Troubleshooting

### No flights showing / "CLEAR SKIES"

This is normal! Not every moment has aircraft in range. Try:
- Increasing range to **20mi** (tap the range button)
- Waiting a few minutes — air traffic varies throughout the day
- If you're far from airports or flight paths, 20mi range is recommended

### Screen is frozen or not updating

Unplug the USB-C cable, wait a few seconds, and plug it back in. The device will reboot and resume tracking automatically.

### WiFi won't connect

- Make sure you entered the WiFi name and password correctly (case-sensitive)
- Double-tap CFG to re-enter setup mode and try again
- The device works on 2.4 GHz WiFi only (not 5 GHz)
- Move the device closer to your router if signal is weak

### Device restarts on its own

This is a built-in safety feature. If WiFi is lost for more than 30 minutes, the device automatically reboots to try reconnecting. It's working as intended.

### Data shows "CACHE" or "DIRECT"

- **PROXY** — Normal operation, getting data from our cloud server
- **DIRECT** — Cloud server temporarily unavailable, getting data directly from aviation APIs (still works fine)
- **CACHE** — Showing recently cached data while reconnecting. Data may be a few seconds old

---

## Updating Firmware

Your device can be updated with new features and improvements through your web browser — no special software needed.

1. Open **Google Chrome** or **Microsoft Edge** on a computer
2. Go to [overheadtracker.com/flash](https://overheadtracker.com/flash)
3. Connect the device to your computer with a USB-C cable
4. Click **CONNECT** and select the device from the popup
5. Click **FLASH** and wait for the update to complete (about 30 seconds)
6. The device reboots with the new firmware — all your settings are preserved

**Note:** Safari and Firefox don't support the Web Serial feature needed for updates. Use Chrome or Edge.

---

## Monitoring Your Device Remotely

You can check your device's status from anywhere:

Visit [overheadtracker.com/status](https://overheadtracker.com/status)

The dashboard shows:
- **Green dot** — Device is online and healthy
- **Red dot** — Device hasn't reported in recently (check power/WiFi)
- **WiFi signal strength** — How strong the connection is
- **Memory health** — Internal diagnostics
- **Firmware version** — What version is installed
- **Last seen** — When the device last checked in

The device reports its status every 60 seconds.

---

## Specifications

| Detail | Value |
|--------|-------|
| Display | 4.3" IPS, 800x480, capacitive touch |
| Processor | ESP32-S3 dual-core, 240 MHz |
| WiFi | 802.11 b/g/n (2.4 GHz) |
| Power | USB-C, 5V, ~300mA typical |
| Data refresh | Every 15 seconds |
| Tracking range | 5, 10, or 20 miles (configurable) |
| Data sources | ADS-B exchange APIs via cloud proxy, with direct fallback |

---

## Support

If you need help, visit [overheadtracker.com/status](https://overheadtracker.com/status) to check device health, or reach out to the person who set up your Spotter.
