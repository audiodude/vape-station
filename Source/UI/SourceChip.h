#pragma once

#include "Theme.h"

namespace vape
{

// Draggable modulation-source pill. Drop it on any knob to create a route.
// ENV/LFO chips show their source colour; performance sources (VEL / WHEEL /
// KEY) render in the muted gray variant per the design.
class SourceChip : public juce::Component
{
public:
    explicit SourceChip (int srcIn) : src (srcIn)
    {
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const bool muted = src >= sVelocity;
        const auto c = muted ? theme::dim : theme::srcColour (src);

        g.setColour (isMouseOverOrDragging() ? theme::input.brighter (0.15f) : theme::input);
        g.fillRoundedRectangle (b, b.getHeight() / 2.0f);

        // 6px dot + 10px/600 label, centred as a unit
        const auto f = theme::fontMedium (10.0f).withExtraKerningFactor (0.06f);
        const auto name = juce::String (srcName (src));
        const float textW = theme::textWidth (f, name);
        const float unitW = 6.0f + 5.0f + textW;
        float x = b.getCentreX() - unitW / 2.0f;

        g.setColour (muted ? theme::srcColour (src) : c);
        if (! muted)
            g.fillEllipse (x, b.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (c);
        g.setFont (f);
        g.drawText (name, juce::Rectangle<float> (muted ? b.getCentreX() - textW / 2.0f : x + 11.0f,
                                                  b.getY(), textW + 2.0f, b.getHeight()),
                    juce::Justification::centredLeft);
    }

    void mouseDrag (const juce::MouseEvent&) override
    {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! container->isDragAndDropActive())
                container->startDragging (juce::String ("modsrc:") + srcName (src), this);
    }

private:
    int src;
};

} // namespace vape
