#include "ModKnob.h"

namespace vape
{

namespace
{
    // OSC knobs show a white value dot (no arc) so modulation arcs stay readable.
    bool isOscDest (int d)
    {
        return d == dPosition || d == dGrainSize || d == dDensity || d == dSpray
            || d == dPitchRand || d == dShape || d == dCoarse || d == dFine || d == dSpread;
    }

    // Value-arc colour for non-OSC knobs: panel source colour for ENV/LFO
    // knobs, brand blue for Filter/Gain.
    juce::Colour arcColourFor (int d)
    {
        if (d >= dEnv1A && d <= dEnv1R) return theme::srcColour (sEnv1);
        if (d >= dEnv2A && d <= dEnv2R) return theme::srcColour (sEnv2);
        if (d >= dEnv3A && d <= dEnv3R) return theme::srcColour (sEnv3);
        if (d == dLfo1Rate)             return theme::srcColour (sLfo1);
        if (d == dLfo2Rate)             return theme::srcColour (sLfo2);
        return theme::accent;
    }
} // namespace

ModKnob::ModKnob (VapeProcessor& p, int destIn) : proc (p), dest (destIn)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    // Rainfall knob: 270 degree sweep starting at 7:30.
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.getProperties().set ("oscKnob", isOscDest (dest));
    slider.setColour (juce::Slider::rotarySliderFillColourId, arcColourFor (dest));
    // Mod rings extend beyond the slider's own bounds and anchor to its value.
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);

    label.setText (destInfos()[(size_t) dest].name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (theme::font (11.0f));
    label.setColour (juce::Label::textColourId, theme::dim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, destInfos()[(size_t) dest].id, slider);

    // The attachment doesn't propagate the parameter's unit label, so the
    // drag popup would show bare numbers ("2" rather than "2 Hz").
    if (auto* param = proc.apvts.getParameter (destInfos()[(size_t) dest].id))
        if (param->getLabel().isNotEmpty())
            slider.setTextValueSuffix (" " + param->getLabel());

    refreshRoutes();
}

void ModKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromBottom (13));
    slider.setBounds (b);
}

void ModKnob::setDisplayNorm (float n)
{
    if (std::abs (n - dispNorm) > 0.004f)
    {
        dispNorm = n;
        if (! routes.empty())
            repaint();
    }
}

void ModKnob::refreshRoutes()
{
    routes.clear();
    if (auto* m = proc.currentMatrix())
        routes = m->routes[(size_t) dest];
    repaint();
}

void ModKnob::paintOverChildren (juce::Graphics& g)
{
    const auto geo = theme::knobGeo (slider.getBounds().toFloat());
    const auto rp = slider.getRotaryParameters();
    auto angleFor = [&] (float prop)
    {
        return rp.startAngleRadians + prop * (rp.endAngleRadians - rp.startAngleRadians);
    };

    const float baseProp = (float) slider.valueToProportionOfLength (slider.getValue());
    const bool oscKnob = (bool) slider.getProperties().getWithDefault ("oscKnob", false);

    // Modulation arcs anchor at the value dot and span the range the source
    // can actually reach: unipolar sources (ENV/VEL/WHEEL) push one way, so
    // the arc sweeps from the value by the depth; bipolar sources (LFO/KEY)
    // swing both ways, so the arc extends symmetrically around the value.
    // First arc sits on the track ring (width 3), the second just outside
    // (width 2), further routes step outward.
    int ringIdx = 0;
    for (const auto& r : routes)
    {
        const float startProp = juce::jlimit (0.0f, 1.0f,
                                              isBipolarSrc (r.src) ? baseProp - r.depth : baseProp);
        const float endProp = juce::jlimit (0.0f, 1.0f, baseProp + r.depth);
        const bool onTrack = oscKnob && ringIdx == 0;
        const float radius = onTrack ? geo.ring()
                                     : geo.recess() + 0.5f * geo.scale
                                           + (float) (ringIdx - (oscKnob ? 1 : 0)) * 3.5f * geo.scale;
        const float width = onTrack ? 3.0f * geo.scale : 2.0f * geo.scale;

        juce::Path arc;
        arc.addCentredArc (geo.centre.x, geo.centre.y, radius, radius, 0.0f,
                           angleFor (startProp), angleFor (endProp), true);
        g.setColour (theme::srcColour (r.src));
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.5f, width), juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        if (++ringIdx >= 4)
            break;
    }

    // Live modulated value dot (white) for modulated knobs.
    if (! routes.empty() && dispNorm >= 0.0f)
    {
        const auto p = geo.centre.getPointOnCircumference (geo.ring(), angleFor (dispNorm));
        g.setColour (theme::strong);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (p));
    }

    // Source-coloured dots (7px) beside the label for modulated knobs.
    if (! routes.empty())
    {
        const auto lb = label.getBounds().toFloat();
        const float textW = theme::textWidth (theme::font (11.0f), label.getText());
        float dx = lb.getCentreX() + textW / 2.0f + 6.0f;
        for (const auto& r : routes)
        {
            g.setColour (theme::srcColour (r.src));
            g.fillEllipse (dx, lb.getCentreY() - 3.5f, 7.0f, 7.0f);
            dx += 10.0f;
            if (dx > lb.getRight() - 4.0f)
                break;
        }
    }

    if (dragOver)
    {
        g.setColour (theme::accent.withAlpha (0.6f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.5f);
    }
}

bool ModKnob::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("modsrc:");
}

void ModKnob::itemDragEnter (const SourceDetails&)
{
    dragOver = true;
    repaint();
}

void ModKnob::itemDragExit (const SourceDetails&)
{
    dragOver = false;
    repaint();
}

void ModKnob::itemDropped (const SourceDetails& d)
{
    dragOver = false;
    const int src = srcFromName (d.description.toString().fromFirstOccurrenceOf ("modsrc:", false, false));
    if (src >= 0)
        proc.addModRoute (src, dest, 0.35f);
    repaint();
}

void ModKnob::showRouteMenu()
{
    auto mm = proc.matrixNode();
    const juce::String myId = destInfos()[(size_t) dest].id;

    juce::Array<juce::ValueTree> myRows;
    juce::PopupMenu menu;

    for (int i = 0; i < mm.getNumChildren(); ++i)
    {
        auto row = mm.getChild (i);
        if (row[idDst].toString() == myId)
        {
            myRows.add (row);
            menu.addItem (myRows.size(),
                          "Remove " + row[idSrc].toString()
                              + " (" + juce::String ((double) row[idDepth], 2) + ")");
        }
    }

    if (myRows.isEmpty())
        menu.addItem (1000, "No modulation - drag a source chip here", false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [mm, myRows] (int result) mutable
                        {
                            if (result > 0 && result <= myRows.size())
                                mm.removeChild (myRows[result - 1], nullptr);
                        });
}

} // namespace vape
