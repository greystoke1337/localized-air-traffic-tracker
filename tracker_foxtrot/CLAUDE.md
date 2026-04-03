# Foxtrot Firmware (Waveshare 4.3", ESP32-S3, 800×480)

## Build & Flash

Use `/flash-and-log foxtrot` or `arduino-cli` directly. **Do not use `build.sh`** — that's Echo-only.

OTA: `/ota-update` skill.

Package for web flasher: `./tools/package-firmware.sh` (copies binaries to `firmware/`). Use `--skip-compile` to skip recompile.

## Rendering

LovyanGFX **immediate-mode** — `tft.fillRect`, `tft.drawString`, etc. No LVGL, no sprites, no lock/unlock needed.

**Do not restore `lvgl_v8_port.cpp`** — it must stay stubbed. Restoring it re-introduces an I2C driver conflict that crashes on boot.

Failed approaches (do not retry): LGFX_Sprite back buffer, esp_lcd double-buffer, LVGL. See memory: `foxtrot-display-attempts.md`.

## Demo Mode

Set `#define DEMO_MODE 1` in `config.h` to boot with 3 fake Sydney flights, skipping all WiFi/network code.

## Key Memory Files

- `firmware-foxtrot.md` — file-by-file map, key functions, display constants, color palette
- `foxtrot-display-stack.md` — board init sequence, LovyanGFX config, build FQBN
- `foxtrot-display-attempts.md` — failed fluidity approaches (DO NOT RETRY)
- `hardware-waveshare-4.3b.md` — board specs, pinout, peripherals
- `foxtrot-chicago-deployment.md` — Chicago client config: airlines filter, synthetic data, heartbeat, sticky failover
- `foxtrot-display-validated.md` — blue tint fix; never use GPIO 10 (CH422G conflict)
