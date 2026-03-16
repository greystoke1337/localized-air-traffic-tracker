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

```bash
eval "$(/opt/homebrew/bin/brew shellenv)" && cd server && railway up
```

---

## Windows Steps

### 1. Syntax check

```bash
node --check server/server.js
```

### 2. Deploy

```bash
cd server && railway up
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
