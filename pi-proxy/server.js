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
const path    = require('path');
const { execSync } = require('child_process');
const nodemailer = require('nodemailer');
const rateLimit  = require('express-rate-limit');

const app      = express();
const PORT     = 3000;
const CACHE_MS         = 10000;
const ROUTE_CACHE_MS   = 30 * 60 * 1000;
const ROUTE_CACHE_FILE = __dirname + '/route-cache.json';

const cache      = new Map();
const inFlight   = new Map();  // key → Promise (dedup concurrent upstream fetches)
const routeCache = new Map();  // callsign -> { dep, arr, timestamp }

// ── .env loader ──────────────────────────────────────────────────────
try {
  const envFile = fs.readFileSync(path.join(__dirname, '.env'), 'utf8');
  for (const line of envFile.split('\n')) {
    const m = line.match(/^\s*([^#=]+?)\s*=\s*(.*?)\s*$/);
    if (m && !process.env[m[1]]) process.env[m[1]] = m[2];
  }
} catch { /* no .env file */ }

// ── Report config ────────────────────────────────────────────────────
const REPORTS_DIR = path.join(__dirname, 'reports');
const HOME_LAT   = parseFloat(process.env.HOME_LAT || '-33.8530');
const HOME_LON   = parseFloat(process.env.HOME_LON || '151.1410');
const TZ         = 'Australia/Sydney';
fs.mkdirSync(REPORTS_DIR, { recursive: true });

// ── Haversine (km) ───────────────────────────────────────────────────
function haversine(lat1, lon1, lat2, lon2) {
  const R = 6371;
  const dLat = (lat2 - lat1) * Math.PI / 180;
  const dLon = (lon2 - lon1) * Math.PI / 180;
  const a = Math.sin(dLat / 2) ** 2 +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(dLon / 2) ** 2;
  return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

// ── Airline ICAO prefix → name ───────────────────────────────────────
const AIRLINE_NAMES = {
  QFA:'Qantas',QLK:'QantasLink',VOZ:'Virgin Australia',JST:'Jetstar',
  RXA:'Rex',UAE:'Emirates',ETD:'Etihad',QTR:'Qatar Airways',SIA:'Singapore Airlines',
  ANZ:'Air New Zealand',CPA:'Cathay Pacific',MAS:'Malaysia Airlines',THA:'Thai Airways',
  KAL:'Korean Air',JAL:'JAL',ANA:'ANA',AAL:'American Airlines',UAL:'United Airlines',
  BAW:'British Airways',DAL:'Delta',AFR:'Air France',DLH:'Lufthansa',
  FJI:'Fiji Airways',EVA:'EVA Air',CCA:'Air China',CSN:'China Southern',CES:'China Eastern',
  HAL:'Hawaiian Airlines',AIC:'Air India',TGW:'Scoot',LAN:'LATAM',
  CHH:'Hainan Airlines',CXA:'Xiamen Air',CEB:'Cebu Pacific',
  VJC:'VietJet',ASA:'Alaska Airlines',KLM:'KLM',ANG:'Air Niugini',
  CAL:'China Airlines',ALK:'SriLankan',NWL:'Network Aviation',UTY:'Alliance Airlines',
  FRE:'FlyPelican',MXD:'Batik Air',ACI:'Aircalin',
};

function airlineName(callsign) {
  if (!callsign) return null;
  const prefix = callsign.replace(/[0-9]/g, '').substring(0, 3).toUpperCase();
  return AIRLINE_NAMES[prefix] || prefix;
}

function airlinePrefix(callsign) {
  if (!callsign) return '???';
  return callsign.replace(/[0-9]/g, '').substring(0, 3).toUpperCase();
}

// ── Daily flight log ─────────────────────────────────────────────────
let todayFlights = {};
let todayHourly  = new Array(24).fill(0);
let todayDate    = '';
let emailSentToday = false;
let lastSaveTime   = 0;

function aestNow() {
  return new Date(new Date().toLocaleString('en-US', { timeZone: TZ }));
}

function dateStr(d) {
  return d.getFullYear() + '-' + String(d.getMonth() + 1).padStart(2, '0') + '-' + String(d.getDate()).padStart(2, '0');
}

function todayFilePath(ds) {
  return path.join(REPORTS_DIR, `flights-${ds}.json`);
}

function logFlights(acArray) {
  const now  = aestNow();
  const ds   = dateStr(now);
  const hour = now.getHours();
  const iso  = new Date().toISOString();

  if (ds !== todayDate) {
    if (todayDate) saveTodayLog(todayDate);
    todayFlights = {};
    todayHourly  = new Array(24).fill(0);
    todayDate    = ds;
    emailSentToday = false;
    loadTodayLog(ds);
  }

  for (const ac of acArray) {
    const cs = (ac.flight || '').trim();
    if (!cs) continue;

    const dist = (ac.lat != null && ac.lon != null)
      ? haversine(HOME_LAT, HOME_LON, ac.lat, ac.lon)
      : null;

    if (todayFlights[cs]) {
      const f = todayFlights[cs];
      f.lastSeen = iso;
      if (ac.alt_baro != null && ac.alt_baro !== 'ground') {
        f.minAlt = Math.min(f.minAlt ?? Infinity, ac.alt_baro);
        f.maxAlt = Math.max(f.maxAlt ?? 0, ac.alt_baro);
      }
      if (dist != null) f.minDist = Math.min(f.minDist ?? Infinity, dist);
      if (ac.gs != null) f.maxGs = Math.max(f.maxGs ?? 0, ac.gs);
    } else {
      todayHourly[hour]++;
      todayFlights[cs] = {
        callsign: cs,
        type:     ac.t || ac.desc || null,
        reg:      ac.r || null,
        operator: ac.ownOp || airlineName(cs),
        dep:      ac.dep || null,
        arr:      ac.arr || null,
        route:    ac.route || null,
        firstSeen: iso,
        lastSeen:  iso,
        minAlt:   (ac.alt_baro != null && ac.alt_baro !== 'ground') ? ac.alt_baro : null,
        maxAlt:   (ac.alt_baro != null && ac.alt_baro !== 'ground') ? ac.alt_baro : null,
        minDist:  dist,
        maxGs:    ac.gs || null,
      };
    }
  }
}

function buildSummary() {
  const total = Object.keys(todayFlights).length;
  const peakIdx = todayHourly.indexOf(Math.max(...todayHourly));
  return {
    totalUnique:   total,
    peakHour:      peakIdx,
    peakHourCount: todayHourly[peakIdx] || 0,
    hourly:        [...todayHourly],
  };
}

function saveTodayLog(ds) {
  ds = ds || todayDate;
  if (!ds) return;
  const data = { date: ds, summary: buildSummary(), flights: todayFlights };
  fs.writeFile(todayFilePath(ds), JSON.stringify(data, null, 2), () => {});
}

function loadTodayLog(ds) {
  try {
    const raw = JSON.parse(fs.readFileSync(todayFilePath(ds), 'utf8'));
    todayFlights = raw.flights || {};
    todayHourly  = raw.summary?.hourly || new Array(24).fill(0);
    console.log(`Loaded ${Object.keys(todayFlights).length} flights from ${ds}`);
  } catch { /* no file yet */ }
}

// Initialize today's state on startup
todayDate = dateStr(aestNow());
loadTodayLog(todayDate);

// ICAO and IATA codes → city name (same DB as web app)
const AIRPORT_DB = {
  // Australia
  YSSY:'Sydney',YSAY:'Sydney',YMLB:'Melbourne',YMML:'Melbourne',YBBN:'Brisbane',
  YPPH:'Perth',YPAD:'Adelaide',YSCB:'Canberra',YBCS:'Cairns',YBHM:'Hamilton Is',
  YBTL:'Townsville',YBAS:'Alice Springs',YBDG:'Bendigo',YDBY:'Derby',
  YSNF:'Norfolk Is',YAGD:'Aganda',
  YBCG:'Gold Coast',YBHI:'Broken Hill',YBMC:'Sunshine Coast',YBMK:'Mackay',
  YBNA:'Ballina',YBPN:'Proserpine',YBSU:'Bundaberg',
  YCFS:'Coffs Harbour',YCOM:'Cooma',YGTH:'Griffith',YLHI:'Lord Howe Is',
  YMAV:'Avalon',YMAY:'Albury',YMER:'Merimbula',YMHB:'Hobart',YMLT:'Launceston',
  YMRY:'Moruya',YNAR:'Narrandera',YORG:'Orange',YPDN:'Darwin',YPKS:'Parkes',
  YPMQ:'Port Macquarie',YSDU:'Dubbo',YSTW:'Tamworth',YSWG:'Wagga Wagga',
  YWLM:'Newcastle',YAYE:'Uluru',
  SYD:'Sydney',MEL:'Melbourne',BNE:'Brisbane',PER:'Perth',ADL:'Adelaide',
  CBR:'Canberra',CNS:'Cairns',OOL:'Gold Coast',TSV:'Townsville',DRW:'Darwin',
  HBA:'Hobart',LST:'Launceston',MKY:'Mackay',ROK:'Rockhampton',
  // Asia Pacific
  NZAA:'Auckland',NZCH:'Christchurch',NZWN:'Wellington',NZQN:'Queenstown',NZHN:'Hamilton',
  AKL:'Auckland',CHC:'Christchurch',WLG:'Wellington',ZQN:'Queenstown',
  WSSS:'Singapore',WSAP:'Singapore',SIN:'Singapore',
  VHHH:'Hong Kong',HKG:'Hong Kong',
  RJAA:'Tokyo',RJTT:'Tokyo',NRT:'Tokyo',HND:'Tokyo',
  RJBB:'Osaka',RJOO:'Osaka',KIX:'Osaka',ITM:'Osaka',
  RJCC:'Sapporo',
  RKSI:'Seoul',RKSS:'Seoul',ICN:'Seoul',GMP:'Seoul',
  RCTP:'Taipei',RCSS:'Taipei',TPE:'Taipei',TSA:'Taipei',
  VTBS:'Bangkok',VTBD:'Bangkok',BKK:'Bangkok',DMK:'Bangkok',
  VTSP:'Phuket',
  WMKK:'Kuala Lumpur',KUL:'Kuala Lumpur',
  WADD:'Bali',WIII:'Jakarta',DPS:'Bali',CGK:'Jakarta',
  RPLL:'Manila',RPLC:'Clark',MNL:'Manila',
  VVTS:'Ho Chi Minh',VVNB:'Hanoi',SGN:'Ho Chi Minh',HAN:'Hanoi',
  VCBI:'Colombo',
  ZBAA:'Beijing',ZGSZ:'Shenzhen',ZGGG:'Guangzhou',ZSPD:'Shanghai',
  ZSSS:'Shanghai',PEK:'Beijing',SZX:'Shenzhen',CAN:'Guangzhou',PVG:'Shanghai',SHA:'Shanghai',
  ZUCK:'Chongqing',ZUUU:'Chengdu',CKG:'Chongqing',CTU:'Chengdu',
  ZBTJ:'Tianjin',ZHCC:'Zhengzhou',ZHHH:'Wuhan',ZJHK:'Haikou',ZLXY:"Xi'an",
  ZSAM:'Xiamen',ZSHC:'Hangzhou',ZSJN:'Jinan',ZSNJ:'Nanjing',ZUTF:'Hefei',ZYTX:'Shenyang',
  OMDB:'Dubai',OMDW:'Dubai',DXB:'Dubai',DWC:'Dubai',
  OMAA:'Abu Dhabi',AUH:'Abu Dhabi',
  OTBD:'Doha',OTHH:'Doha',DOH:'Doha',
  OERK:'Riyadh',OEDF:'Dammam',RUH:'Riyadh',DMM:'Dammam',
  OJAM:'Amman',LLBG:'Tel Aviv',AMM:'Amman',TLV:'Tel Aviv',
  VIDP:'Delhi',VABB:'Mumbai',VOCB:'Chennai',VOBL:'Bangalore',
  DEL:'Delhi',BOM:'Mumbai',MAA:'Chennai',BLR:'Bangalore',
  VOMM:'Chennai',VECC:'Kolkata',CCU:'Kolkata',
  // Pacific
  AYPY:'Port Moresby',NFFN:'Nadi',NVVV:'Port Vila',NWWW:'Noumea',
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
  KIAH:'Houston',KOAK:'Oakland',KONT:'Ontario',KPDX:'Portland',KSDF:'Louisville',
  PHNL:'Honolulu',
  CYYZ:'Toronto',CYVR:'Vancouver',CYUL:'Montreal',
  YYZ:'Toronto',YVR:'Vancouver',YUL:'Montreal',
  SBGR:'São Paulo',SBGL:'Rio de Janeiro',
  GRU:'São Paulo',GIG:'Rio de Janeiro',
  SAEZ:'Buenos Aires',EZE:'Buenos Aires',
  SCEL:'Santiago',SCL:'Santiago',
  SKBO:'Bogotá',BOG:'Bogotá',
  SEQM:'Quito',UIO:'Quito',
  MMMX:'Mexico City',MMGL:'Guadalajara',MEX:'Mexico City',GDL:'Guadalajara',
  MGGT:'Guatemala City',GUA:'Guatemala City',
  MTPP:'Port-au-Prince',PAP:'Port-au-Prince',
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

// ── Security helpers ─────────────────────────────────────────────────
function validateCoord(lat, lon, radius) {
  const la = parseFloat(lat), lo = parseFloat(lon);
  if (isNaN(la) || la < -90 || la > 90) return 'invalid lat';
  if (isNaN(lo) || lo < -180 || lo > 180) return 'invalid lon';
  if (radius !== undefined) {
    const r = parseFloat(radius);
    if (isNaN(r) || r < 1 || r > 500) return 'invalid radius';
  }
  return null;
}

function escapeHtml(str) {
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function requireAdmin(req, res) {
  const token = process.env.ADMIN_TOKEN;
  if (!token) return false; // no token configured = open (backwards compat)
  const auth = req.headers.authorization;
  if (auth !== `Bearer ${token}`) {
    res.status(401).json({ error: 'Unauthorized' });
    return true; // blocked
  }
  return false; // allowed
}

// ── Rate limiting ────────────────────────────────────────────────────
app.use(rateLimit({
  windowMs: 60 * 1000,
  max: 100,
  standardHeaders: true,
  legacyHeaders: false,
  keyGenerator: (req) => req.headers['cf-connecting-ip'] || req.headers['x-forwarded-for'] || req.ip,
}));

// ── CORS ─────────────────────────────────────────────────────────────
const ALLOWED_ORIGINS = [
  'https://overheadtracker.com',
  'https://greystoke1337.github.io',
];
app.use((req, res, next) => {
  const origin = req.get('origin');
  if (ALLOWED_ORIGINS.includes(origin)) {
    res.header('Access-Control-Allow-Origin', origin);
  } else if (!origin) {
    // Allow non-browser requests (ESP32, curl, display.py)
    res.header('Access-Control-Allow-Origin', '*');
  }
  res.header('X-Frame-Options', 'DENY');
  res.header('X-Content-Type-Options', 'nosniff');
  res.header('Strict-Transport-Security', 'max-age=31536000');
  next();
});

// ── Toggle endpoint ───────────────────────────────────────────────────
app.post('/proxy/toggle', (req, res) => {
  if (requireAdmin(req, res)) return;
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
  <a href="/report" style="display:inline-block;margin-left:16px;color:#ffa600;font-size:0.8rem;opacity:0.6;text-decoration:none;border:1px solid #7a5200;padding:4px 12px">VIEW REPORT</a>

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
  const coordErr = validateCoord(lat, lon, radius);
  if (coordErr) return res.status(400).json({ error: coordErr });

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
      logFlights(data.ac || []);
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
    res.status(502).json({ error: 'Flight data temporarily unavailable' });
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
  const coordErr = validateCoord(lat, lon);
  if (coordErr) return res.status(400).json({ error: coordErr });

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
    res.status(502).json({ error: 'Weather data temporarily unavailable' });
  }
});

// ── Report generation ─────────────────────────────────────────────────
function loadDayData(ds) {
  try {
    return JSON.parse(fs.readFileSync(todayFilePath(ds), 'utf8'));
  } catch { return null; }
}

function prevDateStr(ds, daysBack) {
  const d = new Date(ds + 'T12:00:00');
  d.setDate(d.getDate() - daysBack);
  return dateStr(d);
}

function generateReport(ds) {
  let data;
  if (ds === todayDate) {
    data = { date: ds, summary: buildSummary(), flights: todayFlights };
  } else {
    data = loadDayData(ds);
  }
  if (!data) return null;

  const flights = Object.values(data.flights);
  const total = flights.length;

  function tally(extractor, limit = 8) {
    const counts = {};
    for (const f of flights) {
      const k = extractor(f);
      if (k) counts[k] = (counts[k] || 0) + 1;
    }
    return Object.entries(counts).sort((a, b) => b[1] - a[1]).slice(0, limit);
  }
  const topAirlines = tally(f => { const p = airlinePrefix(f.callsign); return AIRLINE_NAMES[p] || p; });
  const topTypes    = tally(f => f.type);
  const topRoutes   = tally(f => f.route);

  // Trends
  const yesterday = loadDayData(prevDateStr(ds, 1));
  let avg7 = 0, days7 = 0;
  for (let i = 1; i <= 7; i++) {
    const d = loadDayData(prevDateStr(ds, i));
    if (d) { avg7 += d.summary.totalUnique; days7++; }
  }
  avg7 = days7 > 0 ? Math.round(avg7 / days7) : null;

  const trends = {};
  if (yesterday) {
    const diff = total - yesterday.summary.totalUnique;
    const pct = yesterday.summary.totalUnique > 0
      ? Math.round((diff / yesterday.summary.totalUnique) * 100) : 0;
    trends.vsYesterday = { diff, pct, total: yesterday.summary.totalUnique };
  }
  if (avg7 != null) {
    const diff = total - avg7;
    const pct = avg7 > 0 ? Math.round((diff / avg7) * 100) : 0;
    trends.vs7day = { diff, pct, avg: avg7 };
  }

  // Sort flights by firstSeen
  flights.sort((a, b) => (a.firstSeen || '').localeCompare(b.firstSeen || ''));

  return {
    date: ds,
    total,
    summary: data.summary,
    topAirlines,
    topTypes,
    topRoutes,
    trends,
    flights,
  };
}

function trendArrow(diff, pct) {
  if (diff > 0) return `<span style="color:#60ff90">▲ +${pct}%</span>`;
  if (diff < 0) return `<span style="color:#ff6060">▼ ${pct}%</span>`;
  return `<span style="color:#ffa600">▬ 0%</span>`;
}

function renderReportHTML(report) {
  if (!report) return '<p>No data for this date.</p>';

  const { date, total, summary, topAirlines, topTypes, topRoutes, trends, flights } = report;

  const hourLabels = summary.hourly.map((c, i) =>
    `<td style="text-align:center;padding:2px 4px;${i === summary.peakHour ? 'color:#60ff90;font-weight:bold' : 'opacity:0.7'}">${c}</td>`
  ).join('');
  const hourHeaders = summary.hourly.map((_, i) =>
    `<td style="text-align:center;padding:2px 4px;opacity:0.4;font-size:0.65rem">${String(i).padStart(2, '0')}</td>`
  ).join('');

  const maxHourly = Math.max(...summary.hourly, 1);
  const hourBars = summary.hourly.map((c, i) => {
    const h = Math.round((c / maxHourly) * 30);
    return `<td style="vertical-align:bottom;text-align:center;padding:0 1px">` +
      `<div style="width:12px;height:${h}px;background:${i === summary.peakHour ? '#60ff90' : '#ffa600'};opacity:${c > 0 ? 0.8 : 0.15};margin:0 auto"></div></td>`;
  }).join('');

  const trendYesterday = trends.vsYesterday
    ? `vs yesterday (${trends.vsYesterday.total}): ${trendArrow(trends.vsYesterday.diff, trends.vsYesterday.pct)}`
    : '<span style="opacity:0.4">no previous data</span>';
  const trend7day = trends.vs7day
    ? `vs 7-day avg (${trends.vs7day.avg}): ${trendArrow(trends.vs7day.diff, trends.vs7day.pct)}`
    : '<span style="opacity:0.4">insufficient data</span>';

  const tableRows = flights.map(f => {
    const time = f.firstSeen ? new Date(f.firstSeen).toLocaleTimeString('en-AU', { timeZone: TZ, hour: '2-digit', minute: '2-digit' }) : '—';
    const alt = f.minAlt != null ? `${Math.round(f.minAlt)}` : '—';
    const dist = f.minDist != null ? `${f.minDist.toFixed(1)}` : '—';
    return `<tr style="border-bottom:1px solid rgba(255,166,0,0.1)">
      <td style="padding:4px 8px">${escapeHtml(f.callsign)}</td>
      <td style="padding:4px 8px;opacity:0.7">${escapeHtml(f.operator || '—')}</td>
      <td style="padding:4px 8px;opacity:0.7">${escapeHtml(f.type || '—')}</td>
      <td style="padding:4px 8px;opacity:0.7">${escapeHtml(f.route || '—')}</td>
      <td style="padding:4px 8px;opacity:0.7">${time}</td>
      <td style="padding:4px 8px;opacity:0.7">${alt} ft</td>
      <td style="padding:4px 8px;opacity:0.7">${dist} km</td>
    </tr>`;
  }).join('');

  const rankRows = (items, label) => items.map(([name, count]) =>
    `<tr><td style="padding:3px 8px">${escapeHtml(name)}</td><td style="padding:3px 8px;text-align:right">${count}</td></tr>`
  ).join('');

  return `
  <div style="font-family:'Courier New',monospace;background:#1a0a00;color:#ffa600;padding:24px;max-width:900px;margin:0 auto">
    <h1 style="font-size:1.3rem;margin:0 0 4px;text-shadow:0 0 8px #ff8000">DAILY AIR TRAFFIC REPORT</h1>
    <p style="opacity:0.5;margin:0 0 20px;font-size:0.85rem">${date} · Russell Lea, Sydney · 15 km geofence</p>

    <div style="display:flex;gap:16px;flex-wrap:wrap;margin-bottom:20px">
      <div style="border:1px solid #7a5200;padding:12px 16px;flex:1;min-width:140px;background:rgba(255,166,0,0.03)">
        <div style="opacity:0.5;font-size:0.7rem">TOTAL FLIGHTS</div>
        <div style="font-size:1.8rem;text-shadow:0 0 8px #ff8000">${total}</div>
      </div>
      <div style="border:1px solid #7a5200;padding:12px 16px;flex:1;min-width:140px;background:rgba(255,166,0,0.03)">
        <div style="opacity:0.5;font-size:0.7rem">PEAK HOUR</div>
        <div style="font-size:1.8rem;text-shadow:0 0 8px #ff8000">${String(summary.peakHour).padStart(2, '0')}:00</div>
        <div style="opacity:0.5;font-size:0.7rem">${summary.peakHourCount} flights</div>
      </div>
      <div style="border:1px solid #7a5200;padding:12px 16px;flex:2;min-width:200px;background:rgba(255,166,0,0.03)">
        <div style="opacity:0.5;font-size:0.7rem">TRENDS</div>
        <div style="font-size:0.85rem;margin-top:4px">${trendYesterday}</div>
        <div style="font-size:0.85rem;margin-top:2px">${trend7day}</div>
      </div>
    </div>

    <div style="margin-bottom:20px;overflow-x:auto">
      <div style="opacity:0.5;font-size:0.72rem;margin-bottom:6px">▸ HOURLY DISTRIBUTION</div>
      <table style="border-collapse:collapse;width:100%"><tr>${hourBars}</tr><tr>${hourLabels}</tr><tr>${hourHeaders}</tr></table>
    </div>

    <div style="display:flex;gap:16px;flex-wrap:wrap;margin-bottom:20px">
      <div style="flex:1;min-width:200px">
        <div style="opacity:0.5;font-size:0.72rem;margin-bottom:6px">▸ TOP AIRLINES</div>
        <table style="border-collapse:collapse;width:100%;border:1px solid #7a5200">${rankRows(topAirlines, 'Airline')}</table>
      </div>
      <div style="flex:1;min-width:200px">
        <div style="opacity:0.5;font-size:0.72rem;margin-bottom:6px">▸ TOP AIRCRAFT</div>
        <table style="border-collapse:collapse;width:100%;border:1px solid #7a5200">${rankRows(topTypes, 'Type')}</table>
      </div>
      <div style="flex:1;min-width:200px">
        <div style="opacity:0.5;font-size:0.72rem;margin-bottom:6px">▸ TOP ROUTES</div>
        <table style="border-collapse:collapse;width:100%;border:1px solid #7a5200">${rankRows(topRoutes, 'Route')}</table>
      </div>
    </div>

    <div style="opacity:0.5;font-size:0.72rem;margin-bottom:6px">▸ ALL FLIGHTS (${total})</div>
    <div style="overflow-x:auto">
      <table style="border-collapse:collapse;width:100%;border:1px solid #7a5200;font-size:0.8rem">
        <tr style="opacity:0.5;border-bottom:1px solid #7a5200">
          <th style="padding:4px 8px;text-align:left">Callsign</th>
          <th style="padding:4px 8px;text-align:left">Airline</th>
          <th style="padding:4px 8px;text-align:left">Type</th>
          <th style="padding:4px 8px;text-align:left">Route</th>
          <th style="padding:4px 8px;text-align:left">Time</th>
          <th style="padding:4px 8px;text-align:left">Alt</th>
          <th style="padding:4px 8px;text-align:left">Dist</th>
        </tr>
        ${tableRows}
      </table>
    </div>

    <p style="opacity:0.3;font-size:0.7rem;margin-top:20px;text-align:center">
      Generated by Overhead Tracker · overheadtracker.com
    </p>
  </div>`;
}

// ── Email ─────────────────────────────────────────────────────────────
async function sendDailyEmail(ds) {
  const smtpHost = process.env.SMTP_HOST;
  const smtpUser = process.env.SMTP_USER;
  const smtpPass = process.env.SMTP_PASS;
  const reportTo = process.env.REPORT_TO;
  if (!smtpHost || !smtpUser || !smtpPass || !reportTo) {
    console.log('Email not configured — skipping daily report email');
    return;
  }

  const report = generateReport(ds);
  if (!report || report.total === 0) {
    console.log(`No flights for ${ds} — skipping email`);
    return;
  }

  const html = renderReportHTML(report);
  const transport = nodemailer.createTransport({
    host: smtpHost,
    port: parseInt(process.env.SMTP_PORT || '587'),
    secure: false,
    auth: { user: smtpUser, pass: smtpPass },
  });

  try {
    await transport.sendMail({
      from: process.env.REPORT_FROM || `Overhead Tracker <${smtpUser}>`,
      to: reportTo,
      subject: `Air Traffic Report — ${ds} — ${report.total} flights`,
      html,
    });
    console.log(`Daily report email sent for ${ds}`);
    addLog({ type: 'SYS', client: 'system', key: `email sent: ${ds} (${report.total} flights)` });
  } catch (e) {
    console.error('Failed to send daily email:', e.message);
    addLog({ type: 'ERR', client: 'system', error: `email failed: ${e.message}` });
  }
}

// ── /report endpoint ──────────────────────────────────────────────────
app.get('/report', (req, res) => {
  const ds = req.query.date || todayDate;
  if (!/^\d{4}-\d{2}-\d{2}$/.test(ds)) return res.status(400).json({ error: 'Invalid date format' });

  const report = generateReport(ds);

  if (req.query.format === 'json') return res.json(report || { error: 'No data' });

  const nav = `<div style="font-family:'Courier New',monospace;background:#1a0a00;padding:12px 24px;text-align:center">
    <a href="/report?date=${prevDateStr(ds, 1)}" style="color:#ffa600;text-decoration:none;margin-right:20px">◀ prev day</a>
    <a href="/report" style="color:#ffa600;text-decoration:none;margin-right:20px">today</a>
    <a href="/report?date=${prevDateStr(ds, -1)}" style="color:#ffa600;text-decoration:none">next day ▶</a>
    <span style="margin-left:20px;opacity:0.4;color:#ffa600">|</span>
    <a href="/" style="color:#ffa600;text-decoration:none;margin-left:20px;opacity:0.6">dashboard</a>
  </div>`;

  res.send(`<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
    <title>Report ${ds} — Overhead Tracker</title>
    <style>body{margin:0;background:#1a0a00}</style></head>
    <body>${nav}${renderReportHTML(report)}</body></html>`);
});

// ── /report/send — manual trigger for testing ─────────────────────────
app.post('/report/send', async (req, res) => {
  if (requireAdmin(req, res)) return;
  const ds = req.query.date || todayDate;
  await sendDailyEmail(ds);
  res.json({ ok: true, date: ds });
});

// ── Periodic save + day rollover + email scheduler ────────────────────
setInterval(() => {
  const now = aestNow();
  const ds  = dateStr(now);

  // Day rollover
  if (ds !== todayDate) {
    saveTodayLog(todayDate);
    todayFlights = {};
    todayHourly  = new Array(24).fill(0);
    todayDate    = ds;
    emailSentToday = false;
    loadTodayLog(ds);
    console.log(`Day rolled over to ${ds}`);
  }

  // Periodic save (every 60s)
  saveTodayLog();

  // Send email at 23:55 AEST
  if (now.getHours() === 23 && now.getMinutes() >= 55 && !emailSentToday) {
    emailSentToday = true;
    sendDailyEmail(ds);
  }
}, 60000);

app.listen(PORT, '0.0.0.0', () => {
  console.log(`Proxy + dashboard running on port ${PORT}`);
});
