#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "ModMatrix.h"
#include "GrainTables.h"
#include "Lfo.h"
#include <array>

namespace vape
{

class VapeProcessor;

struct GrainSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

// One polyphonic voice: a granular scheduler reading windowed grains from the
// graintable, per-voice envelopes/LFOs, per-voice modulation evaluation at
// control rate (every 32 samples), then filter and amp envelope.
class GrainVoice : public juce::SynthesiserVoice
{
public:
    GrainVoice (VapeProcessor& p, int index);

    void prepare (double sampleRate, int maxBlockSize);

    bool canPlaySound (juce::SynthesiserSound* s) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*,
                    int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    static constexpr int subBlock  = 32;
    static constexpr int maxGrains = 24;

    struct Grain
    {
        bool active = false;
        double phase = 0.0;      // waveform phase 0..1 within the frame cycle
        float framePos = 0.0f;   // 0..numFrames-1, fixed at spawn
        int age = 0, dur = 1;    // samples
        float ratioRand = 1.0f;  // per-grain random pitch ratio
        float panL = 0.7f, panR = 0.7f;
        float gamma = 1.0f;      // window skew
        int mip = 0;
    };

    void updateControls (const GrainTable& tbl, const CompiledMatrix* mat, int n);
    void spawnGrain (const GrainTable& tbl);

    VapeProcessor& proc;
    int voiceIndex = 0;

    double sr = 48000.0;
    juce::ADSR env1, env2, env3;
    float env1Last = 0.0f, env2Last = 0.0f, env3Last = 0.0f;
    Lfo lfo1, lfo2;
    float lfo1Last = 0.0f, lfo2Last = 0.0f;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float> cutoffSm, gainSm;

    float velocity = 0.0f, keytrack = 0.0f, pbSemis = 0.0f;
    double baseFreq = 440.0, liveInc = 0.0, voicePhase = 0.0;
    juce::Random rng;
    int noteSeq = 0;

    std::array<Grain, maxGrains> grains;
    int spawnCountdown = 1;
    int curInterval = 1000;
    int curSizeSamps = 4000;
    float curGamma = 1.0f, grainNorm = 1.0f;
    bool releasing = false;

    std::array<float, numDests> eff {}, effNorm {};
};

} // namespace vape
