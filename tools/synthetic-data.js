#!/usr/bin/env node
// Synthetic flight + weather data generator for testing.
// Used by mock-proxy.js and can generate standalone JSON fixtures.
//
// Usage:
//   node tools/synthetic-data.js [lat] [lon]           # print JSON to stdout
//   node tools/synthetic-data.js --scenario crowded    # specific scenario
//   node tools/synthetic-data.js --list                # list scenarios
//
// Also importable: const { generate, SCENARIOS } = require('./synthetic-data');

const DEG = Math.PI / 180;

// ── Airlines & Aircraft ──────────────────────────────────────────────
const AIRLINES_SYDNEY = [
  { prefix: 'QFA', reg: 'VH-OQ', types: ['A388','B789','A332'], country: 'AU' },
  { prefix: 'VOZ', reg: 'VH-YI', types: ['B738','B73H','A320'], country: 'AU' },
  { prefix: 'JST', reg: 'VH-VK', types: ['A320','A321','A20N'], country: 'AU' },
  { prefix: 'UAE', reg: 'A6-EO', types: ['A388','B77W'], country: 'AE' },
  { prefix: 'SIA', reg: '9V-SK', types: ['A388','A359','B78X'], country: 'SG' },
  { prefix: 'BAW', reg: 'G-XLE', types: ['A388','B789','A35K'], country: 'GB' },
  { prefix: 'AAL', reg: 'N178', types: ['B738','A321','B77W'], country: 'US' },
  { prefix: 'DAL', reg: 'N501', types: ['A359','B763','A321'], country: 'US' },
  { prefix: 'UAL', reg: 'N127', types: ['B789','B77W','A320'], country: 'US' },
  { prefix: 'AFR', reg: 'F-HPB', types: ['A388','B77W','A359'], country: 'FR' },
  { prefix: 'DLH', reg: 'D-AIM', types: ['A388','A359','B748'], country: 'DE' },
  { prefix: 'CPA', reg: 'B-LR', types: ['A359','B77W','A333'], country: 'HK' },
  { prefix: 'ANA', reg: 'JA88', types: ['B789','B77W','A321'], country: 'JP' },
  { prefix: 'RYR', reg: 'EI-DL', types: ['B738','B38M'], country: 'IE' },
  { prefix: 'EZY', reg: 'G-EZB', types: ['A320','A20N','A319'], country: 'GB' },
];

const AIRLINES_CHICAGO = [
  { prefix: 'UAL', reg: 'N375', types: ['B738','B739','B789','A320','B77W','B39M'], country: 'US' },
  { prefix: 'AAL', reg: 'N178', types: ['A321','B738','B77W','A21N'], country: 'US' },
  { prefix: 'SWA', reg: 'N832', types: ['B738','B38M','B37M'], country: 'US' },
  { prefix: 'DAL', reg: 'N501', types: ['A321','B763','A359','A21N'], country: 'US' },
  { prefix: 'NKS', reg: 'N650', types: ['A320','A21N','A20N'], country: 'US' },
  { prefix: 'FFT', reg: 'N341', types: ['A320','A21N','A20N'], country: 'US' },
  { prefix: 'JBU', reg: 'N603', types: ['A320','A321','E190'], country: 'US' },
  { prefix: 'SKW', reg: 'N117', types: ['E175','CRJ7','CRJ9'], country: 'US' },
  { prefix: 'ENY', reg: 'N225', types: ['E175','E170','CRJ7'], country: 'US' },
  { prefix: 'RPA', reg: 'N401', types: ['E175','E170'], country: 'US' },
  { prefix: 'ASA', reg: 'N461', types: ['B739','B38M','A321'], country: 'US' },
  { prefix: 'DLH', reg: 'D-AIM', types: ['A388','A359','B748'], country: 'DE' },
  { prefix: 'BAW', reg: 'G-XLE', types: ['B789','A35K','B77W'], country: 'GB' },
  { prefix: 'ANA', reg: 'JA88', types: ['B789','B77W'], country: 'JP' },
  { prefix: 'FDX', reg: 'N68', types: ['B763','B77L','A306'], country: 'US' },
];

const AIRPORTS_SYDNEY = [
  { icao: 'YSSY', iata: 'SYD' }, { icao: 'YMML', iata: 'MEL' },
  { icao: 'YBBN', iata: 'BNE' }, { icao: 'EGLL', iata: 'LHR' },
  { icao: 'KJFK', iata: 'JFK' }, { icao: 'KLAX', iata: 'LAX' },
  { icao: 'OMDB', iata: 'DXB' }, { icao: 'WSSS', iata: 'SIN' },
  { icao: 'VHHH', iata: 'HKG' }, { icao: 'RJTT', iata: 'HND' },
  { icao: 'LFPG', iata: 'CDG' }, { icao: 'EDDF', iata: 'FRA' },
  { icao: 'LEMD', iata: 'MAD' }, { icao: 'LIRF', iata: 'FCO' },
  { icao: 'NZAA', iata: 'AKL' },
];

const AIRPORTS_CHICAGO = [
  { icao: 'KORD', iata: 'ORD' }, { icao: 'KMDW', iata: 'MDW' },
  { icao: 'KLAX', iata: 'LAX' }, { icao: 'KJFK', iata: 'JFK' },
  { icao: 'KSFO', iata: 'SFO' }, { icao: 'KATL', iata: 'ATL' },
  { icao: 'KDFW', iata: 'DFW' }, { icao: 'KMIA', iata: 'MIA' },
  { icao: 'KDEN', iata: 'DEN' }, { icao: 'KLAS', iata: 'LAS' },
  { icao: 'KBOS', iata: 'BOS' }, { icao: 'KMSP', iata: 'MSP' },
  { icao: 'KDTW', iata: 'DTW' }, { icao: 'EGLL', iata: 'LHR' },
  { icao: 'RJTT', iata: 'HND' }, { icao: 'EDDF', iata: 'FRA' },
];

const CITIES = {
  sydney:  { lat: -33.8688, lon: 151.2093, airlines: AIRLINES_SYDNEY, airports: AIRPORTS_SYDNEY },
  chicago: { lat: 41.9028, lon: -87.6773, airlines: AIRLINES_CHICAGO, airports: AIRPORTS_CHICAGO },
};

let AIRLINES = AIRLINES_SYDNEY;
let AIRPORTS = AIRPORTS_SYDNEY;

const CATEGORIES = ['A1', 'A2', 'A3', 'A4', 'A5'];

// ── Helpers ──────────────────────────────────────────────────────────
function rand(min, max) { return min + Math.random() * (max - min); }
function randInt(min, max) { return Math.floor(rand(min, max + 1)); }
function pick(arr) { return arr[Math.floor(Math.random() * arr.length)]; }
function pad(s, n) { return (s + '        ').slice(0, n); }

function offsetLatLon(lat, lon, distKm, bearingDeg) {
  const R = 6371;
  const brng = bearingDeg * DEG;
  const lat1 = lat * DEG;
  const lon1 = lon * DEG;
  const lat2 = Math.asin(Math.sin(lat1) * Math.cos(distKm / R) +
    Math.cos(lat1) * Math.sin(distKm / R) * Math.cos(brng));
  const lon2 = lon1 + Math.atan2(
    Math.sin(brng) * Math.sin(distKm / R) * Math.cos(lat1),
    Math.cos(distKm / R) - Math.sin(lat1) * Math.sin(lat2));
  return { lat: +(lat2 / DEG).toFixed(5), lon: +(lon2 / DEG).toFixed(5) };
}

function randomHex() {
  return Math.floor(Math.random() * 0xFFFFFF).toString(16).padStart(6, '0');
}

// ── Flight generators by phase ───────────────────────────────────────
function makeFlight(centerLat, centerLon, phase, index) {
  const airline = AIRLINES[index % AIRLINES.length];
  const flightNum = randInt(100, 999);
  const callsign = pad(airline.prefix + flightNum, 8);
  const regSuffix = String.fromCharCode(65 + randInt(0, 25));
  const reg = airline.reg + regSuffix;
  const type = pick(airline.types);
  const depApt = pick(AIRPORTS);
  let arrApt = pick(AIRPORTS);
  while (arrApt.icao === depApt.icao) arrApt = pick(AIRPORTS);

  let alt, speed, vs, dist, bearing, squawk, cat;
  bearing = rand(0, 360);
  squawk = String(randInt(1000, 7477)).padStart(4, '0');
  cat = pick(CATEGORIES);

  switch (phase) {
    case 'takeoff':
      alt = randInt(500, 2000);
      speed = randInt(140, 200);
      vs = randInt(1500, 3000);
      dist = rand(1, 5);
      cat = pick(['A3', 'A4', 'A5']);
      break;
    case 'climbing':
      alt = randInt(3000, 15000);
      speed = randInt(250, 380);
      vs = randInt(800, 2500);
      dist = rand(3, 12);
      break;
    case 'cruising':
      alt = randInt(28000, 42000);
      speed = randInt(420, 520);
      vs = randInt(-100, 100);
      dist = rand(5, 18);
      cat = pick(['A4', 'A5']);
      break;
    case 'descending':
      alt = randInt(10000, 25000);
      speed = randInt(300, 420);
      vs = randInt(-2500, -500);
      dist = rand(5, 15);
      break;
    case 'approach':
      alt = randInt(2000, 8000);
      speed = randInt(160, 260);
      vs = randInt(-1500, -400);
      dist = rand(2, 8);
      break;
    case 'landing':
      alt = randInt(300, 1500);
      speed = randInt(130, 170);
      vs = randInt(-1200, -600);
      dist = rand(0.5, 3);
      cat = pick(['A3', 'A4', 'A5']);
      break;
    case 'overhead':
      alt = randInt(1000, 5000);
      speed = randInt(100, 300);
      vs = randInt(-200, 200);
      dist = rand(0.1, 1.5);
      break;
    case 'emergency':
      alt = randInt(5000, 15000);
      speed = randInt(200, 350);
      vs = randInt(-2000, 0);
      dist = rand(2, 10);
      squawk = pick(['7700', '7600', '7500']);
      break;
    default: // random
      alt = randInt(1000, 40000);
      speed = randInt(100, 500);
      vs = randInt(-2000, 2000);
      dist = rand(1, 18);
  }

  const pos = offsetLatLon(centerLat, centerLon, dist, bearing);
  const track = (bearing + 180 + rand(-30, 30)) % 360;

  return {
    hex: randomHex(),
    flight: callsign,
    r: reg,
    t: type,
    lat: pos.lat,
    lon: pos.lon,
    alt_baro: alt,
    gs: speed,
    baro_rate: vs,
    track: +track.toFixed(1),
    squawk: squawk,
    category: cat,
    dep: depApt.icao,
    arr: arrApt.icao,
    route: depApt.icao + ' > ' + arrApt.icao,
    orig_iata: depApt.iata,
    dest_iata: arrApt.iata,
  };
}

// ── Scenarios ────────────────────────────────────────────────────────
const SCENARIOS = {
  busy: {
    desc: 'Busy airspace — 12 flights in all phases',
    phases: ['takeoff', 'climbing', 'climbing', 'cruising', 'cruising', 'cruising',
             'descending', 'descending', 'approach', 'approach', 'landing', 'overhead']
  },
  quiet: {
    desc: 'Quiet airspace — 2 high-altitude cruisers',
    phases: ['cruising', 'cruising']
  },
  crowded: {
    desc: 'Very crowded — 18 flights (stress test)',
    phases: ['takeoff', 'takeoff', 'climbing', 'climbing', 'climbing',
             'cruising', 'cruising', 'cruising', 'cruising', 'cruising',
             'descending', 'descending', 'descending',
             'approach', 'approach', 'landing', 'landing', 'overhead']
  },
  emergency: {
    desc: 'Emergency — 5 normal flights + 1 squawking 7700',
    phases: ['cruising', 'cruising', 'climbing', 'approach', 'descending', 'emergency']
  },
  approach_rush: {
    desc: 'Approach rush — 8 flights on approach/landing',
    phases: ['approach', 'approach', 'approach', 'approach',
             'landing', 'landing', 'landing', 'landing']
  },
  single: {
    desc: 'Single overhead flight',
    phases: ['overhead']
  },
  empty: {
    desc: 'Empty sky — no flights',
    phases: []
  },
  mixed: {
    desc: 'Mixed bag — 6 flights across different phases',
    phases: ['takeoff', 'climbing', 'cruising', 'descending', 'approach', 'landing']
  },
  chicago_busy: {
    desc: 'Chicago busy — 12 flights, O\'Hare + Midway mix',
    phases: ['takeoff', 'climbing', 'climbing', 'cruising', 'cruising', 'cruising',
             'descending', 'descending', 'approach', 'approach', 'landing', 'overhead'],
    city: 'chicago'
  },
  chicago_rush: {
    desc: 'Chicago approach rush — 18 flights, heavy arrivals',
    phases: ['takeoff', 'climbing', 'climbing', 'cruising', 'cruising', 'cruising', 'cruising',
             'descending', 'descending', 'descending',
             'approach', 'approach', 'approach', 'approach',
             'landing', 'landing', 'landing', 'overhead'],
    city: 'chicago'
  },
};

// ── Weather generator ────────────────────────────────────────────────
function generateWeather() {
  const conditions = [
    { code: 0, text: 'Clear Sky' },
    { code: 1, text: 'Mainly Clear' },
    { code: 2, text: 'Partly Cloudy' },
    { code: 3, text: 'Overcast' },
    { code: 45, text: 'Fog' },
    { code: 51, text: 'Light Drizzle' },
    { code: 61, text: 'Light Rain' },
    { code: 63, text: 'Moderate Rain' },
    { code: 80, text: 'Rain Showers' },
    { code: 95, text: 'Thunderstorm' },
  ];
  const wx = pick(conditions);
  const cardinals = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
  const windDir = randInt(0, 359);
  const cardinal = cardinals[Math.round(windDir / 45) % 8];
  const tideTypes = [
    { dir: 'RISING', type: 'HIGH' },
    { dir: 'FALLING', type: 'LOW' },
    { dir: 'RISING', type: 'HIGH' },
    { dir: '', type: '' },
  ];
  const tide = pick(tideTypes);

  return {
    temp: +(rand(5, 38)).toFixed(1),
    feels_like: +(rand(3, 40)).toFixed(1),
    humidity: randInt(20, 95),
    weather_code: wx.code,
    condition: wx.text,
    wind_speed: +(rand(0, 60)).toFixed(1),
    wind_dir: windDir,
    wind_cardinal: cardinal,
    uv_index: +(rand(0, 11)).toFixed(1),
    utc_offset_secs: pick([-18000, 0, 3600, 7200, 28800, 32400, 36000, 39600]),
    tide_dir: tide.dir,
    tide_time: tide.dir ? `${String(randInt(0, 23)).padStart(2, '0')}:${String(randInt(0, 59)).padStart(2, '0')}` : '',
    tide_height: tide.dir ? +(rand(0.3, 2.5)).toFixed(1) : 0,
    tide_type: tide.type,
  };
}

// ── Main generator ───────────────────────────────────────────────────
function generate(lat, lon, scenario, city) {
  const sc = SCENARIOS[scenario] || SCENARIOS.busy;
  const cityKey = sc.city || city || null;
  if (cityKey && CITIES[cityKey]) {
    AIRLINES = CITIES[cityKey].airlines;
    AIRPORTS = CITIES[cityKey].airports;
  }
  const flights = sc.phases.map((phase, i) => makeFlight(lat, lon, phase, i));
  return {
    flights: { ac: flights, total: flights.length, now: Date.now() / 1000 },
    weather: generateWeather(),
  };
}

// ── CLI ──────────────────────────────────────────────────────────────
if (require.main === module) {
  const args = process.argv.slice(2);

  if (args.includes('--list')) {
    console.log('\nAvailable scenarios:\n');
    for (const [name, sc] of Object.entries(SCENARIOS)) {
      console.log(`  ${name.padEnd(16)} ${sc.desc}`);
    }
    console.log('\nUsage: node tools/synthetic-data.js [lat] [lon] [--scenario NAME]\n');
    process.exit(0);
  }

  let lat = -33.8688, lon = 151.2093; // default: Sydney
  let scenario = 'busy';
  let city = null;

  for (let i = 0; i < args.length; i++) {
    if (args[i] === '--scenario' && args[i + 1]) { scenario = args[++i]; continue; }
    if (args[i] === '--city' && args[i + 1]) { city = args[++i].toLowerCase(); continue; }
    const n = parseFloat(args[i]);
    if (!isNaN(n)) {
      if (lat === -33.8688 && i === 0) lat = n;
      else lon = n;
    }
  }

  if (city && CITIES[city]) {
    lat = CITIES[city].lat;
    lon = CITIES[city].lon;
  }

  const data = generate(lat, lon, scenario, city);
  console.log(JSON.stringify(data, null, 2));
}

module.exports = { generate, generateWeather, SCENARIOS };
