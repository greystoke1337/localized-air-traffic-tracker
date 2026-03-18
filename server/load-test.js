#!/usr/bin/env node
// Load test: simulates N concurrent clients hitting the proxy from different locations.
// Usage: node load-test.js [url] [clients] [duration_sec]
//   url:          proxy base URL (default: http://localhost:3000)
//   clients:      number of concurrent clients (default: 100)
//   duration_sec: how long to run (default: 60)

const http = require('http');
const https = require('https');

const BASE_URL  = process.argv[2] || 'http://localhost:3000';
const CLIENTS   = parseInt(process.argv[3] || '100');
const DURATION  = parseInt(process.argv[4] || '60') * 1000;
const INTERVAL  = 10000; // each client polls every 10s (like real ESP32/web clients)

// Generate N locations with realistic clustering (most users are in cities)
function randomLocations(n) {
  const locations = [];
  const clusters = [
    { name: 'Sydney',    lat: -33.85, lon: 151.14, weight: 20 },
    { name: 'London',    lat: 51.47, lon: -0.46,   weight: 15 },
    { name: 'New York',  lat: 40.64, lon: -73.78,  weight: 15 },
    { name: 'Tokyo',     lat: 35.55, lon: 139.78,  weight: 10 },
    { name: 'Dubai',     lat: 25.25, lon: 55.36,   weight: 5 },
    { name: 'Berlin',    lat: 52.52, lon: 13.40,   weight: 5 },
    { name: 'Singapore', lat: 1.35,  lon: 103.82,  weight: 5 },
    { name: 'LA',        lat: 33.94, lon: -118.41, weight: 5 },
    { name: 'Chicago',  lat: 41.90, lon: -87.68,  weight: 25 },
  ];
  // 80% of clients are in clusters, 20% random worldwide
  const clusterShare = Math.floor(n * 0.8);
  const totalWeight = clusters.reduce((s, c) => s + c.weight, 0);

  // Spread within ~500m of city center (0.005 degrees) — clients in same city share geo-bucketed cache
  for (let i = 0; i < clusterShare; i++) {
    let roll = Math.random() * totalWeight, cum = 0;
    let c = clusters[0];
    for (const cl of clusters) { cum += cl.weight; if (roll < cum) { c = cl; break; } }
    locations.push({
      lat: (c.lat + (Math.random() - 0.5) * 0.005).toFixed(4),
      lon: (c.lon + (Math.random() - 0.5) * 0.005).toFixed(4),
      radius: 15,
    });
  }

  // Random worldwide locations
  for (let i = clusterShare; i < n; i++) {
    locations.push({
      lat: ((Math.random() * 140) - 70).toFixed(4),
      lon: ((Math.random() * 360) - 180).toFixed(4),
      radius: [10, 15, 20, 25][Math.floor(Math.random() * 4)],
    });
  }

  return locations;
}

function fetchJson(url) {
  return new Promise((resolve, reject) => {
    const mod = url.startsWith('https') ? https : http;
    const start = Date.now();
    const req = mod.get(url, { timeout: 15000 }, (res) => {
      let body = '';
      res.on('data', (chunk) => body += chunk);
      res.on('end', () => {
        const elapsed = Date.now() - start;
        try { resolve({ status: res.statusCode, elapsed, body: JSON.parse(body) }); }
        catch { resolve({ status: res.statusCode, elapsed, body }); }
      });
    });
    req.on('error', (e) => reject(e));
    req.on('timeout', () => { req.destroy(); reject(new Error('timeout')); });
  });
}

async function runClient(id, location, results) {
  const url = `${BASE_URL}/flights?lat=${location.lat}&lon=${location.lon}&radius=${location.radius}`;
  const end = Date.now() + DURATION;

  while (Date.now() < end) {
    try {
      const res = await fetchJson(url);
      results.requests++;
      results.latencies.push(res.elapsed);
      if (res.status === 200) {
        results.successes++;
        results.flights += (res.body?.ac?.length || 0);
      } else if (res.status === 429) {
        results.rateLimited++;
      } else {
        results.errors++;
        results.errorCodes[res.status] = (results.errorCodes[res.status] || 0) + 1;
      }
    } catch (e) {
      results.requests++;
      results.errors++;
      results.errorTypes[e.message] = (results.errorTypes[e.message] || 0) + 1;
    }

    // Wait before next poll (with jitter)
    await new Promise(r => setTimeout(r, INTERVAL + Math.random() * 2000));
  }
}

async function main() {
  const locations = randomLocations(CLIENTS);
  const results = {
    requests: 0, successes: 0, errors: 0, rateLimited: 0, flights: 0,
    latencies: [], errorCodes: {}, errorTypes: {},
  };

  console.log(`\n  PROXY LOAD TEST`);
  console.log(`  ═══════════════`);
  console.log(`  Target:    ${BASE_URL}`);
  console.log(`  Clients:   ${CLIENTS}`);
  console.log(`  Duration:  ${DURATION / 1000}s`);
  console.log(`  Interval:  ${INTERVAL / 1000}s per client`);
  const rawUnique = new Set(locations.map(l => `${l.lat},${l.lon},${l.radius}`)).size;
  const bucketedUnique = new Set(locations.map(l =>
    `${Number(l.lat).toFixed(2)},${Number(l.lon).toFixed(2)},${l.radius}`
  )).size;
  console.log(`  Unique locations: ${rawUnique} (${bucketedUnique} after geo-bucketing)`);
  console.log();

  // Check proxy is reachable
  try {
    const ping = await fetchJson(`${BASE_URL}/stats`);
    console.log(`  Proxy reachable: ${ping.status === 200 ? 'YES' : 'NO (status ' + ping.status + ')'}`);
    if (ping.status !== 200) { process.exit(1); }
  } catch (e) {
    console.log(`  Proxy unreachable: ${e.message}`);
    process.exit(1);
  }

  // Progress ticker
  const startTime = Date.now();
  const ticker = setInterval(() => {
    const elapsed = Math.floor((Date.now() - startTime) / 1000);
    const p50 = percentile(results.latencies, 50);
    const p99 = percentile(results.latencies, 99);
    process.stdout.write(`\r  [${elapsed}s] requests=${results.requests} ok=${results.successes} err=${results.errors} rate-limited=${results.rateLimited} p50=${p50}ms p99=${p99}ms`);
  }, 2000);

  // Ramp up clients gradually over first 30s (realistic — users don't all connect simultaneously)
  const RAMP_MS = Math.min(30000, DURATION / 2);
  const promises = locations.map((loc, i) => {
    const delay = Math.floor((i / CLIENTS) * RAMP_MS);
    return new Promise(r => setTimeout(r, delay)).then(() => runClient(i, loc, results));
  });
  console.log(`  Ramp-up: ${RAMP_MS / 1000}s\n`);
  await Promise.all(promises);
  clearInterval(ticker);

  // Final report
  const p50 = percentile(results.latencies, 50);
  const p95 = percentile(results.latencies, 95);
  const p99 = percentile(results.latencies, 99);
  const max = results.latencies.length > 0 ? Math.max(...results.latencies) : 0;
  const min = results.latencies.length > 0 ? Math.min(...results.latencies) : 0;
  const avg = results.latencies.length > 0
    ? Math.round(results.latencies.reduce((a, b) => a + b, 0) / results.latencies.length) : 0;

  const successRate = results.requests > 0
    ? ((results.successes / results.requests) * 100).toFixed(1) : '0.0';

  console.log(`\n\n  ═══════════════════════════════════════`);
  console.log(`  RESULTS`);
  console.log(`  ═══════════════════════════════════════`);
  console.log(`  Total requests:  ${results.requests}`);
  console.log(`  Successes:       ${results.successes} (${successRate}%)`);
  console.log(`  Errors:          ${results.errors}`);
  console.log(`  Rate limited:    ${results.rateLimited}`);
  console.log(`  Flights seen:    ${results.flights}`);
  console.log();
  console.log(`  Latency (ms):`);
  console.log(`    min=${min}  avg=${avg}  p50=${p50}  p95=${p95}  p99=${p99}  max=${max}`);

  if (Object.keys(results.errorCodes).length > 0) {
    console.log();
    console.log(`  Error codes:`);
    for (const [code, count] of Object.entries(results.errorCodes)) {
      console.log(`    HTTP ${code}: ${count}`);
    }
  }
  if (Object.keys(results.errorTypes).length > 0) {
    console.log();
    console.log(`  Error types:`);
    for (const [type, count] of Object.entries(results.errorTypes)) {
      console.log(`    ${type}: ${count}`);
    }
  }

  // Check proxy stats after load
  try {
    const after = await fetchJson(`${BASE_URL}/stats`);
    console.log();
    console.log(`  Proxy stats after load:`);
    console.log(`    Total requests: ${after.body.totalRequests}`);
    console.log(`    Cache hit rate: ${after.body.cacheHitRate}`);
    console.log(`    Cache entries:  ${after.body.cacheEntries}`);
    console.log(`    Unique clients: ${after.body.uniqueClients}`);
    console.log(`    Errors:         ${after.body.errors}`);
  } catch {}

  // Verdict (rate limiting from same-IP is expected when load testing — only count real failures)
  const nonRateLimited = results.requests - results.rateLimited;
  const errorRate = nonRateLimited > 0 ? (results.errors / nonRateLimited) : 0;
  console.log();
  const pass = errorRate < 0.1 && p99 < 12000;
  if (pass) {
    console.log(`  VERDICT: PASS`);
  } else {
    console.log(`  VERDICT: FAIL`);
    if (errorRate >= 0.1) console.log(`    - ${results.errors}/${nonRateLimited} non-rate-limited requests failed (${(errorRate * 100).toFixed(1)}%)`);
    if (p99 >= 12000) console.log(`    - p99 latency ${p99}ms exceeds ESP32 timeout (12000ms)`);
  }
  if (results.rateLimited > 0) console.log(`  NOTE: ${results.rateLimited} rate-limited (expected — all clients share one IP)`);
  console.log();

  process.exit(pass ? 0 : 1);
}

function percentile(arr, p) {
  if (arr.length === 0) return 0;
  const sorted = [...arr].sort((a, b) => a - b);
  const idx = Math.ceil((p / 100) * sorted.length) - 1;
  return sorted[Math.max(0, idx)];
}

main().catch(e => { console.error(e); process.exit(1); });
