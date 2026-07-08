#include "TableViz.h"
#include "../GrainTables.h"

namespace vape
{

void TableViz::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (theme::inset);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (theme::panelLine);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    g.setColour (theme::dim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("GRAINTABLE", 12, 6, 140, 14, juce::Justification::centredLeft);

    const auto& tables = grainTables();
    const auto& tbl = tables[(size_t) juce::jlimit (0, (int) tables.size() - 1, table)];

    const float fpos = pos * (float) (GrainTable::numFrames - 1);
    const int f0 = (int) fpos;
    const int f1 = juce::jmin (f0 + 1, GrainTable::numFrames - 1);
    const float ff = fpos - (float) f0;
    const float* a = tbl.frame (0, f0);
    const float* c = tbl.frame (0, f1);

    auto area = getLocalBounds().reduced (10).withTrimmedTop (16).withTrimmedBottom (14).toFloat();

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
        const float y = area.getCentreY() - vals[(size_t) i] / maxAbs * area.getHeight() * 0.46f;
        if (i == 0) wave.startNewSubPath (x, y);
        else        wave.lineTo (x, y);
    }
    g.setColour (theme::accent.withAlpha (0.9f));
    g.strokePath (wave, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

    // Position strip along the bottom.
    const float sy = b.getBottom() - 8.0f;
    g.setColour (theme::track);
    g.drawLine (area.getX(), sy, area.getRight(), sy, 2.0f);
    g.setColour (theme::accent);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f)
                       .withCentre ({ area.getX() + pos * area.getWidth(), sy }));
}

} // namespace vape
