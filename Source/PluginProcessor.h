#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "ModMatrix.h"
#include "GrainTables.h"
#include "GrainVoice.h"
#include <atomic>

namespace vape
{

class VapeProcessor : public juce::AudioProcessor,
                      private juce::ValueTree::Listener,
                      private juce::AsyncUpdater
{
public:
    VapeProcessor();
    ~VapeProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VapeStation"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Modulation matrix (message thread) -------------------------------
    juce::ValueTree matrixNode() { return apvts.state.getChildWithName (idModMatrix); }
    void addModRoute (int src, int dest, float depth);
    void compileMatrixNow();

    // Reset every parameter to its default and restore the default mod
    // matrix - the state of a freshly inserted instance (message thread).
    void initPatch();

    // --- Services for voices (audio thread) -------------------------------
    const CompiledMatrix* currentMatrix() const { return matrix.load (std::memory_order_acquire); }
    std::atomic<float>* rawParam (int d) const { return raws[(size_t) d]; }
    const juce::NormalisableRange<float>& paramRange (int d) const { return ranges[(size_t) d]; }
    juce::RangedAudioParameter* param (int d) const { return params_[(size_t) d]; }
    float wheelValue() const { return wheel.load (std::memory_order_relaxed); }
    int tableIndex() const;
    int filterTypeIndex() const { return (int) filterTypeRaw->load (std::memory_order_relaxed); }
    int lfoShape (int which) const
    {
        return (int) (which == 0 ? lfo1ShapeRaw : lfo2ShapeRaw)->load (std::memory_order_relaxed);
    }
    int lfoMode (int which) const
    {
        return (int) (which == 0 ? lfo1ModeRaw : lfo2ModeRaw)->load (std::memory_order_relaxed);
    }
    // Shared (First Note / Global) LFO value for the control tick covering the
    // given sample offset into the current block. Filled at the top of
    // processBlock and read by voices inside the same call - no locking needed.
    float sharedLfoValue (int which, int sampleOffset) const
    {
        const auto& v = sharedVals[(size_t) which];
        if (v.empty())
            return 0.0f;
        return v[(size_t) juce::jlimit (0, (int) v.size() - 1, sampleOffset / controlBlockSamples)];
    }

    // --- Display feed for the editor's animated mod indicators ------------
    bool isDisplayVoice (const void* v) const { return displayVoice.load (std::memory_order_relaxed) == v; }
    void setDisplayVoice (void* v) { displayVoice.store (v, std::memory_order_relaxed); }
    void pushDisplay (const std::array<float, numDests>& norms);
    float displayNorm (int d) const { return displayNorms[(size_t) d].load (std::memory_order_relaxed); }
    juce::uint32 displayStamp() const { return dispStamp.load (std::memory_order_relaxed); }
    int matrixVersion() const { return matVersion.load (std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    void ensureMatrixNode();
    void maybeRecompile (juce::ValueTree affected);

    // ValueTree::Listener
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&) override { maybeRecompile (tree); }
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&) override { maybeRecompile (parent); }
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int) override { maybeRecompile (parent); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    // AsyncUpdater
    void handleAsyncUpdate() override { compileMatrixNow(); }

    juce::Synthesiser synth;
    std::vector<GrainVoice*> voices; // owned by synth

    std::array<juce::RangedAudioParameter*, numDests> params_ {};
    std::array<std::atomic<float>*, numDests> raws {};
    std::array<juce::NormalisableRange<float>, numDests> ranges;
    std::atomic<float>* tableRaw = nullptr;
    std::atomic<float>* filterTypeRaw = nullptr;
    std::atomic<float>* lfo1ShapeRaw = nullptr;
    std::atomic<float>* lfo2ShapeRaw = nullptr;
    std::atomic<float>* lfo1ModeRaw = nullptr;
    std::atomic<float>* lfo2ModeRaw = nullptr;

    // Shared LFOs for First Note / Global modes (audio thread only).
    std::array<Lfo, 2> sharedLfos;
    std::array<std::vector<float>, 2> sharedVals;
    int heldKeys = 0;
    juce::int64 firstNoteSeq = 0;
    double srHz = 48000.0;

    std::atomic<CompiledMatrix*> matrix { nullptr };
    // Retired snapshots are kept until destruction: edits are rare and small,
    // and this keeps the audio thread wait-free with no reclamation races.
    std::vector<std::unique_ptr<CompiledMatrix>> matrixGraveyard;
    std::atomic<int> matVersion { 0 };

    std::atomic<float> wheel { 0.0f };
    std::atomic<void*> displayVoice { nullptr };
    std::array<std::atomic<float>, numDests> displayNorms {};
    std::atomic<juce::uint32> dispStamp { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VapeProcessor)
};

} // namespace vape
