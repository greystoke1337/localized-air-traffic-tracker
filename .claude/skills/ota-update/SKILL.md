---
name: ota-update
description: Compile and OTA flash ESP32 firmware over WiFi. Use when the user says "OTA", "OTA update", "OTA flash", "wireless update", or wants to update the ESP32 without USB.
allowed-tools: Bash, Read, Grep, Glob
---

# OTA Update ESP32

Compile firmware and flash it to the ESP32 over WiFi (no USB required).

## Platform Detection

```bash
uname -s
```

- **Darwin** → macOS flow
- **MINGW/MSYS/CYGWIN** → Windows/Git Bash flow

---

## macOS Steps

### 1. Compile and OTA flash

```bash
export PATH=~/bin:$PATH && bash build.sh ota
```

This compiles the firmware and uploads via espota.py to `overhead-tracker.local:3232`.

To target a specific IP instead of mDNS:

```bash
export PATH=~/bin:$PATH && OVERHEAD_TRACKER_IP=<IP> bash build.sh ota
```

### Prerequisites (already installed)

- `arduino-cli` at `~/bin/arduino-cli`
- `python` symlink at `~/bin/python` (espota.py needs `python`, macOS only has `python3`)
- ESP32 core 3.3.7 at `~/Library/Arduino15/packages/esp32`

If `arduino-cli` is missing, install it:

```bash
mkdir -p ~/bin && curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/bin sh
```

If `python` symlink is missing:

```bash
ln -sf $(which python3) ~/bin/python
```

---

## Windows (Git Bash) Steps

### 1. Compile and OTA flash

```bash
cd /c/Users/maxim/localized-air-traffic-tracker && ./build.sh ota
```

To target a specific IP:

```bash
cd /c/Users/maxim/localized-air-traffic-tracker && OVERHEAD_TRACKER_IP=<IP> ./build.sh ota
```

---

## Troubleshooting

- **"espota not found"** → The ESP32 core may be missing or at a different path. Check `find ~/Library/Arduino15/packages/esp32 -name "espota.py"` (macOS) or set `ESPOTA` env var.
- **"No response from device"** → ESP32 may not be on the network. Verify it's connected to WiFi and reachable: `ping overhead-tracker.local`
- **"env: python: No such file or directory"** → espota.py needs `python` in PATH. Run: `ln -sf $(which python3) ~/bin/python`

## Report summary

Tell the user:
- Compilation result (flash size / RAM usage)
- OTA result (success/fail)
- If failed, suggest checking network connectivity to the ESP32
