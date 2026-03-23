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
const rateLimit  = require('express-rate-limit');

const app      = express();
const PORT     = parseInt(process.env.PORT, 10) || 3000;
const CACHE_MS         = 45000;
const ROUTE_CACHE_MS   = 30 * 60 * 1000;
const ROUTE_CACHE_FILE = process.env.ROUTE_CACHE_FILE || __dirname + '/route-cache.json';
const KNOWN_ROUTES_FILE = process.env.KNOWN_ROUTES_FILE || __dirname + '/known-routes.json';
const MAX_CACHE_ENTRIES    = 500;   // evict oldest when exceeded
const MAX_ROUTE_ENTRIES    = 5000;  // cap route cache size
const MAX_KNOWN_ROUTES     = 20000; // cap known routes file
const MAX_UPSTREAM_CONCURRENT = 20; // max simultaneous upstream API calls
const SEMAPHORE_TIMEOUT_MS    = 10000; // fail fast if queued too long
const MAX_ROUTE_CONCURRENT    = 5;  // max simultaneous route lookups

const cache        = new Map();
const inFlight     = new Map();  // key → Promise (dedup concurrent upstream fetches)
const routeCache   = new Map();  // callsign -> { dep, arr, timestamp }
const apiCooldowns = new Map();  // apiName → cooldownExpiresAt (timestamp)
const knownRoutes  = new Map();  // "DEP>ARR" → firstSeen date string
let knownRoutesDirty = false;
let todayNewRoutes = [];         // [{ route, callsign, time }] — new routes discovered today
const COOLDOWN_MS  = 60000;
let apiRoundRobin  = 0;         // rotates across upstream APIs to spread load

// ── Concurrency semaphore ───────────────────────────────────────────
function semaphore(max) {
  let active = 0;
  const queue = [];
  return {
    get active() { return active; },
    async acquire(timeoutMs) {
      if (active < max) { active++; return; }
      if (timeoutMs) {
        const acquired = await new Promise(resolve => {
          const entry = { resolve, done: false };
          const timer = setTimeout(() => { entry.done = true; resolve(false); }, timeoutMs);
          queue.push((ok) => { clearTimeout(timer); if (!entry.done) { entry.done = true; resolve(ok !== false); } });
        });
        if (!acquired) throw new Error('Semaphore timeout');
      } else {
        await new Promise(resolve => queue.push(() => resolve()));
      }
      active++;
    },
    release() {
      active--;
      if (queue.length > 0) queue.shift()();
    },
  };
}

const upstreamSem = semaphore(MAX_UPSTREAM_CONCURRENT);
const routeSem    = semaphore(MAX_ROUTE_CONCURRENT);

// ── Geo-bucketing (round coords so nearby clients share cache) ───────
function bucketCoord(v, decimals) { return Number(Number(v).toFixed(decimals)); }
function bucketKey(lat, lon, radius) {
  return `${bucketCoord(lat, 2)},${bucketCoord(lon, 2)},${radius}`;
}

// ── Cache eviction (LRU-like: evict oldest by timestamp) ────────────
function cacheSet(key, entry) {
  cache.set(key, entry);
  if (cache.size > MAX_CACHE_ENTRIES) {
    let oldestKey = null, oldestTs = Infinity;
    for (const [k, v] of cache) {
      if (v.timestamp < oldestTs) { oldestTs = v.timestamp; oldestKey = k; }
    }
    if (oldestKey) cache.delete(oldestKey);
  }
}

function routeCacheSet(key, entry) {
  routeCache.set(key, entry);
  if (routeCache.size > MAX_ROUTE_ENTRIES) {
    let oldestKey = null, oldestTs = Infinity;
    for (const [k, v] of routeCache) {
      if (v.timestamp < oldestTs) { oldestTs = v.timestamp; oldestKey = k; }
    }
    if (oldestKey) routeCache.delete(oldestKey);
  }
}

// ── .env loader ──────────────────────────────────────────────────────
try {
  const envFile = fs.readFileSync(path.join(__dirname, '.env'), 'utf8');
  for (const line of envFile.split('\n')) {
    const m = line.match(/^\s*([^#=]+?)\s*=\s*(.*?)\s*$/);
    if (m && !process.env[m[1]]) process.env[m[1]] = m[2];
  }
} catch { /* no .env file */ }

// ── Report config ────────────────────────────────────────────────────
const REPORTS_DIR = process.env.REPORTS_DIR || path.join(__dirname, 'reports');
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
  // US domestic — Chicago O'Hare + Midway
  SWA:'Southwest',NKS:'Spirit',FFT:'Frontier',JBU:'JetBlue',
  ENY:'Envoy Air',SKW:'SkyWest',RPA:'Republic',EDV:'Endeavor Air',GJS:'GoJet',
  FDX:'FedEx Express',UPS:'UPS Airlines',GTI:'Atlas Air',
  ACA:'Air Canada',WJA:'WestJet',SCX:'Sun Country',
  PDT:'Piedmont Airlines',PSA:'PSA Airlines',MES:'Mesa Airlines',VOI:'Volaris',
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
let emailSentToday        = false;
let statusEmailSentToday  = false;
let routeEmailSentToday   = false;
let lastSaveTime          = 0;
let lastBackfillDate      = '';
let backfillRunning       = false;
let backfillStats         = { date: '', checked: 0, found: 0 };

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
    if (todayDate) { saveTodayLog(todayDate); saveKnownRoutes(); }
    todayFlights = {};
    todayHourly  = new Array(24).fill(0);
    todayNewRoutes = [];
    todayDate    = ds;
    emailSentToday = false;
    routeEmailSentToday = false;
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
      if (ac.route) checkNewRoute(ac.route, cs, ds);
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
  const data = { date: ds, summary: buildSummary(), flights: todayFlights, newRoutes: todayNewRoutes };
  fs.writeFile(todayFilePath(ds), JSON.stringify(data, null, 2), () => {});
}

function loadTodayLog(ds) {
  try {
    const raw = JSON.parse(fs.readFileSync(todayFilePath(ds), 'utf8'));
    todayFlights = raw.flights || {};
    todayHourly  = raw.summary?.hourly || new Array(24).fill(0);
    todayNewRoutes = raw.newRoutes || [];
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
  // Americas — major hubs
  KJFK:'New York',KLGA:'New York',KEWR:'New York',
  JFK:'New York',LGA:'New York',EWR:'New York',
  KLAX:'Los Angeles',LAX:'Los Angeles',
  KSFO:'San Francisco',SFO:'San Francisco',
  KORD:'Chicago',KMDW:'Chicago',ORD:'Chicago',MDW:'Chicago',
  KBOS:'Boston',BOS:'Boston',
  KMIA:'Miami',MIA:'Miami',
  KATL:'Atlanta',ATL:'Atlanta',
  KDFW:'Dallas',DFW:'Dallas',KDAL:'Dallas',DAL:'Dallas',
  KDEN:'Denver',DEN:'Denver',
  KSEA:'Seattle',SEA:'Seattle',
  KLAS:'Las Vegas',LAS:'Las Vegas',
  KPHX:'Phoenix',PHX:'Phoenix',
  KIAH:'Houston',IAH:'Houston',KHOU:'Houston',HOU:'Houston',
  PHNL:'Honolulu',HNL:'Honolulu',
  // US — Midwest
  KMSP:'Minneapolis',MSP:'Minneapolis',KDTW:'Detroit',DTW:'Detroit',
  KSTL:'St. Louis',STL:'St. Louis',KMKE:'Milwaukee',MKE:'Milwaukee',
  KCMH:'Columbus',CMH:'Columbus',KIND:'Indianapolis',IND:'Indianapolis',
  KCLE:'Cleveland',CLE:'Cleveland',KCVG:'Cincinnati',CVG:'Cincinnati',
  KDSM:'Des Moines',DSM:'Des Moines',KOMA:'Omaha',OMA:'Omaha',
  KGRR:'Grand Rapids',GRR:'Grand Rapids',KRFD:'Rockford',RFD:'Rockford',
  KMCI:'Kansas City',MCI:'Kansas City',KICT:'Wichita',ICT:'Wichita',
  KDAY:'Dayton',DAY:'Dayton',KFWA:'Fort Wayne',FWA:'Fort Wayne',
  KSBN:'South Bend',SBN:'South Bend',KMSN:'Madison',MSN:'Madison',
  KFSD:'Sioux Falls',FSD:'Sioux Falls',KFAR:'Fargo',FAR:'Fargo',
  KGRB:'Green Bay',GRB:'Green Bay',KLSE:'La Crosse',LSE:'La Crosse',
  KEAU:'Eau Claire',EAU:'Eau Claire',KCWA:'Wausau',CWA:'Wausau',
  KPIA:'Peoria',PIA:'Peoria',KSPI:'Springfield',SPI:'Springfield',
  KMLI:'Moline',MLI:'Moline',KCID:'Cedar Rapids',CID:'Cedar Rapids',
  KBMI:'Bloomington',BMI:'Bloomington',KAZO:'Kalamazoo',AZO:'Kalamazoo',
  KLAN:'Lansing',LAN:'Lansing',KFNT:'Flint',FNT:'Flint',
  KMBS:'Saginaw',MBS:'Saginaw',KCAK:'Akron',CAK:'Akron',
  KDLH:'Duluth',DLH:'Duluth',KBIS:'Bismarck',BIS:'Bismarck',
  KRAP:'Rapid City',RAP:'Rapid City',KLNK:'Lincoln',LNK:'Lincoln',
  KSGF:'Springfield MO',SGF:'Springfield MO',KLIT:'Little Rock',LIT:'Little Rock',
  KLEX:'Lexington',LEX:'Lexington',KEVV:'Evansville',EVV:'Evansville',
  // US — East Coast
  KCLT:'Charlotte',CLT:'Charlotte',KPHL:'Philadelphia',PHL:'Philadelphia',
  KIAD:'Washington',IAD:'Washington',KDCA:'Washington',DCA:'Washington',
  KBWI:'Baltimore',BWI:'Baltimore',KPIT:'Pittsburgh',PIT:'Pittsburgh',
  KBUF:'Buffalo',BUF:'Buffalo',KRDU:'Raleigh',RDU:'Raleigh',
  KRIC:'Richmond',RIC:'Richmond',KORF:'Norfolk',ORF:'Norfolk',
  KSYR:'Syracuse',SYR:'Syracuse',KROC:'Rochester',ROC:'Rochester',
  KALB:'Albany',ALB:'Albany',KPVD:'Providence',PVD:'Providence',
  KBDL:'Hartford',BDL:'Hartford',KMHT:'Manchester',MHT:'Manchester',
  KPWM:'Portland ME',PWM:'Portland ME',KBTV:'Burlington',BTV:'Burlington',
  KHPN:'Westchester',HPN:'Westchester',KMDT:'Harrisburg',MDT:'Harrisburg',
  KABE:'Allentown',ABE:'Allentown',KERI:'Erie',ERI:'Erie',
  KGSO:'Greensboro',GSO:'Greensboro',KGSP:'Greenville',GSP:'Greenville',
  KBED:'Bedford',BED:'Bedford',
  // US — Southeast
  KBNA:'Nashville',BNA:'Nashville',KMCO:'Orlando',MCO:'Orlando',
  KFLL:'Ft Lauderdale',FLL:'Ft Lauderdale',KTPA:'Tampa',TPA:'Tampa',
  KMSY:'New Orleans',MSY:'New Orleans',
  KJAX:'Jacksonville',JAX:'Jacksonville',KRSW:'Fort Myers',RSW:'Fort Myers',
  KSRQ:'Sarasota',SRQ:'Sarasota',KEYW:'Key West',EYW:'Key West',
  KBHM:'Birmingham',BHM:'Birmingham',KTYS:'Knoxville',TYS:'Knoxville',
  KMEM:'Memphis',MEM:'Memphis',KCHS:'Charleston',CHS:'Charleston',
  KSAV:'Savannah',SAV:'Savannah',KMYR:'Myrtle Beach',MYR:'Myrtle Beach',
  KCAE:'Columbia',CAE:'Columbia',KILM:'Wilmington NC',ILM:'Wilmington NC',
  KAVL:'Asheville',AVL:'Asheville',KHHH:'Hilton Head',HHH:'Hilton Head',
  KPNS:'Pensacola',PNS:'Pensacola',KVPS:'Destin',VPS:'Destin',
  KECP:'Panama City FL',ECP:'Panama City FL',KPGD:'Punta Gorda',PGD:'Punta Gorda',
  KSAT:'San Antonio',SAT:'San Antonio',KELP:'El Paso',ELP:'El Paso',
  // US — West / Southwest
  KSAN:'San Diego',SAN:'San Diego',KSLC:'Salt Lake City',SLC:'Salt Lake City',
  KAUS:'Austin',AUS:'Austin',KOKC:'Oklahoma City',OKC:'Oklahoma City',
  KTUL:'Tulsa',TUL:'Tulsa',KABQ:'Albuquerque',ABQ:'Albuquerque',
  KSJC:'San Jose',SJC:'San Jose',KSMF:'Sacramento',SMF:'Sacramento',
  KOAK:'Oakland',OAK:'Oakland',KONT:'Ontario',ONT:'Ontario',
  KPDX:'Portland',PDX:'Portland',KSDF:'Louisville',SDF:'Louisville',
  KBUR:'Burbank',BUR:'Burbank',KLGB:'Long Beach',LGB:'Long Beach',
  KSNA:'Orange County',SNA:'Orange County',KPSP:'Palm Springs',PSP:'Palm Springs',
  KBOI:'Boise',BOI:'Boise',KRNO:'Reno',RNO:'Reno',
  KGEG:'Spokane',GEG:'Spokane',KEUG:'Eugene',EUG:'Eugene',
  KFAT:'Fresno',FAT:'Fresno',KCOS:'Colorado Springs',COS:'Colorado Springs',
  KTUS:'Tucson',TUS:'Tucson',KSBA:'Santa Barbara',SBA:'Santa Barbara',
  KMRY:'Monterey',MRY:'Monterey',KBZN:'Bozeman',BZN:'Bozeman',
  KMSO:'Missoula',MSO:'Missoula',KBIL:'Billings',BIL:'Billings',
  KJAC:'Jackson Hole',JAC:'Jackson Hole',KSUN:'Sun Valley',SUN:'Sun Valley',
  KMTJ:'Montrose',MTJ:'Montrose',KHDN:'Steamboat',HDN:'Steamboat',
  KASE:'Aspen',ASE:'Aspen',KGUC:'Gunnison',GUC:'Gunnison',
  KFCA:'Kalispell',FCA:'Kalispell',KIDA:'Idaho Falls',IDA:'Idaho Falls',
  KSGU:'St. George',SGU:'St. George',PANC:'Anchorage',ANC:'Anchorage',
  PHOG:'Maui',OGG:'Maui',PHKO:'Kona',KOA:'Kona',
  // Canada
  CYYZ:'Toronto',CYVR:'Vancouver',CYUL:'Montreal',
  YYZ:'Toronto',YVR:'Vancouver',YUL:'Montreal',
  CYYC:'Calgary',YYC:'Calgary',CYEG:'Edmonton',YEG:'Edmonton',
  CYOW:'Ottawa',YOW:'Ottawa',CYHZ:'Halifax',YHZ:'Halifax',
  CYWG:'Winnipeg',YWG:'Winnipeg',CYQB:'Quebec City',YQB:'Quebec City',
  CYTZ:'Toronto Island',YTZ:'Toronto Island',
  // Mexico
  MMMX:'Mexico City',MEX:'Mexico City',MMGL:'Guadalajara',GDL:'Guadalajara',
  MMUN:'Cancun',CUN:'Cancun',MMPR:'Puerto Vallarta',PVR:'Puerto Vallarta',
  MMSD:'San Jose del Cabo',SJD:'San Jose del Cabo',
  MMMY:'Monterrey',MTY:'Monterrey',MMLO:'Leon',BJX:'Leon',
  MMCZ:'Cozumel',CZM:'Cozumel',MMZH:'Ixtapa',ZIH:'Ixtapa',
  MMMM:'Morelia',MLM:'Morelia',MMQT:'Queretaro',QRO:'Queretaro',
  // Caribbean / Central America
  TJSJ:'San Juan',SJU:'San Juan',MDPC:'Punta Cana',PUJ:'Punta Cana',
  MKJS:'Montego Bay',MBJ:'Montego Bay',MYNN:'Nassau',NAS:'Nassau',
  MWCR:'Grand Cayman',GCM:'Grand Cayman',
  TNCA:'Aruba',AUA:'Aruba',TNCC:'Curacao',CUR:'Curacao',
  TNCM:'Sint Maarten',SXM:'Sint Maarten',
  TIST:'St. Thomas',STT:'St. Thomas',TISX:'St. Croix',STX:'St. Croix',
  TLPL:'St. Lucia',UVF:'St. Lucia',MBPV:'Providenciales',PLS:'Providenciales',
  MROC:'San Jose CR',SJO:'San Jose CR',MRLB:'Liberia CR',LIR:'Liberia CR',
  MPTO:'Panama City',PTY:'Panama City',MZBZ:'Belize City',BZE:'Belize City',
  MSLP:'San Salvador',SAL:'San Salvador',
  MGGT:'Guatemala City',GUA:'Guatemala City',
  MTPP:'Port-au-Prince',PAP:'Port-au-Prince',
  // South America
  SBGR:'São Paulo',SBGL:'Rio de Janeiro',
  GRU:'São Paulo',GIG:'Rio de Janeiro',
  SAEZ:'Buenos Aires',EZE:'Buenos Aires',
  SCEL:'Santiago',SCL:'Santiago',
  SKBO:'Bogotá',BOG:'Bogotá',
  SEQM:'Quito',UIO:'Quito',
  // Europe — additional
  LPPT:'Lisbon',LIS:'Lisbon',LYBE:'Belgrade',BEG:'Belgrade',
  EPKK:'Krakow',KRK:'Krakow',EINN:'Shannon',SNN:'Shannon',
  LIRN:'Naples',NAP:'Naples',
  // Africa
  FAOR:'Johannesburg',FACT:'Cape Town',
  JNB:'Johannesburg',CPT:'Cape Town',
  HECA:'Cairo',CAI:'Cairo',
  DNMM:'Lagos',LOS:'Lagos',
  HAAB:'Addis Ababa',ADD:'Addis Ababa',
  HSSS:'Khartoum',KRT:'Khartoum',
  FMMI:'Antananarivo',TNR:'Antananarivo',
  // UK — regional
  EGCC:'Manchester',EGGP:'Liverpool',EGAA:'Belfast',EGNM:'Leeds Bradford',
  EGBB:'Birmingham',EGGD:'Bristol',EGNS:'Isle of Man',EGGW:'Luton',
  EGAC:'Belfast City',EGNT:'Newcastle',EGNX:'East Midlands',EGHQ:'Newquay',
  EGFF:'Cardiff',EGPK:'Prestwick',EGPD:'Aberdeen',EGAE:'Derry',
  EGHH:'Bournemouth',EGPE:'Inverness',EGPN:'Dundee',
  MAN:'Manchester',LPL:'Liverpool',BFS:'Belfast',BHX:'Birmingham',BRS:'Bristol',
  NCL:'Newcastle',EMA:'East Midlands',CWL:'Cardiff',ABZ:'Aberdeen',INV:'Inverness',
  // Iceland
  BIKF:'Keflavik',KEF:'Keflavik',
  // Ireland — regional
  EIKN:'Knock',NOC:'Knock',
  // Spain / Canaries / Balearics
  LEPA:'Palma de Mallorca',LEAL:'Alicante',LEMG:'Málaga',LEVC:'Valencia',
  LEIB:'Ibiza',LEST:'Santiago de Compostela',
  GCTS:'Tenerife South',GCRR:'Lanzarote',GCFV:'Fuerteventura',GCLP:'Gran Canaria',
  PMI:'Palma de Mallorca',ALC:'Alicante',AGP:'Málaga',VLC:'Valencia',IBZ:'Ibiza',
  TFS:'Tenerife South',ACE:'Lanzarote',FUE:'Fuerteventura',LPA:'Gran Canaria',
  // Portugal — regional
  LPFR:'Faro',LPPR:'Porto',LPMA:'Madeira',
  FAO:'Faro',OPO:'Porto',FNC:'Madeira',
  // France — regional
  LFOB:'Beauvais',LFBH:'La Rochelle',LFMN:'Nice',LFBD:'Bordeaux',
  LFML:'Marseille',LFLB:'Chambéry',
  BVA:'Beauvais',NCE:'Nice',BOD:'Bordeaux',MRS:'Marseille',CMF:'Chambéry',
  // Belgium — regional
  EBCI:'Charleroi',EBOS:'Ostend',EBLG:'Liège',
  CRL:'Charleroi',OST:'Ostend',LGG:'Liège',
  // Netherlands — regional
  EHBK:'Maastricht',EHEH:'Eindhoven',EHRD:'Rotterdam',
  MST:'Maastricht',EIN:'Eindhoven',RTM:'Rotterdam',
  // Luxembourg
  ELLX:'Luxembourg',LUX:'Luxembourg',
  // Germany — regional
  EDDP:'Leipzig',EDDK:'Cologne',EDDH:'Hamburg',EDSB:'Baden-Baden',
  EDDW:'Bremen',EDDB:'Berlin',EDDL:'Düsseldorf',EDDG:'Münster',
  EDDC:'Dresden',EDFH:'Hahn',
  LEJ:'Leipzig',CGN:'Cologne',HAM:'Hamburg',FKB:'Baden-Baden',
  BRE:'Bremen',BER:'Berlin',DUS:'Düsseldorf',FMO:'Münster',DRS:'Dresden',HHN:'Hahn',
  // Poland — regional
  EPWR:'Wrocław',WRO:'Wrocław',
  // Baltic states
  EYKA:'Kaunas',EYVI:'Vilnius',EVRA:'Riga',
  KUN:'Kaunas',VNO:'Vilnius',RIX:'Riga',
  // Denmark — regional
  EKBI:'Billund',BLL:'Billund',
  // Norway — regional
  ENTO:'Sandefjord',TRF:'Sandefjord',
  // Italy — regional
  LIME:'Bergamo',LICJ:'Palermo',LIPE:'Bologna',LIRA:'Rome Ciampino',LIPZ:'Venice',
  BGY:'Bergamo',PMO:'Palermo',BLQ:'Bologna',CIA:'Rome Ciampino',VCE:'Venice',
  // Malta
  LMML:'Malta',MLA:'Malta',
  // Croatia
  LDZA:'Zagreb',ZAG:'Zagreb',
  // Greece — regional
  LGKO:'Kos',LGIR:'Heraklion',LGRP:'Rhodes',LGSA:'Chania',LGTS:'Thessaloniki',
  KGS:'Kos',HER:'Heraklion',RHO:'Rhodes',CHQ:'Chania',SKG:'Thessaloniki',
  // Bulgaria
  LBPD:'Plovdiv',LBSF:'Sofia',PDV:'Plovdiv',SOF:'Sofia',
  // Turkey — regional
  LTBS:'Dalaman',LTAI:'Antalya',LTFJ:'Istanbul Sabiha',
  DLM:'Dalaman',AYT:'Antalya',
  // Gibraltar
  LXGB:'Gibraltar',GIB:'Gibraltar',
  // Moldova
  LUKK:'Chișinău',KIV:'Chișinău',
  // Montenegro
  LYPG:'Podgorica',TGD:'Podgorica',
  // Jordan — regional
  OJAI:'Amman',
  // Egypt — regional
  HESH:'Sharm El Sheikh',SSH:'Sharm El Sheikh',
  // Morocco — regional
  GMMX:'Marrakech',GMAD:'Agadir',RAK:'Marrakech',AGA:'Agadir',
  // Cape Verde
  GVBA:'Boa Vista',GVAC:'Sal',BVC:'Boa Vista',SID:'Sal',
  // Japan — regional
  RJGG:'Nagoya',NGO:'Nagoya',
  // China — regional
  ZSNB:'Ningbo',NGB:'Ningbo',
  // Mexico — regional
  MMTM:'Tampico',MMAS:'Aguascalientes',
  TAM:'Tampico',AGU:'Aguascalientes',
  // Dominican Republic
  MDST:'Santiago DR',MDSD:'Santo Domingo',
  STI:'Santiago DR',SDQ:'Santo Domingo',
  // Caribbean — additional
  MYAM:'Marsh Harbour',TBPB:'Barbados',TNCB:'Bonaire',TJBQ:'Aguadilla',
  MHH:'Marsh Harbour',BGI:'Barbados',BON:'Bonaire',BQN:'Aguadilla',
  // Colombia — regional
  SKCG:'Cartagena',CTG:'Cartagena',
  // Canada — regional
  CYQY:'Sydney NS',CYYG:'Charlottetown',
  YQY:'Sydney NS',YYG:'Charlottetown',
  // US — Northeast additional
  KTEB:'Teterboro',KISP:'Islip',KSWF:'Newburgh',KFOK:'Westhampton',
  KOXC:'Oxford CT',KITH:'Ithaca',KELM:'Elmira',KJST:'Johnstown PA',
  KUNV:'State College',KMMU:'Morristown NJ',KMIV:'Millville NJ',
  KPSM:'Portsmouth NH',KBGR:'Bangor',KPQI:'Presque Isle',
  TEB:'Teterboro',ISP:'Islip',SWF:'Newburgh',ITH:'Ithaca',ELM:'Elmira',
  JST:'Johnstown PA',UNV:'State College',MMU:'Morristown NJ',BGR:'Bangor',PQI:'Presque Isle',
  // US — Southeast additional
  KPBI:'West Palm Beach',KPIE:'St. Petersburg FL',KLCQ:'Lake City FL',
  KHSV:'Huntsville',KABY:'Albany GA',KTRI:'Tri-Cities TN',
  KSHD:'Shenandoah Valley',KCHO:'Charlottesville',KROA:'Roanoke',
  KPIB:'Hattiesburg',KGYH:'Greenville Downtown',KFTY:'Fulton County GA',
  KOAJ:'Jacksonville NC',KMCF:'MacDill AFB',KVRB:'Vero Beach',KSFB:'Sanford FL',
  PBI:'West Palm Beach',PIE:'St. Petersburg FL',HSV:'Huntsville',ABY:'Albany GA',
  TRI:'Tri-Cities TN',SHD:'Shenandoah Valley',CHO:'Charlottesville',ROA:'Roanoke',
  OAJ:'Jacksonville NC',VRB:'Vero Beach',SFB:'Sanford FL',
  // US — South / Texas additional
  KMFE:'McAllen',KBRO:'Brownsville',KHRL:'Harlingen',KAEX:'Alexandria LA',
  KACT:'Waco',KFTW:'Fort Worth Meacham',KAFW:'Fort Worth Alliance',KADS:'Addison TX',
  KCLL:'College Station',KLBB:'Lubbock',KSWO:'Stillwater OK',KROW:'Roswell NM',
  MFE:'McAllen',BRO:'Brownsville',HRL:'Harlingen',AEX:'Alexandria LA',
  ACT:'Waco',AFW:'Fort Worth Alliance',CLL:'College Station',LBB:'Lubbock',ROW:'Roswell NM',
  // US — Midwest additional
  KXNA:'Northwest Arkansas',KATW:'Appleton',KSTP:'St. Paul Downtown',
  KLAF:'Lafayette IN',KUIN:'Quincy IL',KDEC:'Decatur IL',KCMI:'Champaign',
  KSBM:'Sheboygan',KCMX:'Houghton MI',KMCW:'Mason City',KYIP:'Willow Run MI',
  KTVF:'Thief River Falls',KRST:'Rochester MN',KSUX:'Sioux City',
  KBRL:'Burlington IA',KALO:'Waterloo IA',KCGF:'Cuyahoga County',KSTC:'St. Cloud MN',
  XNA:'Northwest Arkansas',ATW:'Appleton',CMI:'Champaign',RST:'Rochester MN',SUX:'Sioux City',
  ALO:'Waterloo IA',
  // US — West additional
  KSTS:'Santa Rosa CA',KACV:'Arcata',KMFR:'Medford',KSBP:'San Luis Obispo',
  KOTH:'North Bend OR',KSBD:'San Bernardino',KPAE:'Paine Field',KVCV:'Victorville',
  KSCK:'Stockton',KRDM:'Redmond OR',KSDL:'Scottsdale',KBFI:'Boeing Field',
  KFHR:'Friday Harbor',KIWA:'Mesa Gateway',KILN:'Wilmington OH',KRIV:'March ARB',
  KBLI:'Bellingham',KCRW:'Charleston WV',KHLN:'Helena MT',KBTM:'Butte MT',
  KMVY:'Martha\'s Vineyard',
  STS:'Santa Rosa CA',ACV:'Arcata',MFR:'Medford',SBP:'San Luis Obispo',
  OTH:'North Bend OR',SBD:'San Bernardino',PAE:'Paine Field',SCK:'Stockton',
  RDM:'Redmond OR',SDL:'Scottsdale',BFI:'Boeing Field',IWA:'Mesa Gateway',
  BLI:'Bellingham',CRW:'Charleston WV',HLN:'Helena MT',BTM:'Butte MT',MVY:'Martha\'s Vineyard',
  // US — Hawaii / Alaska additional
  PHNY:'Lanai',PHLI:'Lihue',PHMK:'Molokai',PAKT:'Ketchikan',PAFA:'Fairbanks',
  LNY:'Lanai',LIH:'Lihue',MKK:'Molokai',KTN:'Ketchikan',FAI:'Fairbanks',
  // US — Military
  KADW:'Andrews AFB',KBKF:'Buckley SFB',KNGU:'Norfolk NAS',KNYL:'Yuma MCAS',
  KHXD:'Hilton Head',
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


function enrichRoutes(data) {
  const unrouted = [];
  for (const ac of (data.ac || [])) {
    const cs = (ac.flight || '').trim();
    if (cs && !ac.dep && !ac.arr) {
      const hit = routeCache.get(cs);
      if (hit) {
        if (hit.dep) ac.dep = hit.dep;
        if (hit.arr) ac.arr = hit.arr;
        if ((Date.now() - hit.timestamp) >= ROUTE_CACHE_MS && !unrouted.includes(cs)) {
          unrouted.push(cs);  // refresh stale entry in background
        }
      } else if (!unrouted.includes(cs)) {
        unrouted.push(cs);
      }
    }
    const dep = ac.dep || ac.orig_iata || null;
    const arr = ac.arr || ac.dest_iata || null;
    const routeStr = formatRouteString(dep, arr);
    if (routeStr) ac.route = routeStr;
  }
  return unrouted;
}

try {
  const saved = JSON.parse(fs.readFileSync(ROUTE_CACHE_FILE, 'utf8'));
  for (const [cs, entry] of Object.entries(saved)) routeCache.set(cs, entry);
  console.log(`Loaded ${routeCache.size} routes from disk cache`);
} catch { /* file absent on first run */ }

// ── Known routes (route discovery tracking) ──────────────────────────
let knownRoutesBootstrapped = false;
try {
  const saved = JSON.parse(fs.readFileSync(KNOWN_ROUTES_FILE, 'utf8'));
  for (const [route, firstSeen] of Object.entries(saved)) knownRoutes.set(route, firstSeen);
  knownRoutesBootstrapped = true;
  console.log(`Loaded ${knownRoutes.size} known routes from disk`);
} catch { /* file absent on first run */ }

function saveKnownRoutes() {
  if (!knownRoutesDirty) return;
  knownRoutesDirty = false;
  const obj = {};
  for (const [route, firstSeen] of knownRoutes) obj[route] = firstSeen;
  fs.writeFile(KNOWN_ROUTES_FILE, JSON.stringify(obj, null, 2), () => {});
}

function checkNewRoute(routeStr, callsign, ds) {
  if (!routeStr || routeStr.includes('?')) return;
  const key = routeStr;
  if (knownRoutes.has(key)) return;
  knownRoutes.set(key, ds);
  knownRoutesDirty = true;
  todayNewRoutes.push({ route: key, callsign, time: new Date().toISOString() });
  // Evict oldest if over cap
  if (knownRoutes.size > MAX_KNOWN_ROUTES) {
    let oldestKey = null, oldestDate = 'Z';
    for (const [k, v] of knownRoutes) {
      if (v < oldestDate) { oldestDate = v; oldestKey = k; }
    }
    if (oldestKey) knownRoutes.delete(oldestKey);
  }
}

// Bootstrap: seed known routes from all existing flight logs on first run
if (!knownRoutesBootstrapped) {
  try {
    const files = fs.readdirSync(REPORTS_DIR).filter(f => f.startsWith('flights-') && f.endsWith('.json')).sort();
    let seeded = 0;
    for (const file of files) {
      try {
        const data = JSON.parse(fs.readFileSync(path.join(REPORTS_DIR, file), 'utf8'));
        const ds = data.date || file.replace('flights-', '').replace('.json', '');
        for (const f of Object.values(data.flights || {})) {
          if (f.route && !f.route.includes('?') && !knownRoutes.has(f.route)) {
            knownRoutes.set(f.route, ds);
            seeded++;
          }
        }
      } catch { /* skip corrupt files */ }
    }
    if (seeded > 0) {
      knownRoutesDirty = true;
      saveKnownRoutes();
      console.log(`Bootstrapped ${seeded} known routes from ${files.length} flight logs`);
    }
  } catch { /* no reports dir yet */ }
}

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

let routeCacheDirty = false;
function saveRouteCache() {
  if (!routeCacheDirty) return;
  routeCacheDirty = false;
  const obj = {};
  for (const [cs, entry] of routeCache) obj[cs] = entry;
  fs.writeFile(ROUTE_CACHE_FILE, JSON.stringify(obj, null, 2), () => {});
}

async function lookupRoute(callsign) {
  const cs  = callsign.trim();
  const hit = routeCache.get(cs);
  if (hit && (Date.now() - hit.timestamp) < ROUTE_CACHE_MS) return hit;

  try { await routeSem.acquire(10000); } catch { return hit || null; }
  try {
    // Re-check after acquiring semaphore (another request may have filled it)
    const hit2 = routeCache.get(cs);
    if (hit2 && (Date.now() - hit2.timestamp) < ROUTE_CACHE_MS) return hit2;

    // adsbdb.com route lookup
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
            routeCacheSet(cs, entry);
            routeCacheDirty = true;
            return entry;
          }
        }
      }
    } catch { /* timeout or network error */ }
  } finally {
    routeSem.release();
  }

  if (hit) return hit;  // stale disk entry as fallback
  return null;
}

function cpuTemp() {
  try {
    const raw = fs.readFileSync('/sys/class/thermal/thermal_zone0/temp', 'utf8');
    return (parseInt(raw) / 1000).toFixed(1) + '°C';
  } catch { return 'N/A'; }
}

function diskStats() {
  try {
    const files = fs.readdirSync(REPORTS_DIR);
    let totalBytes = 0;
    for (const f of files) {
      try { totalBytes += fs.statSync(path.join(REPORTS_DIR, f)).size; } catch {}
    }
    const routeBytes = (() => { try { return fs.statSync(ROUTE_CACHE_FILE).size; } catch { return 0; } })();
    const knownBytes = (() => { try { return fs.statSync(KNOWN_ROUTES_FILE).size; } catch { return 0; } })();
    return { reportFiles: files.length, reportsMB: (totalBytes / 1048576).toFixed(1), routeCacheKB: (routeBytes / 1024).toFixed(1), knownRoutesKB: (knownBytes / 1024).toFixed(1) };
  } catch { return { reportFiles: 0, reportsMB: '0.0', routeCacheKB: '0.0', knownRoutesKB: '0.0' }; }
}

let pm2Cache = { data: [], timestamp: 0 };
async function pm2Status() {
  if (Date.now() - pm2Cache.timestamp < 5000) return pm2Cache.data;
  try {
    const { execFile } = require('child_process');
    const out = await new Promise((resolve, reject) => {
      execFile('pm2', ['jlist'], { timeout: 3000 }, (err, stdout) => {
        if (err) reject(err); else resolve(stdout);
      });
    });
    pm2Cache.data = JSON.parse(out).map(p => ({
      name:     p.name,
      status:   p.pm2_env.status,
      uptime:   p.pm2_env.pm_uptime
        ? Math.floor((Date.now() - p.pm2_env.pm_uptime) / 1000)
        : null,
      restarts: p.pm2_env.restart_time,
    }));
    pm2Cache.timestamp = Date.now();
    return pm2Cache.data;
  } catch { return pm2Cache.data; }
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
  if (!token) {
    res.status(403).json({ error: 'Admin access disabled (ADMIN_TOKEN not configured)' });
    return true; // blocked — deny by default when no token is set
  }
  const auth = req.headers.authorization;
  if (auth !== `Bearer ${token}`) {
    res.status(401).json({ error: 'Unauthorized' });
    return true; // blocked
  }
  return false; // allowed
}

// ── Rate limiting ────────────────────────────────────────────────────
// ── CORS (must be before rate limiter so preflights always get headers) ──
const ALLOWED_ORIGINS = [
  'https://overheadtracker.com',
  'https://www.overheadtracker.com',
  'https://greystoke1337.github.io',
];
app.use((req, res, next) => {
  const origin = req.get('origin');
  if (ALLOWED_ORIGINS.includes(origin)) {
    res.header('Access-Control-Allow-Origin', origin);
    res.header('Access-Control-Allow-Methods', 'GET, OPTIONS');
    res.header('Access-Control-Allow-Headers', 'Content-Type');
    res.header('Access-Control-Max-Age', '86400');
  }
  // Non-browser requests (ESP32, curl, display.py) have no Origin header
  // and are not subject to CORS — no Access-Control header needed
  res.header('X-Frame-Options', 'DENY');
  res.header('X-Content-Type-Options', 'nosniff');
  res.header('Strict-Transport-Security', 'max-age=31536000');
  if (req.method === 'OPTIONS') return res.sendStatus(204);
  next();
});

app.use(rateLimit({
  windowMs: 60 * 1000,
  max: 100,
  standardHeaders: true,
  legacyHeaders: false,
  keyGenerator: (req) => req.headers['cf-connecting-ip'] || req.headers['x-forwarded-for'] || req.ip,
}));

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
app.get('/status', async (req, res) => {
  res.json({
    proxyEnabled,
    uptime:  os.uptime(),
    temp:    cpuTemp(),
    loadAvg: os.loadavg(),
    ram:     { total: os.totalmem(), free: os.freemem() },
    network: networkInfo(),
    pm2:     await pm2Status(),
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

  const key = bucketKey(lat, lon, radius);
  const now  = Date.now();
  const hit  = cache.get(key);

  stats.totalRequests++;
  stats.peakHour[new Date().getHours()]++;
  stats.uniqueClients.add(client);

  if (hit && (now - hit.timestamp) < CACHE_MS) {
    stats.cacheHits++;
    addLog({ type: 'HIT', client, key });
    enrichRoutes(hit.data);
    return res.json(hit.data);
  }

  if (!inFlight.has(key)) {
    const promise = (async () => {
      await upstreamSem.acquire(SEMAPHORE_TIMEOUT_MS);
      try {
        const allApis = [
          { name: 'adsb.lol',        url: `https://api.adsb.lol/v2/point/${lat}/${lon}/${radius}` },
          { name: 'adsb.fi',         url: `https://opendata.adsb.fi/api/v3/lat/${lat}/lon/${lon}/dist/${radius}` },
          { name: 'airplanes.live',  url: `https://api.airplanes.live/v2/point/${lat}/${lon}/${radius}` },
          { name: 'adsb-one',        url: `https://api.adsb-one.com/v2/point/${lat}/${lon}/${radius}` },
        ];

        // Skip APIs on 429 cooldown
        const now2 = Date.now();
        const apis = allApis.filter(api => {
          const expires = apiCooldowns.get(api.name);
          if (!expires) return true;
          if (now2 >= expires) { apiCooldowns.delete(api.name); return true; }
          return false;
        });
        if (apis.length === 0) throw new Error('All ADS-B APIs on cooldown');

        // Try APIs sequentially (round-robin) — 1 call per cache miss instead of 4
        let data;
        const startIdx = apiRoundRobin % apis.length;
        for (let attempt = 0; attempt < apis.length; attempt++) {
          const api = apis[(startIdx + attempt) % apis.length];
          try {
            const r = await fetch(api.url, { signal: AbortSignal.timeout(2500) });
            if (r.status === 429) {
              apiCooldowns.set(api.name, Date.now() + COOLDOWN_MS);
              addLog({ type: 'COOLDOWN', client, key, error: `${api.name} → 60s cooldown (429)` });
              continue;
            }
            if (!r.ok) {
              addLog({ type: 'ERR', client, key, error: `${api.name}: HTTP ${r.status}` });
              continue;
            }
            data = await r.json();
            addLog({ type: 'MISS', client, key: key + ` (${api.name})` });
            apiRoundRobin++;
            break;
          } catch (e) {
            addLog({ type: 'ERR', client, key, error: `${api.name}: ${e.message}` });
            continue;
          }
        }
        if (!data) throw new Error('All ADS-B APIs failed');

        // Attach cached routes immediately, fire background lookups for misses
        const unrouted = enrichRoutes(data);

        // Fire-and-forget route lookups so they're cached for next request
        if (unrouted.length > 0) {
          Promise.allSettled(unrouted.map(cs => lookupRoute(cs)))
            .then(() => saveRouteCache())
            .catch(() => {});
        }

        const fetchedAt = Date.now();
        data._fetchedAt = fetchedAt;
        cacheSet(key, { data, timestamp: fetchedAt });
        logFlights(data.ac || []);
        return data;
      } finally {
        upstreamSem.release();
      }
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
    routeCacheEntries: routeCache.size,
    knownRoutes:    knownRoutes.size,
    newRoutesToday: todayNewRoutes.length,
    activeUpstream: upstreamSem.active,
    activeRoutes:   routeSem.active,
    inFlightKeys:   inFlight.size,
    apiCooldowns:   Object.fromEntries([...apiCooldowns].map(([k, v]) => [k, Math.max(0, Math.ceil((v - Date.now()) / 1000))])),
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

// ── Device heartbeat ────────────────────────────────────────────────
const deviceHeartbeats = new Map();

app.post('/device/heartbeat', express.json(), (req, res) => {
  const { device, fw, heap, uptime, rssi, flights, source, location } = req.body || {};
  if (!device) return res.status(400).json({ error: 'missing device' });
  deviceHeartbeats.set(device, {
    fw, heap, uptime, rssi, flights, source, location,
    ts: Date.now(), ip: req.headers['x-forwarded-for'] || req.socket.remoteAddress,
  });
  res.json({ ok: true });
});

app.get('/device/:name/status', (req, res) => {
  const hb = deviceHeartbeats.get(req.params.name);
  if (!hb) return res.json({ online: false, message: 'no heartbeat received' });
  const ageSec = Math.round((Date.now() - hb.ts) / 1000);
  res.json({ online: ageSec < 600, ageSec, ...hb });
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

// ── Tide predictions (BOM Sydney Fort Denison 2026) ──────────────────────────
let tideData = {};
try {
  tideData = JSON.parse(fs.readFileSync(path.join(__dirname, 'tides-2026.json'), 'utf8'));
} catch (e) { console.warn('[TIDE] Could not load tides-2026.json:', e.message); }

function getTideInfo(utcOffsetSecs) {
  if (!utcOffsetSecs || Object.keys(tideData).length === 0) return null;
  const now = new Date(Date.now() + utcOffsetSecs * 1000);
  const dateStr = now.toISOString().slice(0, 10);
  const hhmm = now.toISOString().slice(11, 16);
  const todayEvents = tideData[dateStr];
  if (!todayEvents) return null;

  // find next event today
  let next = todayEvents.find(e => e.time > hhmm);
  if (!next) {
    // look at tomorrow
    const tomorrow = new Date(now.getTime() + 86400000);
    const tomorrowStr = tomorrow.toISOString().slice(0, 10);
    const tomorrowEvents = tideData[tomorrowStr];
    if (tomorrowEvents && tomorrowEvents.length > 0) next = tomorrowEvents[0];
  }
  if (!next) return null;

  const isHigh = next.t === 'H';
  return {
    tide_dir:    isHigh ? 'RISING' : 'FALLING',
    tide_type:   isHigh ? 'HIGH' : 'LOW',
    tide_time:   next.time,
    tide_height: next.h,
  };
}

app.get('/weather', async (req, res) => {
  const { lat, lon } = req.query;
  if (!lat || !lon) return res.status(400).json({ error: 'lat and lon required' });
  const coordErr = validateCoord(lat, lon);
  if (coordErr) return res.status(400).json({ error: coordErr });

  const key = `weather:${bucketCoord(lat, 2)},${bucketCoord(lon, 2)}`;
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
        `wind_speed_10m,wind_direction_10m,uv_index,visibility,precipitation&wind_speed_unit=kmh&timezone=auto` +
        `&daily=sunrise,sunset`;
      const response = await fetch(url, { signal: AbortSignal.timeout(8000) });
      if (!response.ok) throw new Error(`Open-Meteo returned ${response.status}`);
      const raw = await response.json();
      const c = raw.current;
      const srRaw = raw.daily?.sunrise?.[0] || '';
      const ssRaw = raw.daily?.sunset?.[0] || '';
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
        visibility_km:   (c.visibility || 0) / 1000,
        precipitation_mm: c.precipitation || 0,
        sunrise:         srRaw.includes('T') ? srRaw.split('T')[1].slice(0, 5) : '--:--',
        sunset:          ssRaw.includes('T') ? ssRaw.split('T')[1].slice(0, 5) : '--:--',
        utc_offset_secs: raw.utc_offset_seconds,
      };
      const tide = getTideInfo(data.utc_offset_secs);
      if (tide) Object.assign(data, tide);
      cacheSet(key, { data, timestamp: Date.now() });
      return data;
    })();
    inFlight.set(key, promise);
    promise.finally(() => inFlight.delete(key)).catch(() => {});
  }

  try {
    const data = await inFlight.get(key);
    res.json(data);
  } catch (e) {
    stats.errors++;
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

  // New routes discovered this day
  let newRoutes;
  if (ds === todayDate) {
    newRoutes = todayNewRoutes;
  } else {
    newRoutes = data.newRoutes || [];
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
    newRoutes,
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

  const { date, total, summary, topAirlines, topTypes, topRoutes, trends, newRoutes, flights } = report;

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

    ${newRoutes && newRoutes.length > 0 ? `
    <div style="margin-bottom:20px">
      <div style="opacity:0.5;font-size:0.72rem;margin-bottom:6px">▸ NEW ROUTES DISCOVERED (${newRoutes.length})</div>
      <div style="border:1px solid #7a5200;padding:8px;background:rgba(96,255,144,0.03)">
        ${newRoutes.map(nr => {
          const time = nr.time ? new Date(nr.time).toLocaleTimeString('en-AU', { timeZone: TZ, hour: '2-digit', minute: '2-digit' }) : '';
          return `<div style="padding:2px 0;font-size:0.85rem"><span style="color:#60ff90">${escapeHtml(nr.route)}</span><span style="opacity:0.4;margin-left:8px">${escapeHtml(nr.callsign)}${time ? ' · ' + time : ''}</span></div>`;
        }).join('')}
      </div>
      <div style="opacity:0.3;font-size:0.65rem;margin-top:4px">${knownRoutes.size.toLocaleString()} total known routes</div>
    </div>` : `
    <div style="margin-bottom:20px">
      <div style="opacity:0.5;font-size:0.72rem;margin-bottom:6px">▸ NEW ROUTES DISCOVERED</div>
      <div style="opacity:0.3;font-size:0.8rem;padding:8px">No new routes today</div>
    </div>`}

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

// ── Resend helper ─────────────────────────────────────────────────────
async function sendViaResend(to, subject, html) {
  const apiKey = process.env.RESEND_API_KEY;
  if (!apiKey) throw new Error('RESEND_API_KEY not set');
  const r = await fetch('https://api.resend.com/emails', {
    method: 'POST',
    headers: { 'Authorization': `Bearer ${apiKey}`, 'Content-Type': 'application/json' },
    body: JSON.stringify({
      from: process.env.RESEND_FROM || 'Overhead Tracker <status@overheadtracker.com>',
      to: [to],
      subject,
      html,
    }),
    signal: AbortSignal.timeout(10000),
  });
  if (!r.ok) throw new Error(`Resend HTTP ${r.status}: ${await r.text()}`);
  return await r.json();
}

function renderStatusHTML(ds) {
  const S = `font-family:'Courier New',monospace`;
  const amber = '#ffa600';
  const bg = '#1a0a00';
  const dim = '#7a4800';
  const green = '#44ff88';
  const red = '#ff4444';

  const mem = process.memoryUsage();
  const heapMB = (mem.heapUsed / 1048576).toFixed(1);
  const heapTotalMB = (mem.heapTotal / 1048576).toFixed(1);
  const uptimeSec = Math.floor(os.uptime());
  const uptimeStr = `${Math.floor(uptimeSec / 86400)}d ${Math.floor((uptimeSec % 86400) / 3600)}h ${Math.floor((uptimeSec % 3600) / 60)}m`;
  const ramFreeMB = (os.freemem() / 1048576).toFixed(0);
  const ramTotalMB = (os.totalmem() / 1048576).toFixed(0);

  const totalReq = stats.totalRequests;
  const hitRate = totalReq > 0 ? ((stats.cacheHits / totalReq) * 100).toFixed(1) : '0.0';
  const clients = stats.uniqueClients.size;
  const routeEntries = routeCache.size;
  const routePct = ((routeEntries / MAX_ROUTE_ENTRIES) * 100).toFixed(1);

  const disk = diskStats();

  const cutoff = Date.now() - 86400000;
  const recentErrors = requestLog.filter(e => e.type === 'ERR' && new Date(e.time).getTime() > cutoff).length;
  const recentCooldowns = requestLog.filter(e => e.type === 'COOLDOWN' && new Date(e.time).getTime() > cutoff).length;

  const deviceRows = Array.from(deviceHeartbeats.entries()).map(([name, hb]) => {
    const ageSec = Math.round((Date.now() - hb.ts) / 1000);
    const online = ageSec < 600;
    const ageStr = ageSec < 60 ? `${ageSec}s ago` : ageSec < 3600 ? `${Math.floor(ageSec/60)}m ago` : `${Math.floor(ageSec/3600)}h ago`;
    const dot = online ? `<span style="color:${green}">●</span>` : `<span style="color:${red}">●</span>`;
    return `<tr>
      <td style="padding:4px 12px 4px 0">${dot} ${name}</td>
      <td style="padding:4px 12px 4px 0;color:${online ? green : red}">${online ? 'ONLINE' : 'OFFLINE'}</td>
      <td style="padding:4px 12px 4px 0;color:${dim}">${ageStr}</td>
      <td style="padding:4px 0;color:${dim}">${hb.fw || '—'} | heap ${hb.heap ? Math.round(hb.heap/1024)+'KB' : '—'} | rssi ${hb.rssi || '—'}</td>
    </tr>`;
  }).join('') || `<tr><td colspan="4" style="color:${dim}">No device heartbeats received</td></tr>`;

  const bf = backfillStats;
  const bfStr = bf.date
    ? `${bf.date}: checked ${bf.checked} flights, found ${bf.found} routes`
    : 'No backfill run recorded yet';

  const row = (label, value, color) =>
    `<tr><td style="padding:4px 16px 4px 0;color:${dim}">${label}</td><td style="color:${color || amber}">${value}</td></tr>`;

  return `<!DOCTYPE html><html><head><meta charset="UTF-8">
<style>
  body { background:${bg}; color:${amber}; ${S}; margin:0; padding:24px; }
  h1 { color:${amber}; font-size:18px; border-bottom:1px solid ${dim}; padding-bottom:8px; margin-bottom:16px; }
  h2 { color:${amber}; font-size:13px; margin:24px 0 8px; text-transform:uppercase; letter-spacing:2px; }
  table { border-collapse:collapse; }
  td { font-size:13px; vertical-align:top; }
</style></head><body>
<h1>⬡ Overhead Tracker — Server Status Report</h1>
<p style="color:${dim};font-size:12px;margin-top:-8px">${ds} · generated ${new Date().toISOString()}</p>

<h2>Server</h2>
<table>
  ${row('Uptime', uptimeStr)}
  ${row('OS RAM', `${ramFreeMB} MB free / ${ramTotalMB} MB total`)}
  ${row('Heap', `${heapMB} MB used / ${heapTotalMB} MB total`)}
  ${row('CPU temp', cpuTemp())}
</table>

<h2>Traffic (since last restart)</h2>
<table>
  ${row('Total requests', totalReq.toLocaleString())}
  ${row('Cache hit rate', `${hitRate}%`)}
  ${row('Unique clients', clients.toLocaleString())}
  ${row('Errors (24h)', recentErrors, recentErrors > 0 ? red : green)}
  ${row('API cooldowns (24h)', recentCooldowns, recentCooldowns > 0 ? amber : green)}
</table>

<h2>Route Cache</h2>
<table>
  ${row('Entries', `${routeEntries.toLocaleString()} / ${MAX_ROUTE_ENTRIES.toLocaleString()} (${routePct}%)`, routeEntries > MAX_ROUTE_ENTRIES * 0.9 ? red : amber)}
</table>

<h2>Route Discovery</h2>
<table>
  ${row('Known routes (all-time)', knownRoutes.size.toLocaleString())}
  ${row('New routes today', todayNewRoutes.length.toString(), todayNewRoutes.length > 0 ? green : dim)}
</table>

<h2>Route Backfill (last run)</h2>
<table>
  ${row('Result', bfStr, bf.found > 0 ? green : dim)}
</table>

<h2>Disk Usage</h2>
<table>
  ${row('Report log files', disk.reportFiles)}
  ${row('Reports dir size', `${disk.reportsMB} MB`)}
  ${row('Route cache file', `${disk.routeCacheKB} KB`)}
  ${row('Known routes file', `${disk.knownRoutesKB} KB`)}
</table>

<h2>Devices</h2>
<table>${deviceRows}</table>

</body></html>`;
}

async function sendStatusEmail(ds) {
  const to = process.env.REPORT_TO;
  if (!to || !process.env.RESEND_API_KEY) return;
  try {
    const html = renderStatusHTML(ds);
    await sendViaResend(to, `Server Status — ${ds}`, html);
    addLog({ type: 'SYS', client: 'system', key: `status email sent: ${ds}` });
  } catch (e) {
    addLog({ type: 'ERR', client: 'system', error: `status email failed: ${e.message}` });
  }
}

// ── Email ─────────────────────────────────────────────────────────────
async function sendDailyEmail(ds) {
  if (!process.env.RESEND_API_KEY) return;
  const reportTo = process.env.REPORT_TO;
  if (!reportTo) {
    console.log('REPORT_TO not configured — skipping daily report email');
    return;
  }

  const report = generateReport(ds);
  if (!report || report.total === 0) {
    console.log(`No flights for ${ds} — skipping email`);
    return;
  }

  const html = renderReportHTML(report);
  try {
    await sendViaResend(reportTo, `Air Traffic Report — ${ds} — ${report.total} flights`, html);
    console.log(`Daily report email sent for ${ds}`);
    addLog({ type: 'SYS', client: 'system', key: `email sent: ${ds} (${report.total} flights)` });
  } catch (e) {
    console.error('Failed to send daily email:', e.message);
    addLog({ type: 'ERR', client: 'system', error: `email failed: ${e.message}` });
  }
}

function renderRouteDiscoveryHTML(ds, newRoutes) {
  const amber = '#ffa600';
  const bg = '#1a0a00';
  const green = '#60ff90';
  const dim = '#7a4800';
  const totalKnown = knownRoutes.size;
  const count = newRoutes.length;

  // Fun stats: categorize routes
  const longHaul = [];
  const domestic = [];
  const auCities = new Set(['Sydney','Melbourne','Brisbane','Perth','Adelaide','Canberra','Gold Coast','Cairns','Hobart','Darwin','Townsville','Newcastle','Launceston','Ballina','Mackay']);
  const usCities = new Set(['Chicago','New York','Los Angeles','San Francisco','Dallas','Houston','Atlanta','Miami','Denver','Seattle','Boston','Phoenix','Orlando','Las Vegas','Washington','Philadelphia','Charlotte','Minneapolis','Detroit','Tampa','Portland','Salt Lake City','Nashville','Austin','San Diego','San Jose','Raleigh','Indianapolis','Columbus','Cincinnati','Cleveland','Milwaukee','Kansas City','Memphis','Baltimore','Buffalo','Pittsburgh','Sacramento','Oakland','San Antonio','Honolulu','New Orleans','Jacksonville','Hartford','Richmond','Norfolk','Rochester','Syracuse','Albany']);

  for (const nr of newRoutes) {
    const parts = nr.route.split(' > ');
    if (parts.length !== 2) continue;
    const [dep, arr] = parts;
    const bothAu = auCities.has(dep) && auCities.has(arr);
    const bothUs = usCities.has(dep) && usCities.has(arr);
    if (bothAu || bothUs) {
      domestic.push(nr);
    } else {
      longHaul.push(nr);
    }
  }

  // Find most exotic (longest route name as rough proxy for interesting)
  const exotic = [...newRoutes].sort((a, b) => b.route.length - a.route.length).slice(0, 3);

  // Airlines that brought new routes
  const airlines = {};
  for (const nr of newRoutes) {
    const prefix = nr.callsign.replace(/[0-9]/g, '').substring(0, 3).toUpperCase();
    const name = AIRLINE_NAMES[prefix] || prefix;
    airlines[name] = (airlines[name] || 0) + 1;
  }
  const topDiscoverers = Object.entries(airlines).sort((a, b) => b[1] - a[1]).slice(0, 5);

  const routeRows = newRoutes.map(nr => {
    const time = nr.time ? new Date(nr.time).toLocaleTimeString('en-AU', { timeZone: TZ, hour: '2-digit', minute: '2-digit' }) : '';
    const prefix = nr.callsign.replace(/[0-9]/g, '').substring(0, 3).toUpperCase();
    const airline = AIRLINE_NAMES[prefix] || '';
    return `<tr style="border-bottom:1px solid ${dim}">
      <td style="padding:4px 8px;color:${green}">${escapeHtml(nr.route)}</td>
      <td style="padding:4px 8px;opacity:0.7">${escapeHtml(nr.callsign)}</td>
      <td style="padding:4px 8px;opacity:0.7">${escapeHtml(airline)}</td>
      <td style="padding:4px 8px;opacity:0.7">${time}</td>
    </tr>`;
  }).join('');

  return `<!DOCTYPE html><html><head><meta charset="UTF-8"></head>
  <body style="margin:0;background:${bg};font-family:'Courier New',monospace;color:${amber}">
  <div style="max-width:700px;margin:0 auto;padding:24px">
    <h1 style="font-size:1.3rem;margin:0 0 4px;text-shadow:0 0 8px #ff8000">NEW ROUTE DISCOVERIES</h1>
    <p style="opacity:0.5;margin:0 0 20px;font-size:0.85rem">${ds} · Overhead Tracker</p>

    <div style="display:flex;gap:16px;flex-wrap:wrap;margin-bottom:20px">
      <div style="border:1px solid ${dim};padding:12px 16px;flex:1;min-width:120px;background:rgba(96,255,144,0.03)">
        <div style="opacity:0.5;font-size:0.7rem">NEW TODAY</div>
        <div style="font-size:1.8rem;color:${green};text-shadow:0 0 8px ${green}">${count}</div>
      </div>
      <div style="border:1px solid ${dim};padding:12px 16px;flex:1;min-width:120px;background:rgba(255,166,0,0.03)">
        <div style="opacity:0.5;font-size:0.7rem">ALL-TIME KNOWN</div>
        <div style="font-size:1.8rem;text-shadow:0 0 8px #ff8000">${totalKnown.toLocaleString()}</div>
      </div>
      <div style="border:1px solid ${dim};padding:12px 16px;flex:1;min-width:120px;background:rgba(255,166,0,0.03)">
        <div style="opacity:0.5;font-size:0.7rem">BREAKDOWN</div>
        <div style="font-size:0.85rem;margin-top:4px">${longHaul.length} international</div>
        <div style="font-size:0.85rem">${domestic.length} domestic</div>
      </div>
    </div>

    ${exotic.length > 0 ? `
    <div style="margin-bottom:20px;border:1px solid ${dim};padding:12px;background:rgba(96,255,144,0.03)">
      <div style="opacity:0.5;font-size:0.7rem;margin-bottom:8px">MOST NOTABLE</div>
      ${exotic.map(nr => {
        const prefix = nr.callsign.replace(/[0-9]/g, '').substring(0, 3).toUpperCase();
        const airline = AIRLINE_NAMES[prefix] || prefix;
        return `<div style="font-size:0.95rem;margin-bottom:4px"><span style="color:${green}">${escapeHtml(nr.route)}</span> <span style="opacity:0.5">— ${escapeHtml(airline)} (${escapeHtml(nr.callsign)})</span></div>`;
      }).join('')}
    </div>` : ''}

    ${topDiscoverers.length > 0 ? `
    <div style="margin-bottom:20px">
      <div style="opacity:0.5;font-size:0.72rem;margin-bottom:6px">TOP DISCOVERERS</div>
      <table style="border-collapse:collapse;width:100%;border:1px solid ${dim}">
        ${topDiscoverers.map(([name, n]) =>
          `<tr><td style="padding:3px 8px">${escapeHtml(name)}</td><td style="padding:3px 8px;text-align:right">${n} route${n > 1 ? 's' : ''}</td></tr>`
        ).join('')}
      </table>
    </div>` : ''}

    <div style="opacity:0.5;font-size:0.72rem;margin-bottom:6px">ALL NEW ROUTES (${count})</div>
    <div style="overflow-x:auto">
      <table style="border-collapse:collapse;width:100%;border:1px solid ${dim};font-size:0.8rem">
        <tr style="opacity:0.5;border-bottom:1px solid ${dim}">
          <th style="padding:4px 8px;text-align:left">Route</th>
          <th style="padding:4px 8px;text-align:left">Callsign</th>
          <th style="padding:4px 8px;text-align:left">Airline</th>
          <th style="padding:4px 8px;text-align:left">Time</th>
        </tr>
        ${routeRows}
      </table>
    </div>

    <p style="opacity:0.3;font-size:0.7rem;margin-top:20px;text-align:center">
      <a href="https://api.overheadtracker.com/routes/new?date=${ds}" style="color:${amber};opacity:0.6">View JSON</a> ·
      <a href="https://api.overheadtracker.com/report?date=${ds}" style="color:${amber};opacity:0.6">Full report</a> ·
      Overhead Tracker
    </p>
  </div></body></html>`;
}

async function sendRouteDiscoveryEmail(ds) {
  if (!process.env.RESEND_API_KEY) return;
  const to = process.env.ROUTE_EMAIL_TO || process.env.REPORT_TO;
  if (!to) return;

  let routes;
  if (ds === todayDate) {
    routes = todayNewRoutes;
  } else {
    const data = loadDayData(ds);
    routes = data?.newRoutes || [];
  }

  if (routes.length === 0) {
    console.log(`No new routes for ${ds} — skipping route discovery email`);
    addLog({ type: 'SYS', client: 'system', key: `route email skipped: ${ds} (0 new)` });
    return;
  }

  try {
    const html = renderRouteDiscoveryHTML(ds, routes);
    await sendViaResend(to, `${routes.length} New Route${routes.length > 1 ? 's' : ''} Discovered — ${ds}`, html);
    console.log(`Route discovery email sent for ${ds} (${routes.length} routes)`);
    addLog({ type: 'SYS', client: 'system', key: `route email sent: ${ds} (${routes.length} new routes)` });
  } catch (e) {
    console.error('Failed to send route discovery email:', e.message);
    addLog({ type: 'ERR', client: 'system', error: `route email failed: ${e.message}` });
  }
}

async function backfillMissingRoutes(targetDs) {
  if (backfillRunning) return;
  backfillRunning = true;
  try {
    const data = loadDayData(targetDs);
    if (!data?.flights) {
      addLog({ type: 'SYS', client: 'system', key: `backfill ${targetDs}: no data file` });
      return;
    }

    const candidates = Object.values(data.flights).filter(f =>
      AIRLINE_NAMES[airlinePrefix(f.callsign)] && (!f.dep || !f.arr)
    );

    if (!candidates.length) {
      addLog({ type: 'SYS', client: 'system', key: `backfill ${targetDs}: no missing routes` });
      return;
    }

    let found = 0;
    let dirty = false;
    for (let i = 0; i < candidates.length; i++) {
      if (i > 0) await new Promise(r => setTimeout(r, 500));
      const f = candidates[i];
      const result = await lookupRoute(f.callsign);
      if (result && (result.dep || result.arr)) {
        found++;
        dirty = true;
        if (result.dep) f.dep = result.dep;
        if (result.arr) f.arr = result.arr;
        f.route = formatRouteString(f.dep, f.arr);
        if (f.route) checkNewRoute(f.route, f.callsign, targetDs);
      }
    }

    if (dirty) {
      fs.writeFile(todayFilePath(targetDs), JSON.stringify(data, null, 2), () => {});
      saveRouteCache();
      saveKnownRoutes();
    }

    backfillStats = { date: targetDs, checked: candidates.length, found };
    addLog({ type: 'SYS', client: 'system', key: `backfill ${targetDs}: checked ${candidates.length}, found ${found}` });
  } finally {
    backfillRunning = false;
  }
}

// ── /routes/new endpoint ──────────────────────────────────────────────
app.get('/routes/new', (req, res) => {
  const ds = req.query.date || todayDate;
  if (!/^\d{4}-\d{2}-\d{2}$/.test(ds)) return res.status(400).json({ error: 'Invalid date format' });
  let routes;
  if (ds === todayDate) {
    routes = todayNewRoutes;
  } else {
    const data = loadDayData(ds);
    routes = data?.newRoutes || [];
  }
  res.json({ date: ds, count: routes.length, totalKnown: knownRoutes.size, routes });
});

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

// ── /status/send — manual trigger for server status email ─────────────
app.post('/status/send', async (req, res) => {
  if (requireAdmin(req, res)) return;
  const ds = req.query.date || todayDate;
  await sendStatusEmail(ds);
  res.json({ ok: true, date: ds });
});

// ── /routes/send — manual trigger for route discovery email ────────────
app.post('/routes/send', async (req, res) => {
  if (requireAdmin(req, res)) return;
  const ds = req.query.date || todayDate;
  await sendRouteDiscoveryEmail(ds);
  res.json({ ok: true, date: ds });
});

// ── /backfill — manual trigger for route backfill ─────────────────────
app.post('/backfill', async (req, res) => {
  if (requireAdmin(req, res)) return;
  const ds = req.query.date || prevDateStr(todayDate, 1);
  if (!/^\d{4}-\d{2}-\d{2}$/.test(ds)) return res.status(400).json({ error: 'Invalid date' });
  backfillMissingRoutes(ds);
  res.json({ ok: true, date: ds });
});

// ── Periodic save + day rollover + email scheduler ────────────────────
const periodicTimer = setInterval(() => {
  const now = aestNow();
  const ds  = dateStr(now);

  // Day rollover
  if (ds !== todayDate) {
    saveTodayLog(todayDate);
    saveKnownRoutes();
    todayFlights = {};
    todayHourly  = new Array(24).fill(0);
    todayNewRoutes = [];
    todayDate    = ds;
    emailSentToday       = false;
    statusEmailSentToday = false;
    routeEmailSentToday  = false;
    loadTodayLog(ds);
    console.log(`Day rolled over to ${ds}`);
  }

  // Periodic save (every 60s)
  saveTodayLog();
  saveKnownRoutes();

  // Route backfill at 02:00 AEST for yesterday's log
  const backfillTarget = prevDateStr(ds, 1);
  if (now.getHours() === 2 && lastBackfillDate !== backfillTarget && !backfillRunning) {
    lastBackfillDate = backfillTarget;
    backfillMissingRoutes(backfillTarget);
  }

  // Server status email at 08:00 AEST
  if (now.getHours() === 8 && now.getMinutes() < 2 && !statusEmailSentToday) {
    statusEmailSentToday = true;
    sendStatusEmail(ds);
  }

  // Route discovery email at 21:00 AEST
  if (now.getHours() === 21 && now.getMinutes() < 2 && !routeEmailSentToday) {
    routeEmailSentToday = true;
    sendRouteDiscoveryEmail(ds);
  }

  // Send email at 23:55 AEST
  if (now.getHours() === 23 && now.getMinutes() >= 55 && !emailSentToday) {
    emailSentToday = true;
    sendDailyEmail(ds);
  }
}, 60000);

// ── Graceful shutdown ──────────────────────────────────────────────
function gracefulShutdown(signal) {
  console.log(`${signal} received — shutting down gracefully`);
  saveRouteCache();
  saveKnownRoutes();
  saveTodayLog();
  if (server) {
    server.close(() => {
      console.log('All connections drained — exiting');
      process.exit(0);
    });
    setTimeout(() => {
      console.log('Drain timeout — forcing exit');
      process.exit(1);
    }, 5000);
  } else {
    process.exit(0);
  }
}
process.on('SIGTERM', () => gracefulShutdown('SIGTERM'));
process.on('SIGINT',  () => gracefulShutdown('SIGINT'));

let server;
if (require.main === module) {
  server = app.listen(PORT, '0.0.0.0', () => {
    console.log(`Proxy + dashboard running on port ${PORT}`);
  });
} else {
  // When imported as module (tests), don't keep process alive
  periodicTimer.unref();
}

module.exports = {
  app, cache, inFlight, routeCache, knownRoutes, stats, requestLog,
  haversine, airlineName, airlinePrefix, airportName,
  formatRouteString, enrichRoutes, validateCoord, escapeHtml, formatUptime,
  windCardinal, addLog, cacheSet, routeCacheSet,
  semaphore, upstreamSem, routeSem,
  bucketCoord, bucketKey,
  CACHE_MS, ROUTE_CACHE_MS, SEMAPHORE_TIMEOUT_MS,
  MAX_CACHE_ENTRIES, MAX_ROUTE_ENTRIES,
  MAX_UPSTREAM_CONCURRENT, MAX_ROUTE_CONCURRENT,
};
