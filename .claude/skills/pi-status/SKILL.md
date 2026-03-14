---
name: pi-status
description: Check the health of the Pi TFT display. Use when the user says "pi status", "check pi", "is the pi running", or wants to diagnose display issues.
allowed-tools: Bash
---

# Pi Display Health Check

SSH into the Raspberry Pi and check the TFT display service, system resources, and recent logs.

## Steps

### 1. PM2 process status

```bash
ssh piproxy "pm2 list"
```

Expected process: `display` (id 0). The proxy no longer runs on the Pi (it's on Railway).

### 2. System resources

Run these in a single SSH session:

```bash
ssh piproxy "echo '--- CPU TEMP ---' && vcgencmd measure_temp && echo '--- MEMORY ---' && free -h && echo '--- UPTIME ---' && uptime && echo '--- DISK ---' && df -h /"
```

### 3. Recent logs (errors only)

Check the last 20 lines of the display service for errors:

```bash
ssh piproxy "pm2 logs display --nostream --lines 20 2>&1 | grep -i -E 'error|fail|crash|Traceback' | tail -5"
```

### 4. Report summary

Tell the user:
- Service status: whether display is online, restart count
- CPU temperature (flag if >= 75C)
- RAM usage percentage
- Disk usage percentage
- Any errors found in recent logs
- Overall health assessment (healthy / degraded / critical)
