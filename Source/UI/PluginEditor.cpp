#include "PluginEditor.h"

namespace vape
{

VapeEditor::VapeEditor (VapeProcessor& p)
    : juce::AudioProcessorEditor (p),
      proc (p),
      viz (p),
      matrix (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    for (int d = 0; d < numDests; ++d)
        knobFor[(size_t) d] = addKnob (d);

    tableBox.addItemList (tableNames(), 1);
    filterBox.addItemList (filterTypeNames(), 1);
    lfo1ShapeBox.addItemList (lfoShapeNames(), 1);
    lfo2ShapeBox.addItemList (lfoShapeNames(), 1);
    lfo1ModeBox.addItemList (lfoModeNames(), 1);
    lfo2ModeBox.addItemList (lfoModeNames(), 1);
    for (auto* box : { &tableBox, &filterBox, &lfo1ShapeBox, &lfo2ShapeBox, &lfo1ModeBox, &lfo2ModeBox })
        addAndMakeVisible (box);

    tableAtt    = std::make_unique<ComboAtt> (proc.apvts, "table", tableBox);
    filterAtt   = std::make_unique<ComboAtt> (proc.apvts, "filterType", filterBox);
    lfo1Att     = std::make_unique<ComboAtt> (proc.apvts, "lfo1Shape", lfo1ShapeBox);
    lfo2Att     = std::make_unique<ComboAtt> (proc.apvts, "lfo2Shape", lfo2ShapeBox);
    lfo1ModeAtt = std::make_unique<ComboAtt> (proc.apvts, "lfo1Mode", lfo1ModeBox);
    lfo2ModeAtt = std::make_unique<ComboAtt> (proc.apvts, "lfo2Mode", lfo2ModeBox);

    for (auto* chip : { &env1Chip, &env2Chip, &env3Chip, &lfo1Chip, &lfo2Chip })
        addAndMakeVisible (chip);

    initButton.onClick = [this] { proc.initPatch(); };
    addAndMakeVisible (initButton);

    addAndMakeVisible (viz);
    addAndMakeVisible (matrix);
    addAndMakeVisible (keyboard);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (22.0f);

    setSize (1000, 735);
    startTimerHz (30);
}

VapeEditor::~VapeEditor()
{
    setLookAndFeel (nullptr);
}

ModKnob* VapeEditor::addKnob (int dest)
{
    auto* k = knobs.add (new ModKnob (proc, dest));
    addAndMakeVisible (k);
    return k;
}

void VapeEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    g.setFont (juce::FontOptions (21.0f, juce::Font::bold));
    g.setColour (theme::accent);
    g.drawText ("VAPE", 14, 12, 62, 24, juce::Justification::centredLeft);
    g.setColour (theme::text);
    g.drawText ("STATION", 78, 12, 140, 24, juce::Justification::centredLeft);

    g.setColour (theme::dim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("graintable prototype  -  drag a coloured chip onto any knob",
                14, 38, 400, 14, juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("TABLE", 614, 20, 44, 24, juce::Justification::centredLeft);

    for (const auto& p : panels)
    {
        const auto r = p.r.toFloat();
        g.setColour (theme::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (theme::panelLine);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (theme::dim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (p.title, p.r.getX() + 12, p.r.getY() + 6, 150, 12, juce::Justification::centredLeft);
    }
}

void VapeEditor::resized()
{
    panels.clear();
    const int W = getWidth();

    // Header
    initButton.setBounds (548, 20, 52, 24);
    tableBox.setBounds (660, 20, 148, 24);
    knobFor[dGain]->setBounds (908, 2, 82, 62);

    // Row A: graintable viz | OSC | FILTER
    viz.setBounds (10, 66, 300, 225);

    const juce::Rectangle<int> oscR (320, 66, 470, 225);
    panels.push_back ({ "OSC", oscR });
    {
        auto inner = oscR.reduced (8).withTrimmedTop (16);
        const int cw = inner.getWidth() / 3;
        const int ch = inner.getHeight() / 3;
        const int order[9] = { dPosition, dGrainSize, dDensity,
                               dSpray, dPitchRand, dShape,
                               dCoarse, dFine, dSpread };
        for (int i = 0; i < 9; ++i)
            knobFor[(size_t) order[i]]->setBounds (inner.getX() + (i % 3) * cw,
                                                   inner.getY() + (i / 3) * ch, cw, ch);
    }

    const juce::Rectangle<int> filR (800, 66, W - 810, 225);
    panels.push_back ({ "FILTER", filR });
    {
        auto inner = filR.reduced (10).withTrimmedTop (16);
        filterBox.setBounds (inner.removeFromTop (24));
        inner.removeFromTop (8);
        const int half = inner.getWidth() / 2;
        knobFor[dCutoff]->setBounds (inner.getX(), inner.getY() + 14, half, 120);
        knobFor[dRes]->setBounds (inner.getX() + half, inner.getY() + 14, half, 120);
    }

    // Row B: ENV1 / ENV2 / ENV3 / LFO1 / LFO2
    const int by = 299, bh = 190, gap = 8;
    const int pw = (W - 20 - 4 * gap) / 5;
    SourceChip* rowChips[5] = { &env1Chip, &env2Chip, &env3Chip, &lfo1Chip, &lfo2Chip };
    const char* rowTitles[5] = { "ENV 1 - AMP", "ENV 2", "ENV 3", "LFO 1", "LFO 2" };
    const int envDests[3][4] = { { dEnv1A, dEnv1D, dEnv1S, dEnv1R },
                                 { dEnv2A, dEnv2D, dEnv2S, dEnv2R },
                                 { dEnv3A, dEnv3D, dEnv3S, dEnv3R } };

    for (int i = 0; i < 5; ++i)
    {
        const juce::Rectangle<int> r (10 + i * (pw + gap), by, pw, bh);
        panels.push_back ({ rowTitles[i], r });
        rowChips[i]->setBounds (r.getRight() - 64, r.getY() + 5, 58, 17);

        auto inner = r.reduced (8).withTrimmedTop (18);
        if (i < 3)
        {
            const int cw = inner.getWidth() / 2;
            const int ch = inner.getHeight() / 2;
            for (int k = 0; k < 4; ++k)
                knobFor[(size_t) envDests[i][k]]->setBounds (inner.getX() + (k % 2) * cw,
                                                             inner.getY() + (k / 2) * ch, cw, ch);
        }
        else
        {
            auto* shapeBox = i == 3 ? &lfo1ShapeBox : &lfo2ShapeBox;
            auto* modeBox  = i == 3 ? &lfo1ModeBox  : &lfo2ModeBox;
            shapeBox->setBounds (inner.removeFromBottom (24));
            inner.removeFromBottom (4);
            modeBox->setBounds (inner.removeFromBottom (24));
            inner.removeFromBottom (4);
            const int d = i == 3 ? dLfo1Rate : dLfo2Rate;
            knobFor[(size_t) d]->setBounds (inner.reduced (14, 0));
        }
    }

    // Matrix + keyboard
    matrix.setBounds (10, 497, W - 20, 148);
    keyboard.setBounds (10, 653, W - 20, 72);
}

void VapeEditor::timerCallback()
{
    const auto stamp = proc.displayStamp();
    const auto now = juce::Time::getMillisecondCounter();
    if (stamp != lastStamp)
    {
        lastStamp = stamp;
        lastFreshMs = now;
    }
    const bool live = (now - lastFreshMs) < 250;

    for (int d = 0; d < numDests; ++d)
    {
        const float norm = live ? proc.displayNorm (d) : proc.param (d)->getValue();
        knobFor[(size_t) d]->setDisplayNorm (norm);
    }

    viz.refresh (live ? proc.displayNorm (dPosition) : proc.param (dPosition)->getValue(),
                 proc.tableIndex());

    const int mv = proc.matrixVersion();
    if (mv != lastMatVersion)
    {
        lastMatVersion = mv;
        for (auto* k : knobs)
            k->refreshRoutes();
    }
}

} // namespace vape
