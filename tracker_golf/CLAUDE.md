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
- Color derivation with G/B swap — `0xRRGGBB`: R stays R; G hex → B on display; B hex → G on display
  - Amber/orange on display: `0xFF00A0` (full red + ~63% green, no blue)
  - To tune amber: raise `0xA0` toward `0xFF` for more yellow, lower toward `0x60` for deeper orange
- `terminalio.FONT` characters are 6px wide × 8px tall
- **Pseudo-bold / thick text**: render the same label 4 times offset by `(dx, dy)` in `((0,0),(1,0),(0,1),(1,1))` — each font pixel becomes a 2×2 block. No custom font needed.
- **Letter spacing**: `bitmap_label` has no letter-spacing option. Render each character as a separate label, stepping `CHAR_W + GAP` px between centers.

## Rotary encoder

- **Hardware**: Adafruit I2C QT Rotary Encoder with NeoPixel (seesaw, default address `0x36`)
- **Connection**: STEMMA QT port — no soldering, plug-and-play
- **Purpose**: adjusts matrix brightness at runtime (0.05–1.0 in 0.05 steps per detent)
- **Init**: `board.STEMMA_I2C()` then `Seesaw(i2c, addr=0x36)` — do NOT use `busio.I2C(board.SCL, board.SDA)`
- **Read position**: `seesaw.encoder_position(0)` — `IncrementalEncoder` from `adafruit_seesaw.rotaryio` does not work on this board; use the raw seesaw method
- **Brightness target**: `display.framebuffer.brightness` — `display.brightness` (FramebufferDisplay attribute) has no visual effect
- **Encoder LED**: seesaw pin 6, controlled via `adafruit_seesaw.neopixel.NeoPixel`; set to off (`fill(0)`) on boot
- **Encoder button**: seesaw pin 24 — `ss.pin_mode(24, ss.INPUT_PULLUP)` + `ss.digital_read(24)` (not currently used in production code)
- **Library**: `adafruit_seesaw` is NOT in the default CircuitPython bundle that ships with the device — install manually to `CIRCUITPY:/lib/`

## Libraries Required

Install from the [Adafruit CircuitPython Bundle](https://circuitpython.org/libraries) into `/lib` on the CIRCUITPY drive:

- `adafruit_matrixportal/`
- `adafruit_display_text/`
- `adafruit_portalbase/`
- `adafruit_bus_device/`
- `adafruit_requests.mpy`
- `adafruit_connection_manager.mpy`
- `neopixel.mpy`
- `adafruit_seesaw/` — **not in the default bundle**; download separately from the bundle and copy to `CIRCUITPY:/lib/adafruit_seesaw/`

Built into CircuitPython firmware (no install needed):
- `rgbmatrix`, `framebufferio`, `displayio`, `terminalio`, `board`

## Deploy Workflow

No compilation — edit and copy files directly to the CIRCUITPY drive:

1. Connect Matrix Portal M4 via USB — CIRCUITPY drive appears (drive letter varies; find via `wmic logicaldisk get DeviceID,VolumeName`)
2. **Always use Python's `shutil.copy2` to copy files** — `cp` corrupts files on FAT32
3. Copy `tracker_golf/code.py` → `CIRCUITPY:/code.py`
4. Copy `tracker_golf/settings.toml` → `CIRCUITPY:/settings.toml` (WiFi + location, gitignored)
5. Device auto-reloads on file save

Use the `deploy-golf` skill in Claude Code to automate steps 1–3 with syntax checking.

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

Copy `settings.toml.template` to `settings.toml` on the CIRCUITPY drive and fill in real values:

```toml
CIRCUITPY_WIFI_SSID = "your_ssid"
CIRCUITPY_WIFI_PASSWORD = "your_password"
HOME_LAT = -33.8688
HOME_LON = 151.2093
GEOFENCE_KM = 20
ALT_FLOOR_FT = 1000
LOCATION_NAME = "Home"
```

`GEOFENCE_KM` defaults to `10` in code but `20` is recommended for the LED matrix — the wider radius ensures enough flights appear on the small display.

## File Layout

```
tracker_golf/
  code.py                  # Main CircuitPython entry point (copy to CIRCUITPY root)
  test_brightness.py       # Dev-only: radar sweep + Sydney time via worldtimeapi.org
                           # Verifies WiFi, internet, and display; not deployed in production
  settings.toml.template   # Safe to commit — fill in and copy as settings.toml
  .gitignore               # Ignores settings.toml
  CLAUDE.md                # This file
```
