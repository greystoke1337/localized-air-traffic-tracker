---
name: flash-and-log
description: Compile and flash ESP32 firmware via USB, then start a timed serial log capture for crash debugging. Use when the user says "flash", "upload firmware", "flash and log", or wants to test new firmware changes on the device.
argument-hint: "[device] [log_minutes] [log_label]"
---

# Flash ESP32 + Serial Log Capture

Compile, flash, and capture serial output in one workflow.

## Device Selection

| Device | Directory | COM Port | FQBN |
|--------|-----------|----------|------|
| **Echo** (default) | `tracker_echo/` | COM4 | `esp32:esp32:esp32:PartitionScheme=min_spiffs` |
| **Foxtrot** | `tracker_foxtrot/` | COM7 | `esp32:esp32:waveshare_esp32_s3_touch_lcd_43B:PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB` |

If `$ARGUMENTS[0]` is "echo" or "foxtrot" (case-insensitive), use that device and shift remaining args. Otherwise default to Echo.

## Arguments

- `$ARGUMENTS[0]` — Device name ("echo" or "foxtrot") OR log duration in minutes (default: 5)
- `$ARGUMENTS[1]` — Log duration in minutes if device was specified, or label (default: 5)
- `$ARGUMENTS[2]` — Descriptive label for the log file (default: "debug")

---

## Steps

### 1. Compile

**Echo:**
```bash
cd /c/Users/maxim/localized-air-traffic-tracker && ./build.sh compile
```

**Foxtrot:**
```bash
CLI="/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
CFG="C:/Users/maxim/.arduinoIDE/arduino-cli.yaml"
FQBN="esp32:esp32:waveshare_esp32_s3_touch_lcd_43B:PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB"
"$CLI" --config-file "$CFG" compile --fqbn "$FQBN" --build-path /tmp/tracker-foxtrot-build tracker_foxtrot/tracker_foxtrot.ino
```

If compilation fails, stop and report errors. Do NOT proceed to flash.

### 2. Flash

**Echo:**
```bash
cd /c/Users/maxim/localized-air-traffic-tracker && ./build.sh upload COM4
```

**Foxtrot:**
```bash
"$CLI" --config-file "$CFG" upload --fqbn "$FQBN" --port COM7 --input-dir /tmp/tracker-foxtrot-build tracker_foxtrot/tracker_foxtrot.ino
```

If the port is busy, wait 3 seconds and retry once.

### 3. Serial log capture (background)

Start capture immediately after flash completes. Use `run_in_background: true`. Filename: `logs/<label>-YYYY-MM-DD.log`

Set PORT to COM4 (Echo) or COM7 (Foxtrot).

```bash
cd /c/Users/maxim/localized-air-traffic-tracker && mkdir -p logs && sleep 2 && /c/python314/python.exe -u -c "
import serial, sys, time

port, baud, minutes = '<PORT>', 115200, <MINUTES>
logfile = 'logs/<LABEL>-<DATE>.log'
duration = minutes * 60

ser = serial.Serial(port, baud, timeout=1)
ser.reset_input_buffer()

start = time.time()
deadline = start + duration
line_count = 0

print(f'Capturing to {logfile} for {minutes} min...')

with open(logfile, 'w', encoding='utf-8') as f:
    try:
        while time.time() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode('utf-8', errors='replace').rstrip()
            elapsed = time.time() - start
            stamped = f'[{elapsed:8.3f}] {line}'
            f.write(stamped + '\n')
            f.flush()
            print(stamped)
            line_count += 1
    except KeyboardInterrupt:
        pass

ser.close()
elapsed = time.time() - start
print(f'Done: {line_count} lines, {elapsed:.0f}s, {logfile}')
"
```

### 4. Verify and report

Wait 3 seconds, then read the first 10 lines of the log file. Tell the user:
- Compile: success/fail + flash size
- Flash: success/fail
- Log: file path + background task ID
- First boot lines
