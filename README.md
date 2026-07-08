# VapeStation

A graintable synthesizer VST3 (prototype). Classic pitch-synchronous granular
playback over morphing wavetable frame-stacks, with the headline feature being
**Serum-style modulation**: every continuous parameter can be modulated by any
envelope, LFO, or performance source via drag-and-drop.

Built with JUCE 8 (fetched automatically by CMake). Formats: **VST3** and
**Standalone**, tested on Linux.

## Build

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target VapeStation_VST3 VapeStation_Standalone -j$(nproc)
```

The first configure downloads JUCE 8.0.14 (set the `JUCE_TARBALL` env var to a
pre-downloaded tarball path to skip the network fetch). The VST3 is
auto-copied to `~/.vst3/VapeStation.vst3` after building; the standalone app
lands at `build/VapeStation_artefacts/Release/Standalone/VapeStation`.

## Verify

```
cmake --build build --target RenderTest -j$(nproc)
./build/RenderTest_artefacts/Release/RenderTest [outputDir]
```

RenderTest renders MIDI offline through the real processor and checks:
silence without notes, audio + finite output + clean release with notes,
deterministic rendering, that a mod route measurably changes the output, and
state save/load round-tripping. It also writes `vape-demo.wav` (a short
listening demo) and `ui-snapshot.png` (an editor screenshot) to `outputDir`.
Exit code is the number of failed checks.

The plugin passes `pluginval --strictness-level 10`.

## The synth

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

**Modulation** — the point of the prototype. Sources: ENV1 (amp), ENV2, ENV3,
LFO1, LFO2 (per-voice, retriggered), velocity, mod wheel, keytrack. Any source
can target any of the 26 continuous parameters (including envelope times and
LFO rates themselves):

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

- No tempo sync on LFOs; no unison; mip switching between notes can step timbre
  at extreme transpose; matrix depths aren't host-automatable parameters;
  fixed-size UI.
