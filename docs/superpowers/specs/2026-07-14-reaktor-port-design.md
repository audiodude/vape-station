# VapeStation → Reaktor port: design

**Date**: 2026-07-14
**Status**: Approved

## Goal

Produce a buildable version of VapeStation (the granular/wavetable JUCE synth in
this repo) as a Native Instruments Reaktor 6/6.5 ensemble, for the user to
construct by hand in the Reaktor editor (full/paid Reaktor, not Player).

## Source engine summary

VapeStation is a graintable synth: 5 procedurally generated wavetables
(`Source/GrainTables.cpp` — additive harmonic spectra → 2048-point iFFT, 64
frames each, 4 band-limited "mip" variants per frame for anti-aliasing at
high pitch), played by a per-voice grain scheduler
(`Source/GrainVoice.cpp` — up to 24 overlapping Hann-windowed, gamma-skewed
grains, spawned at a variable interval, each with random position scatter,
pitch randomization, and stereo pan spread), 10-voice polyphony, 3 ADSR
envelopes, 2 mode-switchable LFOs (Retrig/First Note/Global —
`Source/Lfo.h`), an 8-source × 26-destination normalized-space modulation
matrix (`Source/ModMatrix.h`, `Source/Params.h`), and a per-voice
state-variable filter (LP/BP/HP).

## Feasibility findings

Two research passes (web search, cited sources) grounded the design before
committing to an approach:

- **`.ensemble` files are proprietary binary**, undocumented by Native
  Instruments, and the Reaktor community's own consensus (KVR forum) is that
  hand-editing them outside the Reaktor application is impractical ("you
  can't open them in any text editor... don't hurt yourself"). No parser or
  generator exists in the wild. **Decision: no attempt to fabricate a raw
  `.ensemble` file.** The only reliable deliverable is a human build guide
  followed inside the real Reaktor editor.
- **Reaktor's stock factory library ships a "Grain Cloud" macro**
  (Building Blocks > Samplers) — confirmed real, with a documented (if
  reverse-engineered — see Polytrope's 2016 port analysis PDF) port list:
  `P`/`D/F`/`PJ`/`PS` (pitch + jitter/slide), `Sel`, `Pos`/`PsJ`,
  `Len`/`LnJ`, `Att`/`Dec`, `Dist`/`DisJ` (inter-grain interval),
  `Pan`/`PnJ`. Max simultaneous grains ("Overlap") is a static
  module-property setting, not a live port.
- Grain Cloud's fidelity limits, weighed and accepted by the user:
  - It grains from **one continuous sample buffer**, not discrete
    interpolated wavetable frames — `Pos` picks a single sample offset per
    grain with no blend between adjacent frames. VapeStation's Position
    control becomes a **hard frame-select** (round to nearest of 64 frames),
    not the original's smooth inter-frame morph.
  - `Att`/`Dec` produce a **piecewise-linear (trapezoidal) window**, not a
    curved shape. VapeStation's gamma-skewed Hann window (`Shape`) becomes a
    **linear Att/Dec split approximation**.
  - Both accepted explicitly: the alternative (a fully custom Primary-level
    grain scheduler built from Timer/Track&Hold/Ramp/Power/Table primitives,
    which *can* replicate both faithfully) was offered and declined in favor
    of Grain Cloud's much smaller build surface.

## Architecture

### Top level

- MIDI In → stock **Voice Combiner** (Poly = 10, sustain pedal on, pitch
  bend range ±2 semitones) wrapping one `Voice` macro built once and
  replicated per active note by the Voice Combiner itself.
- Global (built once, outside the Voice Combiner): `Table` select (5-way),
  `Filter Type` select (LP/BP/HP), `LFO1/2 Shape` selects (Sine/
  Triangle/Saw Up/Saw Down/Square/S&H), `LFO1/2 Mode` selects
  (Retrig/First Note/Global), two shared-LFO macros (used only when a
  voice's LFO mode is First Note or Global, broadcast in via bus), a
  held-note counter macro (note-on minus note-off count, used to detect the
  "no keys held → first note" transition for First Note mode resets), mod
  wheel value (CC1).
- Master: Voice Combiner output sum → `Gain` (itself a mod destination,
  matching `dGain`) → output.

### Voice macro

- **Modulation matrix**: one reusable `MatrixRow` macro — 8 depth knobs (one
  per source: Env1, Env2, Env3, LFO1, LFO2, Velocity, Wheel, Keytrack) plus
  a base-parameter input, summed in **normalized 0–1 space** then
  denormalized to the destination's real range (matching
  `GrainVoice::updateControls` exactly, including the 0–1 clip before
  denormalizing). Instantiated 26 times, one per destination in `Dest`
  (`Params.h`), giving 208 depth knobs total (most default 0). Fixed grid
  instead of the plugin's drag-to-add-route UI; same modulation capability.
- **3× ADSR** (Reaktor Blocks ADSR module), Attack/Decay/Sustain/Release fed
  from the matrix's `env{1,2,3}{A,D,S,R}` outputs (ms → s). Env1 is the
  fixed amp envelope, matching `l *= env1Last * gainSm * grainNorm` in the
  original.
- **2× LFO**: a per-voice Retrig instance (Blocks LFO, phase reset on
  note-on, rate from the matrix's `lfo{1,2}Rate` output — rate modulation
  only applies in Retrig mode, matching the original) plus a 3-way selector
  choosing between this instance and the two shared/global broadcast values,
  driven by the corresponding `LFO Mode` global control.
- **Grain Cloud** instance as the grain engine:
  - `P` = note pitch + Coarse + Fine + pitch bend (from matrix `eff`
    outputs, matching `liveInc` computation)
  - `PJ` = Pitch Rnd (0–12 st)
  - `Pos` = Position (0–1) quantized to the nearest of 64 frame boundaries,
    converted to ms offset into the concatenated sample
  - `PsJ` = Spray (0–1) × table span in ms
  - `Len` = Grain Size (10–500 ms)
  - `Dist` = Grain Size / Density (matches `curInterval` derivation)
  - `Att`/`Dec` = derived from `Shape` (linear-ramp approximation — see
    Feasibility findings)
  - `Pan`/`PnJ` = center / Spread
  - `Sel` = `Table × 4 + mip`, where `mip` is chosen once per note-on by a
    comparator chain replicating `GrainTable::mipForFreq` against the
    voice's base playback frequency (the original re-picks mip per grain
    including that grain's random pitch offset; this is a per-note
    approximation, called out below)
  - `Overlap` (module property) = 8, not 24 — reduces repetitive build
    surface; raise later by adjusting the property if desired.
- **SVF filter** (Blocks, LP/BP/HP from the global `Filter Type`), Cutoff
  and Resonance from the matrix's `cutoff`/`res` outputs.

### Wavetable data pipeline

New script `webapp/tools/export-reaktor-tables.mjs`, reusing the existing,
already-verified `grainTables()` in `webapp/js/engine.js` (its output is
checked byte-identical to the C++ engine by `webapp/test/render-test.mjs`).
Renders each of the 5 tables × 4 mips as a 64-frame-concatenated mono WAV
(131,072 samples at 2048 samples/frame) — 20 files, e.g. `Morph_mip0.wav` …
`Grit_mip3.wav`. This reuses tested code for the additive/iFFT synthesis
instead of re-deriving it by hand for Reaktor.

Generated WAVs are **not committed to the repo** (~10 MB, regenerable,
consumed only by a proprietary external tool) — the build guide documents
running the script once, then importing the 20 files into a Reaktor Sample
Map with the `Sel` index mapping (`tableIndex × 4 + mipIndex`) documented
alongside.

## Known deviations from the plugin

Documented explicitly in the build guide (mirroring the README's existing
"Known rough edges" section), not left implicit:

- Position hard-selects the nearest of 64 frames; no inter-frame morph
  blend (Grain Cloud limitation, accepted).
- Shape is a linear Att/Dec split, not a gamma-curved Hann window (Grain
  Cloud limitation, accepted).
- Grain overlap capped at 8 vs. the original's 24-grain ceiling.
- Mip (anti-aliasing table) selection happens once per note-on from base
  pitch, not per-grain incorporating that grain's random pitch offset.
- The mod matrix evaluates continuously (audio/event rate) rather than the
  original's 32-sample control-rate tick — a CPU optimization in the
  original, not a sonic choice, so continuous evaluation in Reaktor is a
  non-issue (arguably smoother, not a loss).
- Matrix depths are a fixed 8×26 knob grid rather than a dynamic
  drag-to-add/remove route UI — same modulation power, different (standard
  Reaktor) presentation.
- INIT becomes a saved Reaktor Snapshot rather than custom reset logic.

## Deliverables

1. `docs/reaktor-port/build-guide.md` — full macro-by-macro,
   connection-by-connection build guide covering every section above, plus
   the Known Deviations list.
2. `webapp/tools/export-reaktor-tables.mjs` — Node script generating the 20
   wavetable WAV files from the existing, tested engine code.
3. A short pointer to the guide added to `README.md`, sized to match the
   existing README's proportions.

## Out of scope

- A raw `.ensemble` file (see Feasibility findings).
- Faithful frame-interpolated morphing and gamma-curved windowing (Grain
  Cloud limitations, explicitly accepted).
- Host automation parity — Reaktor's own MIDI-learn/automation model
  differs from JUCE's APVTS; the build guide notes marking matrix and main
  knobs "MIDI Automatable" per Reaktor convention as a build step, not a
  gap to solve.
