#!/bin/bash
# Package Foxtrot firmware for the web flasher (flash.html)
# Usage: ./tools/package-firmware.sh [--skip-compile]

set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLI="/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
CFG="C:/Users/maxim/.arduinoIDE/arduino-cli.yaml"
FQBN="esp32:esp32:waveshare_esp32_s3_touch_lcd_43B:PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB"
BUILD_DIR="/tmp/tracker-foxtrot-build"
FW_DIR="$REPO_ROOT/firmware"
CONFIG_H="$REPO_ROOT/tracker_foxtrot/config.h"
BOOT_APP0="/c/Users/maxim/AppData/Local/Arduino15/packages/esp32/hardware/esp32/3.3.7/tools/partitions/boot_app0.bin"

echo "=== OVERHEAD TRACKER — FIRMWARE PACKAGER ==="
echo ""

# ── Compile (unless --skip-compile) ──

if [ "$1" != "--skip-compile" ]; then
  echo "[1/3] Compiling Foxtrot firmware..."
  "$CLI" --config-file "$CFG" compile \
    --fqbn "$FQBN" \
    --build-path "$BUILD_DIR" \
    "$REPO_ROOT/tracker_foxtrot/tracker_foxtrot.ino"
  echo "  Compile OK"
else
  echo "[1/3] Skipping compile (--skip-compile)"
fi

# ── Copy binaries ──

echo "[2/3] Copying binaries to firmware/..."

mkdir -p "$FW_DIR"

cp "$BUILD_DIR/tracker_foxtrot.ino.bootloader.bin" "$FW_DIR/bootloader.bin"
cp "$BUILD_DIR/tracker_foxtrot.ino.partitions.bin"  "$FW_DIR/partitions.bin"
cp "$BUILD_DIR/tracker_foxtrot.ino.bin"             "$FW_DIR/app.bin"
cp "$BOOT_APP0"                                     "$FW_DIR/boot_app0.bin"

echo "  bootloader.bin  $(wc -c < "$FW_DIR/bootloader.bin" | tr -d ' ') bytes"
echo "  partitions.bin  $(wc -c < "$FW_DIR/partitions.bin" | tr -d ' ') bytes"
echo "  boot_app0.bin   $(wc -c < "$FW_DIR/boot_app0.bin" | tr -d ' ') bytes"
echo "  app.bin         $(wc -c < "$FW_DIR/app.bin" | tr -d ' ') bytes"

# ── Update manifest.json ──

echo "[3/3] Updating manifest.json..."

VERSION=$(grep '#define FW_VERSION' "$CONFIG_H" | sed 's/.*"\(.*\)".*/\1/')
TODAY=$(date +%Y-%m-%d)

cat > "$FW_DIR/manifest.json" << EOF
{
  "version": "$VERSION",
  "date": "$TODAY",
  "board": "Waveshare ESP32-S3-Touch-LCD-4.3",
  "changelog": "Firmware update v$VERSION",
  "files": [
    { "name": "bootloader.bin", "offset": "0x0" },
    { "name": "partitions.bin", "offset": "0x8000" },
    { "name": "boot_app0.bin",  "offset": "0xe000" },
    { "name": "app.bin",        "offset": "0x10000" }
  ]
}
EOF

echo ""
echo "=== DONE ==="
echo "  Version: v$VERSION"
echo "  Date:    $TODAY"
echo "  Output:  $FW_DIR/"
echo ""
echo "Next: git add firmware/ && git push (auto-deploys to GitHub Pages)"
