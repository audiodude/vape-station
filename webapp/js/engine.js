// VapeStation DSP engine — a JavaScript port of the C++ engine in Source/.
// Pure ES module: no DOM, no WebAudio. Used by the AudioWorklet processor and
// by the Node offline test harness.

import { DESTS, D, S, NUM_DESTS, PARAMS, defaultParams } from './params.js';

// ---------------------------------------------------------------------------
// Graintables (port of GrainTables.cpp)
// ---------------------------------------------------------------------------

export const NUM_FRAMES = 64;
export const FRAME_LEN = 2048;
export const MIP_CAPS = [64, 16, 4, 1];
const MAX_HARM = 64;

// juce::Random-compatible 48-bit LCG (used only at table-build time, so the
// Grit table matches the plugin bit-for-bit in spirit).
class JuceRandom {
  constructor(seed) { this.seed = BigInt(seed) & 0xffffffffffffn; }
  nextInt() {
    this.seed = (this.seed * 0x5deece66dn + 11n) & 0xffffffffffffn;
    return Number(BigInt.asIntN(32, this.seed >> 16n));
  }
  nextFloat() { return (this.nextInt() >>> 0) / 4294967296; }
}

// Complex in-place FFT, sign=+1 for the inverse transform (no 1/N scaling;
// frames are RMS-normalised afterwards, matching the C++).
function fftInPlace(re, im, sign) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      [re[i], re[j]] = [re[j], re[i]];
      [im[i], im[j]] = [im[j], im[i]];
    }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = (sign * 2 * Math.PI) / len;
    const wr = Math.cos(ang), wi = Math.sin(ang);
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0;
      for (let k = 0; k < len / 2; k++) {
        const ur = re[i + k], ui = im[i + k];
        const vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci;
        const vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
        re[i + k] = ur + vr; im[i + k] = ui + vi;
        re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
        const ncr = cr * wr - ci * wi;
        ci = cr * wi + ci * wr; cr = ncr;
      }
    }
  }
}

const lerp = (a, b, t) => a + t * (b - a);

// Each spec returns {re, im} arrays indexed 1..64 (harmonic amplitudes/phases).
function specMorph(x) {
  const re = new Float64Array(MAX_HARM + 1), im = new Float64Array(MAX_HARM + 1);
  for (let n = 1; n <= MAX_HARM; n++) {
    const odd = n % 2 === 1;
    const saw = 1 / n;
    const sqr = odd ? 1 / n : 0;
    const triSign = (((n - 1) >> 1) % 2 === 0) ? 1 : -1;
    const tri = odd ? triSign / (n * n) : 0;
    re[n] = x < 0.5 ? lerp(saw, sqr, x * 2) : lerp(sqr, tri * 8, (x - 0.5) * 2);
  }
  return { re, im };
}

function specSweep(x) {
  const re = new Float64Array(MAX_HARM + 1), im = new Float64Array(MAX_HARM + 1);
  const centre = 1 + 44 * x;
  for (let n = 1; n <= MAX_HARM; n++) {
    const d = (n - centre) / 3;
    re[n] = Math.exp(-0.5 * d * d) + 0.12 / n;
  }
  return { re, im };
}

const FORMANTS = [
  [730, 1090, 2440], [530, 1840, 2480], [270, 2290, 3010], [570, 840, 2410], [300, 870, 2240],
];
const FWEIGHTS = [1, 0.6, 0.35];

function specVox(x) {
  const re = new Float64Array(MAX_HARM + 1), im = new Float64Array(MAX_HARM + 1);
  const v = x * 4;
  const v0 = Math.min(4, Math.max(0, Math.floor(v)));
  const v1 = Math.min(4, v0 + 1);
  const vf = v - v0;
  for (let n = 1; n <= MAX_HARM; n++) {
    const freq = 110 * n;
    let a0 = 0, a1 = 0;
    for (let k = 0; k < 3; k++) {
      const bw0 = 0.10 * FORMANTS[v0][k] + 40;
      const d0 = (freq - FORMANTS[v0][k]) / bw0;
      a0 += FWEIGHTS[k] * Math.exp(-0.5 * d0 * d0);
      const bw1 = 0.10 * FORMANTS[v1][k] + 40;
      const d1 = (freq - FORMANTS[v1][k]) / bw1;
      a1 += FWEIGHTS[k] * Math.exp(-0.5 * d1 * d1);
    }
    re[n] = lerp(a0, a1, vf) + 0.04 / n;
  }
  return { re, im };
}

function specBells(x) {
  const re = new Float64Array(MAX_HARM + 1), im = new Float64Array(MAX_HARM + 1);
  const partials = [1, 3, 5, 9, 13, 19];
  const weights = [1, 0.8, 0.65, 0.5, 0.4, 0.3];
  for (let k = 0; k < 6; k++) {
    const a = weights[k] * Math.exp(-k * 3.5 * x);
    const p = partials[k];
    re[p] += a;
    if (p + 1 <= MAX_HARM) re[p + 1] += a * 0.3 * x;
  }
  return { re, im };
}

const gritData = (() => {
  const r = new JuceRandom(0xbeef);
  const amps = [new Float64Array(MAX_HARM + 1), new Float64Array(MAX_HARM + 1)];
  const phases = new Float64Array(MAX_HARM + 1);
  for (let n = 1; n <= MAX_HARM; n++) {
    for (let i = 0; i < 2; i++) {
      const u = r.nextFloat();
      amps[i][n] = Math.pow(u, 2.5) * Math.exp(-n / 20);
    }
    phases[n] = r.nextFloat() * 2 * Math.PI;
  }
  return { amps, phases };
})();

function specGrit(x) {
  const re = new Float64Array(MAX_HARM + 1), im = new Float64Array(MAX_HARM + 1);
  for (let n = 1; n <= MAX_HARM; n++) {
    const a = lerp(gritData.amps[0][n], gritData.amps[1][n], x);
    re[n] = a * Math.cos(gritData.phases[n]);
    im[n] = a * Math.sin(gritData.phases[n]);
  }
  return { re, im };
}

function buildTable(gen) {
  const N = FRAME_LEN;
  const mips = MIP_CAPS.map(() => new Float32Array(NUM_FRAMES * (N + 1)));
  const inRe = new Float64Array(N), inIm = new Float64Array(N);

  for (let f = 0; f < NUM_FRAMES; f++) {
    const x = f / (NUM_FRAMES - 1);
    const spec = gen(x);
    for (let m = 0; m < MIP_CAPS.length; m++) {
      const cap = MIP_CAPS[m];
      inRe.fill(0); inIm.fill(0);
      for (let n = 1; n <= cap; n++) {
        // Rotate by -i/2 so real amplitudes synthesise as a sine series.
        const cr = 0.5 * spec.im[n];
        const ci = -0.5 * spec.re[n];
        inRe[n] = cr; inIm[n] = ci;
        inRe[N - n] = cr; inIm[N - n] = -ci;
      }
      fftInPlace(inRe, inIm, +1);
      const dst = mips[m];
      const base = f * (N + 1);
      let sumSq = 0;
      for (let i = 0; i < N; i++) {
        dst[base + i] = inRe[i];
        sumSq += inRe[i] * inRe[i];
      }
      const rms = Math.sqrt(sumSq / N);
      if (rms > 1e-9) {
        const scale = 0.12 / rms;
        for (let i = 0; i < N; i++) dst[base + i] *= scale;
      }
      dst[base + N] = dst[base]; // wrap guard
    }
  }
  return mips;
}

let tablesCache = null;
export function grainTables() {
  if (!tablesCache) {
    tablesCache = [
      { name: 'Morph', mips: buildTable(specMorph) },
      { name: 'Sweep', mips: buildTable(specSweep) },
      { name: 'Vox',   mips: buildTable(specVox) },
      { name: 'Bells', mips: buildTable(specBells) },
      { name: 'Grit',  mips: buildTable(specGrit) },
    ];
  }
  return tablesCache;
}

function mipForFreq(freqHz, sampleRate) {
  for (let m = 0; m < MIP_CAPS.length; m++)
    if (freqHz * MIP_CAPS[m] < 0.45 * sampleRate) return m;
  return MIP_CAPS.length - 1;
}

// ---------------------------------------------------------------------------
// Small DSP pieces
// ---------------------------------------------------------------------------

const SUB_BLOCK = 32;
const MAX_GRAINS = 24;
const NUM_VOICES = 10;

// Hann window LUT with skew applied by the caller (matches GrainVoice).
const hannLut = (() => {
  const a = new Float32Array(1026);
  for (let i = 0; i < 1026; i++) {
    const x = Math.min(1, i / 1024);
    a[i] = 0.5 - 0.5 * Math.cos(2 * Math.PI * x);
  }
  return a;
})();

function hannWin(t) {
  t = Math.min(1, Math.max(0, t));
  const x = t * 1024;
  const i = Math.floor(x);
  return hannLut[i] + (x - i) * (hannLut[i + 1] - hannLut[i]);
}

// Linear-segment ADSR equivalent to juce::ADSR (zero-length stages jump).
class Adsr {
  constructor() { this.sr = 48000; this.reset(); this.a = 0.001; this.d = 0.1; this.s = 1; this.r = 0.1; }
  setSampleRate(sr) { this.sr = sr; }
  setParameters(a, d, s, r) { this.a = a; this.d = d; this.s = s; this.r = r; }
  reset() { this.state = 0; this.env = 0; } // 0 idle, 1 attack, 2 decay, 3 sustain, 4 release
  noteOn() { this.state = 1; }
  noteOff() { if (this.state !== 0) this.state = 4; }
  isActive() { return this.state !== 0; }
  next() {
    switch (this.state) {
      case 1: {
        const rate = this.a > 0 ? 1 / (this.a * this.sr) : 1;
        this.env += rate;
        if (this.env >= 1) { this.env = 1; this.state = 2; }
        break;
      }
      case 2: {
        const rate = this.d > 0 ? (1 - this.s) / (this.d * this.sr) : 1;
        this.env -= rate;
        if (this.env <= this.s) { this.env = this.s; this.state = 3; }
        break;
      }
      case 3: this.env = this.s; break;
      case 4: {
        const rate = this.r > 0 ? 1 / (this.r * this.sr) : 1;
        this.env -= rate;
        if (this.env <= 0) { this.env = 0; this.state = 0; }
        break;
      }
      default: this.env = 0;
    }
    return this.env;
  }
}

// Port of Source/Lfo.h — bipolar -1..1.
export const LFO_MODE = { retrig: 0, firstNote: 1, global: 2 };

class Lfo {
  constructor() { this.phase = 0; this.held = 0; this.rngState = 1; }
  reset(seed) {
    this.phase = 0;
    this.rngState = (seed >>> 0) || 1;
    this.held = this.nextRand() * 2 - 1;
  }
  nextRand() {
    // xorshift32 — fast audio-thread randomness (not bit-compatible with C++,
    // only S&H sequences differ)
    let x = this.rngState;
    x ^= x << 13; x >>>= 0; x ^= x >> 17; x ^= x << 5; x >>>= 0;
    this.rngState = x;
    return x / 4294967296;
  }
  advance(rateHz, dt, shape) {
    this.phase += rateHz * dt;
    if (this.phase >= 1) {
      this.phase -= Math.floor(this.phase);
      this.held = this.nextRand() * 2 - 1;
    }
    return this.value(shape);
  }
  value(shape) {
    const p = this.phase;
    switch (shape) {
      case 0: return Math.sin(2 * Math.PI * p);
      case 1: return 1 - 4 * Math.abs(p - 0.5);
      case 2: return 2 * p - 1;
      case 3: return 1 - 2 * p;
      case 4: return p < 0.5 ? 1 : -1;
      case 5: return this.held;
      default: return 0;
    }
  }
}

// TPT state-variable filter (equivalent to juce::dsp::StateVariableTPTFilter).
class Svf {
  constructor() { this.ic1 = [0, 0]; this.ic2 = [0, 0]; this.g = 0.1; this.k = 1 / 0.707; this.type = 0; }
  reset() { this.ic1 = [0, 0]; this.ic2 = [0, 0]; }
  set(cutoff, resonance, type, sr) {
    this.g = Math.tan(Math.PI * Math.min(0.49 * sr, cutoff) / sr);
    this.k = 1 / Math.max(0.05, resonance);
    this.type = type;
  }
  process(ch, x) {
    const g = this.g, k = this.k;
    const a1 = 1 / (1 + g * (g + k));
    const a2 = g * a1;
    const v3 = x - this.ic2[ch];
    const v1 = a1 * this.ic1[ch] + a2 * v3;
    const v2 = this.ic2[ch] + a2 * this.ic1[ch] + g * a2 * v3;
    this.ic1[ch] = 2 * v1 - this.ic1[ch];
    this.ic2[ch] = 2 * v2 - this.ic2[ch];
    switch (this.type) {
      case 1: return v1;                    // BP
      case 2: return x - k * v1 - v2;       // HP
      default: return v2;                   // LP
    }
  }
}

// ---------------------------------------------------------------------------
// Voice (port of GrainVoice)
// ---------------------------------------------------------------------------

class Voice {
  constructor(engine, index) {
    this.engine = engine;
    this.index = index;
    this.note = -1;
    this.active = false;
    this.releasing = false;
    this.sustained = false;
    this.startedAt = 0;
    this.env1 = new Adsr(); this.env2 = new Adsr(); this.env3 = new Adsr();
    this.lfo1 = new Lfo(); this.lfo2 = new Lfo();
    this.grains = Array.from({ length: MAX_GRAINS }, () => ({
      active: false, phase: 0, framePos: 0, age: 0, dur: 1, ratioRand: 1, panL: 0.7, panR: 0.7, gamma: 1, mip: 0,
    }));
    this.eff = new Float32Array(NUM_DESTS);
    this.effNorm = new Float32Array(NUM_DESTS);
    this.filter = new Svf();
    this.rngState = 1;
    this.noteSeq = 0;
    this.reset();
  }

  reset() {
    this.env1Last = 0; this.env2Last = 0; this.env3Last = 0;
    this.lfo1Last = 0; this.lfo2Last = 0;
    this.voicePhase = 0; this.spawnCountdown = 1;
    this.liveInc = 0; this.curInterval = 1000; this.curSizeSamps = 4000;
    this.curGamma = 1; this.grainNorm = 1;
    this.cutoffSm = 14000; this.gainSm = 0;
  }

  prepare(sr) {
    this.sr = sr;
    this.env1.setSampleRate(sr); this.env2.setSampleRate(sr); this.env3.setSampleRate(sr);
  }

  rand() {
    let x = this.rngState;
    x ^= x << 13; x >>>= 0; x ^= x >> 17; x ^= x << 5; x >>>= 0;
    this.rngState = x;
    return x / 4294967296;
  }
  bip() { return this.rand() * 2 - 1; }

  startNote(note, vel) {
    const e = this.engine;
    this.note = note;
    this.active = true;
    this.releasing = false;
    this.sustained = false;
    this.startedAt = e.clock;
    this.baseFreq = 440 * Math.pow(2, (note - 69) / 12);
    this.velocity = vel;
    this.keytrack = Math.min(1, Math.max(-1, (note - 60) / 36));

    this.noteSeq++;
    this.rngState = ((this.index * 1000003 + this.noteSeq * 7919) >>> 0) || 1;
    this.lfo1.reset((this.rand() * 4294967296) >>> 0);
    this.lfo2.reset((this.rand() * 4294967296) >>> 0);

    this.env1.reset(); this.env2.reset(); this.env3.reset();
    const p = e.params;
    this.env1.setParameters(p[D.env1A] / 1000, p[D.env1D] / 1000, p[D.env1S], p[D.env1R] / 1000);
    this.env2.setParameters(p[D.env2A] / 1000, p[D.env2D] / 1000, p[D.env2S], p[D.env2R] / 1000);
    this.env3.setParameters(p[D.env3A] / 1000, p[D.env3D] / 1000, p[D.env3S], p[D.env3R] / 1000);
    this.env1.noteOn(); this.env2.noteOn(); this.env3.noteOn();
    this.env1Last = 0; this.env2Last = 0; this.env3Last = 0;
    this.lfo1Last = 0; this.lfo2Last = 0;

    for (const g of this.grains) g.active = false;
    this.voicePhase = 0;
    this.spawnCountdown = 1;
    this.filter.reset();
    e.displayVoice = this;
  }

  stopNote(allowTail) {
    if (allowTail) {
      this.env1.noteOff(); this.env2.noteOff(); this.env3.noteOff();
      this.releasing = true;
    } else {
      this.active = false;
      this.note = -1;
      for (const g of this.grains) g.active = false;
    }
  }

  updateControls(tick, n) {
    const e = this.engine;
    const src = e.srcScratch;
    src[S.ENV1] = this.env1Last; src[S.ENV2] = this.env2Last; src[S.ENV3] = this.env3Last;
    src[S.LFO1] = this.lfo1Last; src[S.LFO2] = this.lfo2Last;
    src[S.VEL] = this.velocity; src[S.WHEEL] = e.wheel; src[S.KEY] = this.keytrack;

    for (let d = 0; d < NUM_DESTS; d++) {
      const range = e.ranges[d];
      let norm = range.to01(e.params[d]);
      const routes = e.routes[d];
      if (routes) for (const r of routes) norm += r.depth * src[r.src];
      norm = Math.min(1, Math.max(0, norm));
      this.effNorm[d] = norm;
      this.eff[d] = range.from01(norm);
    }
    const eff = this.eff;

    this.env1.setParameters(eff[D.env1A] / 1000, eff[D.env1D] / 1000, eff[D.env1S], eff[D.env1R] / 1000);
    this.env2.setParameters(eff[D.env2A] / 1000, eff[D.env2D] / 1000, eff[D.env2S], eff[D.env2R] / 1000);
    this.env3.setParameters(eff[D.env3A] / 1000, eff[D.env3D] / 1000, eff[D.env3S], eff[D.env3R] / 1000);

    const dt = n / this.sr;
    this.lfo1Last = e.lfoModes[0] === LFO_MODE.retrig
      ? this.lfo1.advance(eff[D.lfo1Rate], dt, e.lfoShapes[0]) : e.sharedVals[0][tick];
    this.lfo2Last = e.lfoModes[1] === LFO_MODE.retrig
      ? this.lfo2.advance(eff[D.lfo2Rate], dt, e.lfoShapes[1]) : e.sharedVals[1][tick];

    this.liveInc = this.baseFreq * Math.pow(2, (eff[D.coarse] + eff[D.fine] * 0.01 + e.pitchBend) / 12) / this.sr;
    this.curSizeSamps = Math.min(this.sr, Math.max(64, Math.floor(eff[D.grainSize] * 0.001 * this.sr)));
    const density = Math.max(1, eff[D.density]);
    this.curInterval = Math.max(48, Math.floor(this.curSizeSamps / density));
    this.grainNorm = Math.min(1, 2 / density);
    this.curGamma = Math.pow(2, (eff[D.shape] - 0.5) * 3);

    this.filter.set(Math.min(0.45 * this.sr, Math.max(20, eff[D.cutoff])),
                    0.707 * Math.pow(10, eff[D.res] * 1.15), e.filterType, this.sr);
    this.gainTarget = Math.pow(10, eff[D.gain] / 20);
  }

  spawnGrain(tbl) {
    let slot = null;
    for (const g of this.grains) if (!g.active) { slot = g; break; }
    if (!slot) return;
    const eff = this.eff;
    const pos = Math.min(1, Math.max(0, eff[D.position] + eff[D.spray] * this.bip()));
    slot.framePos = pos * (NUM_FRAMES - 1);
    slot.ratioRand = Math.pow(2, (this.bip() * eff[D.pitchRand]) / 12);
    const pp = Math.min(1, Math.max(0, 0.5 + 0.5 * eff[D.spread] * this.bip()));
    slot.panL = Math.cos((pp * Math.PI) / 2);
    slot.panR = Math.sin((pp * Math.PI) / 2);
    slot.dur = this.curSizeSamps;
    slot.age = 0;
    slot.phase = this.voicePhase;
    slot.gamma = this.curGamma;
    slot.mip = mipForFreq(this.liveInc * this.sr * slot.ratioRand, this.sr);
    slot.active = true;
  }

  render(outL, outR, start, numSamples, tick) {
    if (!this.active) return;
    const e = this.engine;
    const tbl = e.tables[e.tableIndex];

    this.updateControls(tick, numSamples);
    const gainSmCoef = Math.exp(-1 / (0.005 * this.sr));

    for (let s = 0; s < numSamples; s++) {
      if (--this.spawnCountdown <= 0) {
        this.spawnGrain(tbl);
        this.spawnCountdown = this.curInterval;
      }
      let l = 0, r = 0;
      for (const g of this.grains) {
        if (!g.active) continue;
        const t = g.age / g.dur;
        const w = hannWin(Math.pow(t, g.gamma));
        // table read: frame + sample interpolation
        const f0 = Math.floor(g.framePos);
        const f1 = Math.min(f0 + 1, NUM_FRAMES - 1);
        const ff = g.framePos - f0;
        const mip = tbl.mips[g.mip];
        let x = g.phase * FRAME_LEN;
        let i = Math.floor(x);
        if (i >= FRAME_LEN) i = FRAME_LEN - 1;
        const fx = x - i;
        const b0 = f0 * (FRAME_LEN + 1) + i;
        const b1 = f1 * (FRAME_LEN + 1) + i;
        const s0 = mip[b0] + fx * (mip[b0 + 1] - mip[b0]);
        const s1 = mip[b1] + fx * (mip[b1 + 1] - mip[b1]);
        const v = (s0 + ff * (s1 - s0)) * w;
        l += v * g.panL;
        r += v * g.panR;
        g.phase += this.liveInc * g.ratioRand;
        if (g.phase >= 1) g.phase -= Math.floor(g.phase);
        if (++g.age >= g.dur) g.active = false;
      }

      this.voicePhase += this.liveInc;
      if (this.voicePhase >= 1) this.voicePhase -= Math.floor(this.voicePhase);

      this.env1Last = this.env1.next();
      this.env2Last = this.env2.next();
      this.env3Last = this.env3.next();

      this.gainSm = this.gainTarget + (this.gainSm - this.gainTarget) * gainSmCoef;
      const amp = this.env1Last * this.gainSm * this.grainNorm;
      l *= amp; r *= amp;
      l = this.filter.process(0, l);
      r = this.filter.process(1, r);
      outL[start + s] += l;
      outR[start + s] += r;
    }

    if (this.releasing && !this.env1.isActive()) {
      this.active = false;
      this.note = -1;
      for (const g of this.grains) g.active = false;
    }
  }
}

// ---------------------------------------------------------------------------
// Engine (port of VapeProcessor)
// ---------------------------------------------------------------------------

export class Engine {
  constructor(sampleRate) {
    this.sr = sampleRate;
    this.tables = grainTables();
    this.ranges = DESTS.map((id) => PARAMS[id].range);
    this.params = new Float32Array(NUM_DESTS);
    const defs = defaultParams();
    DESTS.forEach((id, i) => { this.params[i] = defs[id]; });

    this.tableIndex = 0;
    this.filterType = 0;
    this.lfoShapes = [0, 0];
    this.lfoModes = [0, 0];

    this.routes = Array.from({ length: NUM_DESTS }, () => null);
    this.setMatrix([{ src: S.LFO1, dst: D.position, depth: 0.25 }]); // default patch

    this.voices = Array.from({ length: NUM_VOICES }, (_, i) => new Voice(this, i));
    for (const v of this.voices) v.prepare(sampleRate);

    this.wheel = 0;
    this.pitchBend = 0;
    this.sustainDown = false;
    this.heldKeys = 0;
    this.clock = 0;
    this.srcScratch = new Float32Array(8);
    this.displayVoice = null;

    this.sharedLfos = [new Lfo(), new Lfo()];
    this.sharedLfos[0].reset(0x5eed);
    this.sharedLfos[1].reset(0x5eee);
    this.sharedVals = [new Float32Array(64), new Float32Array(64)];
  }

  setParam(id, value) {
    const d = D[id];
    if (d !== undefined) this.params[d] = value;
  }

  setMatrix(routes) {
    const byDest = Array.from({ length: NUM_DESTS }, () => null);
    for (const r of routes) {
      const depth = Math.min(1, Math.max(-1, r.depth));
      (byDest[r.dst] ||= []).push({ src: r.src, depth });
    }
    this.routes = byDest;
  }

  noteOn(note, vel) {
    if (this.heldKeys === 0)
      for (let i = 0; i < 2; i++)
        if (this.lfoModes[i] === LFO_MODE.firstNote) this.sharedLfos[i].reset((0x5eed + i + 31 * ++this.clock) >>> 0);
    this.heldKeys++;

    let voice = this.voices.find((v) => !v.active);
    if (!voice) {
      // steal the oldest
      voice = this.voices.reduce((a, b) => (a.startedAt <= b.startedAt ? a : b));
      voice.stopNote(false);
    }
    voice.startNote(note, vel);
  }

  noteOff(note) {
    this.heldKeys = Math.max(0, this.heldKeys - 1);
    for (const v of this.voices) {
      if (v.active && v.note === note && !v.releasing && !v.sustained) {
        if (this.sustainDown) v.sustained = true;
        else v.stopNote(true);
      }
    }
  }

  sustain(down) {
    this.sustainDown = down;
    if (!down)
      for (const v of this.voices)
        if (v.sustained) { v.sustained = false; v.stopNote(true); }
  }

  // Renders numSamples into outL/outR (which must be zeroed by the caller).
  process(outL, outR, numSamples) {
    let pos = 0;
    let tick = 0;
    while (pos < numSamples) {
      const n = Math.min(SUB_BLOCK, numSamples - pos);
      const dt = n / this.sr;
      for (let i = 0; i < 2; i++)
        this.sharedVals[i][tick] = this.sharedLfos[i].advance(
          this.params[i === 0 ? D.lfo1Rate : D.lfo2Rate], dt, this.lfoShapes[i]);

      for (const v of this.voices) v.render(outL, outR, pos, n, tick);
      pos += n;
      tick++;
    }
    // soft safety clip
    for (let i = 0; i < numSamples; i++) {
      outL[i] = Math.tanh(outL[i]);
      outR[i] = Math.tanh(outR[i]);
    }
    this.clock += numSamples;
  }

  displayNorms() {
    const v = this.displayVoice;
    return v && v.active ? v.effNorm : null;
  }
}
