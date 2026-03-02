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
#   ./build.sh safe          → test + validate (full pre-push check)
#   OVERHEAD_TRACKER_IP=x.x.x.x ./build.sh ota  → OTA to specific IP
#
# ──────────────────────────────────────────────────────────────────────────────

set -e

SKETCH="tracker_live_fnk0103s/tracker_live_fnk0103s.ino"
FQBN="esp32:esp32:esp32"
BUILD_DIR="/tmp/overhead-tracker-build"
BAUD=115200
OTA_HOST="${OVERHEAD_TRACKER_IP:-overhead-tracker.local}"
OTA_PORT=3232
BIN_FILE="$BUILD_DIR/tracker_live_fnk0103s.ino.bin"
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
  suspicious=$(grep -rn "192\.168\." tracker_live_fnk0103s/*.ino tracker_live_fnk0103s/*.h 2>/dev/null \
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
    local max_kb=1310  # ~1.28 MB usable on ESP32 with default partition
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
    local json_include="tracker_live_fnk0103s/libraries/ArduinoJson/src"
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
  validate)         run_validate ;;
  test)             run_test ;;
  safe)             run_safe ;;
  all|*)            run_compile && run_upload "$@" ;;
esac
