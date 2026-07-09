#pragma once

#include "Theme.h"

namespace vape
{

// Flat Rainfall-styled keyboard: near-white keys with a bottom shade,
// panel-coloured black keys, octave labels on each C.
class VapeKeyboard : public juce::MidiKeyboardComponent
{
public:
    explicit VapeKeyboard (juce::MidiKeyboardState& state)
        : juce::MidiKeyboardComponent (state, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        setScrollButtonsVisible (false);
        setBlackNoteLengthProportion (58.0f / 96.0f);
        setBlackNoteWidthProportion (0.63f);
        setAvailableRange (36, 95); // C2..B6 — five octaves
        setOctaveForMiddleC (4);    // "C4" label style
    }

    void drawWhiteNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour, juce::Colour) override
    {
        auto r = area.reduced (0.5f);
        juce::Path p;
        p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                               3.0f, 3.0f, false, false, true, true);
        g.setColour (juce::Colour (0xfff9fafb));
        g.fillPath (p);
        // inset 0 -4px 0 rgba(0,0,0,.12)
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillRect (r.getX(), r.getBottom() - 4.0f, r.getWidth(), 4.0f);

        if (isDown)
        {
            g.setColour (theme::accent.withAlpha (0.45f));
            g.fillPath (p);
        }
        else if (isOver)
        {
            g.setColour (theme::accent.withAlpha (0.15f));
            g.fillPath (p);
        }

        g.setColour (theme::panel);
        g.strokePath (p, juce::PathStrokeType (1.0f));

        if (midiNoteNumber % 12 == 0) // C: octave label bottom-left
        {
            g.setColour (theme::faint);
            g.setFont (theme::font (10.0f));
            g.drawText ("C" + juce::String (midiNoteNumber / 12 - 1),
                        (int) r.getX() + 4, (int) r.getBottom() - 18, 30, 12,
                        juce::Justification::centredLeft);
        }
    }

    void drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour) override
    {
        auto r = area;
        juce::Path p;
        p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                               2.0f, 2.0f, false, false, true, true);
        g.setColour (theme::panel);
        g.fillPath (p);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (r.getX(), r.getBottom() - 2.0f, r.getWidth(), 2.0f);

        if (isDown)
        {
            g.setColour (theme::accent.withAlpha (0.55f));
            g.fillPath (p);
        }
        else if (isOver)
        {
            g.setColour (theme::accent.withAlpha (0.2f));
            g.fillPath (p);
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VapeKeyboard)
};

} // namespace vape
