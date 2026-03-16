---
name: flash-and-log
description: Compile and flash ESP32 firmware via USB, then start a timed serial log capture for crash debugging. Use when the user says "flash", "upload firmware", "flash and log", or wants to test new firmware changes on the device.
argument-hint: "[device] [log_minutes] [log_label]"
---

# Flash ESP32 + Serial Log Capture

Compile, flash, and capture serial output from the ESP32 in one workflow.

## Device Selection

Two devices exist — determine which one from context or arguments:

| Device | Directory | COM Port | FQBN |
|--------|-----------|----------|------|
| **Echo** (default) | `tracker_live_fnk0103s/` | COM4 | `esp32:esp32:esp32:PartitionScheme=min_spiffs` |
| **Foxtrot** | `tracker_foxtrot/` | COM7 | `esp32:esp32:waveshare_esp32_s3_touch_lcd_43B:PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB` |

If `$ARGUMENTS[0]` is "echo" or "foxtrot" (case-insensitive), use that device and shift remaining args. Otherwise default to Echo.

For **Echo**, use `build.sh` as described below. For **Foxtrot**, use `arduino-cli` directly:
```bash
CLI="/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
CFG="C:/Users/maxim/.arduinoIDE/arduino-cli.yaml"
FQBN="esp32:esp32:waveshare_esp32_s3_touch_lcd_43B:PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB"
"$CLI" --config-file "$CFG" compile --fqbn "$FQBN" --build-path /tmp/tracker-foxtrot-build tracker_foxtrot/tracker_foxtrot.ino
"$CLI" --config-file "$CFG" upload --fqbn "$FQBN" --port COM7 --input-dir /tmp/tracker-foxtrot-build tracker_foxtrot/tracker_foxtrot.ino
```

## Arguments

- `$ARGUMENTS[0]` — Device name ("echo" or "foxtrot") OR log duration in minutes (default: 20)
- `$ARGUMENTS[1]` — Log duration in minutes if device was specified, or label (default: 20)
- `$ARGUMENTS[2]` — Descriptive label for the log file (default: "debug")

## Platform Detection

Detect the current platform first:

```bash
uname -s
```

- **Darwin** → macOS flow (see macOS Steps)
- **MINGW/MSYS/CYGWIN** → Windows/Git Bash flow (see Windows Steps)

---

## macOS Steps

### 1. Compile the firmware

```bash
export PATH=~/bin:$PATH && bash build.sh compile
```

If compilation fails, stop and report the errors. Do NOT proceed to flash.

### 2. Flash via USB

Auto-detect the serial port, then flash:

```bash
ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART* 2>/dev/null | head -1
```

```bash
export PATH=~/bin:$PATH && bash build.sh upload <DETECTED_PORT>
```

If the port is busy, wait 3 seconds and retry once.

### 3. Start serial log capture in background

Wait 5 seconds after flash for the ESP32 to boot, then start capture.

Use a **descriptive filename**: `logs/<label>-YYYY-MM-DD.log`

Where `<label>` comes from `$ARGUMENTS[1]` or defaults to "debug".

Run the capture in the background using `run_in_background: true`.

```bash
mkdir -p logs && sleep 5 && python3 -u -c "
import serial, sys, time

port, baud, minutes = '<DETECTED_PORT>', 115200, <MINUTES>
logfile = 'logs/<LABEL>-<DATE>.log'
duration = minutes * 60

ser = serial.Serial(port, baud, timeout=1)
time.sleep(0.1)
ser.reset_input_buffer()

start = time.time()
deadline = start + duration
line_count = 0

print(f'Capturing to {logfile} for {minutes} min... (Ctrl-C to stop early)')

with open(logfile, 'w', encoding='utf-8') as f:
    try:
        while time.time() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode('utf-8', errors='replace').rstrip()
            elapsed = time.time() - start
            ts = f'[{elapsed:8.3f}]'
            stamped = f'{ts} {line}'
            f.write(stamped + '\n')
            f.flush()
            print(stamped)
            line_count += 1
    except KeyboardInterrupt:
        pass

elapsed = time.time() - start
ser.close()
print(f'\n--- Capture complete ---')
print(f'Lines: {line_count}')
print(f'Duration: {elapsed:.0f}s ({elapsed/60:.1f} min)')
print(f'File: {logfile}')
"
```

---

## Windows (Git Bash) Steps

### 1. Kill any Python processes holding COM4

```bash
powershell -Command "Get-Process python* | Select-Object Id, ProcessName, StartTime"
```

If any Python processes are running, kill them:

```bash
powershell -Command "Stop-Process -Id <PID> -Force"
```

Wait 2 seconds for the port to release.

### 2. Compile the firmware

```bash
cd /c/Users/maxim/localized-air-traffic-tracker && ./build.sh compile
```

If compilation fails, stop and report the errors. Do NOT proceed to flash.

### 3. Flash via USB

```bash
cd /c/Users/maxim/localized-air-traffic-tracker && ./build.sh upload COM4
```

If the port is busy, wait 3 seconds and retry once. If it still fails, report the error.

### 4. Start serial log capture in background

Wait 5 seconds after flash for the ESP32 to boot, then start capture.

Use a **descriptive filename**: `logs/<label>-YYYY-MM-DD.log`

Where `<label>` comes from `$ARGUMENTS[1]` or defaults to "debug".

Run the capture in the background using `run_in_background: true`.

```bash
cd /c/Users/maxim/localized-air-traffic-tracker && mkdir -p logs && sleep 5 && /c/python314/python.exe -u -c "
import serial, sys, time

port, baud, minutes = 'COM4', 115200, <MINUTES>
logfile = 'logs/<LABEL>-<DATE>.log'
duration = minutes * 60

ser = serial.Serial(port, baud, timeout=1)
time.sleep(0.1)
ser.reset_input_buffer()

start = time.time()
deadline = start + duration
line_count = 0

print(f'Capturing to {logfile} for {minutes} min... (Ctrl-C to stop early)')

with open(logfile, 'w', encoding='utf-8') as f:
    try:
        while time.time() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode('utf-8', errors='replace').rstrip()
            elapsed = time.time() - start
            ts = f'[{elapsed:8.3f}]'
            stamped = f'{ts} {line}'
            f.write(stamped + '\n')
            f.flush()
            print(stamped)
            line_count += 1
    except KeyboardInterrupt:
        pass

elapsed = time.time() - start
ser.close()
print(f'\n--- Capture complete ---')
print(f'Lines: {line_count}')
print(f'Duration: {elapsed:.0f}s ({elapsed/60:.1f} min)')
print(f'File: {logfile}')
"
```

---

## Final Steps (both platforms)

### Verify capture started

Wait 10 seconds, then read the first 15 lines of the log file to confirm the ESP32 booted and is producing output.

### Report summary

Tell the user:
- Compilation result (flash size / RAM usage)
- Flash result (success/fail)
- Log file path and duration
- First few lines showing the ESP32 booted (WiFi connected, proxy responding, heap stats)
- Background task ID so they know the capture is running
