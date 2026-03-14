#!/usr/bin/env bash
# watchdog.sh — Restart PM2 services if undervoltage makes them unresponsive.
# Runs every 60s via cron: * * * * * /home/pi/proxy/watchdog.sh

set -euo pipefail

LOG_DIR="/home/pi/proxy/logs"
LOG_FILE="$LOG_DIR/watchdog.log"
STATE_FILE="/tmp/watchdog-strike"
LAST_RESTART_FILE="/tmp/watchdog-last-restart"
COOLDOWN_SECS=300
HEALTH_URL="http://localhost:3000/status"
HEALTH_TIMEOUT=5
MAX_LOG_BYTES=1048576

mkdir -p "$LOG_DIR"

log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') $1" >> "$LOG_FILE"
}

# Rotate log at 1 MB
if [ -f "$LOG_FILE" ] && [ "$(stat -c%s "$LOG_FILE" 2>/dev/null || echo 0)" -gt "$MAX_LOG_BYTES" ]; then
    mv "$LOG_FILE" "$LOG_FILE.old"
    log "LOG rotated"
fi

# 1. Read throttle status
THROTTLED=$(vcgencmd get_throttled 2>/dev/null || echo "throttled=0x0")
HEX_VAL="${THROTTLED#*=}"
THROTTLE_INT=$((HEX_VAL))

CURRENTLY_UV=$(( (THROTTLE_INT >> 0) & 1 ))
HISTORY_UV=$(( (THROTTLE_INT >> 16) & 1 ))

# 2. No undervoltage — clear state and exit silently
if [ "$CURRENTLY_UV" -eq 0 ] && [ "$HISTORY_UV" -eq 0 ]; then
    rm -f "$STATE_FILE"
    exit 0
fi

# 3. Log undervoltage event
if [ "$CURRENTLY_UV" -eq 1 ]; then
    log "UNDERVOLTAGE active (throttled=$HEX_VAL)"
else
    log "UNDERVOLTAGE historical (throttled=$HEX_VAL)"
fi

# 4. Check proxy health
HTTP_CODE=$(curl -s -o /dev/null -w '%{http_code}' --max-time "$HEALTH_TIMEOUT" "$HEALTH_URL" 2>/dev/null || echo "000")

# 5. Healthy — clear strikes, done
if [ "$HTTP_CODE" = "200" ]; then
    rm -f "$STATE_FILE"
    log "HEALTHY proxy OK despite undervoltage"
    exit 0
fi

# 6. Unhealthy — two-strike rule
log "UNHEALTHY proxy returned $HTTP_CODE"

if [ ! -f "$STATE_FILE" ]; then
    echo "$(date +%s)" > "$STATE_FILE"
    log "STRIKE 1 — will restart on next failure"
    exit 0
fi

# 7. Check cooldown
if [ -f "$LAST_RESTART_FILE" ]; then
    LAST_RESTART=$(cat "$LAST_RESTART_FILE")
    NOW=$(date +%s)
    ELAPSED=$(( NOW - LAST_RESTART ))
    if [ "$ELAPSED" -lt "$COOLDOWN_SECS" ]; then
        log "COOLDOWN ${ELAPSED}s/${COOLDOWN_SECS}s — skipping"
        exit 0
    fi
fi

# 8. Restart PM2 services
log "STRIKE 2 — restarting all PM2 services"

if ! timeout 15 pm2 restart all >>"$LOG_FILE" 2>&1; then
    log "pm2 restart failed — killing and resurrecting"
    pm2 kill >>"$LOG_FILE" 2>&1 || true
    sleep 2
    pm2 resurrect >>"$LOG_FILE" 2>&1 || log "pm2 resurrect failed — manual intervention needed"
fi

date +%s > "$LAST_RESTART_FILE"
rm -f "$STATE_FILE"
log "RESTART complete"
