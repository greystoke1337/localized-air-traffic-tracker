# Proxy Server (Railway, Node.js/Express)

## Deploy

Railway's GitHub integration auto-deploys this service on every push to `master` that touches `server/**` (watch path set via `serviceInstanceUpdate`). `railway up` (via the `/railway` skill) is only needed to force an out-of-band deploy, e.g. testing a branch before merge.

A healthcheck against `/status` (30s timeout) is configured on the service, so a deploy that fails to boot won't be promoted to serve traffic.

To force a manual deploy, run from the **project root** (not `server/`):

```bash
railway up
```

**CRITICAL:** Railway's root directory setting requires `railway up` to run from the project root. Running it from `server/` causes "Could not find root directory" failures.

Verify: `https://api.overheadtracker.com/status`

## Tests

```bash
cd server && npm test          # 78 unit tests
node server/load-test.js [url] [clients] [duration]
```

`npm test` can hang after the tests finish (an open handle from importing `server.js`, e.g. a live timer/listener never closed) — if it doesn't return, use `node --test --test-force-exit server.test.js` instead.

## Key Memory Files

- `feedback_railway_deploy.md` — Railway root directory gotcha (must run from project root)
- `architecture.md` — all 9 endpoints, caching strategy, route enrichment pipeline, flight logging, email reports
