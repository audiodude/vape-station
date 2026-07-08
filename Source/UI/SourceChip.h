#pragma once

#include "Theme.h"

namespace vape
{

// Draggable modulation-source pill. Drop it on any knob to create a route.
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
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        const auto c = theme::srcColour (src);

        g.setColour (c.withAlpha (isMouseOverOrDragging() ? 0.30f : 0.14f));
        g.fillRoundedRectangle (b, b.getHeight() / 2.0f);
        g.setColour (c.withAlpha (0.85f));
        g.drawRoundedRectangle (b, b.getHeight() / 2.0f, 1.0f);

        g.fillEllipse (b.getX() + 7.0f, b.getCentreY() - 2.5f, 5.0f, 5.0f);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (srcName (src), b.withTrimmedLeft (16.0f).withTrimmedRight (4.0f),
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
