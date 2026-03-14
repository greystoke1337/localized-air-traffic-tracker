---
name: railway
description: Manage the Railway-hosted proxy server. Use when the user says "railway", "deploy proxy", "proxy logs", "proxy status", "redeploy", or wants to check/manage the Railway deployment.
allowed-tools: Bash, Read
---

# Railway Proxy Management

Manage the overhead-tracker-proxy service on Railway (project: `resourceful-integrity`).

## Prerequisites

Railway CLI must be installed and authenticated:

```bash
eval "$(/opt/homebrew/bin/brew shellenv)"
```

The CLI is linked to:
- **Project:** resourceful-integrity
- **Service:** overhead-tracker-proxy
- **Environment:** production
- **Custom domain:** api.overheadtracker.com
- **Railway domain:** overhead-tracker-proxy-production.up.railway.app
- **Volume:** /data (route cache + reports)

## Commands

Parse the user's intent and run the appropriate command(s) below.

### Deploy (user says "deploy", "push", "update proxy", "railway up")

1. Syntax-check the server first:

```bash
eval "$(/opt/homebrew/bin/brew shellenv)" && node --check /Users/maximecazaly/localized-air-traffic-tracker/server/server.js
```

If this fails, stop and report the error. Do NOT deploy broken code.

2. Deploy:

```bash
eval "$(/opt/homebrew/bin/brew shellenv)" && cd /Users/maximecazaly/localized-air-traffic-tracker/server && railway up
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
eval "$(/opt/homebrew/bin/brew shellenv)" && railway service logs --service overhead-tracker-proxy
```

### Redeploy (user says "redeploy", "restart proxy")

```bash
eval "$(/opt/homebrew/bin/brew shellenv)" && railway service redeploy --service overhead-tracker-proxy --yes
```

Then wait 30 seconds and verify with /status.

### Variables (user says "env", "variables", "config")

```bash
eval "$(/opt/homebrew/bin/brew shellenv)" && railway variables
```

### Set variable (user says "set VAR=value")

```bash
eval "$(/opt/homebrew/bin/brew shellenv)" && railway variables set KEY=VALUE
```

## Report summary

Tell the user:
- Whether the action succeeded
- The /status response if applicable (uptime, cache stats, recent log entries)
- Any errors encountered
