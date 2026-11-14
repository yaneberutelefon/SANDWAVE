#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

/*
    SandEngine - the whole SANDWAVE DSP in one header.

    Signal flow (per channel, everything is *added* to the dry signal):

        in ──┬────────────────────────────────────────────────────────┐
             │                                                        │
             ├─► Mid shelf (4.2k) ─► High shelf (11.5k) ─► (minus in) ─┤  AIR      (airDelta)
             │                                                        │
             ├─► HP 5.2k ─► 2x OS soft-sat (even+odd) ─► HP 5.2k ──────┤  EXCITE   (harmonic air)
             │                                                        │
             ├─► HP 6k ─► envelope ─► gates grains of band-passed noise ┤  SAND     (falling grains)
             │                                                        │
             └─► BP 7.2k ─► envelope ─► gain reduction on all of the ───┘  DE-HARSH (sibilance guard)
                                         extras above

        out = (in + extras * mix * guard) * outputGain      (power = smooth bypass)

    Tuning constants live at the top of the class (k...).
*/
class SandEngine
{
public:
    struct Params
    {
        float airMid   = 0.40f;   // 0..1
        float airHigh  = 0.50f;   // 0..1
        float sand     = 0.35f;   // 0..1
        float grain    = 0.50f;   // 0..1  grain density
        float tone     = 9000.f;  // Hz    colour of the sand
        float width    = 0.55f;   // 0..1  stereo spread of the extras
        float excite   = 0.30f;   // 0..1
        float deharsh  = 0.40f;   // 0..1
        float mix      = 1.00f;   // 0..1
        float outputDb = 0.f;     // dB
        bool  power    = true;
    };

    // ------------------------------------------------------------------ tuning
    static constexpr float kMidShelfMaxDb   = 9.0f;
    static constexpr float kHighShelfMaxDb  = 12.0f;
    static constexpr float kExciteMaxGain   = 0.9f;
    static constexpr float kSandGain        = 0.9f;
    static constexpr float kGrainMinPerSec  = 300.f;
    static constexpr float kGrainMaxPerSec  = 7300.f;

    // ------------------------------------------------------------------ API
    void prepare (double sr, int maxBlock, int /*channels*/)
    {
        sampleRate   = sr;
        maxBlockSize = juce::jmax (maxBlock, 64);

        airBuf.setSize (2, maxBlockSize);
        excBuf.setSize (2, maxBlockSize);

        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampling->initProcessing ((size_t) maxBlockSize);

        const auto exHP  = Biquad::highPass (sr, 5200.0, 0.707);
        const auto sibBp = Biquad::bandPass (sr, 7200.0, 1.1);
        const auto detHp = Biquad::highPass (sr, 6000.0, 0.707);
        for (int ch = 0; ch < 2; ++ch)
        {
            exPre[ch].copyCoeffs (exHP);
            exPost[ch].copyCoeffs (exHP);
            sibBP[ch].copyCoeffs (sibBp);
            detHP[ch].copyCoeffs (detHp);
        }

        grainDecay = (float) std::exp (-1.0 / (sr * 0.0006));
        envAtt     = 1.f - (float) std::exp (-1.0 / (sr * 0.001));
        envRel     = 1.f - (float) std::exp (-1.0 / (sr * 0.08));
        sibAtt     = 1.f - (float) std::exp (-1.0 / (sr * 0.0005));
        sibRel     = 1.f - (float) std::exp (-1.0 / (sr * 0.04));

        for (auto* s : { &airMidDbSm, &airHighDbSm, &toneSm, &excSm, &sandSm,
                         &widthSm, &scaleSm, &dehSm, &outSm, &powerSm })
            s->reset (sr, 0.03);

        reset();
        snap = true;
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            midShelf[ch].clear(); highShelf[ch].clear(); exPre[ch].clear(); exPost[ch].clear();
            sibBP[ch].clear(); detHP[ch].clear(); sandBP[ch].clear();
            sibEnv[ch] = detEnv[ch] = grainEnv[ch] = 0.f;
        }
        gr = 1.f;
        rngShared.s = 0x12345678u; rng[0].s = 0x9E3779B9u; rng[1].s = 0x7F4A7C15u;
        if (oversampling != nullptr) oversampling->reset();
        lastMidDb = lastHighDb = lastTone = -1000.f;
    }

    void setParams (const Params& p) { params = p; }

    float getHighEnergy() const noexcept { return hiLast; }
    float getSandEnergy() const noexcept { return sandLast; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int total = buffer.getNumSamples();
        const int nch   = juce::jmin (buffer.getNumChannels(), 2);
        if (total <= 0 || nch <= 0) return;

        const auto& p = params;
        const float sandT = kSandGain * std::pow (p.sand, 1.5f);

        if (snap)
        {
            airMidDbSm.setCurrentAndTargetValue (p.airMid  * kMidShelfMaxDb);
            airHighDbSm.setCurrentAndTargetValue (p.airHigh * kHighShelfMaxDb);
            toneSm.setCurrentAndTargetValue (p.tone);
            excSm.setCurrentAndTargetValue (p.excite * kExciteMaxGain);
            sandSm.setCurrentAndTargetValue (sandT);
            widthSm.setCurrentAndTargetValue (p.width);
            scaleSm.setCurrentAndTargetValue (p.mix * (p.power ? 1.f : 0.f));
            dehSm.setCurrentAndTargetValue (p.deharsh);
            outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.outputDb));
            powerSm.setCurrentAndTargetValue (p.power ? 1.f : 0.f);
            snap = false;
        }
        else
        {
            airMidDbSm.setTargetValue (p.airMid  * kMidShelfMaxDb);
            airHighDbSm.setTargetValue (p.airHigh * kHighShelfMaxDb);
            toneSm.setTargetValue (p.tone);
            excSm.setTargetValue (p.excite * kExciteMaxGain);
            sandSm.setTargetValue (sandT);
            widthSm.setTargetValue (p.width);
            scaleSm.setTargetValue (p.mix * (p.power ? 1.f : 0.f));
            dehSm.setTargetValue (p.deharsh);
            outSm.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb));
            powerSm.setTargetValue (p.power ? 1.f : 0.f);
        }

        hiMax = 0.f; sandPeak = 0.f;

        float* const* io = buffer.getArrayOfWritePointers();
        for (int start = 0; start < total; start += maxBlockSize)
            processChunk (io, nch, start, juce::jmin (maxBlockSize, total - start));

        hiLast   = juce::jmin (1.f, hiMax * 12.f);
        sandLast = juce::jmin (1.f, sandPeak * 5.f);
    }

private:
    // ------------------------------------------------------------------ helpers
    struct Biquad
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f, z1 = 0.f, z2 = 0.f;

        inline float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() noexcept { z1 = z2 = 0.f; }
        void copyCoeffs (const Biquad& o) noexcept { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }

        static Biquad make (double b0, double b1, double b2, double a0, double a1, double a2)
        {
            Biquad q;
            q.b0 = (float) (b0 / a0); q.b1 = (float) (b1 / a0); q.b2 = (float) (b2 / a0);
            q.a1 = (float) (a1 / a0); q.a2 = (float) (a2 / a0);
            return q;
        }
        static double clampFreq (double sr, double f) { return juce::jlimit (20.0, sr * 0.45, f); }

        static Biquad highShelf (double sr, double f, double q, double gainDb)
        {
            f = clampFreq (sr, f);
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w0 = juce::MathConstants<double>::twoPi * f / sr;
            const double cw = std::cos (w0), sn = std::sin (w0);
            const double alpha = sn / (2.0 * q);
            const double beta = 2.0 * std::sqrt (A) * alpha;
            return make (A * ((A + 1) + (A - 1) * cw + beta),
                         -2.0 * A * ((A - 1) + (A + 1) * cw),
                         A * ((A + 1) + (A - 1) * cw - beta),
                         (A + 1) - (A - 1) * cw + beta,
                         2.0 * ((A - 1) - (A + 1) * cw),
                         (A + 1) - (A - 1) * cw - beta);
        }
        static Biquad highPass (double sr, double f, double q)
        {
            f = clampFreq (sr, f);
            const double w0 = juce::MathConstants<double>::twoPi * f / sr;
            const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
            return make ((1 + cw) * 0.5, -(1 + cw), (1 + cw) * 0.5, 1 + alpha, -2.0 * cw, 1 - alpha);
        }
        static Biquad bandPass (double sr, double f, double q)
        {
            f = clampFreq (sr, f);
            const double w0 = juce::MathConstants<double>::twoPi * f / sr;
            const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
            return make (alpha, 0.0, -alpha, 1 + alpha, -2.0 * cw, 1 - alpha);
        }
    };

    struct Rng
    {
        uint32_t s = 2463534242u;
        inline float next() noexcept   // -1..1
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float) (int32_t) s * 4.6566129e-10f;
        }
        inline float next01() noexcept { return next() * 0.5f + 0.5f; }
    };

    static inline float fastTanh (float x) noexcept
    {
        x = juce::jlimit (-3.f, 3.f, x);
        const float x2 = x * x;
        return x * (27.f + x2) / (27.f + 9.f * x2);
    }

    void updateShelves (float midDb, float highDb)
    {
        if (std::abs (midDb - lastMidDb) < 1e-3f && std::abs (highDb - lastHighDb) < 1e-3f) return;
        lastMidDb = midDb; lastHighDb = highDb;
        const auto m = Biquad::highShelf (sampleRate, 4200.0, 0.6, midDb);
        const auto h = Biquad::highShelf (sampleRate, 11500.0, 0.55, highDb);
        for (int ch = 0; ch < 2; ++ch) { midShelf[ch].copyCoeffs (m); highShelf[ch].copyCoeffs (h); }
    }

    void updateTone (float f)
    {
        if (std::abs (f - lastTone) < 1.f) return;
        lastTone = f;
        const auto c = Biquad::bandPass (sampleRate, f, 0.9);
        for (int ch = 0; ch < 2; ++ch) sandBP[ch].copyCoeffs (c);
    }

    // ------------------------------------------------------------------ core
    void processChunk (float* const* io, int nch, int start, int n)
    {
        // ---- A: air shelves (as a delta) + exciter pre-filter
        for (int pos = 0; pos < n;)
        {
            const int len = juce::jmin (32, n - pos);
            updateShelves (airMidDbSm.skip (len), airHighDbSm.skip (len));

            for (int ch = 0; ch < nch; ++ch)
            {
                const float* in = io[ch] + start + pos;
                float* ad = airBuf.getWritePointer (ch) + pos;
                float* bd = excBuf.getWritePointer (ch) + pos;
                for (int i = 0; i < len; ++i)
                {
                    const float x = in[i];
                    float a = midShelf[ch].process (x);
                    a = highShelf[ch].process (a);
                    ad[i] = a - x;
                    bd[i] = exPre[ch].process (x);
                }
            }
            pos += len;
        }

        // ---- B: 2x oversampled harmonic exciter on the high band
        {
            juce::dsp::AudioBlock<float> exBlock (excBuf.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
            auto up = oversampling->processSamplesUp (exBlock);

            const float g  = 6.f + 30.f * params.excite;
            const float ig = 1.f / g;
            const size_t upN = up.getNumSamples();
            for (int ch = 0; ch < nch; ++ch)
            {
                float* d = up.getChannelPointer ((size_t) ch);
                for (size_t i = 0; i < upN; ++i)
                {
                    const float u = d[i] * g;
                    d[i] = fastTanh (u + 0.25f * u * std::abs (u)) * ig;   // odd + even harmonics
                }
            }
            oversampling->processSamplesDown (exBlock);
        }

        // ---- C: sand, de-harsh, final sum
        const float grainProb = (kGrainMinPerSec + (kGrainMaxPerSec - kGrainMinPerSec)
                                  * std::pow (params.grain, 1.4f)) / (float) sampleRate;

        const float* airP[2] = { airBuf.getReadPointer (0), airBuf.getReadPointer (nch > 1 ? 1 : 0) };
        const float* excP[2] = { excBuf.getReadPointer (0), excBuf.getReadPointer (nch > 1 ? 1 : 0) };

        for (int i = 0; i < n; ++i)
        {
            if ((i & 31) == 0) updateTone (toneSm.skip (32));

            const float exG   = excSm.getNextValue();
            const float sandG = sandSm.getNextValue();
            const float wd    = widthSm.getNextValue();
            const float scale = scaleSm.getNextValue();
            const float deh   = dehSm.getNextValue();
            const float outG  = outSm.getNextValue();
            const float pw    = powerSm.getNextValue();

            // detectors
            float sib = 0.f, det = 0.f;
            for (int ch = 0; ch < nch; ++ch)
            {
                const float x = io[ch][start + i];
                const float s = std::abs (sibBP[ch].process (x));
                sibEnv[ch] += (s > sibEnv[ch] ? sibAtt : sibRel) * (s - sibEnv[ch]);
                const float d = std::abs (detHP[ch].process (x));
                detEnv[ch] += (d > detEnv[ch] ? envAtt : envRel) * (d - detEnv[ch]);
                sib = juce::jmax (sib, sibEnv[ch]);
                det = juce::jmax (det, detEnv[ch]);
            }
            hiMax = juce::jmax (hiMax, det);

            // sibilance guard
            const float thr = 0.5f * std::exp (-2.5257f * deh);
            float grT = 1.f;
            if (sib > thr) grT = juce::jmax (0.2f, std::pow (thr / sib, 0.7f));
            gr += (grT - gr) * (grT < gr ? 0.05f : 0.0015f);

            const float envShaped = juce::jmin (1.2f, std::sqrt (det * 10.f));

            // sand grains
            const float shared      = rngShared.next();
            const bool  sharedTrig  = rngShared.next01() < grainProb;
            const float sharedAmp   = 0.35f + 0.65f * rngShared.next01();

            float ex[2] = { 0.f, 0.f };
            for (int ch = 0; ch < nch; ++ch)
            {
                auto& r = rng[ch];
                const float indep = r.next();
                const float noise = shared * (1.f - wd) + indep * wd;

                const bool ownTrig = r.next01() < grainProb;
                const bool useOwn  = r.next01() < wd;
                if (useOwn ? ownTrig : sharedTrig)
                    grainEnv[ch] = juce::jmax (grainEnv[ch], useOwn ? 0.35f + 0.65f * r.next01() : sharedAmp);
                grainEnv[ch] *= grainDecay;

                const float sandSig = sandBP[ch].process (noise) * grainEnv[ch] * envShaped * sandG;
                sandPeak = juce::jmax (sandPeak, std::abs (sandSig));

                ex[ch] = airP[ch][i] + exPost[ch].process (excP[ch][i]) * exG + sandSig;
            }

            if (nch == 2)   // M/S width on everything we add
            {
                const float mid  = (ex[0] + ex[1]) * 0.5f;
                const float side = (ex[0] - ex[1]) * 0.5f * (0.5f + wd);
                ex[0] = mid + side;
                ex[1] = mid - side;
            }

            const float outGain = 1.f + (outG - 1.f) * pw;
            for (int ch = 0; ch < nch; ++ch)
            {
                float& s = io[ch][start + i];
                s = (s + ex[ch] * scale * gr) * outGain;
            }
        }
    }

    // ------------------------------------------------------------------ state
    using Smoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    Params params;
    double sampleRate = 44100.0;
    int    maxBlockSize = 512;
    bool   snap = true;

    juce::AudioBuffer<float> airBuf, excBuf;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    std::array<Biquad, 2> midShelf, highShelf, exPre, exPost, sibBP, detHP, sandBP;
    std::array<float, 2>  sibEnv {}, detEnv {}, grainEnv {};
    std::array<Rng, 2>    rng;
    Rng                   rngShared;

    Smoother airMidDbSm, airHighDbSm, toneSm, excSm, sandSm, widthSm, scaleSm, dehSm, outSm, powerSm;

    float grainDecay = 0.f, envAtt = 0.f, envRel = 0.f, sibAtt = 0.f, sibRel = 0.f;
    float gr = 1.f;
    float lastMidDb = -1000.f, lastHighDb = -1000.f, lastTone = -1000.f;
    float hiMax = 0.f, sandPeak = 0.f, hiLast = 0.f, sandLast = 0.f;
};
