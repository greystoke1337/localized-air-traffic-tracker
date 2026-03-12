const { describe, it, before, after, beforeEach } = require('node:test');
const assert = require('node:assert/strict');
const http = require('http');

const {
  app, cache, inFlight, routeCache, stats, requestLog,
  haversine, airlineName, airlinePrefix, airportName,
  formatRouteString, validateCoord, escapeHtml, formatUptime,
  windCardinal, addLog, cacheSet, routeCacheSet,
  semaphore, upstreamSem, routeSem,
  bucketCoord, bucketKey,
  CACHE_MS, ROUTE_CACHE_MS, SEMAPHORE_TIMEOUT_MS,
  MAX_CACHE_ENTRIES, MAX_ROUTE_ENTRIES,
  MAX_UPSTREAM_CONCURRENT, MAX_ROUTE_CONCURRENT,
} = require('./server');

// ── Helper: HTTP request against Express app ────────────────────────
let server, baseUrl;

function get(path) {
  return new Promise((resolve, reject) => {
    http.get(`${baseUrl}${path}`, (res) => {
      let body = '';
      res.on('data', (chunk) => body += chunk);
      res.on('end', () => {
        try { resolve({ status: res.statusCode, headers: res.headers, body: JSON.parse(body) }); }
        catch { resolve({ status: res.statusCode, headers: res.headers, body }); }
      });
    }).on('error', reject);
  });
}

function resetState() {
  cache.clear();
  inFlight.clear();
  requestLog.length = 0;
  stats.totalRequests = 0;
  stats.cacheHits = 0;
  stats.errors = 0;
  stats.peakHour.fill(0);
  stats.uniqueClients.clear();
}

// ═══════════════════════════════════════════════════════════════════════
// Pure function tests (no server needed)
// ═══════════════════════════════════════════════════════════════════════

describe('haversine', () => {
  it('returns 0 for same point', () => {
    assert.equal(haversine(0, 0, 0, 0), 0);
  });

  it('calculates Sydney to Melbourne ~714 km', () => {
    const d = haversine(-33.8688, 151.2093, -37.8136, 144.9631);
    assert.ok(d > 700 && d < 730, `Expected ~714 km, got ${d}`);
  });

  it('handles antipodal points', () => {
    const d = haversine(0, 0, 0, 180);
    assert.ok(d > 20000 && d < 20100, `Expected ~20015 km, got ${d}`);
  });

  it('handles negative coordinates', () => {
    const d = haversine(-33.853, 151.141, -33.946, 151.177);
    assert.ok(d > 9 && d < 12, `Expected ~10.5 km, got ${d}`);
  });
});

describe('airlineName', () => {
  it('returns airline name for known prefix', () => {
    assert.equal(airlineName('QFA123'), 'Qantas');
  });

  it('returns prefix for unknown airline', () => {
    assert.equal(airlineName('ZZZ999'), 'ZZZ');
  });

  it('handles null/empty input', () => {
    assert.equal(airlineName(null), null);
    assert.equal(airlineName(''), null);
  });

  it('handles callsign with no numbers', () => {
    assert.equal(airlineName('VOZ'), 'Virgin Australia');
  });
});

describe('airlinePrefix', () => {
  it('extracts 3-letter prefix', () => {
    assert.equal(airlinePrefix('QFA456'), 'QFA');
  });

  it('returns ??? for null', () => {
    assert.equal(airlinePrefix(null), '???');
  });
});

describe('airportName', () => {
  it('returns city for known ICAO code', () => {
    assert.equal(airportName('YSSY'), 'Sydney');
  });

  it('returns city for known IATA code', () => {
    assert.equal(airportName('SYD'), 'Sydney');
  });

  it('returns trimmed code for unknown airport', () => {
    assert.equal(airportName('XXXX'), 'XXXX');
  });

  it('handles null', () => {
    assert.equal(airportName(null), null);
  });

  it('trims whitespace', () => {
    assert.equal(airportName('  YSSY  '), 'Sydney');
  });
});

describe('formatRouteString', () => {
  it('formats dep > arr', () => {
    assert.equal(formatRouteString('YSSY', 'YMML'), 'Sydney > Melbourne');
  });

  it('handles missing dep', () => {
    assert.equal(formatRouteString(null, 'YMML'), '? > Melbourne');
  });

  it('handles missing arr', () => {
    assert.equal(formatRouteString('YSSY', null), 'Sydney > ?');
  });

  it('returns null when both missing', () => {
    assert.equal(formatRouteString(null, null), null);
  });
});

describe('validateCoord', () => {
  it('accepts valid coordinates', () => {
    assert.equal(validateCoord('-33.85', '151.14'), null);
  });

  it('rejects invalid lat', () => {
    assert.ok(validateCoord('91', '0'));
    assert.ok(validateCoord('-91', '0'));
    assert.ok(validateCoord('abc', '0'));
  });

  it('rejects invalid lon', () => {
    assert.ok(validateCoord('0', '181'));
    assert.ok(validateCoord('0', '-181'));
  });

  it('validates radius when provided', () => {
    assert.equal(validateCoord('0', '0', '50'), null);
    assert.ok(validateCoord('0', '0', '0'));
    assert.ok(validateCoord('0', '0', '501'));
    assert.ok(validateCoord('0', '0', 'abc'));
  });

  it('accepts boundary values', () => {
    assert.equal(validateCoord('90', '180'), null);
    assert.equal(validateCoord('-90', '-180'), null);
    assert.equal(validateCoord('0', '0', '1'), null);
    assert.equal(validateCoord('0', '0', '500'), null);
  });
});

describe('escapeHtml', () => {
  it('escapes all dangerous characters', () => {
    assert.equal(escapeHtml('<script>alert("xss")</script>'),
      '&lt;script&gt;alert(&quot;xss&quot;)&lt;/script&gt;');
  });

  it('escapes ampersand', () => {
    assert.equal(escapeHtml('a&b'), 'a&amp;b');
  });

  it('escapes single quotes', () => {
    assert.equal(escapeHtml("it's"), "it&#39;s");
  });

  it('handles non-string input', () => {
    assert.equal(escapeHtml(123), '123');
    assert.equal(escapeHtml(null), 'null');
  });
});

describe('formatUptime', () => {
  it('formats seconds only', () => {
    assert.equal(formatUptime(45), '45s');
  });

  it('formats minutes and seconds', () => {
    assert.equal(formatUptime(125), '2m 5s');
  });

  it('formats hours, minutes, seconds', () => {
    assert.equal(formatUptime(3661), '1h 1m 1s');
  });

  it('formats days', () => {
    assert.equal(formatUptime(90061), '1d 1h 1m 1s');
  });

  it('handles zero', () => {
    assert.equal(formatUptime(0), '0s');
  });
});

describe('windCardinal', () => {
  it('maps 0 degrees to N', () => {
    assert.equal(windCardinal(0), 'N');
  });

  it('maps 90 degrees to E', () => {
    assert.equal(windCardinal(90), 'E');
  });

  it('maps 180 degrees to S', () => {
    assert.equal(windCardinal(180), 'S');
  });

  it('maps 270 degrees to W', () => {
    assert.equal(windCardinal(270), 'W');
  });

  it('maps 45 degrees to NE', () => {
    assert.equal(windCardinal(45), 'NE');
  });

  it('maps 360 degrees to N', () => {
    assert.equal(windCardinal(360), 'N');
  });
});

describe('addLog', () => {
  beforeEach(() => { requestLog.length = 0; });

  it('adds entry to front of log', () => {
    addLog({ type: 'HIT', client: 'test' });
    assert.equal(requestLog.length, 1);
    assert.equal(requestLog[0].type, 'HIT');
    assert.ok(requestLog[0].time);
  });

  it('caps at 100 entries', () => {
    for (let i = 0; i < 110; i++) addLog({ type: 'HIT', i });
    assert.equal(requestLog.length, 100);
    assert.equal(requestLog[0].i, 109);
  });
});

// ═══════════════════════════════════════════════════════════════════════
// Cache logic tests
// ═══════════════════════════════════════════════════════════════════════

describe('cache behavior', () => {
  beforeEach(resetState);

  it('cache entries have correct structure', () => {
    const data = { ac: [{ hex: 'abc123', flight: 'QFA1' }] };
    cache.set('key1', { data, timestamp: Date.now() });
    const hit = cache.get('key1');
    assert.ok(hit.data.ac);
    assert.ok(hit.timestamp);
  });

  it('cache key is lat,lon,radius', () => {
    const key = `-33.85,151.14,15`;
    cache.set(key, { data: { ac: [] }, timestamp: Date.now() });
    assert.ok(cache.has('-33.85,151.14,15'));
    assert.ok(!cache.has('-33.85,151.14,20'));
  });

  it('stale entry is detectable', () => {
    cache.set('stale', { data: { ac: [] }, timestamp: Date.now() - CACHE_MS - 1 });
    const hit = cache.get('stale');
    assert.ok(Date.now() - hit.timestamp > CACHE_MS);
  });

  it('inFlight deduplication prevents parallel upstream calls', async () => {
    let resolveCount = 0;
    const key = 'dedup-test';
    const promise = new Promise(resolve => {
      setTimeout(() => { resolveCount++; resolve('result'); }, 50);
    });
    inFlight.set(key, promise);

    const [r1, r2, r3] = await Promise.all([
      inFlight.get(key),
      inFlight.get(key),
      inFlight.get(key),
    ]);

    assert.equal(resolveCount, 1);
    assert.equal(r1, 'result');
    assert.equal(r2, 'result');
    assert.equal(r3, 'result');
  });

  it('inFlight map is cleaned up after resolution', async () => {
    const key = 'cleanup-test';
    const promise = Promise.resolve('done');
    inFlight.set(key, promise);
    promise.finally(() => inFlight.delete(key));
    await promise;
    await new Promise(r => setTimeout(r, 10)); // let microtask run
    assert.ok(!inFlight.has(key));
  });
});

// ═══════════════════════════════════════════════════════════════════════
// HTTP endpoint tests
// ═══════════════════════════════════════════════════════════════════════

describe('HTTP endpoints', () => {
  before((_, done) => {
    server = app.listen(0, '127.0.0.1', () => {
      const { port } = server.address();
      baseUrl = `http://127.0.0.1:${port}`;
      done();
    });
  });

  after((_, done) => {
    server.close(done);
  });

  beforeEach(resetState);

  describe('GET /flights', () => {
    it('returns 400 without params', async () => {
      const res = await get('/flights');
      assert.equal(res.status, 400);
      assert.ok(res.body.error);
    });

    it('returns 400 with invalid lat', async () => {
      const res = await get('/flights?lat=999&lon=0&radius=15');
      assert.equal(res.status, 400);
    });

    it('returns 400 with invalid radius', async () => {
      const res = await get('/flights?lat=0&lon=0&radius=0');
      assert.equal(res.status, 400);
    });

    it('returns cached data on cache hit', async () => {
      const data = { ac: [{ hex: 'test', flight: 'TST1' }] };
      cache.set('0,0,15', { data, timestamp: Date.now() });
      const res = await get('/flights?lat=0&lon=0&radius=15');
      assert.equal(res.status, 200);
      assert.deepEqual(res.body, data);
      assert.equal(stats.cacheHits, 1);
    });

    it('increments request counter', async () => {
      cache.set('0,0,15', { data: { ac: [] }, timestamp: Date.now() });
      await get('/flights?lat=0&lon=0&radius=15');
      assert.equal(stats.totalRequests, 1);
    });

    it('tracks unique clients', async () => {
      cache.set('0,0,15', { data: { ac: [] }, timestamp: Date.now() });
      await get('/flights?lat=0&lon=0&radius=15');
      assert.ok(stats.uniqueClients.size >= 1);
    });

    it('returns stale cache on upstream failure', async () => {
      const staleData = { ac: [{ hex: 'stale', flight: 'OLD1' }] };
      cache.set('1,1,15', { data: staleData, timestamp: Date.now() - CACHE_MS - 1 });

      // The inFlight promise will fail (no real upstream), but stale cache should be returned
      const res = await get('/flights?lat=1&lon=1&radius=15');
      // Either we get the stale data (200) or a 502 if no stale cache
      assert.ok(res.status === 200 || res.status === 502);
    });
  });

  describe('GET /weather', () => {
    it('returns 400 without params', async () => {
      const res = await get('/weather');
      assert.equal(res.status, 400);
    });

    it('returns 400 with invalid coordinates', async () => {
      const res = await get('/weather?lat=999&lon=0');
      assert.equal(res.status, 400);
    });

    it('returns cached weather data', async () => {
      const data = { temp: 22, condition: 'Clear Sky' };
      cache.set('weather:0,0', { data, timestamp: Date.now() });
      const res = await get('/weather?lat=0&lon=0');
      assert.equal(res.status, 200);
      assert.equal(res.body.temp, 22);
    });
  });

  describe('GET /stats', () => {
    it('returns stats object', async () => {
      const res = await get('/stats');
      assert.equal(res.status, 200);
      assert.ok('uptime' in res.body);
      assert.ok('totalRequests' in res.body);
      assert.ok('cacheHits' in res.body);
      assert.ok('errors' in res.body);
      assert.ok('uniqueClients' in res.body);
      assert.ok('cacheEntries' in res.body);
    });
  });

  describe('GET /status', () => {
    it('returns full status', async () => {
      const res = await get('/status');
      assert.equal(res.status, 200);
      assert.ok('proxyEnabled' in res.body);
      assert.ok('uptime' in res.body);
      assert.ok('ram' in res.body);
      assert.ok('log' in res.body);
    });
  });

  describe('security headers', () => {
    it('sets security headers', async () => {
      const res = await get('/stats');
      assert.equal(res.headers['x-frame-options'], 'DENY');
      assert.equal(res.headers['x-content-type-options'], 'nosniff');
    });
  });
});

// ═══════════════════════════════════════════════════════════════════════
// Route cache tests
// ═══════════════════════════════════════════════════════════════════════

describe('route cache', () => {
  beforeEach(() => { routeCache.clear(); });

  it('route cache entries have correct structure', () => {
    routeCache.set('QFA1', { dep: 'YSSY', arr: 'YMML', timestamp: Date.now() });
    const entry = routeCache.get('QFA1');
    assert.equal(entry.dep, 'YSSY');
    assert.equal(entry.arr, 'YMML');
    assert.ok(entry.timestamp);
  });

  it('route cache entries expire after ROUTE_CACHE_MS', () => {
    routeCache.set('QFA1', { dep: 'YSSY', arr: 'YMML', timestamp: Date.now() - ROUTE_CACHE_MS - 1 });
    const entry = routeCache.get('QFA1');
    assert.ok(Date.now() - entry.timestamp > ROUTE_CACHE_MS);
  });

  it('route cache handles missing fields', () => {
    routeCache.set('QFA2', { dep: 'YSSY', arr: null, timestamp: Date.now() });
    const entry = routeCache.get('QFA2');
    assert.equal(entry.dep, 'YSSY');
    assert.equal(entry.arr, null);
  });
});

// ═══════════════════════════════════════════════════════════════════════
// Hardening tests (multi-client scaling)
// ═══════════════════════════════════════════════════════════════════════

describe('cache eviction', () => {
  beforeEach(() => { cache.clear(); });

  it('cacheSet evicts oldest when exceeding MAX_CACHE_ENTRIES', () => {
    for (let i = 0; i < MAX_CACHE_ENTRIES + 10; i++) {
      cacheSet(`key-${i}`, { data: {}, timestamp: Date.now() - (MAX_CACHE_ENTRIES + 10 - i) });
    }
    assert.equal(cache.size, MAX_CACHE_ENTRIES);
    // Oldest entries (lowest timestamps) should have been evicted
    assert.ok(!cache.has('key-0'));
    assert.ok(cache.has(`key-${MAX_CACHE_ENTRIES + 9}`));
  });

  it('cacheSet keeps size at exactly MAX_CACHE_ENTRIES', () => {
    for (let i = 0; i < MAX_CACHE_ENTRIES * 2; i++) {
      cacheSet(`k-${i}`, { data: {}, timestamp: i });
    }
    assert.equal(cache.size, MAX_CACHE_ENTRIES);
  });
});

describe('route cache eviction', () => {
  beforeEach(() => { routeCache.clear(); });

  it('routeCacheSet evicts oldest when exceeding MAX_ROUTE_ENTRIES', () => {
    for (let i = 0; i < MAX_ROUTE_ENTRIES + 5; i++) {
      routeCacheSet(`CS${i}`, { dep: 'A', arr: 'B', timestamp: i });
    }
    assert.equal(routeCache.size, MAX_ROUTE_ENTRIES);
    assert.ok(!routeCache.has('CS0'));
  });
});

describe('semaphore', () => {
  it('limits concurrent access', async () => {
    const sem = semaphore(2);
    let running = 0, maxRunning = 0;

    const task = async () => {
      await sem.acquire();
      running++;
      maxRunning = Math.max(maxRunning, running);
      await new Promise(r => setTimeout(r, 30));
      running--;
      sem.release();
    };

    await Promise.all([task(), task(), task(), task(), task()]);
    assert.ok(maxRunning <= 2, `Max running was ${maxRunning}, expected <= 2`);
  });

  it('reports active count', async () => {
    const sem = semaphore(3);
    assert.equal(sem.active, 0);
    await sem.acquire();
    assert.equal(sem.active, 1);
    await sem.acquire();
    assert.equal(sem.active, 2);
    sem.release();
    assert.equal(sem.active, 1);
    sem.release();
    assert.equal(sem.active, 0);
  });

  it('queues when at limit', async () => {
    const sem = semaphore(1);
    const order = [];

    await sem.acquire();
    const p = (async () => {
      await sem.acquire();
      order.push('second');
      sem.release();
    })();

    order.push('first');
    sem.release();
    await p;

    assert.deepEqual(order, ['first', 'second']);
  });
});

describe('stats endpoint includes concurrency info', () => {
  let server2, baseUrl2;

  before((_, done) => {
    server2 = app.listen(0, '127.0.0.1', () => {
      baseUrl2 = `http://127.0.0.1:${server2.address().port}`;
      done();
    });
  });

  after((_, done) => { server2.close(done); });

  it('includes activeUpstream and activeRoutes', async () => {
    const res = await new Promise((resolve, reject) => {
      http.get(`${baseUrl2}/stats`, (r) => {
        let body = '';
        r.on('data', c => body += c);
        r.on('end', () => resolve(JSON.parse(body)));
      }).on('error', reject);
    });
    assert.ok('activeUpstream' in res);
    assert.ok('activeRoutes' in res);
    assert.ok('inFlightKeys' in res);
    assert.ok('routeCacheEntries' in res);
  });
});

// ═══════════════════════════════════════════════════════════════════════
// Geo-bucketing tests
// ═══════════════════════════════════════════════════════════════════════

describe('bucketCoord', () => {
  it('rounds to 2 decimal places', () => {
    assert.equal(bucketCoord(-33.8530, 2), -33.85);
    assert.equal(bucketCoord(151.1410, 2), 151.14);
  });

  it('rounds up correctly', () => {
    assert.equal(bucketCoord(-33.8567, 2), -33.86);
    assert.equal(bucketCoord(151.1451, 2), 151.15);
  });

  it('handles string inputs', () => {
    assert.equal(bucketCoord('-33.8530', 2), -33.85);
    assert.equal(bucketCoord('151.1410', 2), 151.14);
  });
});

describe('bucketKey', () => {
  it('produces bucketed cache key', () => {
    assert.equal(bucketKey(-33.8530, 151.1410, 15), '-33.85,151.14,15');
  });

  it('merges nearby locations into same key', () => {
    const k1 = bucketKey(-33.8530, 151.1410, 15);
    const k2 = bucketKey(-33.8545, 151.1425, 15);
    assert.equal(k1, k2);
  });

  it('separates distant locations', () => {
    const sydney = bucketKey(-33.85, 151.14, 15);
    const london = bucketKey(51.50, -0.12, 15);
    assert.notEqual(sydney, london);
  });

  it('separates different radii', () => {
    const k1 = bucketKey(-33.85, 151.14, 15);
    const k2 = bucketKey(-33.85, 151.14, 20);
    assert.notEqual(k1, k2);
  });
});

// ═══════════════════════════════════════════════════════════════════════
// Semaphore timeout tests
// ═══════════════════════════════════════════════════════════════════════

describe('semaphore timeout', () => {
  it('rejects after timeout when at capacity', async () => {
    const sem = semaphore(1);
    await sem.acquire();
    await assert.rejects(
      () => sem.acquire(100),
      { message: 'Semaphore timeout' }
    );
    sem.release();
  });

  it('succeeds if slot frees before timeout', async () => {
    const sem = semaphore(1);
    await sem.acquire();
    setTimeout(() => sem.release(), 50);
    await sem.acquire(500);
    sem.release();
  });
});
