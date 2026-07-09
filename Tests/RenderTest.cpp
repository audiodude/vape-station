// Offline verification harness for VapeStation. Renders MIDI through the real
// processor and asserts on the audio. Exit code = number of failed checks.

#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/UI/PluginEditor.h"
#include <iostream>

using namespace vape;

namespace
{

constexpr double kSR = 48000.0;
constexpr int kBlock = 512;

struct Score
{
    struct Ev { double t; juce::MidiMessage m; };
    std::vector<Ev> evs;
    double dur = 1.0;

    void note (double t, int noteNum, double lenSecs, juce::uint8 vel = 100)
    {
        evs.push_back ({ t, juce::MidiMessage::noteOn (1, noteNum, vel) });
        evs.push_back ({ t + lenSecs, juce::MidiMessage::noteOff (1, noteNum) });
    }
};

juce::AudioBuffer<float> renderScore (VapeProcessor& p, const Score& score)
{
    p.prepareToPlay (kSR, kBlock);

    const int total = (int) (score.dur * kSR);
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    juce::AudioBuffer<float> tmp (2, kBlock);

    int pos = 0;
    while (pos < total)
    {
        const int n = juce::jmin (kBlock, total - pos);
        juce::AudioBuffer<float> view (tmp.getArrayOfWritePointers(), 2, 0, n);

        juce::MidiBuffer midi;
        for (const auto& ev : score.evs)
        {
            const int s = (int) (ev.t * kSR);
            if (s >= pos && s < pos + n)
                midi.addEvent (ev.m, s - pos);
        }

        p.processBlock (view, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, view, ch, 0, n);
        pos += n;
    }
    return out;
}

bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        const float* d = b.getReadPointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (d[i]))
                return false;
    }
    return true;
}

double rmsBetween (const juce::AudioBuffer<float>& b, double t0, double t1)
{
    const int i0 = juce::jlimit (0, b.getNumSamples(), (int) (t0 * kSR));
    const int i1 = juce::jlimit (0, b.getNumSamples(), (int) (t1 * kSR));
    if (i1 <= i0)
        return 0.0;
    double sum = 0.0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        const float* d = b.getReadPointer (ch);
        for (int i = i0; i < i1; ++i)
            sum += (double) d[i] * d[i];
    }
    return std::sqrt (sum / ((i1 - i0) * b.getNumChannels()));
}

float maxAbs (const juce::AudioBuffer<float>& b)
{
    float m = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        m = juce::jmax (m, b.getMagnitude (ch, 0, b.getNumSamples()));
    return m;
}

double relativeDiff (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    const int n = juce::jmin (a.getNumSamples(), b.getNumSamples());
    double diff = 0.0, ref = 0.0;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* da = a.getReadPointer (ch);
        const float* db = b.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const double d = (double) da[i] - db[i];
            diff += d * d;
            ref += (double) da[i] * da[i];
        }
    }
    return ref > 0.0 ? std::sqrt (diff / ref) : 0.0;
}

bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    if (a.getNumSamples() != b.getNumSamples())
        return false;
    for (int ch = 0; ch < 2; ++ch)
        if (memcmp (a.getReadPointer (ch), b.getReadPointer (ch),
                    sizeof (float) * (size_t) a.getNumSamples()) != 0)
            return false;
    return true;
}

void clearMatrix (VapeProcessor& p)
{
    p.matrixNode().removeAllChildren (nullptr);
    p.compileMatrixNow();
}

void setParamNatural (VapeProcessor& p, const juce::String& id, float natural)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (natural));
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File outDir = juce::File::getCurrentWorkingDirectory();
    if (argc > 1)
        outDir = juce::File (juce::String::fromUTF8 (argv[1]));
    outDir.createDirectory();

    int fails = 0;
    auto check = [&] (bool ok, const juce::String& name, const juce::String& detail = {})
    {
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name;
        if (detail.isNotEmpty())
            std::cout << "  (" << detail << ")";
        std::cout << std::endl;
        if (! ok)
            ++fails;
    };

    // T0: graintable generation
    {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto& tables = grainTables();
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        check (tables.size() == 5, "graintables built", juce::String (ms, 1) + " ms");
    }

    // T1: no notes -> true silence
    {
        VapeProcessor p;
        Score s;
        s.dur = 1.0;
        auto buf = renderScore (p, s);
        check (maxAbs (buf) < 1.0e-6f, "silence without notes");
    }

    // T2: a note produces audio, is finite, and releases to silence
    {
        VapeProcessor p;
        Score s;
        s.dur = 4.5;
        s.note (0.0, 60, 1.5);
        auto buf = renderScore (p, s);
        check (allFinite (buf), "output is finite");
        const double body = rmsBetween (buf, 0.2, 1.0);
        check (body > 0.005, "note produces audio", "rms=" + juce::String (body, 5));
        const double tail = rmsBetween (buf, 4.3, 4.5);
        check (tail < 1.0e-4, "release decays to silence", "rms=" + juce::String (tail, 7));
    }

    // T3: deterministic rendering (seeded RNGs, reproducible offline renders)
    {
        Score s;
        s.dur = 2.0;
        s.note (0.0, 55, 1.0);
        s.note (0.5, 62, 1.0);
        VapeProcessor p1, p2;
        auto b1 = renderScore (p1, s);
        auto b2 = renderScore (p2, s);
        check (identical (b1, b2), "render is deterministic");
    }

    // T4: a mod route audibly changes the output
    {
        Score s;
        s.dur = 2.5;
        s.note (0.0, 48, 2.2);

        VapeProcessor a;
        setParamNatural (a, "table", 1.0f); // Sweep: position scan = strong spectral change
        clearMatrix (a);
        auto bufA = renderScore (a, s);

        VapeProcessor b;
        setParamNatural (b, "table", 1.0f);
        setParamNatural (b, "lfo1Rate", 3.0f);
        clearMatrix (b);
        b.addModRoute (sLfo1, dPosition, 0.9f);
        b.compileMatrixNow();
        auto bufB = renderScore (b, s);

        const double d = relativeDiff (bufA, bufB);
        check (d > 0.05, "LFO->position route changes output", "relDiff=" + juce::String (d, 4));
        check (allFinite (bufB), "modulated output is finite");
    }

    // T5: state save/load round-trips params and matrix
    {
        VapeProcessor a;
        setParamNatural (a, "position", 0.7f);
        a.addModRoute (sEnv2, dCutoff, -0.5f);
        a.compileMatrixNow();

        juce::MemoryBlock mb;
        a.getStateInformation (mb);

        VapeProcessor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());

        const float posBack = a.rawParam (dPosition)->load();
        const float posB = b.rawParam (dPosition)->load();
        check (std::abs (posB - 0.7f) < 1.0e-4f && std::abs (posBack - 0.7f) < 1.0e-4f,
               "param value round-trips", "pos=" + juce::String (posB, 4));

        const auto* ma = a.currentMatrix();
        const auto* mbx = b.currentMatrix();
        bool routesMatch = ma->totalRoutes == mbx->totalRoutes && mbx->totalRoutes == 2;
        if (routesMatch)
        {
            const auto& cutRoutes = mbx->routes[dCutoff];
            routesMatch = cutRoutes.size() == 1
                       && cutRoutes[0].src == sEnv2
                       && std::abs (cutRoutes[0].depth + 0.5f) < 1.0e-4f;
        }
        check (routesMatch, "mod matrix round-trips",
               "routes=" + juce::String (mbx->totalRoutes));
    }

    // T6: parameter text is tidy (no fake precision) and coarse snaps to semitones
    {
        VapeProcessor p;
        auto* atk = p.apvts.getParameter ("env1A");
        const auto txt = atk->getText (atk->convertTo0to1 (2.7543f), 0);
        check (txt == "2.75", "param text is tidy", txt);

        setParamNatural (p, "coarse", 7.3f);
        const float coarse = p.apvts.getRawParameterValue ("coarse")->load();
        check (std::abs (coarse - 7.0f) < 1.0e-6f, "coarse snaps to semitones",
               juce::String (coarse, 3));

        // Envelope-time taper: off = 0, 9 o'clock (3/16 travel) = 250 ms,
        // 12 o'clock = 1 s, 3 o'clock (13/16) = 4 s, full = 10 s.
        auto natural = [&] (float prop) { return atk->convertFrom0to1 (prop); };
        const bool taper = natural (0.0f) == 0.0f
                        && std::abs (natural (0.1875f) - 250.0f)   < 0.1f
                        && std::abs (natural (0.5f)    - 1000.0f)  < 0.5f
                        && std::abs (natural (0.8125f) - 4000.0f)  < 2.0f
                        && std::abs (natural (1.0f)    - 10000.0f) < 5.0f;
        check (taper, "env time taper hits clock anchors");

        auto* rel = p.apvts.getParameter ("env1R");
        check (std::abs (rel->convertFrom0to1 (0.0f) - 5.0f) < 1.0e-4f,
               "release bottoms out at 5 ms",
               juce::String (rel->convertFrom0to1 (0.0f), 3));

        const bool grid = std::abs (snapKnobValue (dEnv1A, 3.29f)   - 3.5f)    < 1.0e-4f
                       && std::abs (snapKnobValue (dEnv1A, 137.3f)  - 140.0f)  < 1.0e-3f
                       && std::abs (snapKnobValue (dEnv1A, 2340.0f) - 2400.0f) < 1.0e-2f
                       && std::abs (snapKnobValue (dCutoff, 1234.0f) - 1200.0f) < 1.0e-2f
                       && std::abs (snapKnobValue (dPosition, 0.6789f) - 0.68f) < 1.0e-4f;
        check (grid, "knob step grid snaps to nice values");
    }

    // T7: demo render for listening
    {
        VapeProcessor p; // default patch, including the default LFO1->position route
        Score s;
        s.dur = 6.5;
        s.note (0.0, 36, 0.9);
        s.note (1.0, 43, 0.9);
        s.note (2.0, 48, 0.9);
        s.note (3.0, 48, 2.2);
        s.note (3.0, 55, 2.2);
        s.note (3.0, 60, 2.2);
        s.note (3.0, 63, 2.2);
        auto buf = renderScore (p, s);

        const float peak = maxAbs (buf);
        if (peak > 1.0e-9f)
            buf.applyGain (0.89f / peak);

        auto f = outDir.getChildFile ("vape-demo.wav");
        f.deleteFile();
        bool ok = false;
        juce::WavAudioFormat wav;
        if (auto os = f.createOutputStream())
        {
            if (auto* w = wav.createWriterFor (os.get(), kSR, 2, 16, {}, 0))
            {
                std::unique_ptr<juce::AudioFormatWriter> writer (w);
                os.release(); // writer owns the stream now
                ok = writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
            }
        }
        check (ok, "demo wav written", f.getFullPathName());
    }

    // T8: editor snapshot (needs a display)
    if (juce::SystemStats::getEnvironmentVariable ("DISPLAY", {}).isNotEmpty())
    {
        VapeProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (250);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());

        auto f = outDir.getChildFile ("ui-snapshot.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat png;
        const bool ok = os.openedOk() && png.writeImageToStream (img, os);
        check (ok, "editor snapshot written", f.getFullPathName());
    }
    else
    {
        std::cout << "[SKIP] editor snapshot (no DISPLAY)" << std::endl;
    }

    std::cout << (fails == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << std::endl;
    return fails;
}
