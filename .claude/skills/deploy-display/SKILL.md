---
name: deploy-display
description: Deploy display.py to the Pi proxy TFT display. Use when the user says "deploy display", "push display", "update display", or after making changes to pi-proxy/display.py.
allowed-tools: Bash, Read
---

# Deploy Pi Proxy Display

Syntax-check, SCP, and restart the TFT display process on the Raspberry Pi.

## Steps

### 1. Syntax check

```bash
cd /c/Users/maxim/localized-air-traffic-tracker && python -c "import py_compile; py_compile.compile('pi-proxy/display.py', doraise=True)"
```

If this fails, stop and report the syntax error. Do NOT deploy broken code.

### 2. Deploy via SCP + PM2 restart

```bash
scp /c/Users/maxim/localized-air-traffic-tracker/pi-proxy/display.py piproxy:/home/pi/proxy/display.py && ssh piproxy "pm2 restart display"
```

Use a 15-second timeout. The SSH alias `piproxy` resolves to `piproxy.local` with user `pi` and key `~/.ssh/pi_proxy`.

### 3. Verify process is healthy

Wait 5 seconds for the process to stabilize, then check logs:

```bash
ssh piproxy "pm2 logs display --nostream --lines 10"
```

### 4. Report summary

Tell the user:
- Whether the syntax check passed
- Whether the deploy and restart succeeded
- The PM2 status (online/errored, restart count)
- Any errors from the last 10 log lines (ignore ALSA warnings — they're harmless on headless Pi)
