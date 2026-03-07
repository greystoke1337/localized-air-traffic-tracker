#!/usr/bin/env node
// Serial log analyzer for ESP32 stress testing.
// Zero dependencies — uses only Node.js built-in modules.
//
// Usage:  node tools/serial-stress.js <logfile>
//
// Analyzes a serial log (from build.sh log or stress) for:
//   - Reboots (boot messages, rst: codes)
//   - WDT resets (Guru Meditation, panic)
//   - Crash backtraces
//   - Heap tracking (min free, min max-block)
//   - Fetch failures
//   - State transitions (WX ↔ FLIGHT)

const fs = require('fs');
const path = require('path');

const logfile = process.argv[2];
if (!logfile || !fs.existsSync(logfile)) {
  console.error('Usage: node tools/serial-stress.js <logfile>');
  process.exit(1);
}

const lines = fs.readFileSync(logfile, 'utf-8').split('\n');

const stats = {
  reboots: [],
  wdtResets: [],
  backtraces: [],
  heapMin: Infinity,
  heapMinAt: '',
  maxBlockMin: Infinity,
  maxBlockMinAt: '',
  fetchTotal: 0,
  fetchFail: 0,
  wxToFlight: 0,
  flightToWx: 0,
  heapWarnings: 0,
  duration: 0,
};

// Extract timestamp from log line: [   12.345] ...
function getTimestamp(line) {
  const m = line.match(/^\[\s*([\d.]+)\]/);
  return m ? parseFloat(m[1]) : 0;
}

let lastTimestamp = 0;

for (const line of lines) {
  if (!line.trim()) continue;
  const ts = getTimestamp(line);
  if (ts > lastTimestamp) lastTimestamp = ts;

  // Reboots
  if (/\brst:0x[0-9a-f]/i.test(line) || /\bboot:0x[0-9a-f]/i.test(line) ||
      /OVERHEAD TRACKER/.test(line) || /^ets [A-Z]/i.test(line.replace(/^\[[\d.\s]+\]\s*/, ''))) {
    stats.reboots.push({ ts, line: line.trim().slice(0, 100) });
  }

  // WDT / panic
  if (/Guru Meditation/i.test(line) || /Task watchdog/i.test(line) || /\bpanic\b/i.test(line)) {
    stats.wdtResets.push({ ts, line: line.trim().slice(0, 100) });
  }

  // Backtraces
  if (/Backtrace:\s*0x/i.test(line)) {
    stats.backtraces.push({ ts, line: line.trim().slice(0, 120) });
  }

  // Heap tracking: [HEAP] Free:NNN MaxBlock:NNN  or  [FETCH] Start (heap NNN, maxblk NNN)
  const heapMatch = line.match(/Free[:\s]+(\d+)/);
  const maxBlkMatch = line.match(/Max(?:Block|blk)[:\s]+(\d+)/);
  if (heapMatch) {
    const free = parseInt(heapMatch[1]);
    if (free < stats.heapMin) {
      stats.heapMin = free;
      stats.heapMinAt = `${(ts / 60).toFixed(1)}m`;
    }
  }
  if (maxBlkMatch) {
    const blk = parseInt(maxBlkMatch[1]);
    if (blk < stats.maxBlockMin) {
      stats.maxBlockMin = blk;
      stats.maxBlockMinAt = `${(ts / 60).toFixed(1)}m`;
    }
  }

  // Heap fragmentation warning
  if (/heap fragmented/i.test(line)) {
    stats.heapWarnings++;
  }

  // Fetch tracking
  if (/\[FETCH\] Start/.test(line)) {
    stats.fetchTotal++;
  }
  if (/ALL SOURCES FAILED|CACHE CORRUPT|BAD RESPONSE|Network failed/.test(line)) {
    stats.fetchFail++;
  }

  // State transitions
  if (/No flights, switching to weather/.test(line)) {
    stats.flightToWx++;
  }
  if (/\[FETCH\].*Start.*heap/.test(line)) {
    // Count flight appearances by looking at screen switches in the next cycle
  }
}

// Count WX→FLIGHT transitions from screen change patterns
for (let i = 0; i < lines.length - 1; i++) {
  if (/switching to weather/.test(lines[i])) {
    for (let j = i + 1; j < Math.min(i + 20, lines.length); j++) {
      if (/currentScreen = SCREEN_FLIGHT/.test(lines[j]) ||
          (/\[FETCH\]/.test(lines[j]) && !/No flights/.test(lines[j]) && /AC \d+\/\d+/.test(lines[j]))) {
        stats.wxToFlight++;
        break;
      }
    }
  }
}

stats.duration = lastTimestamp;

// Deduplicate rapid reboots (within 5s of each other = same boot sequence)
function dedup(events, windowSec) {
  if (events.length <= 1) return events;
  const result = [events[0]];
  for (let i = 1; i < events.length; i++) {
    if (events[i].ts - events[i - 1].ts > windowSec) {
      result.push(events[i]);
    }
  }
  return result;
}
stats.reboots = dedup(stats.reboots, 5);
stats.wdtResets = dedup(stats.wdtResets, 5);

// Report
const durMin = (stats.duration / 60).toFixed(1);
const durSec = stats.duration.toFixed(0);
const failPct = stats.fetchTotal > 0
  ? ((stats.fetchFail / stats.fetchTotal) * 100).toFixed(1)
  : '0.0';

const passed = stats.reboots.length === 0 && stats.backtraces.length === 0 && stats.wdtResets.length === 0;

console.log('');
console.log('=== STRESS TEST REPORT ===');
console.log(`Duration:        ${durMin}m (${durSec}s)`);
console.log(`Reboots:         ${stats.reboots.length}`);
console.log(`WDT resets:      ${stats.wdtResets.length}`);
console.log(`Backtraces:      ${stats.backtraces.length}`);
console.log(`Min heap:        ${stats.heapMin === Infinity ? 'N/A' : stats.heapMin.toLocaleString()} ${stats.heapMinAt ? `(at ${stats.heapMinAt})` : ''}`);
console.log(`Min maxBlock:    ${stats.maxBlockMin === Infinity ? 'N/A' : stats.maxBlockMin.toLocaleString()} ${stats.maxBlockMinAt ? `(at ${stats.maxBlockMinAt})` : ''}`);
console.log(`Heap warnings:   ${stats.heapWarnings}`);
console.log(`Fetch failures:  ${stats.fetchFail} / ${stats.fetchTotal} (${failPct}%)`);
console.log(`FLIGHT->WX:      ${stats.flightToWx}`);
console.log('');

if (stats.reboots.length > 0) {
  console.log('--- Reboots ---');
  stats.reboots.forEach(e => console.log(`  [${(e.ts / 60).toFixed(1)}m] ${e.line}`));
  console.log('');
}

if (stats.wdtResets.length > 0) {
  console.log('--- WDT Resets ---');
  stats.wdtResets.forEach(e => console.log(`  [${(e.ts / 60).toFixed(1)}m] ${e.line}`));
  console.log('');
}

if (stats.backtraces.length > 0) {
  console.log('--- Backtraces ---');
  stats.backtraces.forEach(e => console.log(`  [${(e.ts / 60).toFixed(1)}m] ${e.line}`));
  console.log('');
}

console.log(`VERDICT: ${passed ? 'PASS' : 'FAIL'}`);
console.log('');

process.exit(passed ? 0 : 1);
