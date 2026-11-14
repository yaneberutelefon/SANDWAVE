#pragma once
#include <JuceHeader.h>
#include "Style.h"

namespace sw
{
class SandLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SandLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::snow);
        setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::violet.withAlpha (0.85f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ComboBox::textColourId, col::ink);
    }

    //==========================================================================
    // Frosted "pebble" knob with a neon sweep ring. Slider property "hero" = big sand knob.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        const bool hero = (bool) s.getProperties()["hero"];
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.f);
        const float dia = juce::jmin (b.getWidth(), b.getHeight());
        const auto  c   = b.getCentre();
        const float R   = dia * 0.5f;
        const float thick = dia * (hero ? 0.055f : 0.075f);
        const float trackR = R - thick * 0.5f - 1.f;
        const float angle = startA + pos * (endA - startA);

        const juce::PathStrokeType stroke (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        // track
        juce::Path track;
        track.addCentredArc (c.x, c.y, trackR, trackR, 0.f, startA, endA, true);
        g.setColour (col::ink.withAlpha (0.10f));
        g.strokePath (track, stroke);

        // value arc + glow
        if (pos > 0.002f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, trackR, trackR, 0.f, startA, angle, true);
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (col::violet.withAlpha (0.07f * (float) (4 - i)));
                g.strokePath (val, juce::PathStrokeType (thick + (float) i * 4.f,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
            }
            g.setGradientFill (juce::ColourGradient (col::neonCyan, c.x - trackR, c.y,
                                                     col::violet,   c.x + trackR, c.y, false));
            g.strokePath (val, stroke);
        }

        // cap
        const float capR = trackR - thick * 0.5f - dia * 0.05f;
        juce::Path cap;
        cap.addEllipse (c.x - capR, c.y - capR, capR * 2.f, capR * 2.f);
        juce::DropShadow (col::ink.withAlpha (0.28f), juce::roundToInt (dia * 0.12f),
                          { 0, juce::roundToInt (dia * 0.05f) }).drawForPath (g, cap);
        g.setGradientFill (juce::ColourGradient (col::snow, c.x - capR * 0.7f, c.y - capR * 0.9f,
                                                 juce::Colour (0xffBFCBF7), c.x + capR * 0.8f, c.y + capR, false));
        g.fillPath (cap);
        sprinkle (g, cap, hero ? 320 : 110, s.getName().hashCode(),
                  juce::Colours::white.withAlpha (0.8f), col::inkSoft.withAlpha (0.22f));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawEllipse (c.x - capR + 0.7f, c.y - capR + 0.7f, capR * 2.f - 1.4f, capR * 2.f - 1.4f, 1.4f);

        // inner dish
        const float dishR = capR * 0.66f;
        juce::Path dish;
        dish.addEllipse (c.x - dishR, c.y - dishR, dishR * 2.f, dishR * 2.f);
        g.setGradientFill (juce::ColourGradient (col::ink.withAlpha (0.10f), c.x, c.y - dishR,
                                                 juce::Colours::white.withAlpha (0.65f), c.x, c.y + dishR, false));
        g.fillPath (dish);

        // pointer (neon bead)
        const float pr = capR * 0.85f;
        const juce::Point<float> p (c.x + pr * std::sin (angle), c.y - pr * std::cos (angle));
        const float bead = dia * (hero ? 0.045f : 0.06f);
        g.setColour (col::neonCyan.withAlpha (0.20f));
        g.fillEllipse (juce::Rectangle<float> (bead * 3.f, bead * 3.f).withCentre (p));
        g.setColour (col::violet);
        g.fillEllipse (juce::Rectangle<float> (bead * 1.6f, bead * 1.6f).withCentre (p));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (bead, bead).withCentre (p));

        // value
        g.setColour (col::ink);
        g.setFont (font (dia * (hero ? 0.17f : 0.2f)));
        g.drawText (s.getTextFromValue (s.getValue()),
                    juce::Rectangle<float> (dishR * 1.7f, dishR * 0.9f).withCentre (c).toNearestInt(),
                    juce::Justification::centred);
    }

    //==========================================================================
    // Vertical "dune" slider: glass capsule, neon sand fill, pebble thumb.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle style,
                           juce::Slider& s) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, s);
            return;
        }

        const float cx = (float) x + (float) width * 0.5f;
        const float trackW = juce::jmin (22.f, (float) width * 0.4f);
        const float top = (float) y + 8.f, bottom = (float) (y + height) - 8.f;
        const juce::Rectangle<float> tr (cx - trackW * 0.5f, top, trackW, bottom - top);

        juce::Path well;
        well.addRoundedRectangle (tr, trackW * 0.5f);
        g.setGradientFill (juce::ColourGradient (col::ink.withAlpha (0.22f), tr.getX(), 0.f,
                                                 col::ink.withAlpha (0.06f), tr.getRight(), 0.f, false));
        g.fillPath (well);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.strokePath (well, juce::PathStrokeType (1.f));

        // ticks
        const float t0 = top + 14.f, t1 = bottom - 14.f;
        for (int i = 0; i <= 10; ++i)
        {
            const float ty = t0 + (t1 - t0) * (float) i / 10.f;
            const bool major = (i % 5 == 0);
            g.setColour (col::ink.withAlpha (major ? 0.38f : 0.18f));
            const float len = major ? 7.f : 4.f;
            g.drawLine (tr.getX() - 3.f - len, ty, tr.getX() - 3.f, ty, 1.f);
            g.drawLine (tr.getRight() + 3.f, ty, tr.getRight() + 3.f + len, ty, 1.f);
        }

        const float thumbY = juce::jlimit (t0, t1, sliderPos);

        // sand fill
        const juce::Rectangle<float> fr (tr.getX() + 3.f, thumbY, tr.getWidth() - 6.f, bottom - 3.f - thumbY);
        if (fr.getHeight() > 2.f)
        {
            juce::Path fill;
            fill.addRoundedRectangle (fr, (trackW - 6.f) * 0.5f);
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (col::neonCyan.withAlpha (0.07f * (float) (4 - i)));
                g.strokePath (fill, juce::PathStrokeType ((float) i * 4.f));
            }
            g.setGradientFill (juce::ColourGradient (col::neonCyan, 0.f, thumbY, col::violet, 0.f, bottom, false));
            g.fillPath (fill);
            sprinkle (g, fill, 90, s.getName().hashCode(),
                      juce::Colours::white.withAlpha (0.75f), col::violet.withAlpha (0.35f));
        }

        // pebble thumb
        juce::Rectangle<float> th (juce::jmin (48.f, (float) width * 0.9f), 26.f);
        th.setCentre (cx, thumbY);
        juce::Path pebble;
        pebble.addRoundedRectangle (th, 13.f);
        juce::DropShadow (col::ink.withAlpha (0.35f), 10, { 0, 4 }).drawForPath (g, pebble);
        g.setGradientFill (juce::ColourGradient (col::snow, th.getX(), th.getY(),
                                                 juce::Colour (0xffBFCBF7), th.getRight(), th.getBottom(), false));
        g.fillPath (pebble);
        sprinkle (g, pebble, 60, s.getName().hashCode() + 7,
                  juce::Colours::white.withAlpha (0.8f), col::inkSoft.withAlpha (0.2f));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (th.reduced (0.7f), 12.5f, 1.2f);

        g.setColour (col::ink);
        g.setFont (font (11.f));
        g.drawText (s.getTextFromValue (s.getValue()), th.toNearestInt(), juce::Justification::centred);
    }

    int getSliderThumbRadius (juce::Slider&) override { return 14; }

    //==========================================================================
    // Crystal (hexagon) power button.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& bt, bool highlighted, bool /*down*/) override
    {
        const auto b = bt.getLocalBounds().toFloat().reduced (3.f);
        const auto c = b.getCentre();
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const bool on = bt.getToggleState();

        juce::Path hex;
        hex.addPolygon (c, 6, r, 0.f);

        if (on)
        {
            for (int i = 4; i >= 1; --i)
            {
                g.setColour (col::neonCyan.withAlpha (0.08f * (float) (5 - i)));
                g.strokePath (hex, juce::PathStrokeType ((float) i * 3.f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
            }
            g.setGradientFill (juce::ColourGradient (col::neonCyan, c.x, c.y - r, col::violet, c.x, c.y + r, false));
        }
        else
        {
            juce::DropShadow (col::ink.withAlpha (0.25f), 8, { 0, 3 }).drawForPath (g, hex);
            g.setGradientFill (juce::ColourGradient (col::snow, c.x, c.y - r, juce::Colour (0xffBFCBF7), c.x, c.y + r, false));
        }
        g.fillPath (hex);
        g.setColour (juce::Colours::white.withAlpha (highlighted ? 1.f : 0.85f));
        g.strokePath (hex, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float rr = r * 0.38f;
        juce::Path icon;
        icon.addCentredArc (c.x, c.y + r * 0.03f, rr, rr, 0.f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
        icon.startNewSubPath (c.x, c.y - rr * 1.15f);
        icon.lineTo (c.x, c.y - rr * 0.1f);
        g.setColour (on ? juce::Colours::white : col::inkSoft);
        g.strokePath (icon, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    //==========================================================================
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        const auto b = juce::Rectangle<float> ((float) w, (float) h).reduced (1.f);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.2f);

        juce::Path arrow;
        const float ax = b.getRight() - 20.f, ay = b.getCentreY();
        arrow.startNewSubPath (ax - 4.f, ay - 2.f);
        arrow.lineTo (ax, ay + 2.5f);
        arrow.lineTo (ax + 4.f, ay - 2.f);
        g.setColour (col::inkSoft);
        g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.f); }
};
} // namespace sw
