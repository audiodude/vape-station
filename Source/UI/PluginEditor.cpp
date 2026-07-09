#include "PluginEditor.h"

namespace vape
{

// Rainfall layout: 1240px content, 20px side / 24px top padding, 12px gaps.
namespace
{
    constexpr int kPadX = 20, kPadY = 24, kGap = 12;
    constexpr int kHeaderH = 88, kRow1H = 284, kRow2H = 198, kMatrixH = 106, kKeysH = 114;
    constexpr int kWidth = 1280;
    constexpr int kHeight = kPadY + kHeaderH + kGap + kRow1H + kGap + kRow2H + kGap
                          + kMatrixH + kGap + kKeysH + kPadY;
}

VapeEditor::VapeEditor (VapeProcessor& p)
    : juce::AudioProcessorEditor (p),
      proc (p),
      viz (p),
      matrix (p),
      keyboard (p.keyboardState)
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

    posSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    posSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    posSlider.setColour (juce::Slider::trackColourId, theme::accent);
    posSlider.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (posSlider);
    posAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, destInfos()[dPosition].id, posSlider);

    addAndMakeVisible (matrix);
    addAndMakeVisible (keyboard);

    setSize (kWidth, kHeight);
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
    g.fillAll (theme::canvas);

    // Header band
    theme::drawPanel (g, headerRect.toFloat());
    {
        const int x = headerRect.getX() + 20;
        int y = headerRect.getCentreY() - 18;
        auto wordmark = theme::fontBlack (22.0f).withExtraKerningFactor (0.02f);
        g.setFont (wordmark);
        const auto vape = juce::String ("VAPE");
        const float vapeW = theme::textWidth (wordmark, vape);
        g.setColour (theme::accent);
        g.drawText (vape, x, y, 200, 24, juce::Justification::centredLeft);
        g.setColour (theme::strong);
        g.drawText (" STATION", x + (int) vapeW, y, 220, 24, juce::Justification::centredLeft);

        g.setColour (theme::dim);
        g.setFont (theme::font (11.0f));
        g.drawText ("drag a coloured chip onto any knob",
                    x, y + 26, 400, 14, juce::Justification::centredLeft);

        // "TABLE" label sits just left of the table select
        g.setFont (theme::titleFont());
        g.drawText ("TABLE", tableBox.getX() - 58, tableBox.getY(), 50, tableBox.getHeight(),
                    juce::Justification::centredRight);
    }

    for (const auto& pn : panels)
    {
        theme::drawPanel (g, pn.r.toFloat());
        if (pn.title.isNotEmpty())
        {
            g.setColour (theme::dim);
            g.setFont (theme::titleFont());
            g.drawText (pn.title, pn.r.getX() + 16, pn.r.getY() + 12, 180, 13,
                        juce::Justification::centredLeft);
        }
    }

    // Keyboard bed
    theme::drawWell (g, kbBedRect.toFloat());
}

void VapeEditor::resized()
{
    panels.clear();
    auto content = getLocalBounds().reduced (kPadX, kPadY);

    // --- Header ---
    headerRect = content.removeFromTop (kHeaderH);
    {
        auto h = headerRect.reduced (20, 14);
        // right side, laid right-to-left: Gain knob, TABLE select, INIT
        auto gainCell = h.removeFromRight (56);
        knobFor[dGain]->setBounds (gainCell.withSizeKeepingCentre (56, 60));
        h.removeFromRight (16);
        tableBox.setBounds (h.removeFromRight (130).withSizeKeepingCentre (130, 30));
        h.removeFromRight (66); // room for the painted "TABLE" label
        initButton.setBounds (h.removeFromRight (62).withSizeKeepingCentre (62, 28));
    }
    content.removeFromTop (kGap);

    // --- Row 1: GRAINTABLE / OSC / FILTER ---
    auto row1 = content.removeFromTop (kRow1H);
    {
        auto gtR = row1.removeFromLeft (360);
        panels.push_back ({ "GRAINTABLE", gtR });
        auto inner = gtR.reduced (16, 14);
        inner.removeFromTop (13 + 12); // title + gap
        posSlider.setBounds (inner.removeFromBottom (16));
        inner.removeFromBottom (12);
        viz.setBounds (inner);

        row1.removeFromLeft (kGap);
        auto filR = row1.removeFromRight (250);
        auto oscR = row1.withTrimmedRight (kGap);

        panels.push_back ({ "OSC", oscR });
        {
            auto oin = oscR.reduced (16, 12);
            oin.removeFromTop (13 + 10);
            const int cw = oin.getWidth() / 3;
            const int ch = oin.getHeight() / 3;
            const int order[9] = { dPosition, dGrainSize, dDensity,
                                   dSpray, dPitchRand, dShape,
                                   dCoarse, dFine, dSpread };
            for (int i = 0; i < 9; ++i)
                knobFor[(size_t) order[i]]->setBounds (
                    juce::Rectangle<int> (oin.getX() + (i % 3) * cw, oin.getY() + (i / 3) * ch, cw, ch)
                        .withSizeKeepingCentre (84, juce::jmin (ch, 56 + 3 + 13)));
        }

        panels.push_back ({ "FILTER", filR });
        {
            auto fin = filR.reduced (16, 14);
            fin.removeFromTop (13 + 12);
            filterBox.setBounds (fin.removeFromTop (30));
            const int half = fin.getWidth() / 2;
            const int kh = juce::jmin (fin.getHeight(), 72 + 3 + 13);
            knobFor[dCutoff]->setBounds (juce::Rectangle<int> (fin.getX(), fin.getY(), half, fin.getHeight())
                                             .withSizeKeepingCentre (half, kh));
            knobFor[dRes]->setBounds (juce::Rectangle<int> (fin.getX() + half, fin.getY(), half, fin.getHeight())
                                          .withSizeKeepingCentre (half, kh));
        }
    }
    content.removeFromTop (kGap);

    // --- Row 2: ENV1 / ENV2 / ENV3 / LFO1 / LFO2 (LFOs flex 1.1) ---
    auto row2 = content.removeFromTop (kRow2H);
    {
        const float unit = (float) (row2.getWidth() - 4 * kGap) / 5.2f;
        const int envW = (int) unit;
        const int lfoW = (int) (unit * 1.1f);

        SourceChip* rowChips[5] = { &env1Chip, &env2Chip, &env3Chip, &lfo1Chip, &lfo2Chip };
        const char* rowTitles[5] = { "ENV 1 - AMP", "ENV 2", "ENV 3", "LFO 1", "LFO 2" };
        const int envDests[3][4] = { { dEnv1A, dEnv1D, dEnv1S, dEnv1R },
                                     { dEnv2A, dEnv2D, dEnv2S, dEnv2R },
                                     { dEnv3A, dEnv3D, dEnv3S, dEnv3R } };

        for (int i = 0; i < 5; ++i)
        {
            auto r = row2.removeFromLeft (i < 3 ? envW : lfoW);
            if (i < 4)
                row2.removeFromLeft (kGap);
            panels.push_back ({ rowTitles[i], r });

            auto inner = r.reduced (14, 12);
            auto head = inner.removeFromTop (18);
            rowChips[i]->setBounds (head.removeFromRight (i < 3 ? 58 : 56));
            inner.removeFromTop (10);

            if (i < 3)
            {
                const int cw = inner.getWidth() / 2;
                const int ch = inner.getHeight() / 2;
                for (int k = 0; k < 4; ++k)
                    knobFor[(size_t) envDests[i][k]]->setBounds (
                        juce::Rectangle<int> (inner.getX() + (k % 2) * cw, inner.getY() + (k / 2) * ch, cw, ch)
                            .withSizeKeepingCentre (cw, juce::jmin (ch, 50 + 3 + 13)));
            }
            else
            {
                auto* shapeBox = i == 3 ? &lfo1ShapeBox : &lfo2ShapeBox;
                auto* modeBox  = i == 3 ? &lfo1ModeBox  : &lfo2ModeBox;
                shapeBox->setBounds (inner.removeFromBottom (26));
                inner.removeFromBottom (6);
                modeBox->setBounds (inner.removeFromBottom (26));
                inner.removeFromBottom (6);
                const int d = i == 3 ? dLfo1Rate : dLfo2Rate;
                knobFor[(size_t) d]->setBounds (inner.withSizeKeepingCentre (84, juce::jmin (inner.getHeight(), 62 + 3 + 13)));
            }
        }
    }
    content.removeFromTop (kGap);

    // --- Matrix + keyboard ---
    matrix.setBounds (content.removeFromTop (kMatrixH));
    content.removeFromTop (kGap);
    kbBedRect = content.removeFromTop (kKeysH);
    keyboard.setBounds (kbBedRect.reduced (8));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 35.0f);
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
