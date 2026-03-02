---
name: flash-and-log
description: Compile and flash ESP32 firmware via USB, then start a timed serial log capture for crash debugging. Use when the user says "flash", "upload firmware", "flash and log", or wants to test new firmware changes on the device.
argument-hint: [log_minutes] [log_label]
allowed-tools: Bash, Read, Grep, Glob
---

# Flash ESP32 + Serial Log Capture

Compile, flash, and capture serial output from the ESP32 in one workflow.

## Arguments

- `$ARGUMENTS[0]` — Log duration in minutes (default: 20)
- `$ARGUMENTS[1]` — Descriptive label for the log file (default: "debug")

## Steps

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

Use an inline Python script (same pattern as `build.sh log`) with a **descriptive filename**:

```
logs/<label>-YYYY-MM-DD.log
```

Where `<label>` comes from `$ARGUMENTS[1]` or defaults to "debug".

Run the capture in the background using `run_in_background: true` so it doesn't block the conversation.

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

### 5. Verify capture started

Wait 10 seconds, then read the first 15 lines of the log file to confirm the ESP32 booted and is producing output.

### 6. Report summary

Tell the user:
- Compilation result (flash size / RAM usage)
- Flash result (success/fail)
- Log file path and duration
- First few lines showing the ESP32 booted (WiFi connected, proxy responding, heap stats)
- Background task ID so they know the capture is running
