#include "PluginProcessor.h"
#include "UI/PluginEditor.h"

namespace vape
{

VapeProcessor::VapeProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    for (int d = 0; d < numDests; ++d)
    {
        const auto id = destInfos()[(size_t) d].id;
        params_[(size_t) d] = apvts.getParameter (id);
        raws[(size_t) d] = apvts.getRawParameterValue (id);
        ranges[(size_t) d] = params_[(size_t) d]->getNormalisableRange();
    }
    tableRaw      = apvts.getRawParameterValue ("table");
    filterTypeRaw = apvts.getRawParameterValue ("filterType");
    lfo1ShapeRaw  = apvts.getRawParameterValue ("lfo1Shape");
    lfo2ShapeRaw  = apvts.getRawParameterValue ("lfo2Shape");

    grainTables(); // build tables up front

    synth.addSound (new GrainSound());
    for (int i = 0; i < 10; ++i)
    {
        auto* v = new GrainVoice (*this, i);
        voices.push_back (v);
        synth.addVoice (v);
    }
    synth.setNoteStealingEnabled (true);

    ensureMatrixNode();
    compileMatrixNow();
    apvts.state.addListener (this);
}

VapeProcessor::~VapeProcessor()
{
    apvts.state.removeListener (this);
    cancelPendingUpdate();
    delete matrix.exchange (nullptr);
}

void VapeProcessor::ensureMatrixNode()
{
    if (! apvts.state.getChildWithName (idModMatrix).isValid())
    {
        juce::ValueTree mm (idModMatrix);
        // A default assignment so the killer feature is audible out of the box.
        juce::ValueTree row (idMod);
        row.setProperty (idSrc, srcName (sLfo1), nullptr);
        row.setProperty (idDst, destInfos()[dPosition].id, nullptr);
        row.setProperty (idDepth, 0.25f, nullptr);
        mm.appendChild (row, nullptr);
        apvts.state.appendChild (mm, nullptr);
    }
}

void VapeProcessor::maybeRecompile (juce::ValueTree affected)
{
    // Only matrix edits matter; parameter-value churn in the tree is ignored.
    if (affected.hasType (idModMatrix) || affected.hasType (idMod))
        triggerAsyncUpdate();
}

void VapeProcessor::compileMatrixNow()
{
    auto compiled = compileMatrix (matrixNode());
    auto* fresh = compiled.release();
    if (auto* old = matrix.exchange (fresh, std::memory_order_acq_rel))
        matrixGraveyard.emplace_back (old);
    matVersion.fetch_add (1, std::memory_order_relaxed);
}

void VapeProcessor::addModRoute (int src, int dest, float depth)
{
    if (src < 0 || src >= numSrcs || dest < 0 || dest >= numDests)
        return;

    auto mm = matrixNode();
    if (! mm.isValid())
    {
        ensureMatrixNode();
        mm = matrixNode();
    }

    const juce::String srcStr = srcName (src);
    const juce::String dstStr = destInfos()[(size_t) dest].id;

    for (int i = 0; i < mm.getNumChildren(); ++i)
    {
        auto row = mm.getChild (i);
        if (row[idSrc].toString() == srcStr && row[idDst].toString() == dstStr)
        {
            row.setProperty (idDepth, depth, nullptr);
            return;
        }
    }

    juce::ValueTree row (idMod);
    row.setProperty (idSrc, srcStr, nullptr);
    row.setProperty (idDst, dstStr, nullptr);
    row.setProperty (idDepth, depth, nullptr);
    mm.appendChild (row, nullptr);
}

int VapeProcessor::tableIndex() const
{
    return juce::jlimit (0, (int) grainTables().size() - 1,
                         (int) tableRaw->load (std::memory_order_relaxed));
}

void VapeProcessor::pushDisplay (const std::array<float, numDests>& norms)
{
    for (int d = 0; d < numDests; ++d)
        displayNorms[(size_t) d].store (norms[(size_t) d], std::memory_order_relaxed);
    dispStamp.fetch_add (1, std::memory_order_relaxed);
}

void VapeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (auto* v : voices)
        v->prepare (sampleRate, samplesPerBlock);
}

bool VapeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void VapeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isController() && msg.getControllerNumber() == 1)
            wheel.store ((float) msg.getControllerValue() / 127.0f, std::memory_order_relaxed);
    }

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    // Soft safety clip: grain stacks can peak with hot settings.
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            d[i] = std::tanh (d[i]);
    }
}

void VapeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VapeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        apvts.state.removeListener (this);
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        ensureMatrixNode();
        apvts.state.addListener (this);
        compileMatrixNow();
    }
}

juce::AudioProcessorEditor* VapeProcessor::createEditor()
{
    return new VapeEditor (*this);
}

} // namespace vape

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new vape::VapeProcessor();
}
