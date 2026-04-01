# Golf — Adafruit Matrix Portal M4

## Hardware

- **Board**: Adafruit Matrix Portal M4 (SAMD51 + ESP32 WiFi co-processor)
- **Display**: 64×32 HUB75 RGB LED matrix (single panel)
- **Language**: CircuitPython 10.1.4 (not Arduino)
- **COM port**: COM11
- **No FQBN** — CircuitPython, no compilation step

## Purpose

Public-facing flight display. Shows overhead aircraft for a wider audience.
Scrolling ticker or stacked layout: callsign + route + altitude/phase + speed.

## Key Hardware Facts

- Panel is **64×32**, not 128×64 — initialize as `Matrix(width=64, height=32, bit_depth=4)`
- `bit_depth=4` gives 16 PWM levels for smooth fade gradients; `bit_depth=2` (4 levels) is too coarse for animations
- Panel is mounted **upside down** — always set `display.rotation = 180`
- `Matrix` sets `auto_refresh=False` internally — must call `display.refresh()` to push to hardware
- G and B color channels are **swapped** on this panel: use `0x0000FF` for green, `0x00FF00` for blue
- `terminalio.FONT` characters are 6px wide × 8px tall

## Libraries Required

Install from the [Adafruit CircuitPython Bundle](https://circuitpython.org/libraries) into `/lib` on the CIRCUITPY drive:

- `adafruit_matrixportal/`
- `adafruit_display_text/`
- `adafruit_portalbase/`
- `adafruit_bus_device/`
- `adafruit_requests.mpy`
- `adafruit_connection_manager.mpy`
- `neopixel.mpy`

Built into CircuitPython firmware (no install needed):
- `rgbmatrix`, `framebufferio`, `displayio`, `terminalio`, `board`

## Deploy Workflow

No compilation — edit and copy files directly to the CIRCUITPY drive:

1. Connect Matrix Portal M4 via USB — CIRCUITPY drive appears
2. **Always use Python's `shutil.copy2` to copy files** — `cp` corrupts files on FAT32
3. Copy `tracker_golf/code.py` → `CIRCUITPY:/code.py`
4. Copy `tracker_golf/settings.toml` → `CIRCUITPY:/settings.toml` (WiFi + location, gitignored)
5. Device auto-reloads on file save

## Serial Monitor

Use pyserial — VS Code serial monitor and PowerShell miss output due to timing/encoding issues:

```python
import serial, time, sys
s = serial.Serial('COM11', 115200, timeout=0.5)
s.write(b'\x03\x04')  # Ctrl+C + Ctrl+D = soft reset
deadline = time.time() + 15
while time.time() < deadline:
    line = s.readline()
    if line:
        sys.stdout.buffer.write(line)
        sys.stdout.buffer.flush()
s.close()
```

## Config (settings.toml — gitignored)

Copy `settings.toml.template` to `settings.toml` on the CIRCUITPY drive:

```toml
CIRCUITPY_WIFI_SSID = "your_ssid"
CIRCUITPY_WIFI_PASSWORD = "your_password"
HOME_LAT = -33.8688
HOME_LON = 151.2093
GEOFENCE_KM = 10
ALT_FLOOR_FT = 1000
LOCATION_NAME = "Home"
```

## File Layout

```
tracker_golf/
  code.py                  # Main CircuitPython entry point (copy to CIRCUITPY root)
  settings.toml.template   # Safe to commit — fill in and copy as settings.toml
  .gitignore               # Ignores settings.toml
  CLAUDE.md                # This file
```
