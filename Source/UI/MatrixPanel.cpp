#include "MatrixPanel.h"

namespace vape
{

MatrixPanel::Row::Row (VapeProcessor&, juce::ValueTree nodeIn) : node (nodeIn)
{
    src = srcFromName (node[idSrc].toString());
    const int dst = destFromId (node[idDst].toString());
    labelText = juce::String (srcName (src)) + "  >  "
              + (dst >= 0 ? destDisplayName (dst) : node[idDst].toString());

    depth.setSliderStyle (juce::Slider::LinearHorizontal);
    depth.setRange (-1.0, 1.0, 0.0);
    depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
    depth.setNumDecimalPlacesToDisplay (2);
    depth.setValue ((double) node[idDepth], juce::dontSendNotification);
    depth.onValueChange = [this] { node.setProperty (idDepth, depth.getValue(), nullptr); };
    addAndMakeVisible (depth);

    kill.onClick = [n = node]() mutable
    {
        // Deferred so the button isn't destroyed while its click is on the stack.
        juce::MessageManager::callAsync ([n]() mutable
        {
            auto parent = n.getParent();
            if (parent.isValid())
                parent.removeChild (n, nullptr);
        });
    };
    addAndMakeVisible (kill);
}

void MatrixPanel::Row::paint (juce::Graphics& g)
{
    const auto c = theme::srcColour (src);
    g.setColour (c);
    g.fillEllipse (6.0f, (float) getHeight() / 2.0f - 3.0f, 6.0f, 6.0f);
    g.setColour (theme::text);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (labelText, 18, 0, 230, getHeight(), juce::Justification::centredLeft);
}

void MatrixPanel::Row::resized()
{
    auto b = getLocalBounds();
    kill.setBounds (b.removeFromRight (24).reduced (2));
    b.removeFromLeft (252);
    depth.setBounds (b.reduced (2, 2));
}

MatrixPanel::MatrixPanel (VapeProcessor& p) : proc (p)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    addAndMakeVisible (velChip);
    addAndMakeVisible (wheelChip);
    addAndMakeVisible (keyChip);

    proc.apvts.state.addListener (this);
    rebuild();
}

MatrixPanel::~MatrixPanel()
{
    proc.apvts.state.removeListener (this);
}

void MatrixPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (theme::panelLine);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    g.setColour (theme::dim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("MATRIX", 12, 6, 120, 14, juce::Justification::centredLeft);

    if (rows.isEmpty())
    {
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Drag a source chip onto any knob to modulate it",
                    getLocalBounds().withTrimmedTop (20), juce::Justification::centred);
    }
}

void MatrixPanel::resized()
{
    auto b = getLocalBounds().reduced (8);
    auto header = b.removeFromTop (18);

    auto chips = header.removeFromRight (3 * 62);
    velChip.setBounds (chips.removeFromLeft (58).reduced (0, 0));
    chips.removeFromLeft (4);
    wheelChip.setBounds (chips.removeFromLeft (58));
    chips.removeFromLeft (4);
    keyChip.setBounds (chips.removeFromLeft (58));

    b.removeFromTop (4);
    viewport.setBounds (b);
    layoutRows();
}

void MatrixPanel::layoutRows()
{
    const int rowH = 26;
    const int w = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness());
    content.setSize (w, juce::jmax (1, rows.size() * rowH));
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (0, i * rowH, w, rowH);
}

void MatrixPanel::rebuild()
{
    rows.clear();
    auto mm = proc.matrixNode();
    for (int i = 0; i < mm.getNumChildren(); ++i)
    {
        auto rowNode = mm.getChild (i);
        if (! rowNode.hasType (idMod))
            continue;
        auto* row = rows.add (new Row (proc, rowNode));
        content.addAndMakeVisible (row);
    }
    layoutRows();
    repaint();
}

} // namespace vape
