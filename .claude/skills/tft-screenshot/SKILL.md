---
name: tft-screenshot
description: Take Playwright screenshots of all TFT preview scenarios. Use when the user says "screenshot", "TFT preview", "test display", or wants visual verification of firmware display changes.
---

# TFT Preview Screenshot Suite

Serve `tft-preview.html` locally and use Playwright MCP to screenshot every scenario and flight phase.

## Platform Detection

```bash
uname -s
```

- **Darwin** → use `python3`
- **MINGW/MSYS/CYGWIN** → use `python` or `/c/python314/python.exe`

## Steps

### 1. Start a local HTTP server

```bash
python3 -m http.server 8765
```

Run this in the background. The server must stay alive for the entire screenshot session.

### 2. Open the TFT preview

Use the Playwright MCP `browser_navigate` tool to open:
```
http://localhost:8765/tft-preview.html
```

### 3. Screenshot all scenarios

The scenario dropdown is `#ctl-scenario`. Cycle through each value and take a screenshot.

| Scenario Key | Filename |
|---|---|
| `normal` | `tft-preview-normal.png` |
| `emergency-7700` | `tft-preview-emergency-7700.png` |
| `emergency-7600` | `tft-preview-emergency-7600.png` |
| `emergency-7500` | `tft-preview-emergency-7500.png` |
| `missing-data` | `tft-preview-missing-data.png` |
| `boundary-alt` | `tft-preview-boundary-alt.png` |
| `max-values` | `tft-preview-max-values.png` |
| `overhead` | `tft-preview-overhead.png` |
| `landing` | `tft-preview-landing.png` |
| `clear-skies` | `tft-preview-clear-skies.png` |
| `weather-sunny` | `tft-preview-weather-sunny.png` |
| `weather-storm` | `tft-preview-weather-storm.png` |
| `weather-loading` | `tft-preview-weather-loading.png` |

For each scenario:
1. Use `browser_select_option` to set the dropdown value
2. Wait briefly for render
3. Use `browser_take_screenshot` targeting the canvas element

### 4. Screenshot all 8 flight phases

Set scenario to `normal` first, then for each phase (0-7), change `#ctl-phase`:

| Phase Index | Label | Filename |
|---|---|---|
| 0 | TAKEOFF | `tft-preview-phase-takeoff.png` |
| 1 | CLIMBING | `tft-preview-phase-climbing.png` |
| 2 | CRUISING | `tft-preview-phase-cruising.png` |
| 3 | DESCEND | `tft-preview-phase-descend.png` |
| 4 | APPROACH | `tft-preview-phase-approach.png` |
| 5 | LANDING | `tft-preview-phase-landing.png` |
| 6 | OVERHEAD | `tft-preview-phase-overhead.png` |
| 7 | UNKNOWN | `tft-preview-phase-unknown.png` |

### 5. Clean up

- Stop the local HTTP server
- Close the Playwright browser with `browser_close`

### 6. Report summary

Tell the user:
- Total screenshots taken (should be 13 scenarios + 8 phases = 21)
- List the filenames
- Flag any screenshots that look wrong
