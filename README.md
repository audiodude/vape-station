# VapeStation

**A free, cross-platform "graintable" synthesizer.** Wavetable morphing meets
granular texture. Every knob on the panel can be modulated by dragging
an envelope or LFO straight onto it.

VapeStation plays clouds of pitch-tracked grains from morphing wavetable
frame-stacks. Sweep **Position** to morph through a table's 64 frames like a
wavetable synth. Use **Spray**, **Pitch Rnd**, and **Density** and the
same patch dissolves into granular haze, shimmer, or grit. Five built-in
tables (**Morph, Sweep, Vox, Bells, Grit**) cover smooth analog-ish morphs,
vocal formants, inharmonic bell spectra, and raw noise-bitten textures.

## Why you'll like it

- **Drag-and-drop modulation, everywhere.** Grab a coloured source chip
  (three envelopes, two LFOs, velocity, mod wheel, or keytrack) and
  drop it on any knob. All 26 continuous parameters are targets, including
  envelope times and the LFO rates themselves.
- **See your modulation.** Modulated knobs grow coloured halo rings showing
  each route's span, with a white dot riding the live modulated value. A
  matrix panel lists every route with a bipolar depth slider.
- **Play it like an instrument.** 10 voices, sustain pedal, pitch bend, and
  per-LFO **Retrig / First Note / Global** modes so modulation can lock to
  every note, to your phrasing, or run free across the whole performance.
- **Knobs that feel right.** Every control snaps to musically spaced steps
  with real units on the readout ("2.4 Hz", "250 ms"), envelope knobs are
  calibrated to clock positions (250 ms at 9 o'clock, 1 s at noon, 4 s at
  3 o'clock), and automation stays perfectly smooth underneath.
- **A safe playground.** One INIT button back to a known-good default patch;
  the default patch already has a moving route so you hear the point
  immediately.
- **Free, open source, everywhere.** Linux, Windows, and macOS (universal),
  as VST3 and standalone. Passes `pluginval --strictness-level 10`.

## Get it

**Play it in your browser first**: https://vapestation.audiodude.xyz — the
full synth (same DSP, ported to an AudioWorklet) running entirely
client-side. Works with mouse, computer keys, or a MIDI keyboard.

Download from [Releases](../../releases/latest): a zip per platform, or the
`all-platforms` tarball — one cross-platform `.vst3` bundle that works on
all three OSes. Bleeding-edge builds are on the latest
[`build` workflow run](../../actions/workflows/build.yml) as artifacts
(GitHub login required for those). Or build from source (below).

Install by dropping `VapeStation.vst3` into your VST3 folder:

- **Linux** — `~/.vst3/`
- **Windows** — `C:\Program Files\Common Files\VST3\`
- **macOS** — `~/Library/Audio/Plug-Ins/VST3/`; builds are unsigned, so
  clear quarantine after unpacking:
  `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/VapeStation.vst3`

## Quick start

1. Play a note — the default patch already has LFO1 slowly scanning table
   Position.
2. Drag the **LFO2** chip onto **Cutoff**. A green ring appears; the filter
   starts breathing.
3. Drag horizontally on the waveform display to set Position by hand.
4. Open **Spray** and **Pitch Rnd** a little; raise **Size** for washes,
   lower it for buzz.
5. Too far? Hit **INIT** and start clean.

## VapeStation: Detailed specs

**Graintable engine** — five built-in tables (Morph, Sweep, Vox, Bells, Grit),
each 64 single-cycle frames generated procedurally at startup and band-limited
into 4 mip levels. Each voice schedules overlapping Hann-windowed grains that
read from the table at the note's pitch:

- **Position** — scan position through the table's frames (the main timbre control)
- **Size / Density** — grain length (10-500 ms) and overlap count (1-8)
- **Spray** — random per-grain position scatter
- **Pitch Rnd** — random per-grain detune (up to ±12 st)
- **Shape** — grain window skew
- **Coarse / Fine / Spread** — transpose, detune cents, stereo grain panning

Per voice: state-variable filter (LP/BP/HP), ADSR amp envelope. 10 voices,
sustain pedal and pitch bend (±2 st) supported.

Knob gestures snap to a musically spaced step grid (~5-10% "nice number"
increments for times/rates/cutoff, 0.5 ms floor on envelope times, whole
semitones/cents for Coarse/Fine, 1% for 0-1 params); host automation and
modulation stay continuous. Envelope times run 0-10 s on a clock-calibrated
taper: fully CCW = 0 (releases bottom out at 5 ms to stay click-free),
9 o'clock = 250 ms, noon = 1 s, 3 o'clock = 4 s. LFO rates are linear
0.1-5 Hz. The INIT button in the header resets all parameters and the mod
matrix to the default patch.

**Modulation** — the point of the synth. Sources: ENV1 (amp), ENV2, ENV3,
LFO1, LFO2, velocity, mod wheel, keytrack. Each LFO has a mode selector:
**Retrig** (per-voice, phase restarts every note-on and the value follows the
note), **First Note** (one shared LFO that restarts on a note-on when no keys
are held), or **Global** (one shared free-running LFO, never reset). Any source
can target any of the 26 continuous parameters (including envelope times and
LFO rates themselves — rate modulation is per-voice, so it only applies in
Retrig mode):

- **Drag a coloured chip** (on the ENV/LFO panel headers, or VEL/WHEEL/KEY in
  the matrix header) **onto any knob** to create a route at +0.35 depth.
- Modulated knobs grow **coloured rings** showing the modulation span from the
  base value, plus a white dot tracking the live modulated value of the most
  recent voice.
- The **MATRIX panel** lists all routes with a bipolar depth slider (-1..+1,
  scaled to the full parameter range) and an `x` to remove; right-clicking a
  knob also offers per-route removal.
- Modulation is evaluated per voice at control rate (32 samples) and never
  moves the host-visible base value.

The graintable display doubles as a control: drag horizontally to set
Position. Routes persist in the plugin state.

---

## Building from source

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target VapeStation_VST3 VapeStation_Standalone -j$(nproc)
```

The first configure downloads JUCE 8.0.14 (set the `JUCE_TARBALL` env var to a
pre-downloaded tarball path to skip the network fetch). The VST3 is
auto-copied to the platform VST3 dir (`~/.vst3` on Linux,
`~/Library/Audio/Plug-Ins/VST3` on macOS) after building; the standalone app
lands at `build/VapeStation_artefacts/Release/Standalone/VapeStation`.

On macOS, `./build_and_copy_mac.sh` builds a universal (arm64 + x86_64)
release and copies the VST3 into `~/Library/Audio/Plug-Ins/VST3`, replacing
any previous copy.

CI builds all three platforms: the `build` GitHub Actions workflow
(`.github/workflows/build.yml`) builds the VST3 and runs RenderTest on
Linux (x64), Windows (x64), and macOS (universal) on every push to `main`
(or manually via workflow dispatch), then merges the three into the
cross-platform bundle artifact. Pass `-DVAPE_COPY_PLUGIN=OFF` to CMake to
skip the local install-after-build copy step (CI does this).

## Verify

```
cmake --build build --target RenderTest -j$(nproc)
./build/RenderTest_artefacts/Release/RenderTest [outputDir]
```

RenderTest renders MIDI offline through the real processor and checks:
silence without notes, audio + finite output + clean release with notes,
deterministic rendering, that a mod route measurably changes the output,
LFO mode behaviour, parameter taper/step calibration, INIT, and state
save/load round-tripping. It also writes `vape-demo.wav` (a short listening
demo) and `ui-snapshot.png` (an editor screenshot) to `outputDir`. Exit code
is the number of failed checks.

The plugin passes `pluginval --strictness-level 10`.

## Web version

`webapp/` is a static, no-build port of the synth to the Web Audio API: the
DSP engine (tables, grain voices, filter, envelopes, LFO modes, mod matrix)
lives in `webapp/js/engine.js` and runs in an AudioWorklet; the UI mirrors
the plugin. `node webapp/test/render-test.mjs` runs the RenderTest checks
against the JS engine offline. `webapp/serve.sh` serves it locally
(AudioWorklet modules need http, not file://). CI deploys it to Cloudflare
Pages (https://vapestation.audiodude.xyz) via the `deploy-web` job whenever
a release is published.

## Layout

- `Source/Params.h` — parameter/destination definitions (single source of truth)
- `Source/ModMatrix.h` — sources, route compilation to an immutable snapshot
  (atomically swapped for the audio thread)
- `Source/GrainTables.*` — procedural table generation (additive spectra → iFFT)
- `Source/GrainVoice.*` — grain scheduler, per-voice mod evaluation, filter, envs
- `Source/PluginProcessor.*` — APVTS, synth, matrix state in the APVTS tree
- `Source/UI/` — editor: `ModKnob` (drop target + mod rings), `SourceChip`,
  `MatrixPanel`, `TableViz`, `Theme`
- `Tests/RenderTest.cpp` — offline verification harness

## Known rough edges (it's a prototype)

- No tempo sync on LFOs
- No unison
- Mip switching between notes can step timbre at extreme transpose
- Matrix depths aren't host-automatable parameters
- Fixed-size UI
