// Parameter definitions mirrored from Source/Params.h — single source of
// truth for the web build (ids, ranges, tapers, defaults, display).

export const DESTS = [
  'position', 'grainSize', 'density', 'spray', 'pitchRand', 'shape', 'coarse', 'fine', 'spread',
  'cutoff', 'res',
  'env1A', 'env1D', 'env1S', 'env1R',
  'env2A', 'env2D', 'env2S', 'env2R',
  'env3A', 'env3D', 'env3S', 'env3R',
  'lfo1Rate', 'lfo2Rate',
  'gain',
];
export const D = Object.fromEntries(DESTS.map((id, i) => [id, i]));
export const NUM_DESTS = DESTS.length;

export const SRCS = ['ENV1', 'ENV2', 'ENV3', 'LFO1', 'LFO2', 'VEL', 'WHEEL', 'KEY'];
export const S = Object.fromEntries(SRCS.map((n, i) => [n, i]));
export const isBipolarSrc = (s) => s === S.LFO1 || s === S.LFO2 || s === S.KEY;

export const TABLES = ['Morph', 'Sweep', 'Vox', 'Bells', 'Grit'];
export const FILTER_TYPES = ['Low Pass', 'Band Pass', 'High Pass'];
export const LFO_SHAPES = ['Sine', 'Triangle', 'Saw Up', 'Saw Down', 'Square', 'S&H'];
export const LFO_MODES = ['Retrig', 'First Note', 'Global'];

function linRange(lo, hi) {
  return {
    lo, hi,
    to01: (v) => (Math.min(hi, Math.max(lo, v)) - lo) / (hi - lo),
    from01: (p) => lo + (hi - lo) * Math.min(1, Math.max(0, p)),
  };
}

function skewRange(lo, hi, centre) {
  const skew = Math.log(0.5) / Math.log((centre - lo) / (hi - lo));
  return {
    lo, hi,
    to01: (v) => Math.pow((Math.min(hi, Math.max(lo, v)) - lo) / (hi - lo), skew),
    from01: (p) => lo + (hi - lo) * Math.pow(Math.min(1, Math.max(0, p)), 1 / skew),
  };
}

// Envelope-time taper: clock-calibrated over the 270deg knob sweep.
// 0 (or the floor) fully CCW, 250ms at 9 o'clock (1/6), 1s at noon,
// 4s at 3 o'clock (5/6), 10s fully CW. Log-linear between anchors.
function envTimeRange(floorMs = 0) {
  const P = [0, 1 / 6, 0.5, 5 / 6, 1];
  const V = [Math.max(1, floorMs), 250, 1000, 4000, 10000];
  return {
    lo: floorMs, hi: 10000,
    from01: (p) => {
      if (p <= 0) return floorMs;
      p = Math.min(1, p);
      let i = 1;
      while (i < 4 && p > P[i]) ++i;
      const t = (p - P[i - 1]) / (P[i] - P[i - 1]);
      return V[i - 1] * Math.pow(V[i] / V[i - 1], t);
    },
    to01: (v) => {
      if (v < V[0]) return 0;
      v = Math.min(v, V[4]);
      let i = 1;
      while (i < 4 && v > V[i]) ++i;
      const t = Math.log(v / V[i - 1]) / Math.log(V[i] / V[i - 1]);
      return P[i - 1] + t * (P[i] - P[i - 1]);
    },
  };
}

export const PARAMS = {
  position:  { range: linRange(0, 1),              def: 0.15, label: '' },
  grainSize: { range: skewRange(10, 500, 80),      def: 90,   label: 'ms' },
  density:   { range: linRange(1, 8),              def: 3,    label: '' },
  spray:     { range: linRange(0, 1),              def: 0.03, label: '' },
  pitchRand: { range: linRange(0, 12),             def: 0,    label: 'st' },
  shape:     { range: linRange(0, 1),              def: 0.5,  label: '' },
  coarse:    { range: linRange(-24, 24),           def: 0,    label: 'st' },
  fine:      { range: linRange(-100, 100),         def: 0,    label: 'ct' },
  spread:    { range: linRange(0, 1),              def: 0.35, label: '' },
  cutoff:    { range: skewRange(20, 20000, 1200),  def: 14000, label: 'Hz' },
  res:       { range: linRange(0, 1),              def: 0.15, label: '' },
  env1A: { range: envTimeRange(),  def: 4,   label: 'ms' },
  env1D: { range: envTimeRange(),  def: 200, label: 'ms' },
  env1S: { range: linRange(0, 1),  def: 0.8, label: '' },
  env1R: { range: envTimeRange(5), def: 250, label: 'ms' },
  env2A: { range: envTimeRange(),  def: 120, label: 'ms' },
  env2D: { range: envTimeRange(),  def: 300, label: 'ms' },
  env2S: { range: linRange(0, 1),  def: 0.5, label: '' },
  env2R: { range: envTimeRange(5), def: 300, label: 'ms' },
  env3A: { range: envTimeRange(),  def: 400, label: 'ms' },
  env3D: { range: envTimeRange(),  def: 600, label: 'ms' },
  env3S: { range: linRange(0, 1),  def: 0.6, label: '' },
  env3R: { range: envTimeRange(5), def: 800, label: 'ms' },
  lfo1Rate: { range: linRange(0.1, 5), def: 2,    label: 'Hz' },
  lfo2Rate: { range: linRange(0.1, 5), def: 0.35, label: 'Hz' },
  gain:     { range: linRange(-60, 6), def: -8,   label: 'dB' },
};

export const DEST_NAMES = {
  position: 'Position', grainSize: 'Size', density: 'Density', spray: 'Spray',
  pitchRand: 'Pitch Rnd', shape: 'Shape', coarse: 'Coarse', fine: 'Fine', spread: 'Spread',
  cutoff: 'Cutoff', res: 'Res',
  env1A: 'Attack', env1D: 'Decay', env1S: 'Sustain', env1R: 'Release',
  env2A: 'Attack', env2D: 'Decay', env2S: 'Sustain', env2R: 'Release',
  env3A: 'Attack', env3D: 'Decay', env3S: 'Sustain', env3R: 'Release',
  lfo1Rate: 'Rate', lfo2Rate: 'Rate', gain: 'Gain',
};

export function destDisplayName(id) {
  const p = id.startsWith('env1') ? 'Env1 ' : id.startsWith('env2') ? 'Env2 '
          : id.startsWith('env3') ? 'Env3 ' : id === 'lfo1Rate' ? 'LFO1 '
          : id === 'lfo2Rate' ? 'LFO2 ' : '';
  return p + DEST_NAMES[id];
}

export function defaultParams() {
  const o = {};
  for (const id of DESTS) o[id] = PARAMS[id].def;
  return o;
}

// --- Knob step grid (Source/Params.h snapNice / snapKnobValue) --------------
function snapNice(v, minStep) {
  const av = Math.abs(v);
  if (av < 1e-9) return 0;
  const dec = Math.pow(10, Math.floor(Math.log10(av)));
  const m = av / dec;
  const step = Math.max((m < 2 ? 0.1 : m < 5 ? 0.2 : 0.5) * dec, minStep);
  return Math.sign(v) * Math.round(av / step) * step;
}

const ENV_TIMES = new Set(['env1A', 'env1D', 'env1R', 'env2A', 'env2D', 'env2R', 'env3A', 'env3D', 'env3R']);

export function snapKnobValue(id, v) {
  if (ENV_TIMES.has(id) || id === 'grainSize') return snapNice(v, 0.5);
  if (id === 'cutoff' || id === 'lfo1Rate' || id === 'lfo2Rate') return snapNice(v, 0);
  if (id === 'gain') return Math.round(v * 2) / 2;
  if (id === 'density') return Math.round(v * 10) / 10;
  if (id === 'coarse') return Math.round(v);
  if (id === 'fine') return Math.round(v);
  if (id === 'pitchRand') return Math.round(v * 10) / 10;
  return Math.round(v * 100) / 100;
}

// ~3 significant figures, trailing zeros trimmed (Source/Params.h).
export function formatValue(id, v) {
  let s;
  if (v === 0) s = '0';
  else {
    const mag = Math.floor(Math.log10(Math.abs(v)));
    const dec = Math.min(3, Math.max(0, 2 - mag));
    s = v.toFixed(dec).replace(/\.?0+$/, (m) => (m.includes('.') ? '' : m));
    if (s.includes('.')) s = s.replace(/0+$/, '').replace(/\.$/, '');
  }
  const label = PARAMS[id].label;
  return label ? `${s} ${label}` : s;
}
