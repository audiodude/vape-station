#pragma once

#include "Theme.h"
#include "SourceChip.h"
#include "../PluginProcessor.h"

namespace vape
{

// Lists every modulation route with an editable depth slider and remove
// button. Also hosts the performance-source chips (VEL / WHEEL / KEY).
class MatrixPanel : public juce::Component,
                    private juce::ValueTree::Listener
{
public:
    explicit MatrixPanel (VapeProcessor& p);
    ~MatrixPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct Row : juce::Component
    {
        Row (VapeProcessor& p, juce::ValueTree nodeIn);
        void paint (juce::Graphics& g) override;
        void resized() override;

        juce::ValueTree node;
        int src = 0;
        juce::String labelText;
        juce::Slider depth;
        juce::TextButton kill { "x" };
    };

    void rebuild();
    void layoutRows();

    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override
    {
        if (parent.hasType (idModMatrix)) rebuild();
    }
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override
    {
        if (parent.hasType (idModMatrix)) rebuild();
    }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    VapeProcessor& proc;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<Row> rows;
    SourceChip velChip { sVelocity }, wheelChip { sWheel }, keyChip { sKey };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatrixPanel)
};

} // namespace vape
