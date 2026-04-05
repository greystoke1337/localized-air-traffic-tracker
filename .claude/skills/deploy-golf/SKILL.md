---
name: deploy-golf
description: Deploy Golf Arduino firmware to the Adafruit Matrix Portal M4. Use when the user says "deploy golf", "flash golf", "deploy it" after editing Golf code, or after making changes to tracker_golf_m4/.
---

# Deploy Golf (Adafruit Matrix Portal M4 — Arduino)

Compile and upload `tracker_golf_m4/tracker_golf_m4.ino` via arduino-cli to COM9 (running) / COM10 (bootloader).

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
- The COM port used (default COM11)
