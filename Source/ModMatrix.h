#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "Params.h"
#include <array>
#include <memory>
#include <vector>

namespace vape
{

// Modulation sources. Envelopes/velocity/wheel are unipolar 0..1,
// LFOs and keytrack are bipolar -1..1.
enum Src : int
{
    sEnv1, sEnv2, sEnv3, sLfo1, sLfo2, sVelocity, sWheel, sKey,
    numSrcs
};

inline const char* srcName (int s)
{
    static const char* names[numSrcs] = { "ENV1", "ENV2", "ENV3", "LFO1", "LFO2", "VEL", "WHEEL", "KEY" };
    return (s >= 0 && s < numSrcs) ? names[s] : "?";
}

inline int srcFromName (const juce::String& n)
{
    for (int s = 0; s < numSrcs; ++s)
        if (n == srcName (s))
            return s;
    return -1;
}

inline const juce::Identifier idModMatrix { "MODMATRIX" };
inline const juce::Identifier idMod       { "MOD" };
inline const juce::Identifier idSrc       { "src" };
inline const juce::Identifier idDst       { "dst" };
inline const juce::Identifier idDepth     { "depth" };

struct ModRoute
{
    int src = 0;
    float depth = 0.0f;
};

// Immutable snapshot of the matrix, swapped in atomically for the audio thread.
struct CompiledMatrix
{
    std::array<std::vector<ModRoute>, numDests> routes;
    std::array<bool, numDests> any {};
    int totalRoutes = 0;
};

inline std::unique_ptr<CompiledMatrix> compileMatrix (const juce::ValueTree& matrixNode)
{
    auto m = std::make_unique<CompiledMatrix>();
    if (! matrixNode.isValid())
        return m;

    for (int i = 0; i < matrixNode.getNumChildren(); ++i)
    {
        auto row = matrixNode.getChild (i);
        if (! row.hasType (idMod))
            continue;

        const int src = srcFromName (row[idSrc].toString());
        const int dst = destFromId (row[idDst].toString());
        const float depth = (float) (double) row[idDepth];

        if (src >= 0 && dst >= 0 && dst < numDests)
        {
            m->routes[(size_t) dst].push_back ({ src, juce::jlimit (-1.0f, 1.0f, depth) });
            m->any[(size_t) dst] = true;
            ++m->totalRoutes;
        }
    }
    return m;
}

} // namespace vape
