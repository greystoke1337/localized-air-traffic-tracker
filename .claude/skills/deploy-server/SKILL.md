---
name: deploy-server
description: Deploy server.js to Railway. Use when the user says "deploy server", "push server", "update proxy", or after making changes to server/server.js.
---

# Deploy Proxy Server to Railway

Deploy the proxy server to Railway hosting.

## Platform Detection

```bash
uname -s
```

- **Darwin** → macOS flow
- **MINGW/MSYS/CYGWIN** → Windows/Git Bash flow

---

## macOS Steps

### 1. Syntax check

```bash
eval "$(/opt/homebrew/bin/brew shellenv)" && node --check server/server.js
```

### 2. Deploy

IMPORTANT: Deploy from the **project root**, not from `server/`. Railway's service has root directory set to `server` — running `railway up` from inside `server/` causes "Could not find root directory: server" because it looks for `server/server/`.

```bash
eval "$(/opt/homebrew/bin/brew shellenv)" && cd /path/to/localized-air-traffic-tracker && railway up --service overhead-tracker-proxy --project 362c10c6-31f9-4e49-96d8-ec86501fa3f0 --environment production
```

---

## Windows Steps

### 1. Syntax check

```bash
node --check server/server.js
```

### 2. Deploy

IMPORTANT: Deploy from the **project root**, not from `server/`. Railway's service has root directory set to `server` — running `railway up` from inside `server/` causes "Could not find root directory: server" because it looks for `server/server/`.

```bash
cd c:/Users/maxim/localized-air-traffic-tracker && railway up --service overhead-tracker-proxy --project 362c10c6-31f9-4e49-96d8-ec86501fa3f0 --environment production
```

---

## Verify (both platforms)

Wait 30 seconds, then:

```bash
curl -s https://api.overheadtracker.com/status | head -c 300
```

## Report summary

Tell the user:
- Whether the syntax check passed
- Whether the Railway deploy succeeded
- The /status response (confirms the server is live)
- Any errors from the logs
