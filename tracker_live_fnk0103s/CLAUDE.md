# Echo Firmware (Freenove FNK0103S, 4.0", ESP32, 480×320)

## Auto-Flash Policy

**Always compile AND flash** after any file change. Do not stop at compile-only. Use `./build.sh compile` then `./build.sh upload COM5`.

## Build Commands

```bash
./build.sh                          # compile + auto-detect port + upload
./build.sh compile                  # compile only
./build.sh upload COM5              # upload last build
./build.sh monitor COM5             # serial monitor
./build.sh send <cmd> COM5          # send debug command, print JSON response
./build.sh ota                      # compile + OTA upload over WiFi
./build.sh validate                 # compile with all warnings + safety checks
./build.sh test                     # run desktop logic tests (no hardware)
./build.sh log [min] [port]         # capture serial output (default 20 min)
./build.sh safe                     # test + validate (full pre-push check)
./build.sh stress [min] [port]      # chaos stress test (mock proxy + serial capture + analyzer)
./build.sh proxy-host <ip> [port]   # patch PROXY_HOST, compile, flash
```

Pre-push: `./build.sh safe`. Preview layout changes with `tft-preview.html` before flashing.

## Stress Testing

`tools/mock-proxy.js` — mock HTTP proxy, 10 modes: normal, timeout, error503, error502, corrupt, partial, slow, chaos, transition, flap.

`tools/serial-stress.js` — serial log analyzer. Detects reboots, WDT resets, backtraces, heap tracking. Prints PASS/FAIL.

```bash
./build.sh proxy-host 192.168.86.23 COM5   # point to dev machine
./build.sh stress 10 COM5                   # 10-min chaos test
./build.sh proxy-host api.overheadtracker.com COM5   # restore Railway proxy
```

Desktop tests: 95 total (37 flight logic + 58 parsing). MSYS2 gcc required (`/c/msys64/ucrt64/bin`).

## Key Memory Files

- `firmware-echo.md` — file-by-file map, key functions, layout constants, color palette
- `firmware-shared.md` — shared patterns: network pipeline, state machine, NVS keys, Flight struct, serial commands
