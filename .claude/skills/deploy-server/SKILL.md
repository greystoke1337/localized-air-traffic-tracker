---
name: deploy-server
description: Deploy server.js to the Pi proxy. Use when the user says "deploy server", "push server", "update proxy", or after making changes to pi-proxy/server.js.
allowed-tools: Bash, Read
---

# Deploy Pi Proxy Server

SCP and restart the Node.js proxy server on the Raspberry Pi.

## Steps

### 1. Basic validation

```bash
cd /c/Users/maxim/localized-air-traffic-tracker && node --check pi-proxy/server.js
```

If this fails, stop and report the syntax error. Do NOT deploy broken code.

### 2. Deploy via SCP + PM2 restart

```bash
scp /c/Users/maxim/localized-air-traffic-tracker/pi-proxy/server.js piproxy:/home/pi/proxy/server.js && ssh piproxy "pm2 restart proxy"
```

Use a 15-second timeout. The SSH alias `piproxy` resolves to `piproxy.local` with user `pi` and key `~/.ssh/pi_proxy`.

### 3. Verify the server is responding

Wait 5 seconds, then:

```bash
ssh piproxy "curl -s http://localhost:3000/stats | head -c 200"
```

This confirms the proxy is up and serving requests.

### 4. Check for errors in logs

```bash
ssh piproxy "pm2 logs proxy --nostream --lines 15"
```

### 5. Report summary

Tell the user:
- Whether the syntax check passed
- Whether the deploy and restart succeeded
- The /stats response (confirms the server is live)
- Any errors from the last 15 log lines
