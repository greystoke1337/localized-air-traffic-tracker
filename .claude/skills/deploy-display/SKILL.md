---
name: deploy-display
description: Deploy display.py to the Pi TFT display. Use when the user says "deploy display", "deploy pi display", "push display", "update display", or after making changes to pi-display/display.py. Do NOT use for Golf/CircuitPython/CIRCUITPY deployments.
---

# Deploy Pi TFT Display

Syntax-check, SCP, and restart the TFT display process on the Raspberry Pi.

## Steps

### 1. Syntax check

```bash
cd /c/Users/maxim/localized-air-traffic-tracker && python -c "import py_compile; py_compile.compile('pi-display/display.py', doraise=True)"
```

If this fails, stop and report the syntax error. Do NOT deploy broken code.

### 2. Deploy via SCP + systemd restart

```bash
scp /c/Users/maxim/localized-air-traffic-tracker/pi-display/display.py pi@airplanes.local:/home/pi/tft_display.py && ssh -i ~/.ssh/pi_proxy pi@airplanes.local "sudo systemctl restart tft-display"
```

Use a 15-second timeout. Connect to `pi@airplanes.local` with key `~/.ssh/pi_proxy`.

### 3. Verify service is healthy

Wait 5 seconds for the service to stabilize, then check logs:

```bash
ssh -i ~/.ssh/pi_proxy pi@airplanes.local "sudo journalctl -u tft-display -n 20 --no-pager"
```

### 4. Report summary

Tell the user:
- Whether the syntax check passed
- Whether the deploy and restart succeeded
- systemd service status (active/failed)
- Any errors from the last 20 log lines (ignore ALSA warnings — they're harmless on headless Pi)
