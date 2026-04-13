---
name: pi-status
description: Check the health of the Pi TFT display. Use when the user says "pi status", "check pi", "is the pi running", or wants to diagnose display issues.
allowed-tools: Bash
---

# Pi Display Health Check

SSH into the Raspberry Pi and check the TFT display service, system resources, and recent logs.

## Steps

### 1. systemd service status

```bash
ssh -i ~/.ssh/pi_proxy pi@airplanes.local "sudo systemctl status tft-display --no-pager"
```

Expected: `Active: active (running)`. Process manager is **systemd** (not PM2).

### 2. System resources

Run these in a single SSH session:

```bash
ssh -i ~/.ssh/pi_proxy pi@airplanes.local "echo '--- CPU TEMP ---' && vcgencmd measure_temp && echo '--- MEMORY ---' && free -h && echo '--- UPTIME ---' && uptime && echo '--- DISK ---' && df -h /"
```

### 3. Recent logs (errors only)

Check the last 20 lines of the display service for errors:

```bash
ssh -i ~/.ssh/pi_proxy pi@airplanes.local "sudo journalctl -u tft-display -n 20 --no-pager 2>&1 | grep -i -E 'error|fail|crash|Traceback' | tail -5"
```

### 4. Report summary

Tell the user:
- Service status: active/failed, how long it's been running
- CPU temperature (flag if >= 75°C)
- RAM usage percentage
- Disk usage percentage
- Any errors found in recent logs
- Overall health assessment (healthy / degraded / critical)
