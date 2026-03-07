#!/usr/bin/env node
// Mock proxy server for testing ESP32 firmware resilience.
// Zero dependencies — uses only Node.js built-in modules.
//
// Usage:  node tools/mock-proxy.js [mode] [port]
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

const http = require('http');
const os = require('os');

const mode = process.argv[2] || 'normal';
const port = parseInt(process.argv[3]) || 3000;

const MODES = ['normal', 'timeout', 'error503', 'error502', 'corrupt', 'partial', 'slow',
               'chaos', 'transition', 'flap'];
if (!MODES.includes(mode)) {
  console.error(`Unknown mode: "${mode}"\nAvailable: ${MODES.join(', ')}`);
  process.exit(1);
}

let seq = 0;

const SAMPLE_FLIGHTS = JSON.stringify({
  ac: [
    { flight: 'QFA1    ', r: 'VH-OQA', t: 'A388', lat: -33.87, lon: 151.21,
      alt_baro: 35000, gs: 480, baro_rate: 0, track: 180, squawk: '1234',
      dep: 'YMML', arr: 'YSSY' },
    { flight: 'VOZ456  ', r: 'VH-YIA', t: 'B738', lat: -33.88, lon: 151.20,
      alt_baro: 12000, gs: 280, baro_rate: -1200, track: 90, squawk: '2345',
      dep: 'YBBN', arr: 'YSSY' }
  ],
  total: 2, now: Date.now() / 1000
});

const SAMPLE_WEATHER = JSON.stringify({
  temp: 22.5, feels_like: 21.0, humidity: 65,
  condition: 'Partly Cloudy', weather_code: 2,
  wind_speed: 15.0, wind_dir: 180, wind_cardinal: 'S',
  uv_index: 5.0, utc_offset_secs: 36000
});

const EXTRA_FLIGHTS = [
  { flight: 'JST442  ', r: 'VH-VKD', t: 'A320', lat: -33.87, lon: 151.20,
    alt_baro: 8000, gs: 320, baro_rate: 1200, track: 270, squawk: '3456',
    dep: 'YBBN', arr: 'YSSY' },
  { flight: 'UAE417  ', r: 'A6-EON', t: 'A388', lat: -33.88, lon: 151.19,
    alt_baro: 35000, gs: 490, baro_rate: 0, track: 45, squawk: '6501',
    dep: 'OMDB', arr: 'YSSY' },
  { flight: 'SIA221  ', r: '9V-SKA', t: 'A388', lat: -33.86, lon: 151.22,
    alt_baro: 28000, gs: 420, baro_rate: -800, track: 160, squawk: '5512',
    dep: 'WSSS', arr: 'YSSY' }
];

function makeFlightResponse(count) {
  const base = JSON.parse(SAMPLE_FLIGHTS);
  const all = base.ac.concat(EXTRA_FLIGHTS);
  return JSON.stringify({ ac: all.slice(0, count), total: count, now: Date.now() / 1000 });
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
      logReq(req, '200 flights');
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(SAMPLE_FLIGHTS);
    } else if (req.url.startsWith('/weather')) {
      logReq(req, '200 weather');
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(SAMPLE_WEATHER);
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
        res.end(SAMPLE_FLIGHTS);
        console.log(`  [${timestamp()}]   -> sent response`);
      } else if (req.url.startsWith('/weather')) {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(SAMPLE_WEATHER);
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
      res.end(SAMPLE_WEATHER);
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
      res.end(SAMPLE_WEATHER);
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
      res.end(SAMPLE_WEATHER);
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
  console.log(`\n=== MOCK PROXY: ${mode} mode on port ${port} ===\n`);
  console.log(`Local IP: ${ip}`);
  console.log(`Set PROXY_HOST in tracker_live_fnk0103s.ino line 37 to "${ip}"`);
  console.log(`Then:  ./build.sh compile && ./build.sh upload COM4`);
  console.log(`       ./build.sh monitor COM4  (in another terminal)\n`);
  console.log(`Expected ESP32 behavior:`);
  console.log(`  ${expectations[mode]}\n`);
  console.log('Waiting for requests... (Ctrl-C to stop)\n');
});
