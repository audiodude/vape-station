#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

namespace vape
{

// Every continuous parameter is a modulation destination. Order here defines
// the destination index used by the mod matrix, the voice, and the UI.
enum Dest : int
{
    dPosition, dGrainSize, dDensity, dSpray, dPitchRand, dShape, dCoarse, dFine, dSpread,
    dCutoff, dRes,
    dEnv1A, dEnv1D, dEnv1S, dEnv1R,
    dEnv2A, dEnv2D, dEnv2S, dEnv2R,
    dEnv3A, dEnv3D, dEnv3S, dEnv3R,
    dLfo1Rate, dLfo2Rate,
    dGain,
    numDests
};

struct DestInfo { const char* id; const char* name; };

inline const std::array<DestInfo, numDests>& destInfos()
{
    static const std::array<DestInfo, numDests> infos { {
        { "position",  "Position" },
        { "grainSize", "Size" },
        { "density",   "Density" },
        { "spray",     "Spray" },
        { "pitchRand", "Pitch Rnd" },
        { "shape",     "Shape" },
        { "coarse",    "Coarse" },
        { "fine",      "Fine" },
        { "spread",    "Spread" },
        { "cutoff",    "Cutoff" },
        { "res",       "Res" },
        { "env1A", "Attack" }, { "env1D", "Decay" }, { "env1S", "Sustain" }, { "env1R", "Release" },
        { "env2A", "Attack" }, { "env2D", "Decay" }, { "env2S", "Sustain" }, { "env2R", "Release" },
        { "env3A", "Attack" }, { "env3D", "Decay" }, { "env3S", "Sustain" }, { "env3R", "Release" },
        { "lfo1Rate", "Rate" },
        { "lfo2Rate", "Rate" },
        { "gain", "Gain" },
    } };
    return infos;
}

inline int destFromId (const juce::String& id)
{
    for (int d = 0; d < numDests; ++d)
        if (id == destInfos()[(size_t) d].id)
            return d;
    return -1;
}

// Human-readable dest names for the matrix panel ("ENV2 Decay" etc.)
inline juce::String destDisplayName (int d)
{
    juce::String prefix;
    if (d >= dEnv1A && d <= dEnv1R) prefix = "Env1 ";
    else if (d >= dEnv2A && d <= dEnv2R) prefix = "Env2 ";
    else if (d >= dEnv3A && d <= dEnv3R) prefix = "Env3 ";
    else if (d == dLfo1Rate) prefix = "LFO1 ";
    else if (d == dLfo2Rate) prefix = "LFO2 ";
    return prefix + destInfos()[(size_t) d].name;
}

inline const juce::StringArray& tableNames()
{
    static const juce::StringArray s { "Morph", "Sweep", "Vox", "Bells", "Grit" };
    return s;
}

inline const juce::StringArray& filterTypeNames()
{
    static const juce::StringArray s { "Low Pass", "Band Pass", "High Pass" };
    return s;
}

inline const juce::StringArray& lfoShapeNames()
{
    static const juce::StringArray s { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" };
    return s;
}

inline const juce::StringArray& lfoModeNames()
{
    static const juce::StringArray s { "Retrig", "First Note", "Global" };
    return s;
}

inline bool isEnvTime (int d)
{
    return d >= dEnv1A && d <= dEnv3R && d != dEnv1S && d != dEnv2S && d != dEnv3S;
}

// Snap to a "nice number" log grid: mantissa steps of 0.1 below 2, 0.2 below 5,
// 0.5 up to 10, scaled per decade (... 90, 95, 100, 110 ... 200, 220 ... 500,
// 550 ... 1000, 1100 ...) — roughly 5-10% apart. minStep is an absolute floor
// so small values don't get absurdly fine steps.
inline float snapNice (float v, float minStep)
{
    const float av = std::abs (v);
    if (av < 1.0e-9f)
        return 0.0f;
    const float dec = std::pow (10.0f, std::floor (std::log10 (av)));
    const float m = av / dec;
    const float step = juce::jmax ((m < 2.0f ? 0.1f : m < 5.0f ? 0.2f : 0.5f) * dec, minStep);
    return std::copysign (std::round (av / step) * step, v);
}

// The step grid a *knob gesture* lands on. Only the UI snaps: host automation
// and per-voice modulation stay continuous.
inline float snapKnobValue (int dest, float v)
{
    if (isEnvTime (dest) || dest == dGrainSize)
        return snapNice (v, 0.5f);                                     // ms
    if (dest == dCutoff || dest == dLfo1Rate || dest == dLfo2Rate)
        return snapNice (v, 0.0f);                                     // Hz
    if (dest == dGain)
        return std::round (v * 2.0f) * 0.5f;                           // 0.5 dB
    if (dest == dDensity)
        return std::round (v * 10.0f) * 0.1f;
    if (dest == dCoarse || dest == dFine || dest == dPitchRand)
        return v;                                  // range interval already steps these
    return std::round (v * 100.0f) * 0.01f;        // 0..1 params: 1% steps
}

// Value display: ~3 significant figures, trailing zeros trimmed, so knobs read
// "2.75 ms" / "45.3 ms" / "250 ms" rather than "2.7543 ms".
inline juce::String formatParamValue (float v, int)
{
    if (v == 0.0f)
        return "0";
    const int mag = (int) std::floor (std::log10 (std::abs (v)));
    auto s = juce::String (v, juce::jlimit (0, 3, 2 - mag));
    return s.containsChar ('.') ? s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".") : s;
}

// Envelope-time taper, calibrated to the knob's clock positions (it sweeps
// 288 degrees, 7 -> 5 o'clock): fully CCW = floorMs (0 = "off"; releases pass
// 5 ms so they stay click-free), 9 o'clock = 250 ms, 12 o'clock = 1 s,
// 3 o'clock = 4 s, fully CW = 10 s. Log-linear between anchors; the first
// segment starts at max(floorMs, 1 ms) so short times keep usable travel.
inline juce::NormalisableRange<float> envTimeRange (float floorMs = 0.0f)
{
    const std::array<float, 5> P { 0.0f, 0.1875f, 0.5f, 0.8125f, 1.0f };                     // knob travel
    const std::array<float, 5> V { juce::jmax (1.0f, floorMs), 250.0f, 1000.0f, 4000.0f, 10000.0f }; // ms

    auto from01 = [=] (float, float, float p)
    {
        if (p <= 0.0f)
            return floorMs;
        p = juce::jmin (p, 1.0f);
        int i = 1;
        while (i < 4 && p > P[i]) ++i;
        const float t = (p - P[i - 1]) / (P[i] - P[i - 1]);
        return V[i - 1] * std::pow (V[i] / V[i - 1], t);
    };
    auto to01 = [=] (float, float, float v)
    {
        if (v < V[0])
            return 0.0f;   // below the first anchor collapses onto the bottom stop
        v = juce::jmin (v, V[4]);
        int i = 1;
        while (i < 4 && v > V[i]) ++i;
        const float t = std::log (v / V[i - 1]) / std::log (V[i] / V[i - 1]);
        return P[i - 1] + t * (P[i] - P[i - 1]);
    };
    return { floorMs, 10000.0f, from01, to01,
             [floorMs] (float, float, float v) { return juce::jlimit (floorMs, 10000.0f, v); } };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto range = [] (float lo, float hi, float centre = 0.0f, float step = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, step);
        if (centre > 0.0f)
            r.setSkewForCentre (centre);
        return r;
    };
    auto add = [&] (int dest, juce::NormalisableRange<float> r, float def, const juce::String& suffix = {})
    {
        const auto& info = destInfos()[(size_t) dest];
        layout.add (std::make_unique<P> (juce::ParameterID { info.id, 1 }, destDisplayName (dest), r, def,
                                         juce::AudioParameterFloatAttributes()
                                             .withLabel (suffix)
                                             .withStringFromValueFunction (formatParamValue)));
    };

    add (dPosition,  range (0.0f, 1.0f),            0.15f);
    add (dGrainSize, range (10.0f, 500.0f, 80.0f),  90.0f,  "ms");
    add (dDensity,   range (1.0f, 8.0f),            3.0f);
    add (dSpray,     range (0.0f, 1.0f),            0.03f);
    add (dPitchRand, range (0.0f, 12.0f, 0.0f, 0.1f),     0.0f, "st");
    add (dShape,     range (0.0f, 1.0f),                  0.5f);
    add (dCoarse,    range (-24.0f, 24.0f, 0.0f, 1.0f),   0.0f, "st");
    add (dFine,      range (-100.0f, 100.0f, 0.0f, 1.0f), 0.0f, "ct");
    add (dSpread,    range (0.0f, 1.0f),            0.35f);
    add (dCutoff,    range (20.0f, 20000.0f, 1200.0f), 14000.0f, "Hz");
    add (dRes,       range (0.0f, 1.0f),            0.15f);

    add (dEnv1A, envTimeRange(),     4.0f,   "ms");
    add (dEnv1D, envTimeRange(),     200.0f, "ms");
    add (dEnv1S, range (0.0f, 1.0f), 0.8f);
    add (dEnv1R, envTimeRange (5.0f), 250.0f, "ms");

    add (dEnv2A, envTimeRange(),     120.0f, "ms");
    add (dEnv2D, envTimeRange(),     300.0f, "ms");
    add (dEnv2S, range (0.0f, 1.0f), 0.5f);
    add (dEnv2R, envTimeRange (5.0f), 300.0f, "ms");

    add (dEnv3A, envTimeRange(),     400.0f, "ms");
    add (dEnv3D, envTimeRange(),     600.0f, "ms");
    add (dEnv3S, range (0.0f, 1.0f), 0.6f);
    add (dEnv3R, envTimeRange (5.0f), 800.0f, "ms");

    // Linear on purpose: 0.1 Hz is already meaninglessly slow and 5 Hz
    // meaninglessly fast, so the whole sweep is usable without a taper.
    add (dLfo1Rate, range (0.1f, 5.0f), 2.0f,   "Hz");
    add (dLfo2Rate, range (0.1f, 5.0f), 0.35f,  "Hz");

    add (dGain, range (-60.0f, 6.0f), -8.0f, "dB");

    using C = juce::AudioParameterChoice;
    layout.add (std::make_unique<C> (juce::ParameterID { "table", 1 },      "Table",       tableNames(),      0));
    layout.add (std::make_unique<C> (juce::ParameterID { "filterType", 1 }, "Filter Type", filterTypeNames(), 0));
    layout.add (std::make_unique<C> (juce::ParameterID { "lfo1Shape", 1 },  "LFO1 Shape",  lfoShapeNames(),   0));
    layout.add (std::make_unique<C> (juce::ParameterID { "lfo2Shape", 1 },  "LFO2 Shape",  lfoShapeNames(),   0));
    layout.add (std::make_unique<C> (juce::ParameterID { "lfo1Mode", 1 },   "LFO1 Mode",   lfoModeNames(),    0));
    layout.add (std::make_unique<C> (juce::ParameterID { "lfo2Mode", 1 },   "LFO2 Mode",   lfoModeNames(),    0));

    return layout;
}

} // namespace vape
