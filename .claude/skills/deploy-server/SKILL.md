---
name: deploy-server
description: Deploy server.js to Railway. Use when the user says "deploy server", "push server", "update proxy", or after making changes to server/server.js.
allowed-tools: Bash, Read
---

# Deploy Proxy Server to Railway

Deploy the proxy server to Railway hosting.

## Steps

### 1. Syntax check

```bash
eval "$(/opt/homebrew/bin/brew shellenv)" && node --check /Users/maximecazaly/localized-air-traffic-tracker/server/server.js
```

If this fails, stop and report the syntax error. Do NOT deploy broken code.

### 2. Deploy via Railway CLI

```bash
eval "$(/opt/homebrew/bin/brew shellenv)" && cd /Users/maximecazaly/localized-air-traffic-tracker/server && railway up
```

### 3. Wait for deployment and verify

Wait 30 seconds, then:

```bash
curl -s https://api.overheadtracker.com/status | head -c 300
```

This confirms the proxy is up and serving requests.

### 4. Check logs for errors

```bash
eval "$(/opt/homebrew/bin/brew shellenv)" && railway service logs --service overhead-tracker-proxy
```

Look for any errors or crash messages in the output.

### 5. Report summary

Tell the user:
- Whether the syntax check passed
- Whether the Railway deploy succeeded
- The /status response (confirms the server is live)
- Any errors from the logs
