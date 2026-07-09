#include "TableViz.h"
#include "../GrainTables.h"

namespace vape
{

void TableViz::paint (juce::Graphics& g)
{
    theme::drawWell (g, getLocalBounds().toFloat());

    const auto& tables = grainTables();
    const auto& tbl = tables[(size_t) juce::jlimit (0, (int) tables.size() - 1, table)];

    const float fpos = pos * (float) (GrainTable::numFrames - 1);
    const int f0 = (int) fpos;
    const int f1 = juce::jmin (f0 + 1, GrainTable::numFrames - 1);
    const float ff = fpos - (float) f0;
    const float* a = tbl.frame (0, f0);
    const float* c = tbl.frame (0, f1);

    auto area = getLocalBounds().reduced (10).toFloat();

    // Sample the interpolated frame across the width and normalise for display.
    const int steps = juce::jmax (16, (int) area.getWidth() / 2);
    std::vector<float> vals ((size_t) steps + 1);
    float maxAbs = 0.05f;
    for (int i = 0; i <= steps; ++i)
    {
        const int idx = (int) ((float) i / (float) steps * (float) (GrainTable::frameLen - 1));
        const float v = a[idx] + ff * (c[idx] - a[idx]);
        vals[(size_t) i] = v;
        maxAbs = juce::jmax (maxAbs, std::abs (v));
    }

    juce::Path wave;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = area.getX() + (float) i / (float) steps * area.getWidth();
        const float y = area.getCentreY() - vals[(size_t) i] / maxAbs * area.getHeight() * 0.44f;
        if (i == 0) wave.startNewSubPath (x, y);
        else        wave.lineTo (x, y);
    }
    g.setColour (theme::accent);
    g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

} // namespace vape
