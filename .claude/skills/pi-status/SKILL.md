---
name: pi-status
description: Check the health of all Pi proxy services. Use when the user says "pi status", "check pi", "is the pi running", or wants to diagnose proxy issues.
allowed-tools: Bash
---

# Pi Proxy Health Check

SSH into the Raspberry Pi and check all services, system resources, and recent logs.

## Steps

### 1. PM2 process status

```bash
ssh piproxy "pm2 list"
```

Expected processes: `display` (id 0), `tunnel` (id 2), `route-watch` (id 3), `proxy` (id 4).

### 2. System resources

Run these in a single SSH session:

```bash
ssh piproxy "echo '--- CPU TEMP ---' && vcgencmd measure_temp && echo '--- MEMORY ---' && free -h && echo '--- UPTIME ---' && uptime && echo '--- DISK ---' && df -h /"
```

### 3. Proxy health endpoint

```bash
ssh piproxy "curl -s http://localhost:3000/status"
```

This returns JSON with CPU temp, RAM, and PM2 service details.

### 4. Recent logs (errors only)

Check the last 20 lines of each critical service for errors:

```bash
ssh piproxy "echo '=== PROXY ===' && pm2 logs proxy --nostream --lines 20 2>&1 | grep -i -E 'error|fail|crash|ECONNREFUSED|timeout' | tail -5; echo '=== DISPLAY ===' && pm2 logs display --nostream --lines 20 2>&1 | grep -i -E 'error|fail|crash|Traceback' | tail -5"
```

### 5. Report summary

Tell the user:
- Service status: which are online, which are errored, restart counts
- CPU temperature (flag if >= 75C)
- RAM usage percentage
- Disk usage percentage
- Any errors found in recent logs
- Overall health assessment (healthy / degraded / critical)
