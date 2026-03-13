#!/usr/bin/env node
// Mock proxy server for testing ESP32 firmware resilience.
// Zero dependencies — uses only Node.js built-in modules.
//
// Usage:  node tools/mock-proxy.js [mode] [port] [--scenario NAME]
//
// Modes:
//   normal   — valid flight + weather JSON (default)
//   timeout  — accepts TCP, never responds
//   error503 — returns 503 Service Unavailable
//   error502 — returns 502 Bad Gateway
//   corrupt  — returns 200 with broken JSON
//   partial  — returns 200, drops connection mid-body
//   slow     — waits 4s before valid response
//   chaos    — random mix of responses (stress testing)
//   transition — cycles 0→1→0→5 flights (tests WX↔FLIGHT transitions)
//   flap     — alternates 0/1 flights every request (worst-case)
//
// Scenarios (--scenario NAME):
//   busy, quiet, crowded, emergency, approach_rush, single, mixed
//   See tools/synthetic-data.js --list for full list.

const http = require('http');
const os = require('os');
const { generate, SCENARIOS } = require('./synthetic-data');

const mode = process.argv[2] || 'normal';
const port = parseInt(process.argv[3]) || 3000;
const scenarioIdx = process.argv.indexOf('--scenario');
const scenario = scenarioIdx >= 0 ? process.argv[scenarioIdx + 1] : 'busy';

const MODES = ['normal', 'timeout', 'error503', 'error502', 'corrupt', 'partial', 'slow',
               'chaos', 'transition', 'flap'];
if (!MODES.includes(mode)) {
  console.error(`Unknown mode: "${mode}"\nAvailable: ${MODES.join(', ')}`);
  process.exit(1);
}
if (scenarioIdx >= 0 && !SCENARIOS[scenario]) {
  console.error(`Unknown scenario: "${scenario}"\nAvailable: ${Object.keys(SCENARIOS).join(', ')}`);
  process.exit(1);
}

let seq = 0;

// Generate synthetic data from request lat/lon or defaults
function parseLatLon(url) {
  try {
    const u = new URL(url, 'http://localhost');
    const lat = parseFloat(u.searchParams.get('lat')) || -33.8688;
    const lon = parseFloat(u.searchParams.get('lon')) || 151.2093;
    return { lat, lon };
  } catch { return { lat: -33.8688, lon: 151.2093 }; }
}

function makeSyntheticFlights(url, sc) {
  const { lat, lon } = parseLatLon(url);
  return JSON.stringify(generate(lat, lon, sc || scenario).flights);
}

function makeSyntheticWeather() {
  return JSON.stringify(generate(0, 0, 'empty').weather);
}

function makeFlightResponse(count) {
  const data = generate(-33.8688, 151.2093, scenario).flights;
  data.ac = data.ac.slice(0, count);
  data.total = count;
  return JSON.stringify(data);
}

const ZERO_FLIGHTS = JSON.stringify({ ac: [], total: 0, now: Date.now() / 1000 });

function getLocalIP() {
  const nets = os.networkInterfaces();
  for (const name of Object.keys(nets)) {
    for (const net of nets[name]) {
      if (net.family === 'IPv4' && !net.internal) return net.address;
    }
  }
  return '127.0.0.1';
}

function timestamp() {
  return new Date().toISOString().slice(11, 23);
}

function logReq(req, note) {
  console.log(`  [${timestamp()}] ${req.method} ${req.url} -> ${note}`);
}

const handlers = {
  normal(req, res) {
    if (req.url.startsWith('/flights')) {
      logReq(req, `200 flights (${scenario})`);
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(makeSyntheticFlights(req.url));
    } else if (req.url.startsWith('/weather')) {
      logReq(req, '200 weather');
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(makeSyntheticWeather());
    } else {
      logReq(req, '404');
      res.writeHead(404);
      res.end('Not Found');
    }
  },

  timeout(req, res) {
    logReq(req, 'HANG (no response)');
    // Accept connection but never respond — tests HTTP read timeout
  },

  error503(req, res) {
    logReq(req, '503');
    res.writeHead(503, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ error: 'Proxy disabled' }));
  },

  error502(req, res) {
    logReq(req, '502');
    res.writeHead(502, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ error: 'Upstream timeout' }));
  },

  corrupt(req, res) {
    logReq(req, '200 corrupt JSON');
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end('{"ac":[{"flight":"QFA1","lat":-33.87,"lon":151.21,"alt_baro":BROKEN');
  },

  partial(req, res) {
    logReq(req, '200 partial (will drop)');
    res.writeHead(200, { 'Content-Type': 'application/json' });
    const half = SAMPLE_FLIGHTS.slice(0, Math.floor(SAMPLE_FLIGHTS.length / 2));
    res.write(half);
    setTimeout(() => res.destroy(), 500);
  },

  slow(req, res) {
    logReq(req, 'SLOW (4s delay)');
    setTimeout(() => {
      if (req.url.startsWith('/flights')) {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(makeSyntheticFlights(req.url));
        console.log(`  [${timestamp()}]   -> sent response`);
      } else if (req.url.startsWith('/weather')) {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(makeSyntheticWeather());
        console.log(`  [${timestamp()}]   -> sent response`);
      } else {
        res.writeHead(404);
        res.end();
      }
    }, 4000);
  },

  chaos(req, res) {
    seq++;
    if (!req.url.startsWith('/flights')) {
      // Weather always responds normally in chaos mode
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(makeSyntheticWeather());
      return;
    }
    const r = Math.random();
    let what;
    if (r < 0.40) {
      what = '0 flights';
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(ZERO_FLIGHTS);
    } else if (r < 0.55) {
      what = '1 flight';
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(makeFlightResponse(1));
    } else if (r < 0.70) {
      const n = 2 + Math.floor(Math.random() * 4);
      what = `${n} flights`;
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(makeFlightResponse(n));
    } else if (r < 0.85) {
      what = 'slow (2s)';
      setTimeout(() => {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(makeFlightResponse(2));
      }, 2000);
    } else if (r < 0.95) {
      what = 'error 503';
      res.writeHead(503, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ error: 'Chaos error' }));
    } else {
      what = 'corrupt JSON';
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end('{"ac":[{"flight":"BROKEN","lat":INVALID');
    }
    console.log(`  [${timestamp()}] [SEQ:${seq}] chaos -> ${what}`);
  },

  transition(req, res) {
    seq++;
    if (!req.url.startsWith('/flights')) {
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(makeSyntheticWeather());
      return;
    }
    // Cycle: 0,0 | 1 | 0,0,0 | 5,5 — repeating every 8 requests
    const cycle = [0, 0, 1, 0, 0, 0, 5, 5];
    const n = cycle[seq % cycle.length];
    const data = n === 0 ? ZERO_FLIGHTS : makeFlightResponse(n);
    console.log(`  [${timestamp()}] [SEQ:${seq}] transition -> ${n} flights (step ${seq % cycle.length}/${cycle.length})`);
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(data);
  },

  flap(req, res) {
    seq++;
    if (!req.url.startsWith('/flights')) {
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(makeSyntheticWeather());
      return;
    }
    const n = seq % 2 === 0 ? 0 : 1;
    const data = n === 0 ? ZERO_FLIGHTS : makeFlightResponse(1);
    console.log(`  [${timestamp()}] [SEQ:${seq}] flap -> ${n} flights`);
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(data);
  }
};

const expectations = {
  normal:     'ESP32 shows flights normally. Serial: [PROXY] OK',
  timeout:    'ESP32 HTTP read timeout after 5s. Serial: [PROXY] HTTP -1\n  Falls back to direct API.',
  error503:   'ESP32 gets non-200, returns empty. Serial: [PROXY] HTTP 503\n  Falls back to direct API.',
  error502:   'ESP32 gets non-200, returns empty. Serial: [PROXY] HTTP 502\n  Falls back to direct API.',
  corrupt:    'ESP32 gets 200 but JSON parse fails. Serial: JSON parse error\n  Falls back to SD cache.',
  partial:    'ESP32 gets 200, connection drops mid-read. Serial: JSON parse error or partial data\n  Falls back to SD cache.',
  slow:       'ESP32 waits ~4s then gets valid response. Serial: [PROXY] OK ... (slow)',
  chaos:      'Random mix: 40% empty, 30% flights, 15% slow, 10% errors, 5% corrupt.\n  Watch for reboots, WDT resets, heap drops. Use with serial-stress.js analyzer.',
  transition: 'Cycles: 0→0→1→0→0→0→5→5 flights, repeating.\n  Tests weather-to-flight display transitions that cause crashes.',
  flap:       'Alternates 0/1 flights every request.\n  Worst-case state machine exercise for WX↔FLIGHT transitions.'
};

const server = http.createServer(handlers[mode]);
server.listen(port, () => {
  const ip = getLocalIP();
  const scDesc = SCENARIOS[scenario] ? SCENARIOS[scenario].desc : scenario;
  console.log(`\n=== MOCK PROXY: ${mode} mode on port ${port} ===`);
  console.log(`    Scenario: ${scenario} — ${scDesc}\n`);
  console.log(`Local IP: ${ip}`);
  console.log(`Set PROXY_HOST in tracker_live_fnk0103s.ino line 37 to "${ip}"`);
  console.log(`Then:  ./build.sh compile && ./build.sh upload COM4`);
  console.log(`       ./build.sh monitor COM4  (in another terminal)\n`);
  console.log(`Expected ESP32 behavior:`);
  console.log(`  ${expectations[mode]}\n`);
  console.log('Waiting for requests... (Ctrl-C to stop)\n');
});
