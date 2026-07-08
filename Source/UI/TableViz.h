#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"

namespace vape
{

// Draws the graintable frame at the current (possibly modulated) scan
// position. Dragging horizontally sets the Position parameter directly.
class TableViz : public juce::Component
{
public:
    explicit TableViz (VapeProcessor& p) : proc (p) {}

    // Called from the editor's UI timer.
    void refresh (float posNorm, int tableIdx)
    {
        if (std::abs (posNorm - pos) > 0.002f || tableIdx != table)
        {
            pos = posNorm;
            table = tableIdx;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override;

    void mouseDown (const juce::MouseEvent& e) override
    {
        proc.param (dPosition)->beginChangeGesture();
        setFromMouse (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { setFromMouse (e); }
    void mouseUp (const juce::MouseEvent&) override
    {
        proc.param (dPosition)->endChangeGesture();
    }

private:
    void setFromMouse (const juce::MouseEvent& e)
    {
        const float norm = juce::jlimit (0.0f, 1.0f, (float) e.x / (float) juce::jmax (1, getWidth()));
        proc.param (dPosition)->setValueNotifyingHost (norm); // position range is 0..1 linear
    }

    VapeProcessor& proc;
    float pos = 0.0f;
    int table = 0;
};

} // namespace vape
