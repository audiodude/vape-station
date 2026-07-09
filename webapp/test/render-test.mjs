// Offline verification for the web engine, mirroring Tests/RenderTest.cpp.
// Run: node webapp/test/render-test.mjs   (exit code = failed checks)

import { Engine, grainTables } from '../js/engine.js';
import { D, S, PARAMS } from '../js/params.js';

const SR = 48000;
let fails = 0;
const check = (ok, name, detail = '') => {
  console.log(`${ok ? '[PASS]' : '[FAIL]'} ${name}${detail ? `  (${detail})` : ''}`);
  if (!ok) fails++;
};

function render(engine, seconds, events) {
  const total = Math.floor(seconds * SR);
  const L = new Float32Array(total), R = new Float32Array(total);
  const evs = [...events].sort((a, b) => a.t - b.t);
  let pos = 0, ei = 0;
  const block = 128;
  while (pos < total) {
    while (ei < evs.length && Math.floor(evs[ei].t * SR) <= pos) {
      const e = evs[ei++];
      if (e.on) engine.noteOn(e.note, e.vel ?? 0.8);
      else engine.noteOff(e.note);
    }
    const n = Math.min(block, total - pos);
    const l = L.subarray(pos, pos + n), r = R.subarray(pos, pos + n);
    engine.process(l, r, n);
    pos += n;
  }
  return { L, R };
}

const rms = (buf, t0, t1) => {
  const i0 = Math.floor(t0 * SR), i1 = Math.floor(t1 * SR);
  let s = 0;
  for (let i = i0; i < i1; i++) s += buf.L[i] * buf.L[i] + buf.R[i] * buf.R[i];
  return Math.sqrt(s / (2 * (i1 - i0)));
};
const finite = (buf) => buf.L.every(Number.isFinite) && buf.R.every(Number.isFinite);
const relDiff = (a, b) => {
  let d = 0, ref = 0;
  for (let i = 0; i < a.L.length; i++) {
    d += (a.L[i] - b.L[i]) ** 2 + (a.R[i] - b.R[i]) ** 2;
    ref += a.L[i] ** 2 + a.R[i] ** 2;
  }
  return ref > 0 ? Math.sqrt(d / ref) : 0;
};

// T0: tables build and are non-trivial
{
  const t0 = Date.now();
  const tables = grainTables();
  const ms = Date.now() - t0;
  let ok = tables.length === 5;
  for (const t of tables) {
    let maxAbs = 0;
    for (let i = 0; i < 2049; i++) maxAbs = Math.max(maxAbs, Math.abs(t.mips[0][i]));
    ok = ok && maxAbs > 0.01 && maxAbs < 4;
  }
  check(ok, 'graintables built', `${ms} ms`);
}

// T1: silence without notes
{
  const e = new Engine(SR);
  const buf = render(e, 1.0, []);
  check(rms(buf, 0, 1) < 1e-6, 'silence without notes');
}

// T2: a note produces audio, is finite, and releases to silence
{
  const e = new Engine(SR);
  const buf = render(e, 4.5, [{ t: 0, on: true, note: 60 }, { t: 1.5, on: false, note: 60 }]);
  check(finite(buf), 'output is finite');
  const body = rms(buf, 0.2, 1.0);
  check(body > 0.005, 'note produces audio', `rms=${body.toFixed(5)}`);
  const tail = rms(buf, 4.3, 4.5);
  check(tail < 1e-4, 'release decays to silence', `rms=${tail.toFixed(7)}`);
}

// T3: a mod route audibly changes the output
{
  const mk = (routes) => {
    const e = new Engine(SR);
    e.tableIndex = 1; // Sweep
    e.setParam('lfo1Rate', 3.0);
    e.setMatrix(routes);
    return render(e, 2.5, [{ t: 0, on: true, note: 48 }, { t: 2.2, on: false, note: 48 }]);
  };
  const a = mk([]);
  const b = mk([{ src: S.LFO1, dst: D.position, depth: 0.9 }]);
  const d = relDiff(a, b);
  check(d > 0.05, 'LFO->position route changes output', `relDiff=${d.toFixed(4)}`);
  check(finite(b), 'modulated output is finite');
}

// T4: LFO modes — retrig repeats per note, global free-runs
{
  const two = (mode) => {
    const e = new Engine(SR);
    e.tableIndex = 1;
    e.setParam('spray', 0);
    e.setParam('spread', 0);
    e.setParam('lfo1Rate', 0.7);
    e.lfoModes[0] = mode;
    e.setMatrix([{ src: S.LFO1, dst: D.position, depth: 0.9 }]);
    return render(e, 2.0, [
      { t: 0, on: true, note: 48 }, { t: 0.35, on: false, note: 48 },
      { t: 1.0, on: true, note: 48 }, { t: 1.35, on: false, note: 48 },
    ]);
  };
  const seg = (buf, tA, tB, len) => {
    const iA = Math.floor(tA * SR), iB = Math.floor(tB * SR), n = Math.floor(len * SR);
    let d = 0, ref = 0;
    for (let i = 0; i < n; i++) {
      d += (buf.L[iA + i] - buf.L[iB + i]) ** 2 + (buf.R[iA + i] - buf.R[iB + i]) ** 2;
      ref += buf.L[iA + i] ** 2 + buf.R[iA + i] ** 2;
    }
    return ref > 0 ? Math.sqrt(d / ref) : 0;
  };
  const dRetrig = seg(two(0), 0, 1.0, 0.4);
  const dGlobal = seg(two(2), 0, 1.0, 0.4);
  check(dRetrig < 0.05, 'retrig LFO repeats per note', `relDiff=${dRetrig.toFixed(4)}`);
  check(dGlobal > 0.2, 'global LFO free-runs across notes', `relDiff=${dGlobal.toFixed(4)}`);
}

// T5: env time taper hits clock anchors
{
  const r = PARAMS.env1A.range;
  const ok = r.from01(0) === 0
    && Math.abs(r.from01(1 / 6) - 250) < 0.1
    && Math.abs(r.from01(0.5) - 1000) < 0.5
    && Math.abs(r.from01(5 / 6) - 4000) < 2
    && Math.abs(r.from01(1) - 10000) < 5
    && Math.abs(PARAMS.env1R.range.from01(0) - 5) < 1e-4;
  check(ok, 'env taper + release floor match the plugin');
}

// T6: polyphony + chord renders clean
{
  const e = new Engine(SR);
  const evs = [];
  [48, 55, 60, 63].forEach((n, i) => {
    evs.push({ t: 0.02 * i, on: true, note: n });
    evs.push({ t: 1.5, on: false, note: n });
  });
  const buf = render(e, 2.5, evs);
  check(finite(buf) && rms(buf, 0.3, 1.2) > 0.01, 'chord renders', `rms=${rms(buf, 0.3, 1.2).toFixed(4)}`);
  let peak = 0;
  for (let i = 0; i < buf.L.length; i++) peak = Math.max(peak, Math.abs(buf.L[i]), Math.abs(buf.R[i]));
  check(peak <= 1.0, 'output soft-clipped within +/-1', `peak=${peak.toFixed(3)}`);
}

console.log(fails === 0 ? 'ALL TESTS PASSED' : 'TESTS FAILED');
process.exit(fails);
