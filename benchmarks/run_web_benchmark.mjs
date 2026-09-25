// Runs the web build's benchmark mode (index.html?benchmark=N) in headless
// Chromium, stores the full report in docs/benchmarks/results/, appends a
// summary row to docs/benchmarks/results.md and prints the change against the
// previous run. docs/ is git-ignored: results are local records, not code.
//
// Usage: node run_web_benchmark.mjs <site dir> <frames> [label]
// Env:   GIT_COMMIT, GIT_DIRTY (set by scripts/bench_web.sh)

import { chromium } from 'playwright-core';
import fs from 'node:fs';
import path from 'node:path';
import { serve } from './web_server.mjs';

const [siteDir, framesArg = '2000', label = ''] = process.argv.slice(2);
const frames = Number(framesArg);
const benchDir = path.dirname(new URL(import.meta.url).pathname);
const recordsDir = path.join(benchDir, '..', 'docs', 'benchmarks');
const resultsDir = path.join(recordsDir, 'results');
const summaryFile = path.join(recordsDir, 'results.md');

function fileKb(name) {
  return Math.round(fs.statSync(path.join(siteDir, name)).size / 1024);
}

async function run() {
  const server = await serve(siteDir);
  const url = `http://127.0.0.1:${server.address().port}/index.html?benchmark=${frames}`;
  const browser = await chromium.launch({
    headless: true,
    args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader',
           '--autoplay-policy=no-user-gesture-required'],
  });
  const page = await browser.newPage({ viewport: { width: 1280, height: 1000 } });

  const errors = [];
  const report = new Promise((resolve, reject) => {
    page.on('console', (msg) => {
      const text = msg.text();
      if (text.startsWith('BENCHMARK_RESULT ')) resolve(JSON.parse(text.slice(17)));
      if (msg.type() === 'error') errors.push(text);
    });
    page.on('pageerror', (err) => reject(err));
    const timeout = setTimeout(
      () => reject(new Error(`timed out; console errors: ${errors.join(' | ')}`)),
      10 * 60 * 1000);
    timeout.unref();
  });

  const started = Date.now();
  await page.goto(url);
  const result = await report;
  const page_info = await page.evaluate(() => ({
    runtime_ready_ms: window.runtimeReadyMs ?? null,
    cross_origin_isolated: window.crossOriginIsolated,
  }));
  const browserVersion = browser.version();
  await browser.close();
  server.closeAllConnections();
  server.close();

  return {
    date: new Date().toISOString(),
    commit: process.env.GIT_COMMIT ?? 'unknown',
    dirty: process.env.GIT_DIRTY === '1',
    label,
    browser: `Chromium ${browserVersion} (headless, SwiftShader WebGL)`,
    wall_time_s: (Date.now() - started) / 1000,
    ...page_info,
    build_kb: { wasm: fileKb('index.wasm'), js: fileKb('index.js'), data: fileKb('index.data') },
    ...result,
  };
}

const COLUMNS = [
  ['Frame mean', (r) => r.sections_ms.frame.mean],
  ['Frame p95', (r) => r.sections_ms.frame.p95],
  ['Frame p99', (r) => r.sections_ms.frame.p99],
  ['Enemies', (r) => r.sections_ms.update_enemies.mean],
  ['Pathfinding', (r) => r.sections_ms.pathfinding.mean],
  ['Line of sight', (r) => r.sections_ms.line_of_sight.mean],
  ['Player', (r) => r.sections_ms.update_player.mean],
  ['Camera', (r) => r.sections_ms.camera.mean],
  ['Render', (r) => r.sections_ms.render.mean],
  ['Walls', (r) => r.sections_ms.render_walls.mean],
  ['Objects', (r) => r.sections_ms.render_objects.mean],
  ['Draw', (r) => r.sections_ms.render_draw.mean],
  ['HUD', (r) => r.sections_ms.render_hud.mean],
  ['Present', (r) => r.sections_ms.present.mean],
  ['Allocs/frame', (r) => r.allocations_per_frame.mean],
  ['KB alloc/frame', (r) => r.allocated_bytes_per_frame.mean / 1024],
  ['Init ms', (r) => r.startup_ms],
  ['Load ms', (r) => r.runtime_ready_ms],
  ['wasm KB', (r) => r.build_kb.wasm],
  ['data KB', (r) => r.build_kb.data],
];

function fmt(value) {
  if (value === null || value === undefined) return '-';
  return Math.abs(value) >= 100 ? value.toFixed(0) : value.toFixed(3);
}

function appendSummary(result) {
  if (!fs.existsSync(summaryFile)) {
    fs.mkdirSync(recordsDir, { recursive: true });
    const header = ['Date', 'Commit', 'Label', ...COLUMNS.map(([name]) => name)];
    fs.writeFileSync(summaryFile, [
      '# Benchmark results',
      '',
      'Web build, `scripts/bench_web.sh`. Times are per frame in milliseconds (mean unless',
      'noted), measured after warmup frames. Nested sections (e.g. Pathfinding inside',
      'Enemies, Camera inside Player, Walls/Objects/Draw/HUD/Present inside Render) do',
      'not add up to Frame. Full reports are in `results/`.',
      '',
      'Runs use headless Chromium with software WebGL (SwiftShader) in Docker and only',
      'measure main-thread CPU time, so compare rows from the same machine. Frame mean',
      'and allocation counts repeat within ~1% between identical runs; p99 and the',
      'smallest sections vary by up to ~20%.',
      '',
      `| ${header.join(' | ')} |`,
      `|${header.map(() => '---').join('|')}|`,
      '',
    ].join('\n'));
  }
  const commit = result.commit + (result.dirty ? '+dirty' : '');
  const row = [result.date.slice(0, 16).replace('T', ' '), commit, result.label || '-',
               ...COLUMNS.map(([, get]) => fmt(get(result)))];
  fs.appendFileSync(summaryFile, `| ${row.join(' | ')} |\n`);
}

function previousResult() {
  if (!fs.existsSync(resultsDir)) return null;
  const files = fs.readdirSync(resultsDir).filter((f) => f.endsWith('.json')).sort();
  return files.length ? JSON.parse(fs.readFileSync(path.join(resultsDir, files.at(-1)), 'utf8')) : null;
}

function printComparison(result, previous) {
  console.log(`\n${result.frames} frames, cross-origin isolated: ${result.cross_origin_isolated}`);
  if (previous) console.log(`compared with ${previous.commit} ${previous.label || ''} (${previous.date})`);
  const rows = COLUMNS.map(([name, get]) => {
    const now = get(result);
    const before = previous ? get(previous) : null;
    const change = before ? `${(((now - before) / before) * 100).toFixed(1)}%` : '';
    return [name, fmt(now), previous ? fmt(before) : '', change];
  });
  for (const [name, now, before, change] of rows) {
    console.log(`${name.padEnd(16)} ${now.padStart(10)} ${before.padStart(10)} ${change.padStart(9)}`);
  }
}

const result = await run();
const previous = previousResult();
fs.mkdirSync(resultsDir, { recursive: true });
const stamp = result.date.replace(/[:.]/g, '-').slice(0, 19);
const name = [stamp, result.commit, result.label].filter(Boolean).join('_').replace(/[^\w.-]/g, '-');
fs.writeFileSync(path.join(resultsDir, `${name}.json`), JSON.stringify(result, null, 1));
appendSummary(result);
printComparison(result, previous);
