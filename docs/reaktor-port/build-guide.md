# Building VapeStation in Reaktor 6

This is a from-scratch construction guide for building a Reaktor 6/6.5
ensemble (full Reaktor, not Player — Player can't edit structure) that
recreates VapeStation, the graintable synth in this repo. It's a by-hand
port: you build it in the Reaktor editor by following this guide, macro by
macro. Background and the reasoning behind every design decision here lives
in [`docs/superpowers/specs/2026-07-14-reaktor-port-design.md`](../superpowers/specs/2026-07-14-reaktor-port-design.md).

Read the **Known deviations from the plugin** section near the end before
you start — a couple of controls (Position, Shape) work slightly
differently here than in the plugin, on purpose, and it's worth knowing
that going in rather than wondering why the ensemble doesn't sound
byte-identical.

## 0. Wavetable import

VapeStation's 5 tables (Morph, Sweep, Vox, Bells, Grit) are procedurally
generated at startup in the plugin (additive harmonic spectra → iFFT,
`Source/GrainTables.cpp`). Rather than re-deriving that synthesis inside
Reaktor, render it once to WAV files using the project's own (tested)
engine code:

```bash
node webapp/tools/export-reaktor-tables.mjs
```

This writes 20 mono 24-bit/48kHz WAV files to `webapp/tools/reaktor-tables/`:
each table's 64 single-cycle frames (2048 samples each) concatenated
back-to-back, one file per table per anti-aliasing "mip" level (4 mips per
table, capped at 64/16/4/1 harmonics respectively — see `MIP_CAPS` in
`webapp/js/engine.js`).

Import all 20 into a Reaktor **Sample Map**. Assign the `Sel` index (used
throughout this guide to pick which WAV a grain reads from) as:

| Sel | File | Sel | File | Sel | File | Sel | File | Sel | File |
|---|---|---|---|---|---|---|---|---|---|
| 0 | Morph_mip0 | 4 | Sweep_mip0 | 8 | Vox_mip0 | 12 | Bells_mip0 | 16 | Grit_mip0 |
| 1 | Morph_mip1 | 5 | Sweep_mip1 | 9 | Vox_mip1 | 13 | Bells_mip1 | 17 | Grit_mip1 |
| 2 | Morph_mip2 | 6 | Sweep_mip2 | 10 | Vox_mip2 | 14 | Bells_mip2 | 18 | Grit_mip2 |
| 3 | Morph_mip3 | 7 | Sweep_mip3 | 11 | Vox_mip3 | 15 | Bells_mip3 | 19 | Grit_mip3 |

i.e. `Sel = tableIndex * 4 + mipIndex`, table order Morph=0, Sweep=1, Vox=2,
Bells=3, Grit=4 (matches `tableNames()` in `Source/Params.h`).

Because each WAV is declared at 48000 Hz, a frame index maps to a
millisecond offset as:

```
Pos_ms(frameIndex) = frameIndex * (2048 / 48000 * 1000) ≈ frameIndex * 42.667
table span ≈ 64 * 42.667 ≈ 2730.67 ms
```

You'll use this formula in section 5 to drive Grain Cloud's `Pos` input
from the Position knob.

## 1. Top-level structure

- **MIDI In** → a stock **Voice Combiner**: Poly = 10, sustain pedal
  enabled, pitch bend range ±2 semitones. Wrap one `Voice` macro (built in
  sections 2–5) inside it — the Voice Combiner replicates that macro per
  active note automatically, so you only build the voice once.
- **Global controls**, built once outside the Voice Combiner (these are
  read by every voice, not per-voice state):
  - `Table` — 5-way selector: Morph / Sweep / Vox / Bells / Grit.
  - `Filter Type` — 3-way selector: Low Pass / Band Pass / High Pass.
  - `LFO1 Shape`, `LFO2 Shape` — 6-way selectors: Sine / Triangle / Saw Up /
    Saw Down / Square / S&H.
  - `LFO1 Mode`, `LFO2 Mode` — 3-way selectors: Retrig / First Note /
    Global.
  - Two **shared-LFO macros** (used only when a voice's corresponding LFO
    Mode is First Note or Global — see section 3) broadcasting their value
    on a global bus.
  - A **held-note counter** macro: increments on note-on, decrements on
    note-off; used to detect the 0→1 transition ("no keys held → first
    note") that resets the First Note shared LFO.
  - Mod wheel value (MIDI CC1), read globally.
- **Master output**: Voice Combiner's summed output → a `Gain` knob
  (-60..+6 dB, default -8 dB — this is itself a mod-matrix destination, see
  section 3) → main output.

## 2. Modulation matrix

Build one reusable macro, `MatrixRow`:

- 8 depth knobs, range -1..+1, labeled **Env1, Env2, Env3, LFO1, LFO2,
  Velocity, Wheel, Keytrack** (one per modulation source).
- 8 corresponding source-value inputs (wired in section 3/4/5 to the
  actual Env/LFO/Velocity/Wheel/Keytrack values).
- 1 base-parameter input (the destination's own knob, normalized 0-1).
- Output: `norm = clip01(base01 + sum(depth_i * source_i))`, then
  denormalized to the destination's real range.

This ordering matters and must match `GrainVoice::updateControls` in
`Source/GrainVoice.cpp`: **summing happens in normalized 0–1 space**, then
the result is clipped to 0–1 and denormalized — not summed in the
destination's real units.

Source value semantics (must match `Source/ModMatrix.h`):
- **Env1, Env2, Env3, Velocity, Wheel** are unipolar, 0..1.
- **LFO1, LFO2, Keytrack** are bipolar, -1..1. Keytrack =
  `(note - 60) / 36`, clamped to -1..1.

Instantiate `MatrixRow` 26 times, one per destination (verbatim from `Dest`
in `Source/Params.h`), giving 208 depth knobs total (most default to 0):

| # | Destination | Range | Default |
|---|---|---|---|
| 1 | Position | 0–1 | 0.15 |
| 2 | GrainSize | 10–500 ms | 90 |
| 3 | Density | 1–8 | 3 |
| 4 | Spray | 0–1 | 0.03 |
| 5 | PitchRand | 0–12 st | 0 |
| 6 | Shape | 0–1 | 0.5 |
| 7 | Coarse | -24..24 st | 0 |
| 8 | Fine | -100..100 ct | 0 |
| 9 | Spread | 0–1 | 0.35 |
| 10 | Cutoff | 20–20000 Hz | 14000 |
| 11 | Res | 0–1 | 0.15 |
| 12 | Env1 Attack | 0–10 s | 4 ms |
| 13 | Env1 Decay | 0–10 s | 200 ms |
| 14 | Env1 Sustain | 0–1 | 0.8 |
| 15 | Env1 Release | 5ms–10s | 250 ms |
| 16 | Env2 Attack | 0–10 s | 120 ms |
| 17 | Env2 Decay | 0–10 s | 300 ms |
| 18 | Env2 Sustain | 0–1 | 0.5 |
| 19 | Env2 Release | 5ms–10s | 300 ms |
| 20 | Env3 Attack | 0–10 s | 400 ms |
| 21 | Env3 Decay | 0–10 s | 600 ms |
| 22 | Env3 Sustain | 0–1 | 0.6 |
| 23 | Env3 Release | 5ms–10s | 800 ms |
| 24 | LFO1Rate | 0.1–5 Hz | 2 |
| 25 | LFO2Rate | 0.1–5 Hz | 0.35 |
| 26 | Gain | -60..6 dB | -8 |

Reaktor evaluates the matrix continuously (audio/event rate) rather than
the plugin's 32-sample control-rate tick. That tick was a CPU optimization
in the original engine, not a deliberate sonic choice — continuous
evaluation in Reaktor is not a fidelity loss.

## 3. Envelopes and LFOs

- **3× ADSR** (Reaktor Blocks ADSR module), one per voice: Attack/Decay/
  Sustain/Release driven by the matrix's `Env1/2/3 A/D/S/R` outputs from
  section 2 (convert ms → s). **Env1 is the fixed amp envelope** — final
  voice amplitude is `env1_output * Gain` (see section 5's output stage).
- **2× LFO**: for each of LFO1/LFO2:
  - One Blocks LFO module instance per voice, phase reset on note-on (this
    is Retrig mode), rate from the matrix's `LFO{1,2}Rate` output.
  - A 3-way Selector choosing between: (a) this per-voice Retrig instance,
    (b) the shared First-Note macro's broadcast value, (c) the shared
    Global macro's broadcast value — switched by the corresponding
    `LFO Mode` global control from section 1.
  - Rate modulation (the `LFO1Rate`/`LFO2Rate` matrix destinations) only
    has an audible effect in Retrig mode, since First Note/Global LFOs are
    shared macros outside any one voice — this matches the original
    engine's behavior exactly.
- **First Note reset**: the held-note counter macro (section 1) pulses a
  phase reset into the shared First-Note LFO macro whenever the count goes
  from 0 to 1.

## 4. Grain engine (Grain Cloud)

Use the stock factory **Grain Cloud** macro (Building Blocks > Samplers) as
the grain engine, one instance per voice, reading from the Sample Map built
in section 0. Port mapping:

| VapeStation control | Grain Cloud port | Formula / notes |
|---|---|---|
| Position (0–1) | `Pos` | `Pos_ms = round(position * 63) * 42.667` — quantizes to the nearest of the 64 frame boundaries |
| Spray (0–1) | `PsJ` | `PsJ_ms = spray * 2730.67` |
| Pitch Rnd (0–12 st) | `PJ` | Direct — check `PJ`'s unit in the module's properties panel before wiring; scale if it's not already semitones |
| Grain Size (10–500 ms) | `Len` | Direct |
| Density (1–8) | `Dist` | `Dist_ms = grainSize / density` (matches `curInterval` in `GrainVoice.cpp`) |
| Shape (0–1) | `Att` / `Dec` | `Att = 0.5 + (shape - 0.5) * 0.8`, `Dec = 1 - Att` — a linear approximation of the plugin's gamma-curved Hann window (see Known deviations) |
| Spread (0–1) | `Pan` / `PnJ` | `Pan = 0.5` (center), `PnJ = spread` |
| note pitch + Coarse + Fine + pitch bend | `P` | Sum of the Voice Combiner's note pitch, the matrix's `Coarse`/`Fine` outputs, and pitch bend |
| Table × mip | `Sel` | `Sel = tableIndex * 4 + mip` (section 0); `mip` from the comparator chain below |
| — | `Overlap` (module property, not a port) | Set to **8** (reduced from the plugin's 24-grain ceiling — raise this later if you want closer-to-original density at the cost of more voices' worth of CPU) |

**Mip selection**: replicate `GrainTable::mipForFreq` (`Source/GrainTables.h`)
with a small comparator chain. For `mipCaps = [64, 16, 4, 1]` (mip index 0–3),
pick the *first* (lowest-index) mip where `baseFreq * mipCap < 0.45 * sampleRate`;
if none qualify, use mip 3. Wire this from the voice's base note frequency,
evaluated once at note-on (not per-grain — see Known deviations).

## 5. Filter and output

- **SVF filter** (Reaktor Blocks module): type = global `Filter Type`
  (LP/BP/HP), Cutoff from the matrix's `Cutoff` output (20–20000 Hz),
  Resonance from the matrix's `Res` output (0–1) via
  `resonance = 0.707 * 10^(res * 1.15)`.
- **Amp stage**: `voice_out = grain_cloud_out * env1_output`, filtered by
  the SVF above, then summed by the Voice Combiner and multiplied by the
  global `Gain` knob (section 1) before the main output.
- **INIT**: rather than custom reset logic, save a Reaktor **Snapshot**
  named `INIT` with these default values (from `Source/Params.h`): Position
  0.15, GrainSize 90 ms, Density 3, Spray 0.03, PitchRand 0, Shape 0.5,
  Coarse 0, Fine 0, Spread 0.35, Cutoff 14000 Hz, Res 0.15, Env1 A/D/S/R =
  4 ms / 200 ms / 0.8 / 250 ms, Env2 = 120 ms / 300 ms / 0.5 / 300 ms, Env3
  = 400 ms / 600 ms / 0.6 / 800 ms, LFO1Rate 2 Hz, LFO2Rate 0.35 Hz, Gain
  -8 dB, all 208 matrix depth knobs at 0.

## Known deviations from the plugin

These are deliberate, accepted trade-offs — not bugs to chase down:

- **Position hard-selects** the nearest of the 64 frames; there's no
  inter-frame morph/blend the way the plugin smoothly interpolates between
  adjacent frames. This is a Grain Cloud limitation (it grains from one
  continuous buffer, not discrete interpolated frames) — see the design
  spec's Feasibility Findings.
- **Shape** is a linear `Att`/`Dec` split, not the plugin's gamma-curved
  Hann window — it'll sound a bit more angular/triangular rather than
  smoothly rounded, especially at the extremes of the knob.
- **Grain overlap is capped at 8**, not 24 — audible mainly at very high
  Density + long Size combinations, where the plugin can layer more
  simultaneous grains.
- **Mip (anti-aliasing table) selection happens once per note-on** from the
  voice's base pitch, not per-grain incorporating that grain's own random
  pitch offset (Pitch Rnd) the way the plugin recalculates it per grain.
  Only matters at extreme Pitch Rnd settings near a mip boundary.
- The mod matrix evaluating continuously instead of on the plugin's
  32-sample control-rate tick is **not** a fidelity loss (see section 2).
- **Matrix depths are a fixed 8×26 knob grid**, not the plugin's
  drag-a-chip-onto-a-knob UI — same modulation capability, just presented
  as a matrix panel (which is what the plugin's own MATRIX panel already
  looks like, per its README).
- **INIT is a saved Snapshot**, not custom reset logic.

## Automation checklist

Reaktor's MIDI-learn/automation model differs from the plugin's JUCE
APVTS-based host automation. Once the ensemble is built, go through the 26
destination knobs and the 208 matrix depth knobs and flag each as **"MIDI
Automatable"** in its properties panel if you want host/MIDI-CC control
parity with the plugin.
