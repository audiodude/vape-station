#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"

namespace vape
{

// A rotary knob that is also a modulation drop target. Routes targeting its
// parameter are drawn as coloured arcs ("halos") from the base value across
// the modulation depth; a white dot tracks the live modulated value.
class ModKnob : public juce::Component,
                public juce::DragAndDropTarget
{
public:
    ModKnob (VapeProcessor& p, int destIn);

    void resized() override;
    void paintOverChildren (juce::Graphics& g) override;

    void setDisplayNorm (float n);
    void refreshRoutes();

    bool isInterestedInDragSource (const SourceDetails& d) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails& d) override;

private:
    void showRouteMenu();

    struct KSlider : juce::Slider
    {
        explicit KSlider (ModKnob& o) : owner (o) {}
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { owner.showRouteMenu(); return; }
            juce::Slider::mouseDown (e);
        }
        // JUCE calls this for user gestures only (drag/wheel), so host
        // automation and modulation stay continuous while the knob steps.
        double snapValue (double attempted, DragMode) override
        {
            return juce::jlimit (getMinimum(), getMaximum(),
                                 (double) snapKnobValue (owner.dest, (float) attempted));
        }
        ModKnob& owner;
    };

    VapeProcessor& proc;
    int dest;
    KSlider slider { *this };
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::vector<ModRoute> routes;
    float dispNorm = -1.0f;
    bool dragOver = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModKnob)
};

} // namespace vape
