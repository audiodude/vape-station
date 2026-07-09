#include "MatrixPanel.h"

namespace vape
{

MatrixPanel::Row::Row (VapeProcessor&, juce::ValueTree nodeIn) : node (nodeIn)
{
    src = srcFromName (node[idSrc].toString());
    dst = destFromId (node[idDst].toString());
    destText = dst >= 0 ? destDisplayName (dst) : node[idDst].toString();

    depth.setSliderStyle (juce::Slider::LinearHorizontal);
    depth.setRange (-1.0, 1.0, 0.0);
    depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    depth.setNumDecimalPlacesToDisplay (2);
    depth.setScrollWheelEnabled (false); // the wheel scrolls the matrix list
    depth.getProperties().set ("bipolar", true);
    depth.setColour (juce::Slider::trackColourId, theme::srcColour (src));
    depth.setDoubleClickReturnValue (true, 0.0);
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
    const float cy = (float) getHeight() * 0.5f;

    // source chip pill (62px)
    juce::Rectangle<float> pill (0.0f, cy - 9.0f, 62.0f, 18.0f);
    g.setColour (theme::input);
    g.fillRoundedRectangle (pill, 9.0f);
    const auto f = theme::fontMedium (10.0f).withExtraKerningFactor (0.06f);
    const auto name = juce::String (srcName (src));
    const float textW = theme::textWidth (f, name);
    const float unitW = 6.0f + 5.0f + textW;
    float x = pill.getCentreX() - unitW / 2.0f;
    g.setColour (c);
    g.fillEllipse (x, cy - 3.0f, 6.0f, 6.0f);
    g.setFont (f);
    g.drawText (name, juce::Rectangle<float> (x + 11.0f, pill.getY(), textW + 2.0f, pill.getHeight()),
                juce::Justification::centredLeft);

    // chevron-right (Heroicons M9 6l6 6-6 6, 12px)
    {
        const float s = 12.0f / 24.0f;
        juce::Path p;
        p.startNewSubPath (9.0f * s, 6.0f * s);
        p.lineTo (15.0f * s, 12.0f * s);
        p.lineTo (9.0f * s, 18.0f * s);
        p.applyTransform (juce::AffineTransform::translation (72.0f, cy - 6.0f));
        g.setColour (theme::faint);
        g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // destination label
    g.setColour (theme::text);
    g.setFont (theme::font (12.0f));
    g.drawText (destText, 94, 0, 96, getHeight(), juce::Justification::centredLeft);
}

void MatrixPanel::Row::resized()
{
    auto b = getLocalBounds();
    kill.setBounds (b.removeFromRight (22));
    b.removeFromRight (4);
    b.removeFromLeft (190);          // chip + chevron + destination (painted)
    depth.setBounds (b);
}

MatrixPanel::MatrixPanel (VapeProcessor& p) : proc (p)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (14);
    viewport.onScroll = [this] { repaint(); };
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
    theme::drawPanel (g, getLocalBounds().toFloat());

    g.setColour (theme::dim);
    g.setFont (theme::titleFont());
    g.drawText ("MATRIX", 16, 12, 70, 14, juce::Justification::centredLeft);

    if (rows.isEmpty())
    {
        g.setColour (theme::faint);
        g.setFont (theme::font (12.0f));
        g.drawText ("Drag a source chip onto any knob to modulate it",
                    getLocalBounds().withTrimmedTop (24), juce::Justification::centred);
    }
}

void MatrixPanel::paintOverChildren (juce::Graphics& g)
{
    // Amber overflow arrows when there are rows to scroll to.
    const bool canUp = viewport.getViewPositionY() > 0;
    const bool canDown = viewport.getViewPositionY() + viewport.getViewHeight() < content.getHeight();
    if (! canUp && ! canDown)
        return;

    const auto vb = viewport.getBounds().toFloat();
    const float cx = vb.getRight() - 7.0f;
    g.setColour (juce::Colour (0xfffbbf24));

    if (canUp)
    {
        juce::Path p;
        p.addTriangle (cx - 5.0f, vb.getY() + 1.0f, cx + 5.0f, vb.getY() + 1.0f, cx, vb.getY() - 6.0f);
        g.fillPath (p);
    }
    if (canDown)
    {
        juce::Path p;
        p.addTriangle (cx - 5.0f, vb.getBottom() - 1.0f, cx + 5.0f, vb.getBottom() - 1.0f, cx, vb.getBottom() + 6.0f);
        g.fillPath (p);
    }
}

void MatrixPanel::resized()
{
    auto b = getLocalBounds().reduced (16, 12);
    auto header = b.removeFromTop (18);

    auto chips = header.removeFromRight (3 * 62 + 16);
    velChip.setBounds (chips.removeFromLeft (54));
    chips.removeFromLeft (8);
    wheelChip.setBounds (chips.removeFromLeft (66));
    chips.removeFromLeft (8);
    keyChip.setBounds (chips.removeFromLeft (54));

    b.removeFromTop (10);
    viewport.setBounds (b);
    layoutRows();
}

void MatrixPanel::layoutRows()
{
    // Two-column grid, 32px column gap, 12px row gap, 20px rows.
    const int rowH = 20, vGap = 12, hGap = 32;
    const int w = juce::jmax (200, viewport.getWidth() - (rows.size() > 4 ? viewport.getScrollBarThickness() : 0));
    const int colW = (w - hGap) / 2;
    const int numRows = (rows.size() + 1) / 2;
    content.setSize (w, juce::jmax (1, numRows * (rowH + vGap) - vGap));
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds ((i % 2) * (colW + hGap), (i / 2) * (rowH + vGap), colW, rowH);
}

void MatrixPanel::rebuild()
{
    rows.clear();
    auto mm = proc.matrixNode();

    // Collect and sort by source, then destination.
    std::vector<juce::ValueTree> nodes;
    for (int i = 0; i < mm.getNumChildren(); ++i)
        if (mm.getChild (i).hasType (idMod))
            nodes.push_back (mm.getChild (i));

    std::sort (nodes.begin(), nodes.end(), [] (const juce::ValueTree& a, const juce::ValueTree& b)
    {
        const int sa = srcFromName (a[idSrc].toString()), sb = srcFromName (b[idSrc].toString());
        if (sa != sb) return sa < sb;
        return destFromId (a[idDst].toString()) < destFromId (b[idDst].toString());
    });

    for (auto& n : nodes)
    {
        auto* row = rows.add (new Row (proc, n));
        content.addAndMakeVisible (row);
    }
    layoutRows();
    repaint();
}

} // namespace vape
