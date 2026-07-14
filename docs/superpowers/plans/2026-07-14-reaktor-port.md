# VapeStation → Reaktor Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a wavetable-export tool plus a complete, buildable Reaktor 6
construction guide that lets the user hand-build a Reaktor ensemble version
of VapeStation, per `docs/superpowers/specs/2026-07-14-reaktor-port-design.md`.

**Architecture:** A Node script (`webapp/tools/export-reaktor-tables.mjs`)
reuses the existing, tested `grainTables()` in `webapp/js/engine.js` to
render 20 WAV files (5 tables × 4 mips, 64 frames each). A markdown build
guide (`docs/reaktor-port/build-guide.md`) documents the full Reaktor
structure macro-by-macro: Voice Combiner + globals, an 8×26 mod matrix built
from a reusable row macro, 3 ADSRs, 2 mode-switchable LFOs, a Grain
Cloud-based grain engine with the exact port mapping, an SVF filter, and the
wavetable import step. `README.md` gets a short pointer to the guide.

**Tech Stack:** Node.js (ESM, no dependencies — matches the existing
`webapp/js` no-build philosophy), hand-written WAV PCM writer, Markdown.

## Global Constraints

- No new npm dependencies — `webapp/` is a static, no-build project
  (`README.md`: "a static, no-build port... runs in an AudioWorklet").
- Reuse `grainTables()` from `webapp/js/engine.js` verbatim (import it) —
  do not re-derive the additive/iFFT synthesis; it's already verified
  byte-identical to the C++ engine by `webapp/test/render-test.mjs`.
- Generated WAV files are **not committed** to the repo (spec: "Out of
  scope" / "Wavetable data pipeline" — ~10 MB, regenerable, consumed only
  by Reaktor).
- Every stock Reaktor module/macro name used in the guide must be one
  confirmed during design research (Voice Combiner, Reaktor Blocks ADSR/
  LFO/SVF Filter modules, Grain Cloud with its documented port list P, D/F,
  PJ, PS, Sel, Pos, PsJ, Len, LnJ, Att, Dec, Dist, DisJ, Pan, PnJ, Overlap) —
  do not invent module names not in that list.
- The guide must include the "Known deviations from the plugin" list from
  the spec verbatim in substance (frame hard-select, linear Shape ramp,
  8-grain overlap cap, note-level mip selection, continuous vs. control-rate
  matrix evaluation, fixed-grid matrix UI, Snapshot-based INIT).

---

### Task 1: Wavetable export script

**Files:**
- Create: `webapp/tools/export-reaktor-tables.mjs`
- Modify: none (imports `webapp/js/engine.js` read-only)

**Interfaces:**
- Consumes: `grainTables()` from `../js/engine.js`, which returns an array
  of 5 `{ name, mips }` objects, where `mips` is an array of 4 entries, each
  a `Float32Array`-like flat array laid out as `numFrames * (frameLen + 1)`
  floats (`frameLen = 2048`, `numFrames = 64`, per-frame stride
  `frameLen + 1` with a wrap-guard sample at the end of each frame that
  must be dropped on export).
- Produces: 20 WAV files in an output directory (default
  `webapp/tools/reaktor-tables/`, overridable via `process.argv[2]`), named
  `<TableName>_mip<N>.wav` (e.g. `Morph_mip0.wav` … `Grit_mip3.wav`), each a
  mono 24-bit PCM WAV at 48000 Hz sample rate containing exactly
  `64 * 2048 = 131072` samples (the 64 frames concatenated, wrap-guard
  sample dropped from each).

- [ ] **Step 1: Write the script**

```javascript
// webapp/tools/export-reaktor-tables.mjs
// Renders VapeStation's 5 graintables x 4 mip levels to 20 WAV files for
// import into a Reaktor Sample Map. Each WAV is the table's 64 frames
// concatenated back-to-back (2048 samples/frame, wrap-guard sample dropped),
// declared at 48000 Hz so frame boundaries land at frameIndex * 2048/48000*1000 ms.
import { grainTables } from '../js/engine.js';
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const FRAME_LEN = 2048;
const NUM_FRAMES = 64;
const SAMPLE_RATE = 48000;
const BITS = 24;
const BYTES_PER_SAMPLE = BITS / 8;

function floatTo24BitPCM(samples) {
  const buf = Buffer.alloc(samples.length * BYTES_PER_SAMPLE);
  for (let i = 0; i < samples.length; ++i) {
    let v = Math.max(-1, Math.min(1, samples[i]));
    let ival = Math.round(v * 8388607); // 2^23 - 1
    if (ival < 0) ival += 0x1000000; // two's complement in 24 bits
    buf[i * 3] = ival & 0xff;
    buf[i * 3 + 1] = (ival >> 8) & 0xff;
    buf[i * 3 + 2] = (ival >> 16) & 0xff;
  }
  return buf;
}

function writeWav(filePath, samples) {
  const data = floatTo24BitPCM(samples);
  const blockAlign = BYTES_PER_SAMPLE; // mono
  const byteRate = SAMPLE_RATE * blockAlign;
  const header = Buffer.alloc(44);
  header.write('RIFF', 0);
  header.writeUInt32LE(36 + data.length, 4);
  header.write('WAVE', 8);
  header.write('fmt ', 12);
  header.writeUInt32LE(16, 16); // PCM fmt chunk size
  header.writeUInt16LE(1, 20); // PCM format
  header.writeUInt16LE(1, 22); // mono
  header.writeUInt32LE(SAMPLE_RATE, 24);
  header.writeUInt32LE(byteRate, 28);
  header.writeUInt16LE(blockAlign, 32);
  header.writeUInt16LE(BITS, 34);
  header.write('data', 36);
  header.writeUInt32LE(data.length, 40);
  writeFileSync(filePath, Buffer.concat([header, data]));
}

function extractFrames(mip) {
  // mip is laid out as NUM_FRAMES * (FRAME_LEN + 1), each frame followed by
  // a wrap-guard sample (== frame's own first sample) that must be dropped.
  const stride = FRAME_LEN + 1;
  const out = new Float32Array(NUM_FRAMES * FRAME_LEN);
  for (let f = 0; f < NUM_FRAMES; ++f) {
    for (let i = 0; i < FRAME_LEN; ++i) {
      out[f * FRAME_LEN + i] = mip[f * stride + i];
    }
  }
  return out;
}

function main() {
  const outDir = process.argv[2]
    ? path.resolve(process.argv[2])
    : path.join(path.dirname(fileURLToPath(import.meta.url)), 'reaktor-tables');
  mkdirSync(outDir, { recursive: true });

  const tables = grainTables();
  let count = 0;
  for (const table of tables) {
    table.mips.forEach((mip, mipIndex) => {
      const samples = extractFrames(mip);
      const outPath = path.join(outDir, `${table.name}_mip${mipIndex}.wav`);
      writeWav(outPath, samples);
      count++;
      console.log(`wrote ${outPath} (${samples.length} samples)`);
    });
  }
  console.log(`\n${count} WAV files written to ${outDir}`);
}

main();
```

- [ ] **Step 2: Run it and verify file count/size**

Run: `node webapp/tools/export-reaktor-tables.mjs`

Expected: prints 20 `wrote ...` lines (5 table names × `mip0..mip3`) ending
with `20 WAV files written to .../webapp/tools/reaktor-tables`, and:

```bash
ls webapp/tools/reaktor-tables | wc -l   # expect 20
stat -f%z webapp/tools/reaktor-tables/Morph_mip0.wav  # expect 393260 (44 header + 131072*3 data)
```

- [ ] **Step 3: Verify WAV headers are well-formed**

Run: `node -e "const fs=require('fs');const b=fs.readFileSync('webapp/tools/reaktor-tables/Grit_mip3.wav');console.log(b.toString('ascii',0,4), b.toString('ascii',8,12), b.readUInt32LE(24), b.readUInt16LE(34), b.readUInt32LE(40))"`

Expected output: `RIFF WAVE 48000 24 393216` (393216 = 131072 samples × 3
bytes).

- [ ] **Step 4: Confirm the output directory is git-ignored**

Check `webapp/.gitignore` or the root `.gitignore` for a pattern covering
`webapp/tools/reaktor-tables/`. If none exists, add one (root
`.gitignore`, new line: `webapp/tools/reaktor-tables/`) so a local run
never gets accidentally committed.

- [ ] **Step 5: Commit**

```bash
git add webapp/tools/export-reaktor-tables.mjs .gitignore
git commit -m "$(cat <<'EOF'
Add Reaktor wavetable export script

Renders VapeStation's 5 graintables x 4 mip levels to 20 WAV files
(64 frames concatenated each) for import into a Reaktor Sample Map,
reusing the existing tested engine.js grainTables() output instead of
re-deriving the additive/iFFT synthesis.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Reaktor build guide

**Files:**
- Create: `docs/reaktor-port/build-guide.md`

**Interfaces:**
- Consumes: the mapping tables and architecture from
  `docs/superpowers/specs/2026-07-14-reaktor-port-design.md`; the WAV
  naming/indexing scheme from Task 1 (`<TableName>_mip<N>.wav`,
  `Sel = tableIndex * 4 + mipIndex`, frame span `2048/48000*1000 ≈ 42.667 ms`,
  full table span `64 * 2048/48000*1000 ≈ 2730.67 ms`).
- Produces: the guide file other tasks/README link to.

This is a documentation task — "steps" are sections to write, each with
required concrete content (no placeholders permitted: every parameter name,
range, and mapping below must appear literally in the section).

- [ ] **Step 1: Write the intro + wavetable import section**

Content requirements:
- One-paragraph summary of what's being built and a link back to the spec
  and design doc.
- Prerequisite: run `node webapp/tools/export-reaktor-tables.mjs` from the
  repo root, producing 20 WAV files in `webapp/tools/reaktor-tables/`.
- Step-by-step: import all 20 WAVs into a Reaktor **Sample Map**, table
  documenting the `Sel` index for each of the 20 files:
  `Sel 0-3 = Morph mip0-3`, `4-7 = Sweep`, `8-11 = Vox`, `12-15 = Bells`,
  `16-19 = Grit` (i.e. `Sel = tableIndex*4 + mipIndex`, tableIndex order
  Morph=0, Sweep=1, Vox=2, Bells=3, Grit=4 — matching `tableNames()` in
  `Source/Params.h`).
- Document the Pos-in-ms formula: `Pos_ms(frameIndex) = frameIndex * (2048/48000*1000)` ≈ `frameIndex * 42.667`, table span ≈ `2730.67 ms`.

- [ ] **Step 2: Write the top-level structure section**

Content requirements (from the design spec's "Top level" section):
- MIDI In → Voice Combiner: Poly = 10, sustain pedal on, pitch bend range
  ±2 semitones, wrapping one `Voice` macro.
- Global macros built once outside the Voice Combiner: `Table` select
  (5-way: Morph/Sweep/Vox/Bells/Grit), `Filter Type` select (3-way: Low
  Pass/Band Pass/High Pass), `LFO1 Shape`/`LFO2 Shape` (6-way: Sine/
  Triangle/Saw Up/Saw Down/Square/S&H), `LFO1 Mode`/`LFO2 Mode` (3-way:
  Retrig/First Note/Global), two shared-LFO macros, a held-note counter
  macro, mod wheel (CC1) value.
- Master: Voice Combiner output → `Gain` knob (-60..+6 dB, default -8 dB,
  itself a mod destination) → output.

- [ ] **Step 3: Write the modulation matrix section**

Content requirements:
- The `MatrixRow` macro: 8 depth knobs (range -1..+1) labeled Env1, Env2,
  Env3, LFO1, LFO2, Velocity, Wheel, Keytrack; one base-parameter input;
  sums in **normalized 0-1 space** (`norm = base01 + sum(depth_i * src_i)`,
  clip to 0-1, denormalize to the destination's real range) — this ordering
  must be called out explicitly as matching `GrainVoice::updateControls`.
- Full list of the 26 destination instantiations (verbatim from `Dest` in
  `Source/Params.h`): Position (0-1), GrainSize (10-500ms), Density (1-8),
  Spray (0-1), PitchRand (0-12st), Shape (0-1), Coarse (-24..24st), Fine
  (-100..100ct), Spread (0-1), Cutoff (20-20000Hz), Res (0-1), Env1 A/D/S/R,
  Env2 A/D/S/R, Env3 A/D/S/R, LFO1Rate (0.1-5Hz), LFO2Rate (0.1-5Hz), Gain
  (-60..6dB) — 26 total, 208 depth knobs.
- Source value semantics: Env1/Env2/Env3/Velocity/Wheel are unipolar 0..1;
  LFO1/LFO2/Keytrack are bipolar -1..1 (matches `isBipolarSrc` in
  `Source/ModMatrix.h`). Keytrack = `(note - 60) / 36`, clamped -1..1.

- [ ] **Step 4: Write the envelopes and LFOs section**

Content requirements:
- 3x Reaktor Blocks ADSR module instances per voice, Attack/Decay/Sustain/
  Release driven by the matrix's `env{1,2,3}{A,D,S,R}` outputs (convert ms
  to seconds). Env1 is the amp envelope: final voice amplitude =
  `env1_output * Gain`.
- 2x LFO: one Blocks LFO module per voice (phase reset on note-on = Retrig
  mode), rate from `lfo{1,2}Rate` matrix output; plus a 3-way Selector per
  LFO choosing between the per-voice Retrig instance, the shared First-Note
  macro's broadcast, and the shared Global macro's broadcast, switched by
  the corresponding `LFO Mode` global control. Document that rate
  modulation (`lfo{1,2}Rate` as a matrix destination) only has an audible
  effect in Retrig mode, matching the original engine.
- First Note reset logic: the held-note counter macro detects a 0→1
  transition in concurrently-held notes and pulses a reset into the shared
  First-Note LFO macro.

- [ ] **Step 5: Write the grain engine section**

Content requirements — the full Grain Cloud port mapping table:

| VapeStation control | Grain Cloud port | Notes |
|---|---|---|
| Position (0-1) | `Pos` | Quantize to nearest of 64 frame boundaries: `Pos_ms = round(position * 63) * 42.667` |
| Spray (0-1) | `PsJ` | `PsJ_ms = spray * 2730.67` |
| Pitch Rnd (0-12st) | `PJ` | Direct, verify PJ's unit in Reaktor's module properties before wiring |
| Grain Size (10-500ms) | `Len` | Direct |
| Density (1-8) via `Size/Density` | `Dist` | `Dist_ms = grainSize / density` |
| Shape (0-1) | `Att`/`Dec` | Linear approximation: `Att = 0.5 + (shape-0.5)*0.8`, `Dec = 1 - Att` |
| Spread (0-1) | `Pan`(center)/`PnJ` | `Pan = 0.5`, `PnJ = spread` |
| note pitch + Coarse + Fine + pitch bend | `P` | From matrix `coarse`/`fine` outputs + Voice Combiner pitch + pitch bend |
| `Table` select x mip | `Sel` | `Sel = tableIndex*4 + mip`; mip from the comparator chain below |
| — | `Overlap` (module property, not a port) | Set to 8 |

- Mip-selection comparator chain: replicate `GrainTable::mipForFreq`
  (`Source/GrainTables.h`) — for `mipCaps = [64, 16, 4, 1]`, pick the first
  (lowest-index) mip where `baseFreq * mipCap < 0.45 * sampleRate`, else the
  last mip. Document this is evaluated once per note-on from the voice's
  base frequency (an approximation — the original re-evaluates per grain
  including that grain's random pitch offset).

- [ ] **Step 6: Write the filter, output, and known-deviations sections**

Content requirements:
- SVF filter (Reaktor Blocks module): type = global `Filter Type`
  (LP/BP/HP), Cutoff from matrix `cutoff` output (20-20000 Hz), Resonance
  from matrix `res` output (0-1) via `0.707 * 10^(res*1.15)`.
- INIT: document saving a Reaktor Snapshot named "INIT" with the default
  values from `Source/Params.h` (Position 0.15, GrainSize 90ms, Density 3,
  Spray 0.03, PitchRand 0, Shape 0.5, Coarse 0, Fine 0, Spread 0.35, Cutoff
  14000Hz, Res 0.15, Env1 A/D/S/R = 4/200/0.8/250, Env2 = 120/300/0.5/300,
  Env3 = 400/600/0.6/800, LFO1Rate 2Hz, LFO2Rate 0.35Hz, Gain -8dB) instead
  of custom reset logic.
- The "Known deviations from the plugin" list verbatim from the design
  spec's section of the same name (frame hard-select, linear Shape ramp,
  8-grain overlap cap, note-level mip selection, continuous vs.
  control-rate matrix evaluation being a non-issue, fixed-grid matrix UI,
  Snapshot-based INIT).
- A closing checklist: "MIDI Automatable" flag reminder for the 26
  destination knobs and 208 matrix depth knobs (Reaktor's automation
  convention, called out in the spec's Out of scope section).

- [ ] **Step 7: Self-check the guide against the spec**

Run: `grep -c '^|' docs/reaktor-port/build-guide.md` — expect at least 27
(the 26-row destination table + header separator).

Run this check to confirm every destination name from `Params.h` appears in
the guide:

```bash
for d in Position GrainSize Density Spray PitchRand Shape Coarse Fine Spread Cutoff Res "Env1 A" "Env1 D" "Env1 S" "Env1 R" "Env2 A" "Env2 D" "Env2 S" "Env2 R" "Env3 A" "Env3 D" "Env3 S" "Env3 R" LFO1Rate LFO2Rate Gain; do
  grep -qi "$d" docs/reaktor-port/build-guide.md || echo "MISSING: $d"
done
```

Expected: no `MISSING` lines printed. Fix the guide if any print.

- [ ] **Step 8: Commit**

```bash
git add docs/reaktor-port/build-guide.md
git commit -m "$(cat <<'EOF'
Add Reaktor 6 build guide for the VapeStation port

Macro-by-macro construction guide: Voice Combiner + globals, 8x26 mod
matrix, 3 ADSRs, 2 mode-switchable LFOs, a Grain Cloud-based grain
engine with the full port mapping, SVF filter, wavetable import via
the Task 1 export script, and the known deviations from the plugin.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: README pointer

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: nothing new.
- Produces: nothing consumed by other tasks (terminal task).

- [ ] **Step 1: Add a short section after "## Web version"**

Insert into `README.md` immediately after the existing `## Web version`
section (before `## Layout`):

```markdown
## Reaktor port

`docs/reaktor-port/build-guide.md` is a from-scratch construction guide for
building VapeStation as a Native Instruments Reaktor 6 ensemble by hand (full
Reaktor, not Player). `webapp/tools/export-reaktor-tables.mjs` renders the 5
graintables to WAV files for the guide's Sample Map import step. It's a
by-hand port, not an automated one — see the guide's "Known deviations"
section for where Reaktor's stock Grain Cloud module can't fully match the
original engine (frame-interpolated morphing, curved grain windows).
```

- [ ] **Step 2: Verify placement**

Run: `grep -n '^## ' README.md` and confirm `## Reaktor port` appears
directly after `## Web version` and before `## Layout`.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "$(cat <<'EOF'
Document the Reaktor port build guide in the README

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
