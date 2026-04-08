---
name: deploy-golf
description: Deploy Golf Arduino firmware to the Adafruit Matrix Portal M4. Use when the user says "deploy golf", "flash golf", "deploy it" after editing Golf code, or after making changes to tracker_golf/.
---

# Deploy Golf (Adafruit Matrix Portal M4 — Arduino)

Compile and upload `tracker_golf/tracker_golf.ino` via arduino-cli to COM9 (running) / COM10 (bootloader).

## Steps

### 1. Compile + upload

Run from `c:/Users/maxim/localized-air-traffic-tracker`:

```bash
./build.sh golf
```

This compiles for `adafruit:samd:adafruit_matrixportal_m4`, sends a 1200-baud touch to COM9 to trigger the bootloader automatically, then uploads. No button press needed.

If you only want to check for compile errors without flashing:

```bash
./build.sh golf-compile
```

### 2. Verify

After upload, open the serial monitor at 115200 baud to confirm:
- `[WIFI] Connected` — WiFi connected successfully
- `[NET] <callsign> ...` — first flight fetch succeeded

### 3. Report summary

Tell the user:
- Whether compile succeeded (or show any errors)
- Whether upload succeeded
- The COM port used (default COM9)

---

## OTA (wireless) deployment

Golf supports WiFi OTA. The device checks for updates on boot and every 6 hours.

### Local OTA (fast dev iteration — no Railway needed)

```bash
# 1. Compile + serve binary locally on :8080
./build.sh golf-serve
# Prints the exact #define lines to add to secrets.h, e.g.:
#   #define OTA_LOCAL_HOST  "192.168.1.42"
#   #define OTA_LOCAL_PORT  8080

# 2. Add those lines to tracker_golf/secrets.h, then flash once via USB
./build.sh golf

# 3. From now on: edit code → golf-serve → reboot device → done (no USB needed)
```

The local server always reports version 9999 so the device always pulls the latest while the server is running. Ctrl-C to stop; device falls back to Railway on the next check.

### Remote OTA (Railway — for deploying to the live device)

```bash
# 1. Compile + stage binary + increment version counter
./build.sh golf-publish

# 2. Deploy the server (must run from project root)
railway up

# 3. Device self-updates within 6 hours, or reboot it to trigger immediately
```

`golf-publish` copies the compiled `.bin` to `server/firmware/golf.bin` and increments `server/firmware/golf-version.txt`. The device compares its `FIRMWARE_VERSION` (in `config.h`) against the server value and self-flashes if the server is newer.

**When to use each method:**
- USB (`./build.sh golf`): first-time setup, or when OTA is broken
- Local OTA (`./build.sh golf-serve`): active development — fast iteration without USB
- Remote OTA (`./build.sh golf-publish` + `railway up`): deploying a finished update to the live device
