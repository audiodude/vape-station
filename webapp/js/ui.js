// VapeStation web UI — Rainfall design, wired to the AudioWorklet engine.

import {
  DESTS, D, S, SRCS, TABLES, FILTER_TYPES, LFO_SHAPES, LFO_MODES,
  PARAMS, DEST_NAMES, destDisplayName, defaultParams, snapKnobValue,
  formatValue, isBipolarSrc,
} from './params.js';
import { grainTables, NUM_FRAMES, FRAME_LEN } from './engine.js';
import { VERSION } from './version.js';

document.getElementById('verstamp').textContent = ` · ${VERSION}`;

const SRC_COLOURS = ['#60a5fa', '#34d399', '#f87171', '#c4b5fd', '#e6f3ff', '#fbbf24', '#2dd4bf', '#f472b6'];
const ACCENT = '#4aa5ff';
const SVGNS = 'http://www.w3.org/2000/svg';

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

const state = {
  params: defaultParams(),
  table: 0, filterType: 0,
  lfoShapes: [0, 0], lfoModes: [0, 0],
  routes: [{ src: S.LFO1, dst: D.position, depth: 0.25 }],
};

try {
  const saved = JSON.parse(localStorage.getItem('vapestation') || 'null');
  if (saved && saved.params) Object.assign(state, saved);
} catch { /* fresh start */ }

let audio = null; // { ctx, node }
const send = (msg) => { if (audio) audio.node.port.postMessage(msg); };

let persistTimer = 0;
function persist() {
  clearTimeout(persistTimer);
  persistTimer = setTimeout(() => localStorage.setItem('vapestation', JSON.stringify(state)), 250);
}

function sendFullState() {
  for (const id of DESTS) send({ type: 'param', id, value: state.params[id] });
  send({ type: 'table', value: state.table });
  send({ type: 'filterType', value: state.filterType });
  for (let i = 0; i < 2; i++) {
    send({ type: 'lfoShape', which: i, value: state.lfoShapes[i] });
    send({ type: 'lfoMode', which: i, value: state.lfoModes[i] });
  }
  send({ type: 'matrix', routes: state.routes });
}

// ---------------------------------------------------------------------------
// Tooltip
// ---------------------------------------------------------------------------

const tooltip = document.getElementById('tooltip');
function showTip(x, y, text) {
  tooltip.textContent = text;
  tooltip.style.display = 'block';
  tooltip.style.left = `${x + 14}px`;
  tooltip.style.top = `${y - 28}px`;
}
const hideTip = () => { tooltip.style.display = 'none'; };

// ---------------------------------------------------------------------------
// Knobs
// ---------------------------------------------------------------------------

const knobs = {}; // id -> knob object
const OSC_DESTS = new Set(['position', 'grainSize', 'density', 'spray', 'pitchRand', 'shape', 'coarse', 'fine', 'spread']);

function arcColourFor(id) {
  if (id.startsWith('env1')) return SRC_COLOURS[S.ENV1];
  if (id.startsWith('env2')) return SRC_COLOURS[S.ENV2];
  if (id.startsWith('env3')) return SRC_COLOURS[S.ENV3];
  if (id === 'lfo1Rate') return SRC_COLOURS[S.LFO1];
  if (id === 'lfo2Rate') return SRC_COLOURS[S.LFO2];
  return ACCENT;
}

const el = (tag, attrs = {}) => {
  const n = document.createElementNS(SVGNS, tag);
  for (const k in attrs) n.setAttribute(k, attrs[k]);
  return n;
};

// r=23 ring: C = 144.51, 270deg = 108.4
const RING_C = 2 * Math.PI * 23;
const RING2_C = 2 * Math.PI * 26.5;

function makeKnob(id, sizePx) {
  const osc = OSC_DESTS.has(id);
  const cell = document.createElement('div');
  cell.className = 'knobcell';
  cell.dataset.dest = id;

  const svg = el('svg', { width: sizePx, height: sizePx, viewBox: '0 0 56 56' });
  svg.appendChild(el('circle', { cx: 28, cy: 28, r: 26, fill: '#111827' }));
  svg.appendChild(el('circle', { cx: 28, cy: 28, r: 16, fill: '#374151', stroke: '#4b5563', 'stroke-width': 1 }));
  svg.appendChild(el('circle', {
    cx: 28, cy: 28, r: 23, fill: 'none', stroke: '#374151', 'stroke-width': 3,
    'stroke-dasharray': `${(RING_C * 270) / 360} ${RING_C}`, 'stroke-linecap': 'round', transform: 'rotate(135 28 28)',
  }));

  const modGroup = el('g');
  svg.appendChild(modGroup);

  let valArc = null, valDot = null;
  if (osc) {
    valDot = el('circle', { cx: 28, cy: 5, r: 3, fill: '#fff' });
  } else {
    valArc = el('circle', {
      cx: 28, cy: 28, r: 23, fill: 'none', stroke: arcColourFor(id), 'stroke-width': 3,
      'stroke-dasharray': `0 ${RING_C}`, 'stroke-linecap': 'round', transform: 'rotate(135 28 28)',
    });
  }
  if (valArc) svg.appendChild(valArc);

  const liveDot = el('circle', { cx: 28, cy: 5, r: 2.5, fill: '#fff', visibility: 'hidden' });
  const pointer = el('line', { x1: 28, y1: 28, x2: 28, y2: 15, stroke: '#fff', 'stroke-width': 2, 'stroke-linecap': 'round' });
  svg.appendChild(pointer);
  if (valDot) svg.appendChild(valDot);
  svg.appendChild(liveDot);

  const labelRow = document.createElement('div');
  labelRow.className = 'knoblabel';
  const labelText = document.createElement('span');
  labelText.textContent = DEST_NAMES[id];
  labelRow.appendChild(labelText);

  cell.appendChild(svg);
  cell.appendChild(labelRow);

  const knob = {
    id, cell, svg, modGroup, valArc, valDot, liveDot, pointer, labelRow, osc,
    routes: [],
    norm() { return PARAMS[id].range.to01(state.params[id]); },
    refresh() {
      const p = this.norm();
      const angle = -135 + 270 * p;
      this.pointer.setAttribute('transform', `rotate(${angle} 28 28)`);
      if (this.valDot) this.valDot.setAttribute('transform', `rotate(${angle} 28 28)`);
      if (this.valArc) this.valArc.setAttribute('stroke-dasharray', `${(p * RING_C * 270) / 360} ${RING_C}`);
      this.refreshMods();
    },
    refreshMods() {
      this.modGroup.replaceChildren();
      this.labelRow.querySelectorAll('.mdot').forEach((d) => d.remove());
      const base = this.norm();
      this.routes.forEach((r, i) => {
        const colour = SRC_COLOURS[r.src];
        const start = Math.min(1, Math.max(0, isBipolarSrc(r.src) ? base - r.depth : base));
        const end = Math.min(1, Math.max(0, base + r.depth));
        const a0 = Math.min(start, end), a1 = Math.max(start, end);
        const onTrack = this.osc && i === 0;
        const rad = onTrack ? 23 : 26.5 + (i - (this.osc ? 1 : 0)) * 3.5;
        const circ = 2 * Math.PI * rad;
        this.modGroup.appendChild(el('circle', {
          cx: 28, cy: 28, r: rad, fill: 'none', stroke: colour,
          'stroke-width': onTrack ? 3 : 2, 'stroke-linecap': 'round',
          'stroke-dasharray': `${Math.max(2, ((a1 - a0) * 270 / 360) * circ)} ${circ}`,
          transform: `rotate(${135 + 270 * a0} 28 28)`,
        }));
        const dot = document.createElement('span');
        dot.className = 'mdot';
        dot.style.background = colour;
        this.labelRow.appendChild(dot);
      });
      this.liveDot.setAttribute('visibility', this.routes.length ? 'visible' : 'hidden');
    },
    setLiveNorm(p) {
      if (!this.routes.length) return;
      this.liveDot.setAttribute('transform', `rotate(${-135 + 270 * p} 28 28)`);
    },
  };

  // drag interaction
  let dragStart = null;
  svg.addEventListener('pointerdown', (e) => {
    svg.setPointerCapture(e.pointerId);
    dragStart = { y: e.clientY, norm: knob.norm() };
    e.preventDefault();
  });
  svg.addEventListener('pointermove', (e) => {
    if (!dragStart) return;
    const fine = e.shiftKey ? 0.125 : 1;
    const p = Math.min(1, Math.max(0, dragStart.norm + ((dragStart.y - e.clientY) / 200) * fine));
    let v = PARAMS[id].range.from01(p);
    if (!e.shiftKey) v = snapKnobValue(id, v);
    v = Math.min(PARAMS[id].range.hi, Math.max(PARAMS[id].range.lo, v));
    setParam(id, v);
    showTip(e.clientX, e.clientY, formatValue(id, v));
  });
  const endDrag = () => { dragStart = null; hideTip(); };
  svg.addEventListener('pointerup', endDrag);
  svg.addEventListener('pointercancel', endDrag);
  svg.addEventListener('dblclick', () => setParam(id, PARAMS[id].def));
  svg.addEventListener('wheel', (e) => {
    e.preventDefault();
    const p = Math.min(1, Math.max(0, knob.norm() - Math.sign(e.deltaY) * 0.02));
    setParam(id, snapKnobValue(id, PARAMS[id].range.from01(p)));
  }, { passive: false });

  knobs[id] = knob;
  knob.refresh();
  return cell;
}

function setParam(id, value) {
  state.params[id] = value;
  send({ type: 'param', id, value });
  knobs[id]?.refresh();
  if (id === 'position') refreshPosSlider();
  persist();
}

// ---------------------------------------------------------------------------
// Layout: knob grids, env/lfo panels, header gain
// ---------------------------------------------------------------------------

const oscGrid = document.getElementById('oscgrid');
['position', 'grainSize', 'density', 'spray', 'pitchRand', 'shape', 'coarse', 'fine', 'spread']
  .forEach((id) => oscGrid.appendChild(makeKnob(id, 56)));

const filterKnobs = document.getElementById('filterknobs');
['cutoff', 'res'].forEach((id) => filterKnobs.appendChild(makeKnob(id, 72)));

document.getElementById('gaincell').appendChild(makeKnob('gain', 44));

const row2 = document.getElementById('row2');
const ENV_DEFS = [
  { title: 'ENV 1 – AMP', chip: 'ENV1', src: S.ENV1, ids: ['env1A', 'env1D', 'env1S', 'env1R'] },
  { title: 'ENV 2', chip: 'ENV2', src: S.ENV2, ids: ['env2A', 'env2D', 'env2S', 'env2R'] },
  { title: 'ENV 3', chip: 'ENV3', src: S.ENV3, ids: ['env3A', 'env3D', 'env3S', 'env3R'] },
];

function makeChip(src, muted = false) {
  const chip = document.createElement('span');
  chip.className = 'chip';
  const colour = SRC_COLOURS[src];
  if (!muted) {
    const dot = document.createElement('span');
    dot.className = 'dot';
    dot.style.background = colour;
    chip.appendChild(dot);
  }
  const label = document.createElement('span');
  label.textContent = SRCS[src];
  label.style.color = muted ? '#9ca3af' : colour;
  chip.appendChild(label);
  makeChipDraggable(chip, src);
  return chip;
}

for (const def of ENV_DEFS) {
  const p = document.createElement('div');
  p.className = 'panel';
  p.style.cssText = 'flex: 1; padding: 12px 14px; display: flex; flex-direction: column; gap: 10px;';
  const head = document.createElement('div');
  head.style.cssText = 'display: flex; align-items: center; justify-content: space-between; gap: 8px;';
  head.innerHTML = `<span class="title">${def.title}</span>`;
  head.appendChild(makeChip(def.src));
  p.appendChild(head);
  const grid = document.createElement('div');
  grid.style.cssText = 'flex: 1; display: grid; grid-template-columns: 1fr 1fr; gap: 8px 4px; justify-items: center; align-items: center;';
  def.ids.forEach((id) => grid.appendChild(makeKnob(id, 50)));
  p.appendChild(grid);
  row2.appendChild(p);
}

for (let i = 0; i < 2; i++) {
  const p = document.createElement('div');
  p.className = 'panel';
  p.style.cssText = 'flex: 1.1; padding: 12px 14px; display: flex; flex-direction: column; gap: 10px;';
  const head = document.createElement('div');
  head.style.cssText = 'display: flex; align-items: center; justify-content: space-between; gap: 8px;';
  head.innerHTML = `<span class="title">LFO ${i + 1}</span>`;
  head.appendChild(makeChip(i === 0 ? S.LFO1 : S.LFO2));
  p.appendChild(head);
  const mid = document.createElement('div');
  mid.style.cssText = 'flex: 1; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 3px;';
  mid.appendChild(makeKnob(i === 0 ? 'lfo1Rate' : 'lfo2Rate', 62));
  p.appendChild(mid);
  const sels = document.createElement('div');
  sels.style.cssText = 'display: flex; flex-direction: column; gap: 6px;';
  const modeSel = document.createElement('select');
  LFO_MODES.forEach((n, v) => modeSel.add(new Option(n, v)));
  modeSel.value = state.lfoModes[i];
  modeSel.onchange = () => { state.lfoModes[i] = +modeSel.value; send({ type: 'lfoMode', which: i, value: +modeSel.value }); persist(); };
  const shapeSel = document.createElement('select');
  LFO_SHAPES.forEach((n, v) => shapeSel.add(new Option(n, v)));
  shapeSel.value = state.lfoShapes[i];
  shapeSel.onchange = () => { state.lfoShapes[i] = +shapeSel.value; send({ type: 'lfoShape', which: i, value: +shapeSel.value }); persist(); };
  sels.appendChild(modeSel);
  sels.appendChild(shapeSel);
  p.appendChild(sels);
  row2.appendChild(p);
}

// header selects + INIT
const tableSel = document.getElementById('tablesel');
TABLES.forEach((n, v) => tableSel.add(new Option(n, v)));
tableSel.value = state.table;
tableSel.onchange = () => { state.table = +tableSel.value; send({ type: 'table', value: state.table }); drawViz(); persist(); };

const filterSel = document.getElementById('filtersel');
FILTER_TYPES.forEach((n, v) => filterSel.add(new Option(n, v)));
filterSel.value = state.filterType;
filterSel.onchange = () => { state.filterType = +filterSel.value; send({ type: 'filterType', value: state.filterType }); persist(); };

document.getElementById('initbtn').onclick = () => {
  state.params = defaultParams();
  state.table = 0; state.filterType = 0;
  state.lfoShapes = [0, 0]; state.lfoModes = [0, 0];
  state.routes = [{ src: S.LFO1, dst: D.position, depth: 0.25 }];
  send({ type: 'init' });
  tableSel.value = 0; filterSel.value = 0;
  document.querySelectorAll('select').forEach((s) => { if (s !== tableSel && s !== filterSel) s.value = 0; });
  for (const id of DESTS) knobs[id].refresh();
  syncMatrix();
  drawViz();
  refreshPosSlider();
  persist();
};

// ---------------------------------------------------------------------------
// Chip drag & drop
// ---------------------------------------------------------------------------

const ghost = document.getElementById('dragghost');

function makeChipDraggable(chip, src) {
  chip.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    chip.setPointerCapture(e.pointerId);
    let target = null;
    ghost.replaceChildren(chip.cloneNode(true));
    ghost.style.display = 'block';

    const move = (ev) => {
      ghost.style.left = `${ev.clientX}px`;
      ghost.style.top = `${ev.clientY}px`;
      const under = document.elementFromPoint(ev.clientX, ev.clientY);
      const cell = under ? under.closest('.knobcell') : null;
      if (target && target !== cell) target.classList.remove('droptarget');
      target = cell;
      if (target) target.classList.add('droptarget');
    };
    const up = () => {
      chip.removeEventListener('pointermove', move);
      chip.removeEventListener('pointerup', up);
      ghost.style.display = 'none';
      if (target) {
        target.classList.remove('droptarget');
        addRoute(src, D[target.dataset.dest], 0.35);
      }
    };
    chip.addEventListener('pointermove', move);
    chip.addEventListener('pointerup', up);
  });
}

document.getElementById('chip-vel').appendChild(makeChip(S.VEL, true));
document.getElementById('chip-wheel').appendChild(makeChip(S.WHEEL, true));
document.getElementById('chip-key').appendChild(makeChip(S.KEY, true));

// ---------------------------------------------------------------------------
// Matrix
// ---------------------------------------------------------------------------

const matrixRows = document.getElementById('matrixrows');
const matrixEmpty = document.getElementById('matrixempty');

function addRoute(src, dst, depth) {
  const existing = state.routes.find((r) => r.src === src && r.dst === dst);
  if (existing) existing.depth = depth;
  else state.routes.push({ src, dst, depth });
  syncMatrix();
}

function removeRoute(route) {
  state.routes = state.routes.filter((r) => r !== route);
  syncMatrix();
}

function syncMatrix() {
  send({ type: 'matrix', routes: state.routes });
  persist();

  // knobs
  for (const id of DESTS) {
    knobs[id].routes = state.routes.filter((r) => r.dst === D[id]);
    knobs[id].refreshMods();
  }

  // rows, sorted by source then destination
  const sorted = [...state.routes].sort((a, b) => (a.src - b.src) || (a.dst - b.dst));
  matrixRows.replaceChildren();
  matrixEmpty.style.display = sorted.length ? 'none' : 'block';

  for (const route of sorted) {
    const colour = SRC_COLOURS[route.src];
    const row = document.createElement('div');
    row.className = 'mrow';

    const chip = document.createElement('span');
    chip.className = 'chip';
    chip.style.cssText = 'width: 62px; justify-content: center; cursor: default;';
    chip.innerHTML = `<span class="dot" style="background:${colour}"></span><span style="color:${colour}">${SRCS[route.src]}</span>`;
    row.appendChild(chip);

    const chev = document.createElement('span');
    chev.innerHTML = '<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="#6b7280" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M9 6l6 6-6 6"/></svg>';
    chev.style.display = 'flex';
    row.appendChild(chev);

    const dest = document.createElement('span');
    dest.className = 'dest';
    dest.textContent = destDisplayName(DESTS[route.dst]);
    row.appendChild(dest);

    // bipolar slider
    const slider = document.createElement('div');
    slider.className = 'hslider';
    slider.style.flex = '1';
    slider.innerHTML = `<div class="track"></div><div class="tick"></div><div class="fill" style="background:${colour}"></div><div class="thumb"></div>`;
    const fill = slider.querySelector('.fill');
    const thumb = slider.querySelector('.thumb');
    const refreshSlider = () => {
      const pc = 50 + route.depth * 50;
      thumb.style.left = `${pc}%`;
      fill.style.left = `${Math.min(50, pc)}%`;
      fill.style.width = `${Math.abs(route.depth) * 50}%`;
    };
    refreshSlider();
    const setFromX = (ev) => {
      const r = slider.getBoundingClientRect();
      route.depth = Math.min(1, Math.max(-1, ((ev.clientX - r.left) / r.width) * 2 - 1));
      route.depth = Math.round(route.depth * 100) / 100;
      refreshSlider();
      valBox.value = route.depth.toFixed(2);
      send({ type: 'matrix', routes: state.routes });
      knobs[DESTS[route.dst]].refreshMods();
      persist();
    };
    slider.addEventListener('pointerdown', (ev) => {
      slider.setPointerCapture(ev.pointerId);
      setFromX(ev);
      const mv = (e2) => setFromX(e2);
      const up = () => { slider.removeEventListener('pointermove', mv); slider.removeEventListener('pointerup', up); };
      slider.addEventListener('pointermove', mv);
      slider.addEventListener('pointerup', up);
    });
    slider.addEventListener('dblclick', () => { route.depth = 0; refreshSlider(); valBox.value = '0.00'; syncMatrix(); });
    row.appendChild(slider);

    const valBox = document.createElement('input');
    valBox.value = route.depth.toFixed(2);
    valBox.onchange = () => {
      const v = parseFloat(valBox.value);
      if (Number.isFinite(v)) { route.depth = Math.min(1, Math.max(-1, v)); }
      valBox.value = route.depth.toFixed(2);
      refreshSlider();
      send({ type: 'matrix', routes: state.routes });
      knobs[DESTS[route.dst]].refreshMods();
      persist();
    };
    row.appendChild(valBox);

    const kill = document.createElement('span');
    kill.className = 'kill';
    kill.innerHTML = '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#f87171" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="9"/><path d="M9 9l6 6M15 9l-6 6"/></svg>';
    kill.onclick = () => removeRoute(route);
    row.appendChild(kill);

    matrixRows.appendChild(row);
  }
  refreshArrows();
}

const arrowUp = document.getElementById('arrowup');
const arrowDown = document.getElementById('arrowdown');
function refreshArrows() {
  arrowUp.style.display = matrixRows.scrollTop > 1 ? 'block' : 'none';
  arrowDown.style.display =
    matrixRows.scrollTop + matrixRows.clientHeight < matrixRows.scrollHeight - 1 ? 'block' : 'none';
}
matrixRows.addEventListener('scroll', refreshArrows);

// ---------------------------------------------------------------------------
// Graintable viz + position slider
// ---------------------------------------------------------------------------

const viz = document.getElementById('viz');
const vctx = viz.getContext('2d');
let vizPos = null; // live modulated position (norm), null = use param

function drawViz() {
  const w = viz.width, h = viz.height;
  vctx.clearRect(0, 0, w, h);
  const tbl = grainTables()[state.table];
  const pos = vizPos ?? state.params.position;
  const fpos = pos * (NUM_FRAMES - 1);
  const f0 = Math.floor(fpos);
  const f1 = Math.min(f0 + 1, NUM_FRAMES - 1);
  const ff = fpos - f0;
  const mip = tbl.mips[0];
  const steps = 320;
  let maxAbs = 0.05;
  const vals = new Float32Array(steps + 1);
  for (let i = 0; i <= steps; i++) {
    const idx = Math.floor((i / steps) * (FRAME_LEN - 1));
    const v = mip[f0 * (FRAME_LEN + 1) + idx] + ff * (mip[f1 * (FRAME_LEN + 1) + idx] - mip[f0 * (FRAME_LEN + 1) + idx]);
    vals[i] = v;
    maxAbs = Math.max(maxAbs, Math.abs(v));
  }
  vctx.strokeStyle = ACCENT;
  vctx.lineWidth = 4;
  vctx.lineJoin = vctx.lineCap = 'round';
  vctx.beginPath();
  for (let i = 0; i <= steps; i++) {
    const x = (i / steps) * w;
    const y = h / 2 - (vals[i] / maxAbs) * h * 0.44;
    i === 0 ? vctx.moveTo(x, y) : vctx.lineTo(x, y);
  }
  vctx.stroke();
}

viz.addEventListener('pointerdown', (e) => {
  viz.setPointerCapture(e.pointerId);
  const set = (ev) => {
    const r = viz.getBoundingClientRect();
    setParam('position', Math.min(1, Math.max(0, (ev.clientX - r.left) / r.width)));
  };
  set(e);
  const mv = (e2) => set(e2);
  const up = () => { viz.removeEventListener('pointermove', mv); viz.removeEventListener('pointerup', up); };
  viz.addEventListener('pointermove', mv);
  viz.addEventListener('pointerup', up);
});

const posSlider = document.getElementById('posslider');
function refreshPosSlider() {
  const p = state.params.position;
  posSlider.querySelector('.thumb').style.left = `${p * 100}%`;
  posSlider.querySelector('.fill').style.width = `${p * 100}%`;
  drawViz();
}
posSlider.addEventListener('pointerdown', (e) => {
  posSlider.setPointerCapture(e.pointerId);
  const set = (ev) => {
    const r = posSlider.getBoundingClientRect();
    setParam('position', Math.min(1, Math.max(0, (ev.clientX - r.left) / r.width)));
  };
  set(e);
  const mv = (e2) => set(e2);
  const up = () => { posSlider.removeEventListener('pointermove', mv); posSlider.removeEventListener('pointerup', up); };
  posSlider.addEventListener('pointermove', mv);
  posSlider.addEventListener('pointerup', up);
});

// ---------------------------------------------------------------------------
// Keyboard (C2..B6) — pointer, computer keys, MIDI
// ---------------------------------------------------------------------------

const kb = document.getElementById('keyboard');
const WHITE_OFFSETS = [0, 2, 4, 5, 7, 9, 11];
const BLACK = [[1, 14.3], [3, 28.6], [6, 57.1], [8, 71.4], [10, 85.7]];
const keyEls = {};

for (let oct = 0; oct < 5; oct++) {
  const base = 36 + oct * 12;
  const octDiv = document.createElement('div');
  octDiv.className = 'octave';
  WHITE_OFFSETS.forEach((off, i) => {
    const key = document.createElement('div');
    key.className = 'wkey';
    key.dataset.note = base + off;
    if (i === 0) key.innerHTML = `<span class="oct">C${oct + 2}</span>`;
    octDiv.appendChild(key);
    keyEls[base + off] = key;
  });
  for (const [off, left] of BLACK) {
    const key = document.createElement('div');
    key.className = 'bkey';
    key.style.left = `${left}%`;
    key.dataset.note = base + off;
    octDiv.appendChild(key);
    keyEls[base + off] = key;
  }
  kb.appendChild(octDiv);
}

const downNotes = new Set();
function noteOn(note, vel = 0.8) {
  if (downNotes.has(note)) return;
  downNotes.add(note);
  send({ type: 'noteOn', note, vel });
  keyEls[note]?.classList.add('down');
}
function noteOff(note) {
  if (!downNotes.delete(note)) return;
  send({ type: 'noteOff', note });
  keyEls[note]?.classList.remove('down');
}

let pointerNote = null;
kb.addEventListener('pointerdown', (e) => {
  const note = e.target.dataset?.note;
  if (!note) return;
  kb.setPointerCapture(e.pointerId);
  pointerNote = +note;
  noteOn(pointerNote);
});
kb.addEventListener('pointermove', (e) => {
  if (pointerNote === null) return;
  const under = document.elementFromPoint(e.clientX, e.clientY);
  const note = under?.dataset?.note ? +under.dataset.note : null;
  if (note !== null && note !== pointerNote) {
    noteOff(pointerNote);
    pointerNote = note;
    noteOn(pointerNote);
  }
});
const kbUp = () => { if (pointerNote !== null) { noteOff(pointerNote); pointerNote = null; } };
kb.addEventListener('pointerup', kbUp);
kb.addEventListener('pointercancel', kbUp);

const KEYMAP = { KeyA: 60, KeyW: 61, KeyS: 62, KeyE: 63, KeyD: 64, KeyF: 65, KeyT: 66, KeyG: 67, KeyY: 68, KeyH: 69, KeyU: 70, KeyJ: 71, KeyK: 72, KeyO: 73, KeyL: 74, KeyP: 75, Semicolon: 76 };
window.addEventListener('keydown', (e) => {
  if (e.repeat || e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
  const n = KEYMAP[e.code];
  if (n !== undefined) noteOn(n);
});
window.addEventListener('keyup', (e) => {
  const n = KEYMAP[e.code];
  if (n !== undefined) noteOff(n);
});

if (navigator.requestMIDIAccess) {
  navigator.requestMIDIAccess().then((access) => {
    const hook = () => {
      let names = [];
      for (const input of access.inputs.values()) {
        names.push(input.name);
        input.onmidimessage = (m) => {
          const [st, d1, d2] = m.data;
          const cmd = st & 0xf0;
          if (cmd === 0x90 && d2 > 0) noteOn(d1, d2 / 127);
          else if (cmd === 0x80 || (cmd === 0x90 && d2 === 0)) noteOff(d1);
          else if (cmd === 0xb0 && d1 === 1) send({ type: 'wheel', value: d2 / 127 });
          else if (cmd === 0xb0 && d1 === 64) send({ type: 'sustain', down: d2 >= 64 });
          else if (cmd === 0xe0) send({ type: 'bend', value: ((d2 << 7 | d1) - 8192) / 8192 });
        };
      }
      document.getElementById('midistatus').textContent = names.length ? `MIDI: ${names.join(', ')}` : '';
    };
    hook();
    access.onstatechange = hook;
  }).catch(() => {});
}

// ---------------------------------------------------------------------------
// Audio startup + display loop
// ---------------------------------------------------------------------------

let latestNorms = null;
let engineReady = false;
let voicesActive = false;
const diag = document.getElementById('diag');

function showDiag(text, isError = false) {
  diag.textContent = text;
  diag.style.color = isError ? '#f87171' : '#6b7280';
}

function refreshDiag() {
  if (!audio) return;
  const st = audio.ctx.state;
  // The note indicator leads (always present, opacity-toggled) so it
  // doesn't reflow the header when notes start and stop.
  diag.innerHTML = `<span style="opacity:${voicesActive ? 1 : 0.15}">♪</span> `
    + `audio: ${st} ${Math.round(audio.ctx.sampleRate / 100) / 10}k`
    + ` · engine ${engineReady ? 'ok' : 'loading'}`;
  diag.style.color = st !== 'running' ? '#f87171' : '#6b7280';
}

async function startAudio() {
  if (audio) return;
  try {
    // iOS: ask for the 'playback' session so the hardware silent switch
    // doesn't mute Web Audio.
    try { if (navigator.audioSession) navigator.audioSession.type = 'playback'; } catch { /* optional */ }
    const ctx = new AudioContext({ latencyHint: 'interactive' });
    // Mobile browsers can leave a gesture-created context suspended.
    if (ctx.state === 'suspended') await ctx.resume();
    await ctx.audioWorklet.addModule(new URL('./worklet.js', import.meta.url));
    const node = new AudioWorkletNode(ctx, 'vape-station', { outputChannelCount: [2] });
    node.connect(ctx.destination);
    node.port.onmessage = (e) => {
      if (e.data.type === 'display') { latestNorms = e.data.norms; voicesActive = !!e.data.active; refreshDiag(); }
      else if (e.data.type === 'ready') { engineReady = true; refreshDiag(); }
      else if (e.data.type === 'error') showDiag(`engine error: ${e.data.message}`.slice(0, 140), true);
    };
    node.onprocessorerror = () => showDiag('audio engine crashed - reload the page', true);
    ctx.onstatechange = refreshDiag;
    audio = { ctx, node };
    sendFullState();
    refreshDiag();
    document.getElementById('startoverlay').remove();
  } catch (err) {
    audio = null;
    showDiag(`audio failed: ${err}`.slice(0, 140), true);
    const hint = document.querySelector('#startoverlay .hint');
    if (hint) hint.textContent = `audio failed: ${err} - tap to retry`;
  }
}
// Start on 'click', not pointerdown: on touch devices the browser only
// grants the user-activation Web Audio needs when the tap completes.
// Not {once:true} so a failed startup can be retried by tapping again.
document.getElementById('startoverlay').addEventListener('click', startAudio);

// ?nosplash: hide the start overlay without starting audio (screenshots).
if (new URLSearchParams(location.search).has('nosplash'))
  document.getElementById('startoverlay').remove();

// Safety net: if the context gets suspended (tab switch, phone
// interruptions), the next completed tap revives it.
window.addEventListener('pointerup', () => {
  if (audio && audio.ctx.state === 'suspended') audio.ctx.resume();
}, { capture: true });

let lastVizPos = -1;
function tick() {
  if (latestNorms) {
    for (const id of DESTS) knobs[id].setLiveNorm(latestNorms[D[id]]);
    vizPos = latestNorms[D.position];
  } else {
    vizPos = null;
    for (const id of DESTS) knobs[id].setLiveNorm(knobs[id].norm());
  }
  const p = vizPos ?? state.params.position;
  if (Math.abs(p - lastVizPos) > 0.002) {
    lastVizPos = p;
    drawViz();
  }
  requestAnimationFrame(tick);
}

// initial paint
for (const id of DESTS) knobs[id].refresh();
syncMatrix();
refreshPosSlider();
drawViz();
requestAnimationFrame(tick);
