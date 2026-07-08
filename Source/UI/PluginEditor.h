#pragma once

#include "Theme.h"
#include "SourceChip.h"
#include "ModKnob.h"
#include "MatrixPanel.h"
#include "TableViz.h"
#include "../PluginProcessor.h"

namespace vape
{

class VapeEditor : public juce::AudioProcessorEditor,
                   public juce::DragAndDropContainer,
                   private juce::Timer
{
public:
    explicit VapeEditor (VapeProcessor&);
    ~VapeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    ModKnob* addKnob (int dest);

    VapeProcessor& proc;
    VapeLnF lnf;

    juce::OwnedArray<ModKnob> knobs;
    std::array<ModKnob*, numDests> knobFor {};

    juce::ComboBox tableBox, filterBox, lfo1ShapeBox, lfo2ShapeBox;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ComboAtt> tableAtt, filterAtt, lfo1Att, lfo2Att;

    SourceChip env1Chip { sEnv1 }, env2Chip { sEnv2 }, env3Chip { sEnv3 },
               lfo1Chip { sLfo1 }, lfo2Chip { sLfo2 };

    TableViz viz;
    MatrixPanel matrix;
    juce::MidiKeyboardComponent keyboard;

    struct Panel { juce::String title; juce::Rectangle<int> r; };
    std::vector<Panel> panels;

    juce::uint32 lastStamp = 0;
    juce::uint32 lastFreshMs = 0;
    int lastMatVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VapeEditor)
};

} // namespace vape
