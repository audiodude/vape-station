#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace vape
{

// A graintable: a stack of single-cycle frames the grain engine scans through.
// Frames are stored at several band-limited mip levels; a grain picks the mip
// whose harmonic cap keeps it below Nyquist for its playback frequency.
struct GrainTable
{
    static constexpr int numFrames = 64;
    static constexpr int frameLen  = 2048;
    static constexpr int numMips   = 4;
    static constexpr int mipCaps[numMips] = { 64, 16, 4, 1 };

    juce::String name;
    std::array<std::vector<float>, numMips> mips; // numFrames * (frameLen + 1), +1 wrap guard

    const float* frame (int mip, int f) const noexcept
    {
        return mips[(size_t) mip].data() + (size_t) f * (frameLen + 1);
    }

    int mipForFreq (double freqHz, double sampleRate) const noexcept
    {
        for (int m = 0; m < numMips; ++m)
            if (freqHz * mipCaps[m] < 0.45 * sampleRate)
                return m;
        return numMips - 1;
    }
};

// Built once on first use (a few hundred ms), shared by all instances.
const std::vector<GrainTable>& grainTables();

} // namespace vape
