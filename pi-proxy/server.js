const dns = require('dns'); dns.setDefaultResultOrder('ipv4first');
const https = require('https');
const http  = require('http');
const ipv4Agent     = new https.Agent({ family: 4 });
const ipv4HttpAgent = new http.Agent({ family: 4 });
const express = require('express');
const _fetch  = (...args) => import('node-fetch').then(({ default: f }) => f(...args));
const fetch   = (url, opts = {}) => {
  if (!opts.agent) opts.agent = url.startsWith('https') ? ipv4Agent : ipv4HttpAgent;
  return _fetch(url, opts);
};
const os      = require('os');
const fs      = require('fs');
const { execSync } = require('child_process');

const app      = express();
const PORT     = 3000;
const CACHE_MS         = 10000;
const ROUTE_CACHE_MS   = 30 * 60 * 1000;
const ROUTE_CACHE_FILE = __dirname + '/route-cache.json';

const cache      = new Map();
const inFlight   = new Map();  // key → Promise (dedup concurrent upstream fetches)
const routeCache = new Map();  // callsign -> { dep, arr, timestamp }

// ICAO and IATA codes → city name (same DB as web app)
const AIRPORT_DB = {
  // Australia
  YSSY:'Sydney',YSAY:'Sydney',YMLB:'Melbourne',YMML:'Melbourne',YBBN:'Brisbane',
  YPPH:'Perth',YPAD:'Adelaide',YSCB:'Canberra',YBCS:'Cairns',YBHM:'Hamilton Is',
  YBTL:'Townsville',YBAS:'Alice Springs',YBDG:'Bendigo',YDBY:'Derby',
  YSNF:'Norfolk Is',YAGD:'Aganda',
  SYD:'Sydney',MEL:'Melbourne',BNE:'Brisbane',PER:'Perth',ADL:'Adelaide',
  CBR:'Canberra',CNS:'Cairns',OOL:'Gold Coast',TSV:'Townsville',DRW:'Darwin',
  HBA:'Hobart',LST:'Launceston',MKY:'Mackay',ROK:'Rockhampton',
  // Asia Pacific
  NZAA:'Auckland',NZCH:'Christchurch',NZWN:'Wellington',NZQN:'Queenstown',
  AKL:'Auckland',CHC:'Christchurch',WLG:'Wellington',ZQN:'Queenstown',
  WSSS:'Singapore',WSAP:'Singapore',SIN:'Singapore',
  VHHH:'Hong Kong',HKG:'Hong Kong',
  RJAA:'Tokyo',RJTT:'Tokyo',NRT:'Tokyo',HND:'Tokyo',
  RJBB:'Osaka',RJOO:'Osaka',KIX:'Osaka',ITM:'Osaka',
  RKSI:'Seoul',RKSS:'Seoul',ICN:'Seoul',GMP:'Seoul',
  RCTP:'Taipei',RCSS:'Taipei',TPE:'Taipei',TSA:'Taipei',
  VTBS:'Bangkok',VTBD:'Bangkok',BKK:'Bangkok',DMK:'Bangkok',
  WMKK:'Kuala Lumpur',KUL:'Kuala Lumpur',
  WADD:'Bali',WIII:'Jakarta',DPS:'Bali',CGK:'Jakarta',
  RPLL:'Manila',MNL:'Manila',
  VVTS:'Ho Chi Minh',VVNB:'Hanoi',SGN:'Ho Chi Minh',HAN:'Hanoi',
  ZBAA:'Beijing',ZGSZ:'Shenzhen',ZGGG:'Guangzhou',ZSPD:'Shanghai',
  ZSSS:'Shanghai',PEK:'Beijing',SZX:'Shenzhen',CAN:'Guangzhou',PVG:'Shanghai',SHA:'Shanghai',
  ZUCK:'Chongqing',ZUUU:'Chengdu',CKG:'Chongqing',CTU:'Chengdu',
  OMDB:'Dubai',OMDW:'Dubai',DXB:'Dubai',DWC:'Dubai',
  OMAA:'Abu Dhabi',AUH:'Abu Dhabi',
  OTBD:'Doha',OTHH:'Doha',DOH:'Doha',
  OERK:'Riyadh',OEDF:'Dammam',RUH:'Riyadh',DMM:'Dammam',
  OJAM:'Amman',LLBG:'Tel Aviv',AMM:'Amman',TLV:'Tel Aviv',
  VIDP:'Delhi',VABB:'Mumbai',VOCB:'Chennai',VOBL:'Bangalore',
  DEL:'Delhi',BOM:'Mumbai',MAA:'Chennai',BLR:'Bangalore',
  VOMM:'Chennai',VECC:'Kolkata',CCU:'Kolkata',
  // Europe
  EGLL:'London',EGKK:'London',EGSS:'London',EGLC:'London',
  LHR:'London',LGW:'London',STN:'London',LCY:'London',LTN:'London',
  LFPG:'Paris',LFPO:'Paris',CDG:'Paris',ORY:'Paris',
  EHAM:'Amsterdam',AMS:'Amsterdam',
  EDDF:'Frankfurt',FRA:'Frankfurt',
  EDDM:'Munich',MUC:'Munich',
  LEMD:'Madrid',MAD:'Madrid',
  LEBL:'Barcelona',BCN:'Barcelona',
  LIRF:'Rome',LIMC:'Milan',FCO:'Rome',MXP:'Milan',LIN:'Milan',
  EGPH:'Edinburgh',EGPF:'Glasgow',EDI:'Edinburgh',GLA:'Glasgow',
  EIDW:'Dublin',DUB:'Dublin',
  EBBR:'Brussels',BRU:'Brussels',
  EKCH:'Copenhagen',CPH:'Copenhagen',
  ESSA:'Stockholm',ARN:'Stockholm',
  ENGM:'Oslo',OSL:'Oslo',
  EFHK:'Helsinki',HEL:'Helsinki',
  LSZH:'Zurich',ZRH:'Zurich',
  LSGG:'Geneva',GVA:'Geneva',
  LOWW:'Vienna',VIE:'Vienna',
  EPWA:'Warsaw',WAW:'Warsaw',
  LKPR:'Prague',PRG:'Prague',
  LHBP:'Budapest',BUD:'Budapest',
  LGAV:'Athens',ATH:'Athens',
  LTFM:'Istanbul',LTBA:'Istanbul',IST:'Istanbul',SAW:'Istanbul',
  // Americas
  KJFK:'New York',KLGA:'New York',KEWR:'New York',
  JFK:'New York',LGA:'New York',EWR:'New York',
  KLAX:'Los Angeles',LAX:'Los Angeles',
  KSFO:'San Francisco',SFO:'San Francisco',
  KORD:'Chicago',KMDW:'Chicago',ORD:'Chicago',MDW:'Chicago',
  KBOS:'Boston',BOS:'Boston',
  KMIA:'Miami',MIA:'Miami',
  KATL:'Atlanta',ATL:'Atlanta',
  KDFW:'Dallas',DFW:'Dallas',DAL:'Dallas',
  KDEN:'Denver',DEN:'Denver',
  KSEA:'Seattle',SEA:'Seattle',
  KLAS:'Las Vegas',LAS:'Las Vegas',
  KPHX:'Phoenix',PHX:'Phoenix',
  CYYZ:'Toronto',CYVR:'Vancouver',CYUL:'Montreal',
  YYZ:'Toronto',YVR:'Vancouver',YUL:'Montreal',
  SBGR:'São Paulo',SBGL:'Rio de Janeiro',
  GRU:'São Paulo',GIG:'Rio de Janeiro',
  SAEZ:'Buenos Aires',EZE:'Buenos Aires',
  SKBO:'Bogotá',BOG:'Bogotá',
  SEQM:'Quito',UIO:'Quito',
  // Africa
  FAOR:'Johannesburg',FACT:'Cape Town',
  JNB:'Johannesburg',CPT:'Cape Town',
  HECA:'Cairo',CAI:'Cairo',
  DNMM:'Lagos',LOS:'Lagos',
  HAAB:'Addis Ababa',ADD:'Addis Ababa',
  HSSS:'Khartoum',KRT:'Khartoum',
  FMMI:'Antananarivo',TNR:'Antananarivo',
};

function airportName(code) {
  if (!code) return null;
  return AIRPORT_DB[code.trim().toUpperCase()] || code.trim().toUpperCase();
}

function formatRouteString(dep, arr) {
  if (!dep && !arr) return null;
  const depName = dep ? airportName(dep) : '?';
  const arrName = arr ? airportName(arr) : '?';
  return depName + ' > ' + arrName;
}

try {
  const saved = JSON.parse(fs.readFileSync(ROUTE_CACHE_FILE, 'utf8'));
  for (const [cs, entry] of Object.entries(saved)) routeCache.set(cs, entry);
  console.log(`Loaded ${routeCache.size} routes from disk cache`);
} catch { /* file absent on first run */ }
let   proxyEnabled = true; // ← soft on/off toggle

const startTime = Date.now();
const stats = {
  totalRequests: 0,
  cacheHits:     0,
  errors:        0,
  peakHour:      new Array(24).fill(0),
  uniqueClients: new Set(),
};

// ── Request log (last 100 entries) ──────────────────────────────────
const requestLog = [];
function addLog(entry) {
  requestLog.unshift({ ...entry, time: new Date().toISOString() });
  if (requestLog.length > 100) requestLog.pop();
}

function saveRouteCache() {
  const obj = {};
  for (const [cs, entry] of routeCache) obj[cs] = entry;
  fs.writeFile(ROUTE_CACHE_FILE, JSON.stringify(obj, null, 2), () => {});
}

async function lookupRoute(callsign) {
  const cs  = callsign.trim();
  const hit = routeCache.get(cs);
  if (hit && (Date.now() - hit.timestamp) < ROUTE_CACHE_MS) return hit;

  // Source 1: OpenSky Network
  try {
    const url = `https://opensky-network.org/api/routes?callsign=${encodeURIComponent(cs)}`;
    const r   = await fetch(url, { signal: AbortSignal.timeout(4000) });
    if (r.ok) {
      const d = await r.json();
      if (Array.isArray(d.route) && d.route.length >= 2) {
        const entry = { dep: d.route[0], arr: d.route[d.route.length - 1], timestamp: Date.now() };
        routeCache.set(cs, entry);
        saveRouteCache();
        return entry;
      }
    }
  } catch { /* timeout or network error */ }

  // Source 2: adsbdb.com fallback
  try {
    const url = `https://api.adsbdb.com/v0/callsign/${encodeURIComponent(cs)}`;
    const r   = await fetch(url, { signal: AbortSignal.timeout(5000) });
    if (r.ok) {
      const d = await r.json();
      const fr = d?.response?.flightroute;
      if (fr) {
        const dep = fr.origin?.icao_code || fr.origin?.iata_code || null;
        const arr = fr.destination?.icao_code || fr.destination?.iata_code || null;
        if (dep || arr) {
          const entry = { dep, arr, timestamp: Date.now() };
          routeCache.set(cs, entry);
          saveRouteCache();
          return entry;
        }
      }
    }
  } catch { /* timeout or network error */ }

  if (hit) return hit;  // stale disk entry as fallback
  return null;
}

function cpuTemp() {
  try {
    const raw = fs.readFileSync('/sys/class/thermal/thermal_zone0/temp', 'utf8');
    return (parseInt(raw) / 1000).toFixed(1) + '°C';
  } catch { return 'N/A'; }
}

function pm2Status() {
  try {
    const out = execSync('pm2 jlist', { timeout: 3000 }).toString();
    return JSON.parse(out).map(p => ({
      name:     p.name,
      status:   p.pm2_env.status,
      uptime:   p.pm2_env.pm_uptime
        ? Math.floor((Date.now() - p.pm2_env.pm_uptime) / 1000)
        : null,
      restarts: p.pm2_env.restart_time,
    }));
  } catch { return []; }
}

function networkInfo() {
  const ifaces = os.networkInterfaces();
  const result = {};
  for (const [name, addrs] of Object.entries(ifaces)) {
    const ipv4 = (addrs || []).find(a => a.family === 'IPv4' && !a.internal);
    if (ipv4) result[name] = ipv4.address;
  }
  return result;
}

function formatUptime(secs) {
  const d = Math.floor(secs / 86400);
  const h = Math.floor((secs % 86400) / 3600);
  const m = Math.floor((secs % 3600) / 60);
  const s = Math.floor(secs % 60);
  return (d ? d + 'd ' : '') + (h ? h + 'h ' : '') + (m ? m + 'm ' : '') + s + 's';
}

// ── CORS ─────────────────────────────────────────────────────────────
app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  next();
});

// ── Toggle endpoint ───────────────────────────────────────────────────
app.post('/proxy/toggle', (req, res) => {
  proxyEnabled = !proxyEnabled;
  addLog({ type: 'SYS', client: req.headers['x-forwarded-for'] || req.socket.remoteAddress, key: 'proxy ' + (proxyEnabled ? 'ENABLED' : 'DISABLED') });
  res.json({ enabled: proxyEnabled });
});

// ── Dashboard UI ──────────────────────────────────────────────────────
app.get('/', (req, res) => {
  res.send(`<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>PIPROXY // DASHBOARD</title>
  <link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap" rel="stylesheet">
  <style>
    *, *::before, *::after { box-sizing: border-box; }
    body {
      background: #1a0a00; color: #ffa600;
      font-family: 'Share Tech Mono', monospace;
      font-size: 0.88rem; padding: 20px;
      max-width: 700px; margin: 0 auto;
    }
    body::after {
      content: ''; position: fixed; inset: 0; pointer-events: none; z-index: 99;
      background: repeating-linear-gradient(to bottom, transparent 0px, transparent 3px, rgba(0,0,0,0.05) 3px, rgba(0,0,0,0.05) 4px);
    }
    * { text-shadow: 0 0 2px #ffa600, 0 0 8px #ff8000; }
    h1 { font-size: 1.1rem; margin: 0 0 4px; }
    .sub { opacity: 0.5; font-size: 0.75rem; margin-bottom: 20px; }
    hr { border-color: #ffa600; opacity: 0.25; margin: 16px 0; }
    .section { margin-bottom: 20px; }
    .section-title { opacity: 0.5; font-size: 0.72rem; margin-bottom: 8px; letter-spacing: 0.1em; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
    .card { border: 1px solid #7a5200; padding: 10px 12px; background: rgba(255,166,0,0.03); }
    .card .label { opacity: 0.5; font-size: 0.7rem; margin-bottom: 4px; }
    .card .value { font-size: 1.05rem; }
    .badge { display: inline-block; padding: 2px 8px; font-size: 0.72rem; border: 1px solid; }
    .badge.online  { color: #60ff90; border-color: #60ff90; text-shadow: 0 0 6px #60ff90; }
    .badge.stopped { color: #ff6060; border-color: #ff6060; text-shadow: 0 0 6px #ff6060; }
    .badge.errored { color: #ff6060; border-color: #ff6060; text-shadow: 0 0 6px #ff6060; }
    .log-entry { border-bottom: 1px solid rgba(255,166,0,0.1); padding: 5px 0; font-size: 0.75rem; line-height: 1.6; }
    .log-entry:last-child { border: none; }
    .hit  { color: #60ff90; text-shadow: 0 0 6px #60ff90; }
    .miss { color: #ffa600; }
    .err  { color: #ff6060; text-shadow: 0 0 6px #ff6060; }
    .sys  { color: #60c8ff; text-shadow: 0 0 6px #60c8ff; }
    .ts   { opacity: 0.45; }
    .pm2-row { display: flex; align-items: center; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid rgba(255,166,0,0.1); }
    .pm2-row:last-child { border: none; }
    #refresh-note { opacity: 0.4; font-size: 0.7rem; margin-top: 20px; }

    /* Toggle button */
    #toggle-btn {
      background: none; border: 1px solid #ffa600; color: #ffa600;
      font-family: 'Share Tech Mono', monospace; font-size: 0.9rem;
      padding: 10px 24px; cursor: pointer; margin-bottom: 20px;
      text-shadow: 0 0 6px #ff8000; min-width: 200px;
      transition: background 0.2s, border-color 0.2s;
    }
    #toggle-btn:hover { background: rgba(255,166,0,0.1); }
    #toggle-btn.off {
      border-color: #ff6060; color: #ff6060;
      text-shadow: 0 0 6px #ff6060;
    }
    #proxy-status-label {
      display: inline-block; margin-left: 12px;
      font-size: 0.75rem; opacity: 0.6;
    }
  </style>
</head>
<body>
  <h1>PIPROXY // DASHBOARD</h1>
  <p class="sub" id="sub">Loading...</p>

  <button id="toggle-btn" onclick="toggleProxy()">⬤ PROXY ON</button>
  <span id="proxy-status-label"></span>

  <div id="root"></div>
  <p id="refresh-note"></p>

  <script>
    let proxyOn = true;

    function fmt(secs) {
      const d = Math.floor(secs/86400), h = Math.floor((secs%86400)/3600),
            m = Math.floor((secs%3600)/60), s = Math.floor(secs%60);
      return (d?d+'d ':'')+(h?h+'h ':'')+(m?m+'m ':'')+s+'s';
    }

    function card(label, value) {
      return '<div class="card"><div class="label">'+label+'</div><div class="value">'+value+'</div></div>';
    }

    function updateToggleBtn(enabled) {
      proxyOn = enabled;
      const btn = document.getElementById('toggle-btn');
      const lbl = document.getElementById('proxy-status-label');
      if (enabled) {
        btn.textContent = '⬤ PROXY ON';
        btn.classList.remove('off');
        lbl.textContent = 'clients are being served';
      } else {
        btn.textContent = '◯ PROXY OFF';
        btn.classList.add('off');
        lbl.textContent = 'returning 503 to all clients';
      }
    }

    async function toggleProxy() {
      try {
        const res  = await fetch('/proxy/toggle', { method: 'POST' });
        const data = await res.json();
        updateToggleBtn(data.enabled);
      } catch(e) {
        document.getElementById('proxy-status-label').textContent = 'toggle failed: ' + e.message;
      }
    }

    async function load() {
      try {
        const res = await fetch('/status');
        const d   = await res.json();
        const now = new Date();

        updateToggleBtn(d.proxyEnabled);
        document.getElementById('sub').textContent =
          'piproxy.local  ·  ' + now.toLocaleTimeString();

        const pm2Rows = (d.pm2 || []).map(p => {
          const up  = p.uptime != null ? fmt(p.uptime) : '---';
          const cls = p.status === 'online' ? 'online' : (p.status === 'stopped' ? 'stopped' : 'errored');
          return '<div class="pm2-row">'
            + '<span>' + p.name + '</span>'
            + '<span><span class="badge ' + cls + '">' + p.status.toUpperCase() + '</span></span>'
            + '<span style="opacity:0.6">up ' + up + '</span>'
            + '<span style="opacity:0.6">↺ ' + p.restarts + '</span>'
            + '</div>';
        }).join('') || '<span style="opacity:0.5">No PM2 data</span>';

        const netRows = Object.entries(d.network || {}).map(([iface, ip]) =>
          '<div class="log-entry"><span style="opacity:0.5">'+iface+'</span>&nbsp;&nbsp;'+ip+'</div>'
        ).join('') || '<span style="opacity:0.5">No interfaces</span>';

        const logRows = (d.log || []).map(e => {
          const t       = new Date(e.time).toLocaleTimeString();
          const typeCls = e.type === 'HIT' ? 'hit' : e.type === 'MISS' ? 'miss' : e.type === 'SYS' ? 'sys' : 'err';
          const client  = e.client || '?';
          const key     = e.key || e.error || '';
          return '<div class="log-entry">'
            + '<span class="ts">'+t+'</span>  '
            + '<span class="'+typeCls+'">['+e.type+']</span>  '
            + '<span style="opacity:0.6">'+client+'</span>  '
            + '<span style="opacity:0.5">'+key+'</span>'
            + '</div>';
        }).join('') || '<span style="opacity:0.5">No requests yet.</span>';

        const ram    = d.ram;
        const ramPct = ram ? Math.round((1 - ram.free / ram.total) * 100) : '?';

        document.getElementById('root').innerHTML =
          '<div class="section"><div class="section-title">▸ SYSTEM</div>'
          + '<div class="grid">'
          + card('PI UPTIME', fmt(d.uptime))
          + card('CPU TEMP',  d.temp)
          + card('RAM USED',  ramPct + '% of ' + Math.round(ram.total/1024/1024) + ' MB')
          + card('LOAD AVG',  (d.loadAvg||[]).map(l=>l.toFixed(2)).join(' / '))
          + '</div></div>'
          + '<hr>'
          + '<div class="section"><div class="section-title">▸ PM2 SERVICES</div>' + pm2Rows + '</div>'
          + '<hr>'
          + '<div class="section"><div class="section-title">▸ NETWORK</div>' + netRows + '</div>'
          + '<hr>'
          + '<div class="section"><div class="section-title">▸ REQUEST LOG (last 100)</div>' + logRows + '</div>';

        document.getElementById('refresh-note').textContent =
          'Auto-refreshes every 10s  ·  Last: ' + now.toLocaleTimeString();
      } catch(e) {
        document.getElementById('sub').textContent = 'ERROR: ' + e.message;
      }
    }

    load();
    setInterval(load, 10000);
  </script>
</body>
</html>`);
});

// ── Status API ────────────────────────────────────────────────────────
app.get('/status', (req, res) => {
  res.json({
    proxyEnabled,
    uptime:  os.uptime(),
    temp:    cpuTemp(),
    loadAvg: os.loadavg(),
    ram:     { total: os.totalmem(), free: os.freemem() },
    network: networkInfo(),
    pm2:     pm2Status(),
    log:     requestLog,
  });
});

// ── Flights proxy ─────────────────────────────────────────────────────
app.get('/flights', async (req, res) => {
  const client = req.headers['x-forwarded-for'] || req.socket.remoteAddress || '?';

  if (!proxyEnabled) {
    addLog({ type: 'ERR', client, error: 'proxy disabled' });
    return res.status(503).json({ error: 'Proxy is disabled' });
  }

  const { lat, lon, radius } = req.query;
  if (!lat || !lon || !radius) {
    addLog({ type: 'ERR', client, error: 'missing params' });
    return res.status(400).json({ error: 'lat, lon, radius required' });
  }

  const key = `${lat},${lon},${radius}`;
  const now  = Date.now();
  const hit  = cache.get(key);

  stats.totalRequests++;
  stats.peakHour[new Date().getHours()]++;
  stats.uniqueClients.add(client);

  if (hit && (now - hit.timestamp) < CACHE_MS) {
    stats.cacheHits++;
    addLog({ type: 'HIT', client, key });
    return res.json(hit.data);
  }

  if (!inFlight.has(key)) {
    const promise = (async () => {
      const apis = [
        { name: 'adsb.lol',        url: `https://api.adsb.lol/v2/point/${lat}/${lon}/${radius}` },
        { name: 'adsb.fi',         url: `https://opendata.adsb.fi/api/v3/lat/${lat}/lon/${lon}/dist/${radius}` },
        { name: 'airplanes.live',  url: `https://api.airplanes.live/v2/point/${lat}/${lon}/${radius}` },
      ];
      let data = null, lastErr = null;
      for (const api of apis) {
        try {
          const response = await fetch(api.url, { signal: AbortSignal.timeout(8000) });
          if (!response.ok) throw new Error(`${api.name} returned ${response.status}`);
          data = await response.json();
          addLog({ type: 'MISS', client, key: key + ` (${api.name})` });
          break;
        } catch (e) {
          lastErr = e;
          addLog({ type: 'ERR', client, key, error: `${api.name}: ${e.message}` });
        }
      }
      if (!data) throw lastErr || new Error('All APIs failed');
      const unrouted = (data.ac || []).filter(ac => ac.flight?.trim() && !ac.dep && !ac.arr);
      if (unrouted.length > 0) {
        const routes = await Promise.all(unrouted.map(ac => lookupRoute(ac.flight.trim())));
        unrouted.forEach((ac, i) => {
          if (routes[i]?.dep) ac.dep = routes[i].dep;
          if (routes[i]?.arr) ac.arr = routes[i].arr;
        });
      }
      for (const ac of (data.ac || [])) {
        const dep = ac.dep || ac.orig_iata || null;
        const arr = ac.arr || ac.dest_iata || null;
        const routeStr = formatRouteString(dep, arr);
        if (routeStr) ac.route = routeStr;
      }
      cache.set(key, { data, timestamp: Date.now() });
      return data;
    })();
    inFlight.set(key, promise);
    promise.finally(() => inFlight.delete(key)).catch(() => {});
  }

  try {
    const data = await inFlight.get(key);
    return res.json(data);
  } catch (e) {
    stats.errors++;
    if (hit) {
      addLog({ type: 'HIT', client, key: key + ' (stale fallback)' });
      return res.json(hit.data);
    }
    res.status(502).json({ error: e.message });
  }
});

// ── Stats endpoint (used by display.py) ──────────────────────────────
app.get('/stats', (req, res) => {
  const uptimeSec = Math.floor((Date.now() - startTime) / 1000);
  const hitRate = stats.totalRequests > 0
    ? ((stats.cacheHits / stats.totalRequests) * 100).toFixed(1)
    : '0.0';
  res.json({
    uptime:        formatUptime(uptimeSec),
    totalRequests: stats.totalRequests,
    cacheHits:     stats.cacheHits,
    cacheHitRate:  hitRate + '%',
    errors:        stats.errors,
    uniqueClients: stats.uniqueClients.size,
    cacheEntries:  cache.size,
  });
});

// ── Peak hour endpoint (used by display.py) ───────────────────────────
app.get('/peak', (req, res) => {
  const total  = stats.peakHour.reduce((a, b) => a + b, 0);
  const max    = Math.max(...stats.peakHour, 1);
  const now    = new Date().getHours();
  const peakIdx = stats.peakHour.indexOf(Math.max(...stats.peakHour));

  res.json({
    hours: stats.peakHour.map((count, i) => ({
      hour:    i,
      label:   String(i).padStart(2, '0') + ':00',
      count,
      pct:     total > 0 ? ((count / total) * 100).toFixed(1) : '0.0',
      bar:     Math.round((count / max) * 20),
      current: i === now,
    })),
    total,
    peakHour:    peakIdx,
    peakLabel:   String(peakIdx).padStart(2, '0') + ':00',
    peakCount:   stats.peakHour[peakIdx],
    currentHour: now,
  });
});

// ── Weather endpoint (used by ESP32 + web app) ────────────────────────
const WEATHER_CACHE_MS = 10 * 60 * 1000;

const WMO_CODES = {
  0: 'Clear Sky', 1: 'Mainly Clear', 2: 'Partly Cloudy', 3: 'Overcast',
  45: 'Fog', 48: 'Icy Fog',
  51: 'Light Drizzle', 53: 'Moderate Drizzle', 55: 'Dense Drizzle',
  61: 'Slight Rain', 63: 'Moderate Rain', 65: 'Heavy Rain',
  71: 'Slight Snow', 73: 'Moderate Snow', 75: 'Heavy Snow', 77: 'Snow Grains',
  80: 'Slight Showers', 81: 'Moderate Showers', 82: 'Violent Showers',
  85: 'Slight Snow Showers', 86: 'Heavy Snow Showers',
  95: 'Thunderstorm', 96: 'Thunderstorm w/ Hail', 99: 'Thunderstorm w/ Heavy Hail',
};

function windCardinal(deg) {
  const dirs = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
  return dirs[Math.round(deg / 45) % 8];
}

app.get('/weather', async (req, res) => {
  const { lat, lon } = req.query;
  if (!lat || !lon) return res.status(400).json({ error: 'lat and lon required' });

  const key = `weather:${lat},${lon}`;
  const now = Date.now();
  const hit = cache.get(key);

  if (hit && (now - hit.timestamp) < WEATHER_CACHE_MS) {
    addLog({ type: 'HIT', client: req.headers['x-forwarded-for'] || req.socket.remoteAddress || '?', key });
    return res.json(hit.data);
  }

  const client = req.headers['x-forwarded-for'] || req.socket.remoteAddress || '?';

  if (!inFlight.has(key)) {
    const promise = (async () => {
      const url = `https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}` +
        `&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,` +
        `wind_speed_10m,wind_direction_10m,uv_index&wind_speed_unit=kmh&timezone=auto`;
      const response = await fetch(url, { signal: AbortSignal.timeout(8000) });
      if (!response.ok) throw new Error(`Open-Meteo returned ${response.status}`);
      const raw = await response.json();
      const c = raw.current;
      const data = {
        temp:            c.temperature_2m,
        feels_like:      c.apparent_temperature,
        humidity:        c.relative_humidity_2m,
        weather_code:    c.weather_code,
        condition:       WMO_CODES[c.weather_code] || 'Unknown',
        wind_speed:      c.wind_speed_10m,
        wind_dir:        c.wind_direction_10m,
        wind_cardinal:   windCardinal(c.wind_direction_10m),
        uv_index:        c.uv_index,
        utc_offset_secs: raw.utc_offset_seconds,
      };
      cache.set(key, { data, timestamp: Date.now() });
      return data;
    })();
    inFlight.set(key, promise);
    promise.finally(() => inFlight.delete(key)).catch(() => {});
  }

  try {
    const data = await inFlight.get(key);
    res.json(data);
  } catch (e) {
    if (hit) {
      addLog({ type: 'STALE', client, key });
      return res.json({ ...hit.data, stale: true });
    }
    res.status(502).json({ error: e.message });
  }
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`Proxy + dashboard running on port ${PORT}`);
});
