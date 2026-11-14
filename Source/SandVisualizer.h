#pragma once
#include <JuceHeader.h>
#include "Style.h"
#include "PluginProcessor.h"

// The "dune window": night-sky aurora, twinkling air sparkles and real falling sand
// that reacts to the high-band energy and to the SAND / AIR parameters.
class SandVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit SandVisualizer (SandwaveProcessor& p) : proc (p)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);

        pSand    = proc.apvts.getRawParameterValue (ids::sand);
        pAirMid  = proc.apvts.getRawParameterValue (ids::airMid);
        pAirHigh = proc.apvts.getRawParameterValue (ids::airHigh);

        particles.resize (maxParticles);
        for (auto& pt : particles) pt.active = false;

        stars.resize (80);
        for (auto& s : stars)
        {
            s.x = rng.nextFloat(); s.y = rng.nextFloat();
            s.size = 1.f + rng.nextFloat() * 1.6f;
            s.phase = rng.nextFloat() * 6.28f;
            s.speed = rng.nextFloat() * 1.5f;
            s.big = rng.nextInt (6) == 0;
        }
        startTimerHz (50);
    }

    ~SandVisualizer() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        using namespace sw;
        const auto b = getLocalBounds().toFloat();
        const float W = b.getWidth(), H = b.getHeight();
        if (W < 10.f) return;

        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (b, 26.f);
            g.reduceClipRegion (clip, juce::AffineTransform());

            // sky
            juce::ColourGradient bg (col::night, 0.f, 0.f,
                                     col::indigo.interpolatedWith (col::violet, 0.30f), 0.f, H, false);
            bg.addColour (0.55, col::indigo);
            g.setGradientFill (bg);
            g.fillRect (b);

            // aurora
            const juce::Colour aur[3] = { col::neonCyan, col::violet, col::neonPink };
            for (int k = 0; k < 3; ++k)
            {
                const float cx = W * (0.18f + 0.32f * (float) k) + std::sin (time * 0.35f + (float) k * 2.1f) * W * 0.06f;
                const float cy = H * (0.30f + 0.08f * std::cos (time * 0.4f + (float) k * 1.3f));
                const float r  = W * 0.30f;
                const float a  = 0.16f + 0.30f * hi + 0.10f * air;
                g.setGradientFill (juce::ColourGradient (aur[k].withAlpha (a), cx, cy,
                                                         aur[k].withAlpha (0.f), cx + r, cy, true));
                g.fillRect (b);
            }

            // sparkles
            for (const auto& st : stars)
            {
                const float tw = 0.5f + 0.5f * std::sin (time * (1.5f + st.speed) + st.phase);
                const float a = juce::jmin (1.f, (0.10f + 0.60f * tw) * (0.35f + 0.9f * air));
                const float x = st.x * W, y = st.y * H * 0.7f;
                g.setColour (col::snow.withAlpha (a));
                g.fillEllipse (x, y, st.size, st.size);
                if (st.big && tw > 0.6f)
                {
                    const float L = 5.f * tw * (0.5f + air);
                    const float mx = x + st.size * 0.5f, my = y + st.size * 0.5f;
                    g.drawLine (mx - L, my, mx + L, my, 1.f);
                    g.drawLine (mx, my - L, mx, my + L, 1.f);
                }
            }

            // dunes (back, mid) -> sand -> front dune
            drawDune (g, b, layerAt (0, H), col::violet.withAlpha (0.55f), col::indigo.withAlpha (0.95f), 0.f);
            drawDune (g, b, layerAt (1, H), col::sky.withAlpha (0.50f), juce::Colour (0xff1B2F8F).withAlpha (0.95f), 0.25f);

            for (const auto& p : particles)
            {
                if (! p.active) continue;
                const juce::Colour c = p.kind == 0 ? col::snow
                                     : p.kind == 1 ? col::neonCyan
                                     : p.kind == 2 ? col::violet.brighter (0.3f) : col::neonPink;
                g.setColour (c.withAlpha (p.alpha));
                if (p.size < 1.5f) g.fillRect (p.x, p.y, p.size, p.size);
                else               g.fillEllipse (p.x, p.y, p.size, p.size);
            }

            drawDune (g, b, layerAt (2, H), col::mist.withAlpha (0.95f), col::ice.withAlpha (0.98f), 0.85f);

            // title
            g.setFont (font (juce::jmax (18.f, H * 0.16f), true, 0.45f));
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (col::neonCyan.withAlpha (0.05f * (float) (4 - i)));
                g.drawText ("SANDWAVE", b.withTrimmedBottom (H * 0.55f).translated ((float) i, 0.f), juce::Justification::centred);
            }
            g.setColour (juce::Colours::white);
            g.drawText ("SANDWAVE", b.withTrimmedBottom (H * 0.55f), juce::Justification::centred);

            g.setFont (font (juce::jmax (8.f, H * 0.055f), true, 0.55f));
            g.setColour (col::neonCyan.withAlpha (0.75f));
            g.drawText ("A I R    \xc2\xb7    S A N D    \xc2\xb7    S P A R K L E",
                        b.withTrimmedTop (H * 0.30f).withHeight (H * 0.12f), juce::Justification::centred);

            // meters
            drawMeter (g, 16.f, H * 0.18f, H * 0.42f, inS, "IN");
            drawMeter (g, W - 28.f, H * 0.18f, H * 0.42f, outS, "OUT");

            // glass sheen
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), 0.f, 0.f,
                                                     juce::Colours::white.withAlpha (0.f), 0.f, H * 0.4f, false));
            g.fillRect (b);
        }

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawRoundedRectangle (b.reduced (0.75f), 26.f, 1.5f);
    }

private:
    struct Particle { float x = 0, y = 0, vx = 0, vy = 0, size = 1, alpha = 1, depth = 1, phase = 0; int kind = 0; bool active = false; };
    struct Star     { float x = 0, y = 0, size = 1, phase = 0, speed = 0; bool big = false; };
    struct Layer    { float base, amp, freq, phase, speed; };

    static constexpr int maxParticles = 700;

    Layer layerAt (int i, float H) const
    {
        switch (i)
        {
            case 0:  return { H * 0.74f, 16.f + 8.f * hi,               0.011f, 0.f, 0.35f };
            case 1:  return { H * 0.83f, 16.f + 10.f * hi + 4.f * sd,   0.017f, 2.f, -0.5f };
            default: return { H * 0.92f, 12.f + 16.f * hi + 10.f * sd,  0.024f, 4.f, 0.7f };
        }
    }

    float duneY (float x, const Layer& L) const
    {
        return L.base - L.amp * (0.5f + 0.5f * std::sin (x * L.freq + L.phase + time * L.speed))
                      - L.amp * 0.35f * std::sin (x * L.freq * 2.3f + time * L.speed * 1.7f + L.phase * 2.f);
    }

    void drawDune (juce::Graphics& g, juce::Rectangle<float> b, const Layer& L,
                   juce::Colour top, juce::Colour bottom, float edgeAlpha) const
    {
        juce::Path edge;
        bool first = true;
        for (float x = b.getX(); x <= b.getRight() + 6.f; x += 5.f)
        {
            const float y = duneY (x, L);
            if (first) { edge.startNewSubPath (x, y); first = false; }
            else         edge.lineTo (x, y);
        }
        juce::Path fill (edge);
        fill.lineTo (b.getRight() + 6.f, b.getBottom());
        fill.lineTo (b.getX(), b.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (top, 0.f, L.base - L.amp, bottom, 0.f, b.getBottom(), false));
        g.fillPath (fill);

        if (edgeAlpha > 0.f)
        {
            g.setColour (sw::col::neonCyan.withAlpha (edgeAlpha * 0.25f));
            g.strokePath (edge, juce::PathStrokeType (5.f));
            g.setColour (sw::col::neonCyan.withAlpha (edgeAlpha));
            g.strokePath (edge, juce::PathStrokeType (1.4f));
        }
    }

    void drawMeter (juce::Graphics& g, float x, float y, float h, float level, const char* label) const
    {
        using namespace sw;
        const float w = 6.f;
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (x, y, w, h, 3.f);
        const float db = juce::Decibels::gainToDecibels (level, -60.f);
        const float v = juce::jlimit (0.f, 1.f, (db + 60.f) / 60.f);
        if (v > 0.01f)
        {
            g.setGradientFill (juce::ColourGradient (col::neonCyan, 0.f, y + h * (1.f - v), col::violet, 0.f, y + h, false));
            g.fillRoundedRectangle (x, y + h * (1.f - v), w, h * v, 3.f);
        }
        g.setFont (font (8.f, true, 0.3f));
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawText (label, juce::Rectangle<float> (x - 10.f, y + h + 2.f, w + 20.f, 10.f), juce::Justification::centred);
    }

    void spawn (Particle& p, float W)
    {
        p.active = true;
        p.x = rng.nextFloat() * W;
        p.y = -4.f - rng.nextFloat() * 20.f;
        p.vy = 0.6f + rng.nextFloat() * 1.6f;
        p.vx = (rng.nextFloat() - 0.5f) * 0.35f + 0.18f;
        p.size = 0.8f + rng.nextFloat() * rng.nextFloat() * 2.4f;
        p.depth = 0.3f + 0.7f * rng.nextFloat();
        p.alpha = 0.35f + 0.65f * p.depth;
        p.phase = rng.nextFloat() * 6.28f;
        const float r = rng.nextFloat();
        p.kind = r < 0.45f ? 0 : r < 0.75f ? 1 : r < 0.92f ? 2 : 3;
    }

    void timerCallback() override
    {
        const float W = (float) getWidth(), H = (float) getHeight();
        if (W < 10.f) return;

        time += 1.f / 50.f;

        const float hiT = proc.highEnergy.load(), sdT = proc.sandEnergy.load();
        hi += (hiT - hi) * (hiT > hi ? 0.35f : 0.08f);
        sd += (sdT - sd) * (sdT > sd ? 0.35f : 0.08f);
        const float inT = proc.inLevel.load(), outT = proc.outLevel.load();
        inS  += (inT  - inS)  * (inT  > inS  ? 0.5f : 0.10f);
        outS += (outT - outS) * (outT > outS ? 0.5f : 0.10f);

        const float sandAmt = pSand->load() * 0.01f;
        air = (pAirMid->load() * 0.4f + pAirHigh->load() * 0.6f) * 0.01f;

        const float spawnP = (0.01f + 0.10f * sandAmt + 0.18f * sd + 0.06f * hi) * 0.35f;
        const auto front = layerAt (2, H);
        const float speed = (0.7f + 1.6f * hi + 0.8f * sd) * (H / 190.f);

        for (auto& p : particles)
        {
            if (! p.active)
            {
                if (rng.nextFloat() < spawnP) spawn (p, W);
                continue;
            }
            p.y += p.vy * speed;
            p.x += p.vx + std::sin (time * 1.7f + p.phase) * 0.12f * p.depth;
            if (p.y > duneY (p.x, front) || p.x < -4.f || p.x > W + 4.f) p.active = false;
        }
        repaint();
    }

    SandwaveProcessor& proc;
    std::atomic<float>* pSand = nullptr;
    std::atomic<float>* pAirMid = nullptr;
    std::atomic<float>* pAirHigh = nullptr;

    std::vector<Particle> particles;
    std::vector<Star> stars;
    juce::Random rng { 20260514 };

    float time = 0.f, hi = 0.f, sd = 0.f, air = 0.f, inS = 0.f, outS = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SandVisualizer)
};
