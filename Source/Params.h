#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

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

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto range = [] (float lo, float hi, float centre = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi);
        if (centre > 0.0f)
            r.setSkewForCentre (centre);
        return r;
    };
    auto add = [&] (int dest, juce::NormalisableRange<float> r, float def, const juce::String& suffix = {})
    {
        const auto& info = destInfos()[(size_t) dest];
        layout.add (std::make_unique<P> (juce::ParameterID { info.id, 1 }, destDisplayName (dest), r, def,
                                         juce::AudioParameterFloatAttributes().withLabel (suffix)));
    };

    add (dPosition,  range (0.0f, 1.0f),            0.15f);
    add (dGrainSize, range (10.0f, 500.0f, 80.0f),  90.0f,  "ms");
    add (dDensity,   range (1.0f, 8.0f),            3.0f);
    add (dSpray,     range (0.0f, 1.0f),            0.03f);
    add (dPitchRand, range (0.0f, 12.0f),           0.0f,   "st");
    add (dShape,     range (0.0f, 1.0f),            0.5f);
    add (dCoarse,    range (-24.0f, 24.0f),         0.0f,   "st");
    add (dFine,      range (-100.0f, 100.0f),       0.0f,   "ct");
    add (dSpread,    range (0.0f, 1.0f),            0.35f);
    add (dCutoff,    range (20.0f, 20000.0f, 1200.0f), 14000.0f, "Hz");
    add (dRes,       range (0.0f, 1.0f),            0.15f);

    add (dEnv1A, range (1.0f, 5000.0f, 150.0f), 4.0f,   "ms");
    add (dEnv1D, range (1.0f, 5000.0f, 300.0f), 200.0f, "ms");
    add (dEnv1S, range (0.0f, 1.0f),            0.8f);
    add (dEnv1R, range (5.0f, 8000.0f, 400.0f), 250.0f, "ms");

    add (dEnv2A, range (1.0f, 5000.0f, 150.0f), 120.0f, "ms");
    add (dEnv2D, range (1.0f, 5000.0f, 300.0f), 300.0f, "ms");
    add (dEnv2S, range (0.0f, 1.0f),            0.5f);
    add (dEnv2R, range (5.0f, 8000.0f, 400.0f), 300.0f, "ms");

    add (dEnv3A, range (1.0f, 5000.0f, 150.0f), 400.0f, "ms");
    add (dEnv3D, range (1.0f, 5000.0f, 300.0f), 600.0f, "ms");
    add (dEnv3S, range (0.0f, 1.0f),            0.6f);
    add (dEnv3R, range (5.0f, 8000.0f, 400.0f), 800.0f, "ms");

    add (dLfo1Rate, range (0.02f, 20.0f, 2.0f), 2.0f,   "Hz");
    add (dLfo2Rate, range (0.02f, 20.0f, 2.0f), 0.35f,  "Hz");

    add (dGain, range (-60.0f, 6.0f), -8.0f, "dB");

    using C = juce::AudioParameterChoice;
    layout.add (std::make_unique<C> (juce::ParameterID { "table", 1 },      "Table",       tableNames(),      0));
    layout.add (std::make_unique<C> (juce::ParameterID { "filterType", 1 }, "Filter Type", filterTypeNames(), 0));
    layout.add (std::make_unique<C> (juce::ParameterID { "lfo1Shape", 1 },  "LFO1 Shape",  lfoShapeNames(),   0));
    layout.add (std::make_unique<C> (juce::ParameterID { "lfo2Shape", 1 },  "LFO2 Shape",  lfoShapeNames(),   0));

    return layout;
}

} // namespace vape
