#!/usr/bin/env bash
# ─── Overhead Tracker — build / upload / test helper ─────────────────────────
#
# Usage (run from project root):
#   ./build.sh               → compile + auto-detect port + upload via USB
#   ./build.sh compile       → compile only (error-check)
#   ./build.sh upload        → upload last build via USB (auto-detect port)
#   ./build.sh upload COM5   → upload last build to a specific port
#   ./build.sh monitor       → open serial monitor on auto-detected port
#   ./build.sh monitor COM5  → open serial monitor on specific port
#   ./build.sh send <cmd>    → send debug command to ESP32, print JSON response
#   ./build.sh send <cmd> COM5 → send to specific port
#   ./build.sh ota           → compile + OTA upload (overhead-tracker.local)
#   ./build.sh validate      → compile with all warnings + safety checks
#   ./build.sh test          → run desktop logic tests (no hardware needed)
#   ./build.sh log           → capture serial output for 20 min (default)
#   ./build.sh log 5         → capture for 5 minutes
#   ./build.sh log 20 COM5   → capture on specific port
#   ./build.sh safe          → test + validate (full pre-push check)
#   ./build.sh stress        → 10-min chaos stress test (auto-patches HOST+PORT, flashes, restores)
#   ./build.sh stress 20     → 20-min stress test
#   ./build.sh stress 10 COM5 → stress test on specific port
#   ./build.sh stress 10 COM4 transition → stress with transition mode
#   ./build.sh proxy-host 192.168.86.30 3001 COM4 → patch HOST+PORT + compile + flash
#   ./build.sh foxtrot-stress        → 10-min chaos stress test for Foxtrot (auto-patches, flashes, restores)
#   ./build.sh foxtrot-stress 20     → 20-min Foxtrot stress test
#   ./build.sh foxtrot-stress 10 COM8 → Foxtrot stress test on specific port
#   ./build.sh foxtrot-stress 10 COM7 transition → Foxtrot stress with transition mode
#   ./build.sh foxtrot-proxy-host 192.168.86.30 3001 COM7 → patch Foxtrot HOST+PORT + compile + flash
#   OVERHEAD_TRACKER_IP=x.x.x.x ./build.sh ota  → OTA to specific IP
#   ./build.sh golf              → compile + upload Golf (Matrix Portal M4) to COM9 (auto-touches bootloader)
#   ./build.sh golf-compile      → compile Golf only
#   ./build.sh golf-publish      → compile Golf + copy binary + increment version in server/firmware/ (then railway up)
#   ./build.sh golf-serve        → compile Golf + serve binary locally on :8080 (HTTP OTA for dev; set OTA_LOCAL_HOST in secrets.h)
#
# ──────────────────────────────────────────────────────────────────────────────

set -e

SKETCH="tracker_echo/tracker_echo.ino"
FQBN="esp32:esp32:esp32:PartitionScheme=min_spiffs"
FOXTROT_SKETCH="tracker_foxtrot/tracker_foxtrot.ino"
FOXTROT_FQBN="esp32:esp32:waveshare_esp32_s3_touch_lcd_43B:PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB"
FOXTROT_BUILD_DIR="/tmp/overhead-tracker-foxtrot-build"
DELTA_SKETCH="tracker_delta/tracker_delta.ino"
DELTA_FQBN="esp32:esp32:esp32s3:PSRAM=opi,USBMode=hwcdc,PartitionScheme=app3M_fat9M_16MB,FlashSize=16M"
DELTA_BUILD_DIR="/tmp/overhead-tracker-delta-build"
DELTA_PORT="${DELTA_PORT:-COM8}"
LVGL_LIB_ROOT="C:\\Users\\maxim\\OneDrive\\Documents\\Arduino\\libraries\\lvgl"
GOLF_SKETCH="tracker_golf/tracker_golf.ino"
GOLF_FQBN="adafruit:samd:adafruit_matrixportal_m4"
GOLF_BUILD_DIR="/tmp/overhead-tracker-golf-build"
GOLF_PORT="${GOLF_PORT:-COM9}"
BUILD_DIR="/tmp/overhead-tracker-build"
BAUD=115200
OTA_HOST="${OVERHEAD_TRACKER_IP:-overhead-tracker.local}"
OTA_PORT=3232
BIN_FILE="$BUILD_DIR/tracker_echo.ino.bin"
TEST_DIR="tests"

# ── Platform detection ───────────────────────────────────────────────────────
detect_platform() {
  case "$(uname -s)" in
    Darwin*)
      PLATFORM="mac"
      ARDUINO_CLI="${ARDUINO_CLI:-$(command -v arduino-cli 2>/dev/null || echo "arduino-cli")}"
      CONFIG_FILE="${ARDUINO_CLI_CONFIG:-}"
      ESPOTA="${ESPOTA:-$(find ~/Library/Arduino15/packages/esp32 -name "espota.py" 2>/dev/null | head -1)}"
      ;;
    MINGW*|MSYS*|CYGWIN*)
      PLATFORM="windows"
      ARDUINO_CLI="${ARDUINO_CLI:-/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe}"
      CONFIG_FILE="${ARDUINO_CLI_CONFIG:-C:/Users/$(whoami)/.arduinoIDE/arduino-cli.yaml}"
      ESPOTA="${ESPOTA:-/c/Users/$(whoami)/AppData/Local/Arduino15/packages/esp32/hardware/esp32/3.3.7/tools/espota.exe}"
      # Add MSYS2 gcc/g++ to PATH if available
      if [ -d "/c/msys64/ucrt64/bin" ]; then
        export PATH="/c/msys64/ucrt64/bin:$PATH"
      fi
      ;;
    Linux*)
      PLATFORM="linux"
      ARDUINO_CLI="${ARDUINO_CLI:-$(command -v arduino-cli 2>/dev/null || echo "arduino-cli")}"
      CONFIG_FILE="${ARDUINO_CLI_CONFIG:-}"
      ESPOTA="${ESPOTA:-$(find ~/.arduino15/packages/esp32 -name "espota.py" 2>/dev/null | head -1)}"
      ;;
    *)
      PLATFORM="unknown"
      ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
      CONFIG_FILE=""
      ;;
  esac
}

detect_platform

CMD="${1:-all}"

die() { echo -e "\n[ERROR] $*" >&2; exit 1; }
info() { echo -e "\n>>> $*"; }
pass() { echo -e "  [PASS] $*"; }
fail() { echo -e "  [FAIL] $*"; }

# Build config-file flag (only if set)
config_flag() {
  if [ -n "$CONFIG_FILE" ] && [ -f "$CONFIG_FILE" ]; then
    echo "--config-file $CONFIG_FILE"
  fi
}

# ── Compile ──────────────────────────────────────────────────────────────────
run_compile() {
  info "COMPILE  ($FQBN) [$PLATFORM]"
  "$ARDUINO_CLI" compile \
    $(config_flag) \
    --fqbn        "$FQBN" \
    --build-path  "$BUILD_DIR" \
    "$SKETCH"
  info "Compile OK — binary in $BUILD_DIR"
}

# ── Port detection ────────────────────────────────────────────────────────────
detect_port() {
  case "$PLATFORM" in
    windows)
      powershell -Command "
        Get-WMIObject Win32_SerialPort |
        Where-Object { \$_.Caption -match 'CH340|CP210|UART|USB Serial|USB-SERIAL' } |
        Select-Object -First 1 -ExpandProperty DeviceID
      " 2>/dev/null | tr -d '\r\n'
      ;;
    mac)
      ls /dev/cu.usbserial-* /dev/cu.wchusbserial-* /dev/cu.SLAB_USBtoUART 2>/dev/null | head -1
      ;;
    linux)
      ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1
      ;;
  esac
}

resolve_port() {
  local port="${1:-}"
  if [ -z "$port" ]; then
    info "Detecting port..."
    port=$(detect_port)
    [ -n "$port" ] || die "No ESP32 serial port found. Is it plugged in?"
    echo "    Found: $port"
  fi
  echo "$port"
}

# ── Upload ───────────────────────────────────────────────────────────────────
run_upload() {
  local port
  port=$(resolve_port "${2:-}")
  info "UPLOAD → $port"
  "$ARDUINO_CLI" upload \
    $(config_flag) \
    --fqbn        "$FQBN" \
    --port        "$port" \
    --input-dir   "$BUILD_DIR" \
    "$SKETCH"
  info "Upload complete."
}

# ── OTA Upload ────────────────────────────────────────────────────────────────
run_ota() {
  run_compile
  info "OTA UPLOAD → $OTA_HOST:$OTA_PORT"
  if [ -n "$ESPOTA" ] && [ -f "$ESPOTA" ]; then
    "$ESPOTA" -i "$OTA_HOST" -p "$OTA_PORT" -f "$BIN_FILE" -r
  else
    die "espota not found. Set ESPOTA environment variable."
  fi
  info "OTA upload complete."
}

# ── Serial monitor ────────────────────────────────────────────────────────────
run_monitor() {
  local port
  port=$(resolve_port "${2:-}")
  info "MONITOR → $port @ ${BAUD} baud  (Ctrl-C to exit)"
  "$ARDUINO_CLI" monitor \
    $(config_flag) \
    --port        "$port" \
    --config      "baudrate=$BAUD"
}

# ── Send serial command ───────────────────────────────────────────────────────
run_send() {
  local cmd="${2:?Usage: build.sh send <command> [port]}"
  local port
  port=$(resolve_port "${3:-}")
  info "SEND '$cmd' → $port"
  /c/python314/python.exe -u -c "
import serial, sys, time

port, cmd, timeout = sys.argv[1], sys.argv[2], 10

ser = serial.Serial(port, 115200, timeout=1)
time.sleep(0.1)
ser.reset_input_buffer()
ser.write((cmd + '\n').encode())
ser.flush()

deadline = time.time() + timeout
capturing = False
lines = []

while time.time() < deadline:
    raw = ser.readline()
    if not raw:
        continue
    line = raw.decode('utf-8', errors='replace').strip()
    if line == '>>>CMD_START<<<':
        capturing = True
        continue
    if line == '>>>CMD_END<<<':
        print('\n'.join(lines))
        ser.close()
        sys.exit(0)
    if capturing:
        lines.append(line)

ser.close()
print('ERROR: timeout waiting for response', file=sys.stderr)
sys.exit(1)
" "$port" "$cmd"
}

# ── Validate (compile with warnings + safety checks) ─────────────────────────
run_validate() {
  info "VALIDATE — compile with warnings + safety checks"
  local errors=0

  # 1. Check for secrets.h in git staging
  if git diff --cached --name-only 2>/dev/null | grep -q "secrets\.h$"; then
    fail "secrets.h is staged for commit — remove it!"
    errors=$((errors + 1))
  else
    pass "secrets.h not staged"
  fi

  # 2. Check for hardcoded local IPs in committed code (proxy IP in .ino is expected)
  local suspicious
  suspicious=$(grep -rn "192\.168\." tracker_echo/*.ino tracker_echo/*.h 2>/dev/null \
    | grep -v "PROXY_HOST" | grep -v "192\.168\.4\.1" || true)
  if [ -n "$suspicious" ]; then
    fail "Hardcoded local IP found (not PROXY_HOST):"
    echo "$suspicious"
    errors=$((errors + 1))
  else
    pass "No unexpected hardcoded IPs"
  fi

  # 3. Compile with all warnings
  info "Compiling with --warnings all..."
  "$ARDUINO_CLI" compile \
    $(config_flag) \
    --fqbn        "$FQBN" \
    --build-path  "$BUILD_DIR" \
    --warnings    all \
    "$SKETCH" 2>&1 | tee /tmp/overhead-tracker-warnings.log
  local compile_status=${PIPESTATUS[0]}
  if [ $compile_status -ne 0 ]; then
    fail "Compilation failed!"
    errors=$((errors + 1))
  else
    pass "Compilation succeeded"
  fi

  # 4. Report binary size
  if [ -f "$BIN_FILE" ]; then
    local size
    size=$(wc -c < "$BIN_FILE" | tr -d ' ')
    local size_kb=$((size / 1024))
    local max_kb=1920  # ~1.9 MB usable on ESP32 with min_spiffs partition
    local pct=$((size_kb * 100 / max_kb))
    if [ $pct -gt 90 ]; then
      fail "Binary size: ${size_kb} KB / ${max_kb} KB (${pct}%) — CLOSE TO LIMIT"
      errors=$((errors + 1))
    elif [ $pct -gt 75 ]; then
      echo "  [WARN] Binary size: ${size_kb} KB / ${max_kb} KB (${pct}%)"
    else
      pass "Binary size: ${size_kb} KB / ${max_kb} KB (${pct}%)"
    fi
  fi

  # 5. Check for warning count
  local warn_count
  warn_count=$(grep -c "warning:" /tmp/overhead-tracker-warnings.log 2>/dev/null || echo 0)
  if [ "$warn_count" -gt 0 ]; then
    echo "  [WARN] $warn_count compiler warning(s) — review /tmp/overhead-tracker-warnings.log"
  else
    pass "Zero compiler warnings"
  fi

  echo ""
  if [ $errors -gt 0 ]; then
    die "Validation failed with $errors error(s)"
  fi
  info "Validation PASSED"
}

# ── Test (run desktop logic tests) ───────────────────────────────────────────
run_test() {
  info "TEST — running desktop logic tests"
  local errors=0

  # Test 1: Flight logic (pure C)
  if [ -f "$TEST_DIR/test_flight_logic.c" ]; then
    info "Compiling test_flight_logic..."
    if gcc -std=c11 -Wall -Wextra -o /tmp/test_flight_logic "$TEST_DIR/test_flight_logic.c" -lm; then
      if /tmp/test_flight_logic; then
        pass "test_flight_logic"
      else
        fail "test_flight_logic — assertions failed"
        errors=$((errors + 1))
      fi
    else
      fail "test_flight_logic — compilation failed"
      errors=$((errors + 1))
    fi
  else
    echo "  [SKIP] test_flight_logic.c not found"
  fi

  # Test 2: JSON parsing (C++ with ArduinoJson)
  if [ -f "$TEST_DIR/test_parsing.cpp" ]; then
    info "Compiling test_parsing..."
    local json_include="tracker_echo/libraries/ArduinoJson/src"
    if [ ! -d "$json_include" ]; then
      echo "  [SKIP] ArduinoJson not found at $json_include"
    elif g++ -std=c++17 -Wall -Wextra -I"$json_include" -o /tmp/test_parsing "$TEST_DIR/test_parsing.cpp"; then
      if /tmp/test_parsing; then
        pass "test_parsing"
      else
        fail "test_parsing — assertions failed"
        errors=$((errors + 1))
      fi
    else
      fail "test_parsing — compilation failed"
      errors=$((errors + 1))
    fi
  else
    echo "  [SKIP] test_parsing.cpp not found"
  fi

  echo ""
  if [ $errors -gt 0 ]; then
    die "Tests failed ($errors error(s))"
  fi
  info "All tests PASSED"
}

# ── Serial log capture ────────────────────────────────────────────────────────
run_log() {
  local minutes="${2:-20}"
  local port
  port=$(resolve_port "${3:-}")
  mkdir -p logs
  local logfile="logs/esp32-$(date +%Y-%m-%d-%H%M%S).log"
  info "LOG CAPTURE → $port @ ${BAUD} baud  ($minutes min → $logfile)"
  /c/python314/python.exe -u -c "
import serial, sys, time, os

port, baud, minutes = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
logfile = sys.argv[4]
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
" "$port" "$BAUD" "$minutes" "$logfile"
}

# ── Free port / local IP helpers ─────────────────────────────────────────────
find_free_port() {
  node -e "const net=require('net');const s=net.createServer();s.listen(0,()=>{process.stdout.write(String(s.address().port));s.close()})"
}

detect_local_ip() {
  node -e "const os=require('os');const a=Object.values(os.networkInterfaces()).flat().find(i=>i.family==='IPv4'&&!i.internal);process.stdout.write(a?a.address:'')"
}

# ── Stress test ──────────────────────────────────────────────────────────────
run_stress() {
  local minutes="${2:-10}"
  local com_port
  com_port=$(resolve_port "${3:-}")
  local proxy_mode="${4:-chaos}"
  mkdir -p logs
  local logfile="logs/stress-$(date +%Y-%m-%d-%H%M%S).log"
  local proxy_port my_ip
  proxy_port=$(find_free_port)
  my_ip=$(detect_local_ip)
  [ -n "$my_ip" ] || die "Could not detect local IP"

  info "STRESS TEST — ${minutes}m, ${proxy_mode} mode, serial on ${com_port}"

  info "Patching Echo → ${my_ip}:${proxy_port}, compiling, flashing..."
  _echo_patch_flash "$my_ip" "$proxy_port" "$com_port"

  # Start mock proxy in background
  info "Starting mock proxy (${proxy_mode} mode, port ${proxy_port})..."
  node tools/mock-proxy.js "$proxy_mode" "$proxy_port" &
  local proxy_pid=$!
  sleep 1

  # Capture serial output
  info "Capturing serial → $logfile (${minutes}m)..."
  /c/python314/python.exe -u -c "
import serial, sys, time

port, baud, minutes = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
logfile = sys.argv[4]
duration = minutes * 60

ser = serial.Serial(port, baud, timeout=1)
time.sleep(0.1)
ser.reset_input_buffer()

start = time.time()
deadline = start + duration
line_count = 0

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
print(f'\nCapture complete: {line_count} lines, {(time.time()-start)/60:.1f} min')
" "$com_port" "$BAUD" "$minutes" "$logfile" || true

  # Kill mock proxy
  kill $proxy_pid 2>/dev/null || true
  wait $proxy_pid 2>/dev/null || true

  info "Restoring Echo → api.overheadtracker.com:443, compiling, flashing..."
  _echo_patch_flash "api.overheadtracker.com" 443 "$com_port"

  # Analyze
  echo ""
  info "Analyzing log..."
  node tools/serial-stress.js "$logfile"
}

# ── Echo patch+flash (internal) ───────────────────────────────────────────────
_echo_patch_flash() {
  local new_ip="$1" new_proxy_port="$2" com_port="$3"
  local ino_file="tracker_echo/tracker_echo.ino"
  local old_ip old_port
  old_ip=$(sed -n 's/.*PROXY_HOST *= *"\([^"]*\)".*/\1/p' "$ino_file")
  old_port=$(sed -n 's/.*PROXY_PORT *= *\([0-9]*\).*/\1/p' "$ino_file")
  sed -i "s|PROXY_HOST = \"${old_ip}\"|PROXY_HOST = \"${new_ip}\"|" "$ino_file"
  sed -i "s|PROXY_PORT = ${old_port}|PROXY_PORT = ${new_proxy_port}|" "$ino_file"
  grep -E "PROXY_HOST|PROXY_PORT" "$ino_file"
  run_compile
  run_upload _ "$com_port"
}

# ── Proxy host ────────────────────────────────────────────────────────────────
run_proxy_host() {
  local new_ip="${2:-}"
  local new_proxy_port="${3:-443}"
  local com_port="${4:-COM4}"
  if [ -z "$new_ip" ]; then
    echo "Usage: ./build.sh proxy-host <ip> [proxy_port] [com_port]"
    echo "  Example: ./build.sh proxy-host 192.168.86.30 3001 COM4"
    exit 1
  fi
  info "Patching Echo PROXY_HOST → ${new_ip}:${new_proxy_port}"
  _echo_patch_flash "$new_ip" "$new_proxy_port" "$com_port"
}

# ── Foxtrot stress test ───────────────────────────────────────────────────────
run_foxtrot_stress() {
  local minutes="${2:-10}"
  local com_port="${3:-COM7}"
  local proxy_mode="${4:-chaos}"
  mkdir -p logs
  local logfile="logs/foxtrot-stress-$(date +%Y-%m-%d-%H%M%S).log"
  local proxy_port my_ip
  proxy_port=$(find_free_port)
  my_ip=$(detect_local_ip)
  [ -n "$my_ip" ] || die "Could not detect local IP"

  info "FOXTROT STRESS TEST — ${minutes}m, ${proxy_mode} mode, serial on ${com_port}"

  info "Patching Foxtrot → ${my_ip}:${proxy_port}, compiling, flashing..."
  _foxtrot_patch_flash "$my_ip" "$proxy_port" "$com_port"

  info "Starting mock proxy (${proxy_mode} mode, port ${proxy_port})..."
  node tools/mock-proxy.js "$proxy_mode" "$proxy_port" &
  local proxy_pid=$!
  sleep 1

  info "Capturing serial → $logfile (${minutes}m)..."
  /c/python314/python.exe -u -c "
import serial, sys, time

port, baud, minutes = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
logfile = sys.argv[4]
duration = minutes * 60

ser = serial.Serial(port, baud, timeout=1)
time.sleep(0.1)
ser.reset_input_buffer()

start = time.time()
deadline = start + duration
line_count = 0

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
print(f'\nCapture complete: {line_count} lines, {(time.time()-start)/60:.1f} min')
" "$com_port" "$BAUD" "$minutes" "$logfile" || true

  kill $proxy_pid 2>/dev/null || true
  wait $proxy_pid 2>/dev/null || true

  info "Restoring Foxtrot → api.overheadtracker.com:443, compiling, flashing..."
  _foxtrot_patch_flash "api.overheadtracker.com" 443 "$com_port"

  echo ""
  info "Analyzing log..."
  node tools/serial-stress.js "$logfile"
}

# ── Foxtrot patch+flash (internal) ───────────────────────────────────────────
_foxtrot_patch_flash() {
  local new_ip="$1" new_proxy_port="$2" com_port="$3"
  local ino_file="tracker_foxtrot/tracker_foxtrot.ino"
  local old_ip old_port
  old_ip=$(sed -n 's/.*PROXY_HOST *= *"\([^"]*\)".*/\1/p' "$ino_file")
  old_port=$(sed -n 's/.*PROXY_PORT *= *\([0-9]*\).*/\1/p' "$ino_file")
  sed -i "s|PROXY_HOST = \"${old_ip}\"|PROXY_HOST = \"${new_ip}\"|" "$ino_file"
  sed -i "s|PROXY_PORT = ${old_port}|PROXY_PORT = ${new_proxy_port}|" "$ino_file"
  grep -E "PROXY_HOST|PROXY_PORT" "$ino_file"
  info "COMPILE Foxtrot ($FOXTROT_FQBN)"
  "$ARDUINO_CLI" compile \
    $(config_flag) \
    --fqbn        "$FOXTROT_FQBN" \
    --build-path  "$FOXTROT_BUILD_DIR" \
    "$FOXTROT_SKETCH"
  info "UPLOAD Foxtrot → $com_port"
  "$ARDUINO_CLI" upload \
    $(config_flag) \
    --fqbn        "$FOXTROT_FQBN" \
    --port        "$com_port" \
    --input-dir   "$FOXTROT_BUILD_DIR" \
    "$FOXTROT_SKETCH"
  info "Foxtrot upload complete."
}

# ── Foxtrot proxy-host ────────────────────────────────────────────────────────
run_foxtrot_proxy_host() {
  local new_ip="${2:-}"
  local new_proxy_port="${3:-443}"
  local com_port="${4:-COM7}"
  if [ -z "$new_ip" ]; then
    echo "Usage: ./build.sh foxtrot-proxy-host <ip> [proxy_port] [com_port]"
    echo "  Example: ./build.sh foxtrot-proxy-host 192.168.86.30 3001 COM7"
    exit 1
  fi
  info "Patching Foxtrot PROXY_HOST → ${new_ip}:${new_proxy_port}"
  _foxtrot_patch_flash "$new_ip" "$new_proxy_port" "$com_port"
}

# ── Delta compile + upload ────────────────────────────────────────────────────
run_delta() {
  local com_port="${2:-$DELTA_PORT}"
  info "COMPILE Delta ($DELTA_FQBN)"
  mkdir -p "$DELTA_BUILD_DIR"
  "$ARDUINO_CLI" compile \
    $(config_flag) \
    --fqbn        "$DELTA_FQBN" \
    --build-path  "$DELTA_BUILD_DIR" \
    "$DELTA_SKETCH"
  info "UPLOAD Delta → $com_port"
  "$ARDUINO_CLI" upload \
    $(config_flag) \
    --fqbn        "$DELTA_FQBN" \
    --port        "$com_port" \
    --input-dir   "$DELTA_BUILD_DIR" \
    "$DELTA_SKETCH"
  info "Delta upload complete."
}

run_delta_compile() {
  info "COMPILE Delta ($DELTA_FQBN)"
  mkdir -p "$DELTA_BUILD_DIR"
  "$ARDUINO_CLI" compile \
    $(config_flag) \
    --fqbn        "$DELTA_FQBN" \
    --build-path  "$DELTA_BUILD_DIR" \
    "$DELTA_SKETCH"
  info "Delta compile complete."
}

# ── Delta compile + upload (alias: delta-test) ───────────────────────────────
run_delta_test() {
  run_delta "$@"
}

# ── Golf compile + upload ─────────────────────────────────────────────────────
run_golf() {
  local run_port="${2:-$GOLF_PORT}"
  info "COMPILE Golf ($GOLF_FQBN)"
  mkdir -p "$GOLF_BUILD_DIR"
  "$ARDUINO_CLI" compile \
    $(config_flag) \
    --fqbn        "$GOLF_FQBN" \
    --build-path  "$GOLF_BUILD_DIR" \
    --warnings    all \
    "$GOLF_SKETCH"

  # Touch the running port at 1200 baud — triggers SAMD bootloader reset
  info "1200-baud touch on $run_port → waiting for bootloader port..."
  python -c "
import serial, time, sys
try:
    s = serial.Serial('$run_port', 1200, timeout=1)
    s.close()
except Exception as e:
    print('Touch failed:', e, file=sys.stderr)
"

  # Snapshot existing ports, then poll for any new one after the 1200-baud touch
  local boot_port
  boot_port=$(python -c "
import serial.tools.list_ports, time, sys
seen = {p.device for p in serial.tools.list_ports.comports()}
deadline = time.time() + 5
while time.time() < deadline:
    ports = {p.device for p in serial.tools.list_ports.comports()}
    new = ports - seen
    if new:
        print(sorted(new)[-1])
        sys.exit(0)
    time.sleep(0.2)
print('', end='')
")

  if [ -z "$boot_port" ]; then
    echo "ERROR: bootloader port not found after 1200-baud touch. Is the board connected?" >&2
    exit 1
  fi

  info "UPLOAD Golf → $boot_port (bootloader)"
  "$ARDUINO_CLI" upload \
    $(config_flag) \
    --fqbn        "$GOLF_FQBN" \
    --port        "$boot_port" \
    --input-dir   "$GOLF_BUILD_DIR" \
    "$GOLF_SKETCH"
  info "Golf upload complete."
}

run_golf_compile() {
  info "COMPILE Golf ($GOLF_FQBN)"
  mkdir -p "$GOLF_BUILD_DIR"
  "$ARDUINO_CLI" compile \
    $(config_flag) \
    --fqbn        "$GOLF_FQBN" \
    --build-path  "$GOLF_BUILD_DIR" \
    --warnings    all \
    "$GOLF_SKETCH"
  info "Golf compile complete."
}

run_golf_serve() {
  run_golf_compile

  local BIN="$GOLF_BUILD_DIR/tracker_golf.ino.bin"
  local PORT=8080
  local TMP_JS
  TMP_JS=$(mktemp "/tmp/golf-serve-XXXXXX.js")
  # Node.js is a native Windows process — use mixed (forward-slash) Windows path it can open
  local WIN_BIN
  WIN_BIN=$(cygpath -m "$BIN" 2>/dev/null || echo "$BIN")

  cat > "$TMP_JS" << JSEOF
const http = require('http');
const fs   = require('fs');
const os   = require('os');
const bin  = fs.readFileSync('$WIN_BIN');
const port = $PORT;

let localIP = 'YOUR_MACHINE_IP';
for (const iface of Object.values(os.networkInterfaces())) {
  for (const addr of iface) {
    if (addr.family === 'IPv4' && !addr.internal) { localIP = addr.address; break; }
  }
  if (localIP !== 'YOUR_MACHINE_IP') break;
}

console.log('[OTA] Local server on :' + port + '  (' + bin.length + ' bytes)');
console.log('[OTA] Add to secrets.h:');
console.log('        #define OTA_LOCAL_HOST  "' + localIP + '"');
console.log('        #define OTA_LOCAL_PORT   ' + port);
console.log('[OTA] Ctrl-C to stop.');

http.createServer((req, res) => {
  if (req.url === '/firmware/golf/version') {
    res.setHeader('Content-Type', 'application/json');
    res.end(JSON.stringify({ version: 9999 }));
    console.log('[OTA] Version check — served 9999');
  } else if (req.url === '/firmware/golf/binary') {
    res.setHeader('Content-Type', 'application/octet-stream');
    res.setHeader('Content-Disposition', 'attachment; filename="golf.bin"');
    res.end(bin);
    console.log('[OTA] Binary served (' + bin.length + ' bytes)');
  } else {
    res.statusCode = 404;
    res.end('Not found');
  }
}).listen(port, '0.0.0.0');
JSEOF

  node "$TMP_JS"
  rm -f "$TMP_JS"
}

run_golf_publish() {
  run_golf_compile

  local BIN="$GOLF_BUILD_DIR/tracker_golf.ino.bin"
  local DEST="server/firmware"
  local VER_FILE="$DEST/golf-version.txt"

  mkdir -p "$DEST"
  cp "$BIN" "$DEST/golf.bin"
  info "Binary → $DEST/golf.bin ($(wc -c < "$DEST/golf.bin") bytes)"

  local cur=0
  [ -f "$VER_FILE" ] && cur=$(cat "$VER_FILE")
  local next=$((cur + 1))
  echo "$next" > "$VER_FILE"
  info "Version: $cur → $next"

  info "golf-publish done. Run 'railway up' from project root to deploy."
}

# ── Safe (test + validate — full pre-push check) ─────────────────────────────
run_safe() {
  info "SAFE — full pre-push validation"
  echo "═══════════════════════════════════════"
  run_test
  echo "═══════════════════════════════════════"
  run_validate
  echo "═══════════════════════════════════════"
  info "PRE-PUSH CHECK PASSED — safe to push"
}

# ── Dispatch ──────────────────────────────────────────────────────────────────
case "$CMD" in
  compile)          run_compile ;;
  upload)           run_upload  "$@" ;;
  ota)              run_ota ;;
  monitor)          run_monitor "$@" ;;
  send)             run_send    "$@" ;;
  log)              run_log     "$@" ;;
  validate)         run_validate ;;
  test)             run_test ;;
  safe)             run_safe ;;
  stress)           run_stress  "$@" ;;
  proxy-host)       run_proxy_host "$@" ;;
  foxtrot-stress)   run_foxtrot_stress "$@" ;;
  foxtrot-proxy-host) run_foxtrot_proxy_host "$@" ;;
  delta)            run_delta "$@" ;;
  delta-compile)    run_delta_compile ;;
  delta-test)       run_delta_test "$@" ;;
  golf)             run_golf "$@" ;;
  golf-compile)     run_golf_compile ;;
  golf-publish)     run_golf_publish ;;
  golf-serve)       run_golf_serve ;;
  all|*)            run_compile && run_upload "$@" ;;
esac
