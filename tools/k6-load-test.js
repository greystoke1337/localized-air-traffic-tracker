// k6 distributed load test — run from k6 Cloud for real multi-IP testing.
// Usage:
//   k6 run tools/k6-load-test.js                          # local (50 VUs, 60s)
//   k6 cloud tools/k6-load-test.js                        # k6 Cloud (real multi-IP)
//   K6_TARGET=https://api.overheadtracker.com k6 run ...   # custom target

import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend } from 'k6/metrics';

const BASE = __ENV.K6_TARGET || 'https://api.overheadtracker.com';

const errorRate = new Rate('errors');
const flightLatency = new Trend('flight_latency', true);

export const options = {
  stages: [
    { duration: '10s', target: 20 },
    { duration: '40s', target: 50 },
    { duration: '10s', target: 0 },
  ],
  thresholds: {
    errors: ['rate<0.1'],
    flight_latency: ['p(95)<5000', 'p(99)<12000'],
  },
};

const locations = [
  { lat: 41.90, lon: -87.68, radius: 25, name: 'Chicago' },
  { lat: 41.92, lon: -87.65, radius: 20, name: 'Chicago-N' },
  { lat: 41.88, lon: -87.72, radius: 25, name: 'Chicago-W' },
  { lat: -33.85, lon: 151.14, radius: 15, name: 'Sydney' },
  { lat: 51.47, lon: -0.46, radius: 15, name: 'London' },
  { lat: 40.64, lon: -73.78, radius: 15, name: 'NewYork' },
];

export default function () {
  const loc = locations[Math.floor(Math.random() * locations.length)];
  const url = `${BASE}/flights?lat=${loc.lat}&lon=${loc.lon}&radius=${loc.radius}`;
  const res = http.get(url, { timeout: '15s' });

  flightLatency.add(res.timings.duration);
  const ok = check(res, {
    'status is 200': (r) => r.status === 200,
    'has aircraft array': (r) => {
      try { return JSON.parse(r.body).ac !== undefined; }
      catch { return false; }
    },
  });
  errorRate.add(!ok);

  sleep(Math.random() * 5 + 5);
}
