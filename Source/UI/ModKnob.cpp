#include "ModKnob.h"

namespace vape
{

ModKnob::ModKnob (VapeProcessor& p, int destIn) : proc (p), dest (destIn)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f, true);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    // Mod rings extend beyond the slider's own bounds and anchor to its value.
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);

    label.setText (destInfos()[(size_t) dest].name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (10.5f)));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, destInfos()[(size_t) dest].id, slider);

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

    int ringIdx = 0;
    for (const auto& r : routes)
    {
        const float endProp = juce::jlimit (0.0f, 1.0f, baseProp + r.depth);
        const float radius = geo.radius + 3.5f + (float) ringIdx * 3.0f;

        juce::Path arc;
        arc.addCentredArc (geo.centre.x, geo.centre.y, radius, radius, 0.0f,
                           angleFor (baseProp), angleFor (endProp), true);
        g.setColour (theme::srcColour (r.src).withAlpha (0.85f));
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        if (++ringIdx >= 4)
            break;
    }

    if (! routes.empty() && dispNorm >= 0.0f)
    {
        const auto p = geo.centre.getPointOnCircumference (geo.radius, angleFor (dispNorm));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (p));
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
