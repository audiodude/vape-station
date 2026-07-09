#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "BinaryData.h"
#include "../ModMatrix.h"

namespace vape
{

// Rainfall design system tokens (see docs/design handoff).
namespace theme
{
    inline const juce::Colour canvas    { 0xff475569 }; // app background (slate-600)
    inline const juce::Colour panel     { 0xff1f2937 }; // panel surface (gray-800)
    inline const juce::Colour panelLine { 0xff374151 }; // panel border / track ring
    inline const juce::Colour well      { 0xff111827 }; // recessed wells / knob recess
    inline const juce::Colour input     { 0xff374151 }; // input & chip fill
    inline const juce::Colour inputLine { 0xff4b5563 }; // input border
    inline const juce::Colour text      { 0xffd1d5db }; // body
    inline const juce::Colour dim       { 0xff9ca3af }; // muted
    inline const juce::Colour faint     { 0xff6b7280 }; // faint
    inline const juce::Colour strong    { 0xffffffff }; // strong
    inline const juce::Colour accent    { 0xff4aa5ff }; // brand cloud blue
    inline const juce::Colour destroy   { 0xfff87171 }; // destructive (remove icons)

    // Legacy aliases still used across the UI sources.
    inline const juce::Colour bg    = canvas;
    inline const juce::Colour inset = well;
    inline const juce::Colour track = panelLine;

    inline juce::Colour srcColour (int s)
    {
        static const juce::uint32 c[numSrcs] = {
            0xff60a5fa, // ENV1 blue-400
            0xff34d399, // ENV2 emerald
            0xfff87171, // ENV3 red-400
            0xffc4b5fd, // LFO1 purple-300
            0xffe6f3ff, // LFO2 ice / cloud-light
            0xfffbbf24, // VEL amber (chips render gray; this colours routes/arcs)
            0xff2dd4bf, // WHEEL teal
            0xfff472b6, // KEY pink
        };
        return juce::Colour (s >= 0 && s < numSrcs ? c[s] : 0xffffffff);
    }

    // --- Roboto (embedded) -------------------------------------------------
    inline const juce::Typeface::Ptr& robotoRegular()
    {
        static juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor (
            BinaryData::RobotoRegular_ttf, (size_t) BinaryData::RobotoRegular_ttfSize);
        return t;
    }
    inline const juce::Typeface::Ptr& robotoMedium()
    {
        static juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor (
            BinaryData::RobotoMedium_ttf, (size_t) BinaryData::RobotoMedium_ttfSize);
        return t;
    }
    inline const juce::Typeface::Ptr& robotoBlack()
    {
        static juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor (
            BinaryData::RobotoBlack_ttf, (size_t) BinaryData::RobotoBlack_ttfSize);
        return t;
    }

    inline juce::Font font (float px)        { return juce::Font (juce::FontOptions (robotoRegular()).withHeight (px)); }
    inline juce::Font fontMedium (float px)  { return juce::Font (juce::FontOptions (robotoMedium()).withHeight (px)); }
    inline juce::Font fontBlack (float px)   { return juce::Font (juce::FontOptions (robotoBlack()).withHeight (px)); }
    inline juce::Font fontMono (float px)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), px, juce::Font::plain));
    }
    // Section titles: 11px, weight 600, 0.1em tracking, uppercase, dim.
    inline juce::Font titleFont() { return fontMedium (11.0f).withExtraKerningFactor (0.1f); }

    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        return juce::GlyphArrangement::getStringWidth (f, s);
    }

    // --- Shared drawing ----------------------------------------------------
    inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (panelLine);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        // inset 0 1px 0 rgba(255,255,255,.05)
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (r.getX() + 4.0f, r.getY() + 1.0f, r.getWidth() - 8.0f, 1.0f);
    }

    inline void drawWell (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (well);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (panelLine);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        // inset 0 2px 6px rgba(0,0,0,.45) — approximated as a top shade band
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRect (r.getX() + 1.0f, r.getY() + 1.0f, r.getWidth() - 2.0f, 3.0f);
    }

    inline void drawChevronDown (juce::Graphics& g, juce::Rectangle<float> box, float px)
    {
        // Heroicons "M6 9l6 6 6-6" in a 24px viewBox, scaled to px.
        const float s = px / 24.0f;
        juce::Path p;
        p.startNewSubPath (6.0f * s, 9.0f * s);
        p.lineTo (12.0f * s, 15.0f * s);
        p.lineTo (18.0f * s, 9.0f * s);
        p.applyTransform (juce::AffineTransform::translation (box.getCentreX() - px / 2.0f,
                                                              box.getCentreY() - px / 2.0f));
        g.setColour (dim);
        g.strokePath (p, juce::PathStrokeType (juce::jmax (1.4f, 2.0f * s), juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // Knob geometry: the design draws in a 56x56 viewBox — recess disc r=26,
    // body cap r=16, track ring r=23, pointer to r=13, value dot r=3.
    // Everything scales from the bounds' short side.
    struct KnobGeo
    {
        juce::Point<float> centre;
        float scale;   // px per design unit (side / 56)
        float ring()   const { return 23.0f * scale; }
        float recess() const { return 26.0f * scale; }
        float cap()    const { return 16.0f * scale; }
        float radius;  // legacy: ring radius (mod arcs base)
    };

    inline KnobGeo knobGeo (juce::Rectangle<float> r)
    {
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        KnobGeo geo { r.getCentre(), size / 56.0f, 0.0f };
        geo.radius = geo.ring();
        return geo;
    }
} // namespace theme

class VapeLnF : public juce::LookAndFeel_V4
{
public:
    VapeLnF()
    {
        setDefaultSansSerifTypeface (theme::robotoRegular());

        setColour (juce::Slider::rotarySliderFillColourId, theme::accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, theme::panelLine);
        setColour (juce::Slider::thumbColourId, theme::strong);
        setColour (juce::Slider::trackColourId, theme::accent);
        setColour (juce::Slider::backgroundColourId, theme::well);
        setColour (juce::Slider::textBoxTextColourId, theme::strong);
        setColour (juce::Slider::textBoxBackgroundColourId, theme::input);
        setColour (juce::Slider::textBoxOutlineColourId, theme::inputLine);
        setColour (juce::Label::textColourId, theme::dim);
        setColour (juce::ComboBox::backgroundColourId, theme::input);
        setColour (juce::ComboBox::textColourId, theme::strong);
        setColour (juce::ComboBox::outlineColourId, theme::inputLine);
        setColour (juce::ComboBox::arrowColourId, theme::dim);
        setColour (juce::PopupMenu::backgroundColourId, theme::panel);
        setColour (juce::PopupMenu::textColourId, theme::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::input);
        setColour (juce::PopupMenu::highlightedTextColourId, theme::strong);
        setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        setColour (juce::TextButton::textColourOffId, theme::text);
        setColour (juce::ScrollBar::thumbColourId, theme::faint);
        setColour (juce::ScrollBar::trackColourId, theme::well);
        setColour (juce::ScrollBar::backgroundColourId, theme::well);
        setColour (juce::BubbleComponent::backgroundColourId, theme::well);
        setColour (juce::BubbleComponent::outlineColourId, theme::inputLine);
        setColour (juce::TooltipWindow::textColourId, theme::strong);
    }

    // --- Rotary: Rainfall knob anatomy --------------------------------------
    // Variants via slider properties: "oscKnob" (white value dot, no arc).
    // Value arc colour comes from rotarySliderFillColourId.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& s) override
    {
        const auto geo = theme::knobGeo (juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height));
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float ringW = 3.0f * geo.scale;

        // recess disc + body cap
        g.setColour (theme::well);
        g.fillEllipse (juce::Rectangle<float> (geo.recess() * 2.0f, geo.recess() * 2.0f).withCentre (geo.centre));
        g.setColour (theme::input);
        g.fillEllipse (juce::Rectangle<float> (geo.cap() * 2.0f, geo.cap() * 2.0f).withCentre (geo.centre));
        g.setColour (theme::inputLine);
        g.drawEllipse (juce::Rectangle<float> (geo.cap() * 2.0f, geo.cap() * 2.0f).withCentre (geo.centre), 1.0f);

        // track ring
        juce::Path trackPath;
        trackPath.addCentredArc (geo.centre.x, geo.centre.y, geo.ring(), geo.ring(), 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (s.findColour (juce::Slider::rotarySliderOutlineColourId));
        g.strokePath (trackPath, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const bool oscKnob = (bool) s.getProperties().getWithDefault ("oscKnob", false);
        if (oscKnob)
        {
            // white value dot on the ring
            g.setColour (theme::strong);
            const auto p = geo.centre.getPointOnCircumference (geo.ring(), angle);
            g.fillEllipse (juce::Rectangle<float> (6.0f * geo.scale, 6.0f * geo.scale).withCentre (p));
        }
        else
        {
            // filled value arc from sweep start to the value
            juce::Path valPath;
            valPath.addCentredArc (geo.centre.x, geo.centre.y, geo.ring(), geo.ring(), 0.0f,
                                   rotaryStartAngle, angle, true);
            g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
            g.strokePath (valPath, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // pointer: centre to r=13
        g.setColour (theme::strong);
        const auto tip = geo.centre.getPointOnCircumference (13.0f * geo.scale, angle);
        g.drawLine ({ geo.centre, tip }, 2.0f);
    }

    // --- Linear sliders: 4px well track, optional centre tick (bipolar),
    // fill in trackColourId, 14px white round thumb -------------------------
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float, float, juce::Slider::SliderStyle style,
                           juce::Slider& s) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, s);
            return;
        }

        const float cy = (float) y + (float) height * 0.5f;
        const float x0 = (float) x + 7.0f, x1 = (float) x + (float) width - 7.0f;
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);

        g.setColour (theme::well);
        g.fillRoundedRectangle (x0, cy - 2.0f, x1 - x0, 4.0f, 2.0f);

        if (bipolar)
        {
            const float cx = (x0 + x1) * 0.5f;
            g.setColour (theme::faint);
            g.fillRoundedRectangle (cx - 1.0f, cy - 6.0f, 2.0f, 12.0f, 1.0f);
            g.setColour (s.findColour (juce::Slider::trackColourId));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (cx, sliderPos), cy - 2.0f,
                                                                    juce::jmax (cx, sliderPos), cy + 2.0f));
        }
        else
        {
            g.setColour (s.findColour (juce::Slider::trackColourId));
            g.fillRoundedRectangle (x0, cy - 2.0f, juce::jmax (0.0f, sliderPos - x0), 4.0f, 2.0f);
        }

        g.setColour (theme::strong);
        g.fillEllipse (sliderPos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
        g.setColour (theme::inputLine);
        g.drawEllipse (sliderPos - 7.0f, cy - 7.0f, 14.0f, 14.0f, 1.0f);
    }

    // --- Select / dropdown ---------------------------------------------------
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRect (r.getX() + 2.0f, r.getY() + 1.0f, r.getWidth() - 4.0f, 2.0f);

        theme::drawChevronDown (g, r.removeFromRight ((float) height), 14.0f);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (13.0f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (10, 1, box.getWidth() - 10 - box.getHeight(), box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
    juce::Font getPopupMenuFont() override { return theme::font (13.0f); }

    // --- INIT ghost button ---------------------------------------------------
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat();
        if (down || highlighted)
        {
            g.setColour (juce::Colours::white.withAlpha (down ? 0.10f : 0.05f));
            g.fillRoundedRectangle (r, 4.0f);
        }
        g.setColour (theme::inputLine);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return theme::fontMedium (12.0f).withExtraKerningFactor (0.06f);
    }

    // Stock ScrollBar refuses to draw a thumb unless the bar is taller than
    // 32 + 2*thickness; the matrix viewport is only ~54px tall, so relax the
    // minimum or the scrollbar renders as nothing at all.
    int getMinimumScrollbarThumbSize (juce::ScrollBar& sb) override
    {
        return juce::jmin (sb.getWidth(), sb.getHeight());
    }

    // --- Slider value boxes: monospace, rounded, recessed ---------------------
    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (theme::fontMono (12.0f));
        l->setJustificationType (juce::Justification::centred);
        l->getProperties().set ("valueBox", true);
        return l;
    }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        if (! (bool) label.getProperties().getWithDefault ("valueBox", false))
        {
            juce::LookAndFeel_V4::drawLabel (g, label);
            return;
        }
        auto r = label.getLocalBounds().toFloat();
        g.setColour (theme::input);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::inputLine);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        if (! label.isBeingEdited())
        {
            g.setColour (theme::strong);
            g.setFont (theme::fontMono (12.0f));
            g.drawText (label.getText(), label.getLocalBounds().reduced (6, 0),
                        juce::Justification::centredRight);
        }
    }
};

// 18px circled-X outline remove button (destructive red).
class KillButton : public juce::Button
{
public:
    KillButton() : juce::Button ("remove") {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre (18.0f, 18.0f).reduced (1.5f);
        g.setColour (down ? theme::destroy.brighter (0.4f)
                          : highlighted ? theme::destroy.brighter (0.2f) : theme::destroy);
        g.drawEllipse (r, 1.5f);
        const auto c = r.getCentre();
        const float k = r.getWidth() * 0.22f;
        g.drawLine (c.x - k, c.y - k, c.x + k, c.y + k, 1.5f);
        g.drawLine (c.x + k, c.y - k, c.x - k, c.y + k, 1.5f);
    }
};

} // namespace vape
