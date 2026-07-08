#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace vape
{

enum LfoShape : int { lfoSine, lfoTriangle, lfoSawUp, lfoSawDown, lfoSquare, lfoSH };

// Simple per-voice LFO, retriggered on note-on. Bipolar -1..1 output.
struct Lfo
{
    double phase = 0.0;
    juce::Random rng;
    float held = 0.0f;

    void reset (juce::int64 seed)
    {
        phase = 0.0;
        rng.setSeed (seed);
        held = rng.nextFloat() * 2.0f - 1.0f;
    }

    float advance (double rateHz, double dt, int shape)
    {
        phase += rateHz * dt;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = rng.nextFloat() * 2.0f - 1.0f;
        }
        return value (shape);
    }

    float value (int shape) const
    {
        const float p = (float) phase;
        switch (shape)
        {
            case lfoSine:     return std::sin (juce::MathConstants<float>::twoPi * p);
            case lfoTriangle: return 1.0f - 4.0f * std::abs (p - 0.5f);
            case lfoSawUp:    return 2.0f * p - 1.0f;
            case lfoSawDown:  return 1.0f - 2.0f * p;
            case lfoSquare:   return p < 0.5f ? 1.0f : -1.0f;
            case lfoSH:       return held;
            default:          return 0.0f;
        }
    }
};

} // namespace vape
