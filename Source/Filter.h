#pragma once

#include <algorithm>
#include <cmath>

namespace vape
{

// Filter types. Append-only: `filterType` is stored as an index, both in the
// plugin state and in the webapp's localStorage.
enum FilterType
{
    ftLowPass = 0,
    ftBandPass,
    ftHighPass,
    ftNotch,
    ftPeak,
    ftLowPass24,
    ftHighPass24,
    ftLadder,
    numFilterTypes
};

// One filter per voice. webapp/js/engine.js carries a line-for-line copy of
// this — keep the two in step.
//
// Types 0-6 run a TPT state-variable core (Zavalishin). Every output falls out
// of the same pair of integrator states, so notch and peak cost nothing beyond
// a different combination, and the 24 dB/oct types just run the core twice.
//
// Type 7 is a 4-pole zero-delay Moog ladder. The soft clipper sits inside the
// feedback path, which is what lets it self-oscillate and stay bounded rather
// than merely ring: a linear filter can only decay.
struct VapeFilter
{
    static constexpr int numChannels = 2;
    static constexpr int numStages   = 2; // SVF cascade depth for the 24 dB types

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        for (int c = 0; c < numChannels; ++c)
        {
            for (int s = 0; s < numStages; ++s)
                ic1[c][s] = ic2[c][s] = 0.0f;
            for (int p = 0; p < 4; ++p)
                ladderZ[c][p] = 0.0f;
        }
    }

    void set (float cutoffHz, float res01, int typeIn) noexcept
    {
        type = typeIn;
        const float fc = std::min ((float) (0.49 * sr), std::max (20.0f, cutoffHz));
        g = std::tan (3.14159265358979f * fc / (float) sr);

        if (type == ftLadder)
        {
            G = g / (1.0f + g);
            const float g2 = G * G;
            G4 = g2 * g2;
            k = 4.5f * res01; // 4 is the threshold; the extra pushes it over
        }
        else
        {
            // Unchanged from the original single-mode filter, so existing
            // sessions keep the resonance response they were dialled in with.
            const float q = 0.707f * std::pow (10.0f, res01 * 1.15f);
            k = 1.0f / std::max (0.05f, q);
        }
    }

    float process (int ch, float x) noexcept
    {
        if (type == ftLadder)
            return processLadder (ch, x);

        const float y = processSvf (ch, 0, x);
        if (type == ftLowPass24 || type == ftHighPass24)
            return processSvf (ch, 1, y);
        return y;
    }

private:
    float processSvf (int ch, int stage, float x) noexcept
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float v3 = x - ic2[ch][stage];
        const float v1 = a1 * ic1[ch][stage] + a2 * v3;
        const float v2 = ic2[ch][stage] + a2 * ic1[ch][stage] + g * a2 * v3;
        ic1[ch][stage] = 2.0f * v1 - ic1[ch][stage];
        ic2[ch][stage] = 2.0f * v2 - ic2[ch][stage];

        switch (type)
        {
            case ftBandPass:    return v1;
            case ftHighPass:
            case ftHighPass24:  return x - k * v1 - v2;
            case ftNotch:       return x - k * v1;              // LP + HP
            case ftPeak:        return 2.0f * v2 - x + k * v1;  // LP - HP
            default:            return v2;                      // LP, LP24
        }
    }

    float processLadder (int ch, float x) noexcept
    {
        // Each one-pole answers y = G*in + (1-G)*z, so the 4-pole output is
        // G^4*u + S with S the weighted sum of the four states. Solving
        // u = x - k*y4 for u resolves the feedback without a unit delay.
        float* z = ladderZ[ch];
        const float m = 1.0f - G;
        const float S = (((m * z[0]) * G + m * z[1]) * G + m * z[2]) * G + m * z[3];

        // Saturate the feedback, not the forward path. Clipping the forward
        // path costs passband level and chokes the loop gain the resonance
        // needs; clipping only the feedback keeps the passband clean and still
        // bounds the oscillation that k > 4 would otherwise grow without limit.
        const float sn = S / kHeadroom;
        const float sat = sn / (1.0f + std::abs (sn));

        // A ladder's DC gain is 1/(1+k), so it thins out as resonance rises.
        // Scaling the input claws most of that back without flattening the
        // character entirely, and drives the saturator harder as a bonus.
        float u = (x * (1.0f + 0.5f * k) - k * kHeadroom * sat) / (1.0f + k * G4);

        for (int p = 0; p < 4; ++p)
        {
            const float d = (u - z[p]) * G;
            const float y = d + z[p];
            z[p] = y + d;
            u = y;
        }
        return u;
    }

    static constexpr float kHeadroom = 0.5f; // where the feedback clipper bites

    double sr = 48000.0;
    int type = ftLowPass;
    float g = 0.1f, k = 1.0f / 0.707f, G = 0.0f, G4 = 0.0f;
    float ic1[numChannels][numStages] {};
    float ic2[numChannels][numStages] {};
    float ladderZ[numChannels][4] {};
};

} // namespace vape
