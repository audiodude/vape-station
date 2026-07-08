#include "GrainVoice.h"
#include "PluginProcessor.h"

namespace vape
{

namespace
{

float hannWin (float t)
{
    static const auto lut = []
    {
        std::array<float, 1026> a {};
        for (int i = 0; i < 1026; ++i)
        {
            const float x = juce::jmin (1.0f, (float) i / 1024.0f);
            a[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * x);
        }
        return a;
    }();

    t = juce::jlimit (0.0f, 1.0f, t);
    const float x = t * 1024.0f;
    const int i = (int) x;
    return lut[(size_t) i] + (x - (float) i) * (lut[(size_t) i + 1] - lut[(size_t) i]);
}

float readTable (const GrainTable& t, int mip, float framePos, float phase)
{
    const int f0 = (int) framePos;
    const int f1 = juce::jmin (f0 + 1, GrainTable::numFrames - 1);
    const float ff = framePos - (float) f0;

    float x = phase * (float) GrainTable::frameLen;
    int i = (int) x;
    if (i >= GrainTable::frameLen)
        i = GrainTable::frameLen - 1;
    const float fx = x - (float) i;

    const float* a = t.frame (mip, f0);
    const float* b = t.frame (mip, f1);
    const float s0 = a[i] + fx * (a[i + 1] - a[i]);
    const float s1 = b[i] + fx * (b[i + 1] - b[i]);
    return s0 + ff * (s1 - s0);
}

} // namespace

GrainVoice::GrainVoice (VapeProcessor& p, int index) : proc (p), voiceIndex (index) {}

bool GrainVoice::canPlaySound (juce::SynthesiserSound* s)
{
    return dynamic_cast<GrainSound*> (s) != nullptr;
}

void GrainVoice::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    env1.setSampleRate (sr);
    env2.setSampleRate (sr);
    env3.setSampleRate (sr);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlockSize, 2 };
    filter.prepare (spec);

    cutoffSm.reset (sr / subBlock, 0.02);
    gainSm.reset (sr, 0.005);
}

void GrainVoice::startNote (int note, float vel, juce::SynthesiserSound*, int pwPos)
{
    baseFreq = juce::MidiMessage::getMidiNoteInHertz (note);
    velocity = vel;
    keytrack = juce::jlimit (-1.0f, 1.0f, (float) (note - 60) / 36.0f);
    pbSemis  = ((float) pwPos - 8192.0f) / 8192.0f * 2.0f;

    // Deterministic per-note seeding (keeps offline renders reproducible).
    ++noteSeq;
    rng.setSeed ((juce::int64) voiceIndex * 1000003 + (juce::int64) noteSeq * 7919);
    lfo1.reset (rng.nextInt64());
    lfo2.reset (rng.nextInt64());

    env1.reset(); env2.reset(); env3.reset();
    // Seed envelope rates from unmodulated params; the first control tick
    // replaces them with modulated values.
    auto secs = [this] (int d) { return proc.rawParam (d)->load() * 0.001f; };
    env1.setParameters ({ secs (dEnv1A), secs (dEnv1D), proc.rawParam (dEnv1S)->load(), secs (dEnv1R) });
    env2.setParameters ({ secs (dEnv2A), secs (dEnv2D), proc.rawParam (dEnv2S)->load(), secs (dEnv2R) });
    env3.setParameters ({ secs (dEnv3A), secs (dEnv3D), proc.rawParam (dEnv3S)->load(), secs (dEnv3R) });
    env1.noteOn(); env2.noteOn(); env3.noteOn();
    env1Last = env2Last = env3Last = 0.0f;
    lfo1Last = lfo2Last = 0.0f;

    for (auto& g : grains)
        g.active = false;
    voicePhase = 0.0;
    spawnCountdown = 1;
    releasing = false;
    filter.reset();

    proc.setDisplayVoice (this);
}

void GrainVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        env1.noteOff(); env2.noteOff(); env3.noteOff();
        releasing = true;
    }
    else
    {
        clearCurrentNote();
        for (auto& g : grains)
            g.active = false;
    }
}

void GrainVoice::pitchWheelMoved (int v)
{
    pbSemis = ((float) v - 8192.0f) / 8192.0f * 2.0f;
}

void GrainVoice::updateControls (const GrainTable&, const CompiledMatrix* mat, int n)
{
    float srcVals[numSrcs];
    srcVals[sEnv1]     = env1Last;
    srcVals[sEnv2]     = env2Last;
    srcVals[sEnv3]     = env3Last;
    srcVals[sLfo1]     = lfo1Last;
    srcVals[sLfo2]     = lfo2Last;
    srcVals[sVelocity] = velocity;
    srcVals[sWheel]    = proc.wheelValue();
    srcVals[sKey]      = keytrack;

    for (int d = 0; d < numDests; ++d)
    {
        const auto& range = proc.paramRange (d);
        const float base = proc.rawParam (d)->load (std::memory_order_relaxed);
        float norm = range.convertTo0to1 (juce::jlimit (range.start, range.end, base));

        if (mat != nullptr && mat->any[(size_t) d])
            for (const auto& r : mat->routes[(size_t) d])
                norm += r.depth * srcVals[r.src];

        norm = juce::jlimit (0.0f, 1.0f, norm);
        effNorm[(size_t) d] = norm;
        eff[(size_t) d] = range.convertFrom0to1 (norm);
    }

    env1.setParameters ({ eff[dEnv1A] * 0.001f, eff[dEnv1D] * 0.001f, eff[dEnv1S], eff[dEnv1R] * 0.001f });
    env2.setParameters ({ eff[dEnv2A] * 0.001f, eff[dEnv2D] * 0.001f, eff[dEnv2S], eff[dEnv2R] * 0.001f });
    env3.setParameters ({ eff[dEnv3A] * 0.001f, eff[dEnv3D] * 0.001f, eff[dEnv3S], eff[dEnv3R] * 0.001f });

    const double dt = (double) n / sr;
    lfo1Last = lfo1.advance (eff[dLfo1Rate], dt, proc.lfoShape (0));
    lfo2Last = lfo2.advance (eff[dLfo2Rate], dt, proc.lfoShape (1));

    liveInc = baseFreq * std::exp2 ((double) (eff[dCoarse] + eff[dFine] * 0.01f + pbSemis) / 12.0) / sr;

    curSizeSamps = juce::jlimit (64, (int) sr, (int) (eff[dGrainSize] * 0.001 * sr));
    const float density = juce::jmax (1.0f, eff[dDensity]);
    curInterval = juce::jmax (48, (int) ((float) curSizeSamps / density));
    grainNorm = juce::jmin (1.0f, 2.0f / density);
    curGamma = std::exp2 ((eff[dShape] - 0.5f) * 3.0f);

    switch (proc.filterTypeIndex())
    {
        case 1:  filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass); break;
        case 2:  filter.setType (juce::dsp::StateVariableTPTFilterType::highpass); break;
        default: filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);  break;
    }
    cutoffSm.setTargetValue (juce::jlimit (20.0f, (float) (0.45 * sr), eff[dCutoff]));
    filter.setCutoffFrequency (cutoffSm.getNextValue());
    filter.setResonance (0.707f * std::pow (10.0f, eff[dRes] * 1.15f));

    gainSm.setTargetValue (juce::Decibels::decibelsToGain (eff[dGain]));
}

void GrainVoice::spawnGrain (const GrainTable& tbl)
{
    Grain* slot = nullptr;
    for (auto& g : grains)
        if (! g.active) { slot = &g; break; }
    if (slot == nullptr)
        return;

    auto bip = [this] { return rng.nextFloat() * 2.0f - 1.0f; };

    const float pos = juce::jlimit (0.0f, 1.0f, eff[dPosition] + eff[dSpray] * bip());
    slot->framePos = pos * (float) (GrainTable::numFrames - 1);
    slot->ratioRand = std::exp2 (bip() * eff[dPitchRand] / 12.0f);

    const float pp = juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * eff[dSpread] * bip());
    slot->panL = std::cos (pp * juce::MathConstants<float>::halfPi);
    slot->panR = std::sin (pp * juce::MathConstants<float>::halfPi);

    slot->dur = curSizeSamps;
    slot->age = 0;
    slot->phase = voicePhase; // phase-coherent with the voice: clean COLA sum at spray=0
    slot->gamma = curGamma;
    slot->mip = tbl.mipForFreq (liveInc * sr * slot->ratioRand, sr);
    slot->active = true;
}

void GrainVoice::renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const auto& tbl = grainTables()[(size_t) proc.tableIndex()];
    const CompiledMatrix* mat = proc.currentMatrix();

    while (numSamples > 0)
    {
        const int n = juce::jmin (subBlock, numSamples);
        updateControls (tbl, mat, n);

        float* L = out.getWritePointer (0, startSample);
        float* R = out.getNumChannels() > 1 ? out.getWritePointer (1, startSample) : nullptr;

        for (int s = 0; s < n; ++s)
        {
            if (--spawnCountdown <= 0)
            {
                spawnGrain (tbl);
                spawnCountdown = curInterval;
            }

            float l = 0.0f, r = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active)
                    continue;
                const float t = (float) g.age / (float) g.dur;
                const float w = hannWin (std::pow (t, g.gamma));
                const float v = readTable (tbl, g.mip, g.framePos, (float) g.phase) * w;
                l += v * g.panL;
                r += v * g.panR;

                g.phase += liveInc * g.ratioRand;
                if (g.phase >= 1.0)
                    g.phase -= std::floor (g.phase);
                if (++g.age >= g.dur)
                    g.active = false;
            }

            voicePhase += liveInc;
            if (voicePhase >= 1.0)
                voicePhase -= std::floor (voicePhase);

            env1Last = env1.getNextSample();
            env2Last = env2.getNextSample();
            env3Last = env3.getNextSample();

            const float amp = env1Last * gainSm.getNextValue() * grainNorm;
            l *= amp;
            r *= amp;
            l = filter.processSample (0, l);
            if (R != nullptr)
                r = filter.processSample (1, r);

            L[s] += l;
            if (R != nullptr)
                R[s] += r;
        }

        if (proc.isDisplayVoice (this))
            proc.pushDisplay (effNorm);

        startSample += n;
        numSamples -= n;

        if (releasing && ! env1.isActive())
        {
            clearCurrentNote();
            for (auto& g : grains)
                g.active = false;
            break;
        }
    }
}

} // namespace vape
