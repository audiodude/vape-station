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

Spectrum specPulse (float x)
{
    // Pulse-width sweep. A rectangular pulse is a saw minus the same saw
    // phase-shifted by the duty cycle, which gives both a sine and a cosine
    // term per harmonic. 50% (square) at the left of the table, 4% at the right.
    const double phi = juce::MathConstants<double>::twoPi * (0.5 - 0.46 * (double) x);

    Spectrum s {};
    for (int n = 1; n <= kMaxHarm; ++n)
    {
        const double sinPart = (1.0 - std::cos (phi * n)) / n;
        const double cosPart = std::sin (phi * n) / n;
        s[(size_t) n] = { (float) sinPart, (float) cosPart };
    }
    return s;
}

// Bessel function of the first kind, integer order, by the ascending series.
// Build-time only and z stays under 10, so 40 terms is far past convergence.
double besselJ (int k, double z)
{
    double term = 1.0;                       // becomes (z/2)^k / k!
    for (int j = 1; j <= k; ++j)
        term *= 0.5 * z / (double) j;

    double sum = term;
    const double quarterZ2 = 0.25 * z * z;
    for (int m = 1; m <= 40; ++m)
    {
        term *= -quarterZ2 / ((double) m * (double) (m + k));
        sum += term;
    }
    return sum;
}

Spectrum specFm (float x)
{
    // Two-operator FM at a 1:1 ratio, modulation index sweeping 0 -> 9:
    // sin(t + I sin t) = sum_k J_k(I) sin((1+k)t). Sidebands that land below
    // DC fold back onto harmonic n with a sign flip.
    const double index = 9.0 * (double) x;

    Spectrum s {};
    for (int n = 1; n <= kMaxHarm; ++n)
    {
        const double fold = (n % 2 == 0) ? 1.0 : -1.0;
        s[(size_t) n] = { (float) (besselJ (n - 1, index) + fold * besselJ (n + 1, index)), 0.0f };
    }
    return s;
}

Spectrum specOrgan (float x)
{
    // Drawbar registration: partials pull in one at a time as x rises, from a
    // bare sine to a full stack. The 3rd and 5th are what make it read "organ".
    static constexpr int   drawbars[7] = { 1, 2, 3, 4, 5, 6, 8 };
    static constexpr float levels[7]   = { 1.0f, 0.7f, 0.55f, 0.5f, 0.4f, 0.36f, 0.3f };

    Spectrum s {};
    for (int k = 0; k < 7; ++k)
    {
        const float pull = juce::jlimit (0.0f, 1.0f, x * 6.5f - (float) k + 1.0f);
        s[(size_t) drawbars[k]] += std::complex<float> (levels[k] * pull, 0.0f);
    }
    return s;
}

Spectrum specFold (float x)
{
    // Wavefolder: a sine driven into a triangle folder, drive 1 -> 8. The
    // folded shape has no closed-form spectrum, so analyse one cycle directly.
    // fold() is odd and half-wave symmetric, so only odd harmonics survive.
    constexpr int N = GrainTable::frameLen;

    static const auto sinTab = []
    {
        std::array<double, N> t {};
        for (int i = 0; i < N; ++i)
            t[(size_t) i] = std::sin (juce::MathConstants<double>::twoPi * i / N);
        return t;
    }();

    const double drive = 1.0 + 7.0 * (double) x;

    std::array<double, N> wave {};
    for (int i = 0; i < N; ++i)
    {
        const double driven = drive * sinTab[(size_t) i] * juce::MathConstants<double>::halfPi;
        wave[(size_t) i] = std::asin (std::sin (driven)) / juce::MathConstants<double>::halfPi;
    }

    Spectrum s {};
    for (int n = 1; n <= kMaxHarm; ++n)
    {
        double acc = 0.0;
        for (int i = 0; i < N; ++i)
            acc += wave[(size_t) i] * sinTab[(size_t) ((n * i) % N)];
        s[(size_t) n] = { (float) (2.0 * acc / N), 0.0f };
    }
    return s;
}

Spectrum specMouth (float x)
{
    // A talkbox glide where Vox is a vowel hop. Two differences: the formant
    // *frequencies* are interpolated (peaks slide continuously instead of one
    // vowel's spectrum crossfading into the next), and they run in acoustic
    // order oo-oh-ah-eh-ee. Narrower bands than Vox plus a nasal notch that
    // opens up as the mouth does. Assumed f0 = 110 Hz.
    static constexpr float f1[5] = {  300.0f,  570.0f,  730.0f,  530.0f,  270.0f };
    static constexpr float f2[5] = {  870.0f,  840.0f, 1090.0f, 1840.0f, 2290.0f };
    static constexpr float f3[5] = { 2240.0f, 2410.0f, 2440.0f, 2480.0f, 3010.0f };
    static constexpr float weights[4] = { 1.0f, 0.8f, 0.5f, 0.22f };

    const float v = x * 4.0f;
    const int v0 = juce::jlimit (0, 4, (int) v);
    const int v1 = juce::jmin (4, v0 + 1);
    const float vf = v - (float) v0;

    const float centres[4] = { lerp (f1[(size_t) v0], f1[(size_t) v1], vf),
                               lerp (f2[(size_t) v0], f2[(size_t) v1], vf),
                               lerp (f3[(size_t) v0], f3[(size_t) v1], vf),
                               3300.0f }; // fixed singer's formant for presence
    const float nasal = juce::jmax (0.0f, 1.0f - 2.0f * x);

    Spectrum s {};
    for (int n = 1; n <= kMaxHarm; ++n)
    {
        const float freq = 110.0f * (float) n;
        float a = 0.0f;
        for (int k = 0; k < 4; ++k)
        {
            const float bw = 0.055f * centres[k] + 30.0f;
            const float d = (freq - centres[k]) / bw;
            a += weights[k] * std::exp (-0.5f * d * d);
        }
        const float dz = (freq - 1000.0f) / 300.0f;
        a *= 1.0f - 0.8f * nasal * std::exp (-0.5f * dz * dz); // nasal anti-formant
        s[(size_t) n] = { a + 0.03f / (float) n, 0.0f };
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
        // Append-only: the table parameter stores an index, so reordering
        // this list would repoint every saved patch at a different table.
        std::vector<GrainTable> v (10);
        v[0].name = "Morph"; buildTable (v[0], specMorph);
        v[1].name = "Sweep"; buildTable (v[1], specSweep);
        v[2].name = "Vox";   buildTable (v[2], specVox);
        v[3].name = "Bells"; buildTable (v[3], specBells);
        v[4].name = "Grit";  buildTable (v[4], specGrit);
        v[5].name = "Pulse"; buildTable (v[5], specPulse);
        v[6].name = "FM";    buildTable (v[6], specFm);
        v[7].name = "Fold";  buildTable (v[7], specFold);
        v[8].name = "Organ"; buildTable (v[8], specOrgan);
        v[9].name = "Mouth"; buildTable (v[9], specMouth);
        return v;
    }();
    return tables;
}

} // namespace vape
