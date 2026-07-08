#include "GrainTables.h"
#include <juce_dsp/juce_dsp.h>
#include <complex>

namespace vape
{

namespace
{

constexpr int kMaxHarm = 64;
using Spectrum = std::array<std::complex<float>, kMaxHarm + 1>; // index 1..64

float lerp (float a, float b, float t) { return a + t * (b - a); }

// x = frame position 0..1, returns harmonic spectrum for one frame.

Spectrum specMorph (float x)
{
    // saw -> square -> triangle
    Spectrum s {};
    for (int n = 1; n <= kMaxHarm; ++n)
    {
        const bool odd = (n % 2) == 1;
        const float saw = 1.0f / (float) n;
        const float sqr = odd ? 1.0f / (float) n : 0.0f;
        const float triSign = (((n - 1) / 2) % 2 == 0) ? 1.0f : -1.0f;
        const float tri = odd ? triSign / (float) (n * n) : 0.0f;

        float a = x < 0.5f ? lerp (saw, sqr, x * 2.0f)
                           : lerp (sqr, tri * 8.0f, (x - 0.5f) * 2.0f);
        s[(size_t) n] = { a, 0.0f };
    }
    return s;
}

Spectrum specSweep (float x)
{
    // A resonant band sweeping up the harmonic series over the fundamental.
    Spectrum s {};
    const float centre = 1.0f + 44.0f * x;
    for (int n = 1; n <= kMaxHarm; ++n)
    {
        const float d = ((float) n - centre) / 3.0f;
        const float a = std::exp (-0.5f * d * d) + 0.12f / (float) n;
        s[(size_t) n] = { a, 0.0f };
    }
    return s;
}

Spectrum specVox (float x)
{
    // Vowel formants A-E-I-O-U morphed across the table, assumed f0 = 110 Hz.
    static constexpr float formants[5][3] = {
        { 730.0f, 1090.0f, 2440.0f },  // A
        { 530.0f, 1840.0f, 2480.0f },  // E
        { 270.0f, 2290.0f, 3010.0f },  // I
        { 570.0f,  840.0f, 2410.0f },  // O
        { 300.0f,  870.0f, 2240.0f },  // U
    };
    static constexpr float weights[3] = { 1.0f, 0.6f, 0.35f };

    const float v = x * 4.0f;
    const int v0 = juce::jlimit (0, 4, (int) v);
    const int v1 = juce::jmin (4, v0 + 1);
    const float vf = v - (float) v0;

    Spectrum s {};
    for (int n = 1; n <= kMaxHarm; ++n)
    {
        const float freq = 110.0f * (float) n;
        float a0 = 0.0f, a1 = 0.0f;
        for (int k = 0; k < 3; ++k)
        {
            {
                const float bw = 0.10f * formants[v0][k] + 40.0f;
                const float d = (freq - formants[v0][k]) / bw;
                a0 += weights[k] * std::exp (-0.5f * d * d);
            }
            {
                const float bw = 0.10f * formants[v1][k] + 40.0f;
                const float d = (freq - formants[v1][k]) / bw;
                a1 += weights[k] * std::exp (-0.5f * d * d);
            }
        }
        const float a = lerp (a0, a1, vf) + 0.04f / (float) n;
        s[(size_t) n] = { a, 0.0f };
    }
    return s;
}

Spectrum specBells (float x)
{
    // Sparse bell-ish partial set; x sweeps strike (bright) -> hum (dark).
    static constexpr int   partials[6] = { 1, 3, 5, 9, 13, 19 };
    static constexpr float weights[6]  = { 1.0f, 0.8f, 0.65f, 0.5f, 0.4f, 0.3f };

    Spectrum s {};
    for (int k = 0; k < 6; ++k)
    {
        const float a = weights[k] * std::exp (-(float) k * 3.5f * x);
        const int p = partials[k];
        s[(size_t) p] += std::complex<float> (a, 0.0f);
        if (p + 1 <= kMaxHarm)
            s[(size_t) (p + 1)] += std::complex<float> (a * 0.3f * x, 0.0f); // shimmer
    }
    return s;
}

Spectrum specGrit (float x)
{
    // Two fixed random spectra crossfaded across the table; random (fixed)
    // phases make it noisy/textural rather than buzzy.
    static const auto data = []
    {
        juce::Random r (0xBEEF);
        std::array<std::array<float, kMaxHarm + 1>, 2> amps {};
        std::array<float, kMaxHarm + 1> phases {};
        for (int n = 1; n <= kMaxHarm; ++n)
        {
            for (int i = 0; i < 2; ++i)
            {
                const float u = r.nextFloat();
                amps[(size_t) i][(size_t) n] = std::pow (u, 2.5f) * std::exp (-(float) n / 20.0f);
            }
            phases[(size_t) n] = r.nextFloat() * juce::MathConstants<float>::twoPi;
        }
        return std::make_pair (amps, phases);
    }();

    Spectrum s {};
    for (int n = 1; n <= kMaxHarm; ++n)
    {
        const float a = lerp (data.first[0][(size_t) n], data.first[1][(size_t) n], x);
        const float ph = data.second[(size_t) n];
        s[(size_t) n] = std::polar (a, ph);
    }
    return s;
}

void buildTable (GrainTable& t, Spectrum (*gen) (float))
{
    juce::dsp::FFT fft (11); // 2048
    const int N = GrainTable::frameLen;

    std::vector<std::complex<float>> in ((size_t) N), out ((size_t) N);

    for (auto& mip : t.mips)
        mip.assign ((size_t) GrainTable::numFrames * (N + 1), 0.0f);

    for (int f = 0; f < GrainTable::numFrames; ++f)
    {
        const float x = (float) f / (float) (GrainTable::numFrames - 1);
        const Spectrum spec = gen (x);

        for (int m = 0; m < GrainTable::numMips; ++m)
        {
            const int cap = GrainTable::mipCaps[m];
            std::fill (in.begin(), in.end(), std::complex<float> ());
            for (int n = 1; n <= cap; ++n)
            {
                // Rotate by -i so real amplitudes synthesise as sine series
                // (proper saw/square/triangle waveshapes, lower crest factor).
                const auto c = spec[(size_t) n] * std::complex<float> (0.0f, -0.5f);
                in[(size_t) n] = c;
                in[(size_t) (N - n)] = std::conj (c);
            }
            fft.perform (in.data(), out.data(), true);

            float* dst = t.mips[(size_t) m].data() + (size_t) f * (N + 1);
            double sumSq = 0.0;
            for (int i = 0; i < N; ++i)
            {
                dst[i] = out[(size_t) i].real();
                sumSq += (double) dst[i] * dst[i];
            }
            const double rms = std::sqrt (sumSq / N);
            if (rms > 1.0e-9)
            {
                const float scale = (float) (0.12 / rms);
                for (int i = 0; i < N; ++i)
                    dst[i] *= scale;
            }
            dst[N] = dst[0]; // wrap guard
        }
    }
}

} // namespace

const std::vector<GrainTable>& grainTables()
{
    static const std::vector<GrainTable> tables = []
    {
        std::vector<GrainTable> v (5);
        v[0].name = "Morph"; buildTable (v[0], specMorph);
        v[1].name = "Sweep"; buildTable (v[1], specSweep);
        v[2].name = "Vox";   buildTable (v[2], specVox);
        v[3].name = "Bells"; buildTable (v[3], specBells);
        v[4].name = "Grit";  buildTable (v[4], specGrit);
        return v;
    }();
    return tables;
}

} // namespace vape
