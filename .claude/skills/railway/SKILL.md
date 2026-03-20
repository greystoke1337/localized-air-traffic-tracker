---
name: railway
description: Manage the Railway-hosted proxy server. Use when the user says "railway", "deploy proxy", "proxy logs", "proxy status", "redeploy", or wants to check/manage the Railway deployment.
---

# Railway Proxy Management

Manage the overhead-tracker-proxy service on Railway (project: `resourceful-integrity`).

## Platform Detection

```bash
uname -s
```

- **Darwin** → prefix railway/brew commands with `eval "$(/opt/homebrew/bin/brew shellenv)" &&`
- **MINGW/MSYS/CYGWIN** → run commands directly (railway CLI must be in PATH)

## Service info

- **Project:** resourceful-integrity
- **Service:** overhead-tracker-proxy
- **Custom domain:** api.overheadtracker.com
- **Volume:** /data (route cache + reports)

## Commands

Parse the user's intent and run the appropriate command(s) below.

### Deploy (user says "deploy", "push", "update proxy", "railway up")

1. Syntax-check:
```bash
node --check server/server.js
```

2. Deploy (MUST run from project root, NOT from server/):
```bash
railway up --service overhead-tracker-proxy
```

3. Wait 30 seconds, then verify:
```bash
curl -s https://api.overheadtracker.com/status | head -c 300
```

### Status (user says "status", "check proxy", "is proxy up")

```bash
curl -s https://api.overheadtracker.com/status
```

### Logs (user says "logs", "proxy logs", "what happened")

```bash
railway service logs --service overhead-tracker-proxy
```

### Redeploy (user says "redeploy", "restart proxy")

```bash
railway service redeploy --service overhead-tracker-proxy --yes
```

Then wait 30 seconds and verify with /status.

### Variables (user says "env", "variables", "config")

```bash
railway variables
```

### Set variable (user says "set VAR=value")

```bash
railway variables set KEY=VALUE
```

## Report summary

Tell the user:
- Whether the action succeeded
- The /status response if applicable
- Any errors encountered
