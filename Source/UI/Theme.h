#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../ModMatrix.h"

namespace vape
{

namespace theme
{
    inline const juce::Colour bg        { 0xff16141c };
    inline const juce::Colour panel     { 0xff1e1b26 };
    inline const juce::Colour panelLine { 0xff2a2635 };
    inline const juce::Colour inset     { 0xff121017 };
    inline const juce::Colour track     { 0xff332f40 };
    inline const juce::Colour knobBody  { 0xff262230 };
    inline const juce::Colour text      { 0xffd6d2e0 };
    inline const juce::Colour dim       { 0xff8a8496 };
    inline const juce::Colour accent    { 0xff5fe6d0 };

    inline juce::Colour srcColour (int s)
    {
        static const juce::uint32 c[numSrcs] = {
            0xffffc94d, // ENV1 amber
            0xffff8a3d, // ENV2 orange
            0xffff5470, // ENV3 pink-red
            0xff4dd8ff, // LFO1 cyan
            0xff7de05a, // LFO2 green
            0xffc77dff, // VEL violet
            0xff8f9bff, // WHEEL periwinkle
            0xffff9ae0, // KEY pink
        };
        return juce::Colour (s >= 0 && s < numSrcs ? c[s] : 0xffffffff);
    }

    struct KnobGeo { juce::Point<float> centre; float radius; };

    inline KnobGeo knobGeo (juce::Rectangle<float> r)
    {
        r = r.reduced (7.0f);
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (size, size);
        return { r.getCentre(), size / 2.0f - 2.0f };
    }
} // namespace theme

class VapeLnF : public juce::LookAndFeel_V4
{
public:
    VapeLnF()
    {
        setColour (juce::Slider::rotarySliderFillColourId, theme::accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, theme::track);
        setColour (juce::Slider::thumbColourId, theme::text);
        setColour (juce::Slider::trackColourId, theme::track);
        setColour (juce::Slider::backgroundColourId, theme::inset);
        setColour (juce::Slider::textBoxTextColourId, theme::dim);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, theme::dim);
        setColour (juce::ComboBox::backgroundColourId, theme::inset);
        setColour (juce::ComboBox::textColourId, theme::text);
        setColour (juce::ComboBox::outlineColourId, theme::panelLine);
        setColour (juce::ComboBox::arrowColourId, theme::dim);
        setColour (juce::PopupMenu::backgroundColourId, theme::panel);
        setColour (juce::PopupMenu::textColourId, theme::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::track);
        setColour (juce::PopupMenu::highlightedTextColourId, theme::text);
        setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        setColour (juce::TextButton::textColourOffId, theme::dim);
        setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd8d4e2));
        setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff211e2b));
        setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff474252));
        setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, theme::accent.withAlpha (0.7f));
        setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, theme::accent.withAlpha (0.25f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& s) override
    {
        const auto geo = theme::knobGeo (juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height));
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float lw = juce::jmax (2.0f, geo.radius * 0.16f);

        juce::Path trackPath;
        trackPath.addCentredArc (geo.centre.x, geo.centre.y, geo.radius, geo.radius, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (s.findColour (juce::Slider::rotarySliderOutlineColourId));
        g.strokePath (trackPath, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valPath;
        valPath.addCentredArc (geo.centre.x, geo.centre.y, geo.radius, geo.radius, 0.0f,
                               rotaryStartAngle, angle, true);
        g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (valPath, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (theme::knobBody);
        g.fillEllipse (juce::Rectangle<float> (geo.radius * 1.24f, geo.radius * 1.24f).withCentre (geo.centre));

        g.setColour (s.findColour (juce::Slider::thumbColourId));
        const auto p1 = geo.centre.getPointOnCircumference (geo.radius * 0.24f, angle);
        const auto p2 = geo.centre.getPointOnCircumference (geo.radius * 0.58f, angle);
        g.drawLine ({ p1, p2 }, 2.0f);
    }
};

} // namespace vape
