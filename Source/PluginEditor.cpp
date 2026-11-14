#include "PluginEditor.h"

using namespace sw;

namespace
{
    constexpr float kW = 780.f, kH = 500.f;
}

SandwaveEditor::SandwaveEditor (SandwaveProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), visualizer (p)
{
    setLookAndFeel (&laf);

    const auto pct = [] (double v) { return juce::String (juce::roundToInt (v)); };
    const auto hz  = [] (double v) { return v >= 1000.0 ? juce::String (v / 1000.0, 1) + "k"
                                                        : juce::String (juce::roundToInt (v)); };
    const auto db  = [] (double v) { return juce::String (v, 1); };

    setupSlider (airMid,  "MID AIR",  ids::airMid,  aAirMid,  pct, true);
    setupSlider (airHigh, "HIGH AIR", ids::airHigh, aAirHigh, pct, true);
    setupSlider (sand,    "SAND",     ids::sand,    aSand,    pct, false, true);
    setupSlider (grain,   "GRAIN",    ids::grain,   aGrain,   pct);
    setupSlider (tone,    "TONE",     ids::tone,    aTone,    hz);
    setupSlider (width,   "WIDTH",    ids::width,   aWidth,   pct);
    setupSlider (excite,  "EXCITE",   ids::excite,  aExcite,  pct);
    setupSlider (deharsh, "DE-HARSH", ids::deharsh, aDeh,     pct);
    setupSlider (mix,     "MIX",      ids::mix,     aMix,     pct);
    setupSlider (output,  "OUTPUT",   ids::output,  aOut,     db);

    addAndMakeVisible (visualizer);

    powerButton.setClickingTogglesState (true);
    addAndMakeVisible (powerButton);
    aPower = std::make_unique<ButtonAtt> (proc.apvts, ids::power, powerButton);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    setResizable (true, true);
    setResizeLimits (585, 375, 1560, 1000);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setSize ((int) kW, (int) kH);

    startTimerHz (30);
}

SandwaveEditor::~SandwaveEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SandwaveEditor::setupSlider (juce::Slider& s, const juce::String& name, const char* paramId,
                                  std::unique_ptr<SliderAtt>& att, std::function<juce::String (double)> fmt,
                                  bool vertical, bool hero)
{
    s.setName (name);
    s.setSliderStyle (vertical ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s.setMouseDragSensitivity (220);
    s.textFromValueFunction = std::move (fmt);
    s.getProperties().set ("hero", hero);
    addAndMakeVisible (s);

    att = std::make_unique<SliderAtt> (proc.apvts, paramId, s);

    if (auto* prm = proc.apvts.getParameter (paramId))
        s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void SandwaveEditor::timerCallback()
{
    const float i = proc.inLevel.load(), o = proc.outLevel.load();
    inS  += (i - inS)  * (i > inS  ? 0.5f : 0.12f);
    outS += (o - outS) * (o > outS ? 0.5f : 0.12f);
    repaint (meterCell);
}

void SandwaveEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / kW;
    const float W = (float) getWidth(), H = (float) getHeight();

    // airy background
    g.setGradientFill (juce::ColourGradient (col::mist, 0.f, 0.f, juce::Colour (0xffF3EEFF), W, H, false));
    g.fillAll();
    auto blob = [&] (juce::Colour c, float cx, float cy, float r, float a)
    {
        g.setGradientFill (juce::ColourGradient (c.withAlpha (a), cx, cy, c.withAlpha (0.f), cx + r, cy, true));
        g.fillRect (getLocalBounds());
    };
    blob (col::neonPink, W * 0.05f, H * 0.98f, W * 0.55f, 0.32f);
    blob (col::neonCyan, W * 0.97f, H * 0.08f, W * 0.50f, 0.32f);
    blob (col::violet,   W * 0.50f, H * 0.65f, W * 0.40f, 0.14f);

    // header
    g.setFont (font (10.f * s, true, 0.3f));
    g.setColour (col::inkSoft.withAlpha (0.6f));
    g.drawText ("v1.0  \xc2\xb7  by Neptun", juce::Rectangle<float> (480.f * s, 16.f * s, 220.f * s, 26.f * s),
                juce::Justification::centredRight);

    // frosted control panel
    const juce::Rectangle<float> panel (20.f * s, 258.f * s, 740.f * s, 228.f * s);
    juce::Path pp;
    pp.addRoundedRectangle (panel, 26.f * s);
    juce::DropShadow (col::ink.withAlpha (0.20f), juce::roundToInt (26.f * s), { 0, juce::roundToInt (10.f * s) }).drawForPath (g, pp);
    g.setGradientFill (juce::ColourGradient (col::snow.withAlpha (0.97f), 0.f, panel.getY(),
                                             col::mist.withAlpha (0.90f), 0.f, panel.getBottom(), false));
    g.fillPath (pp);
    sprinkle (g, pp, juce::roundToInt (700.f * s * s), 4242,
              juce::Colours::white.withAlpha (0.9f), col::inkSoft.withAlpha (0.14f));
    g.setColour (juce::Colours::white);
    g.drawRoundedRectangle (panel.reduced (0.8f), 26.f * s, 1.6f);

    // dividers
    g.setColour (col::ink.withAlpha (0.10f));
    g.drawLine (205.f * s, 282.f * s, 205.f * s, 462.f * s, 1.f);
    g.drawLine (418.f * s, 282.f * s, 418.f * s, 462.f * s, 1.f);

    // labels
    auto label = [&] (const juce::Component& c, const char* t, bool big = false)
    {
        g.setFont (font ((big ? 13.f : 11.f) * s, true, 0.22f));
        g.setColour (big ? col::ink : col::inkSoft);
        g.drawText (t, juce::Rectangle<float> ((float) c.getX() - 20.f * s, (float) c.getBottom() + 2.f * s,
                                               (float) c.getWidth() + 40.f * s, 18.f * s),
                    juce::Justification::centred);
    };
    label (airMid, "MID AIR");   label (airHigh, "HIGH AIR"); label (sand, "S A N D", true);
    label (grain, "GRAIN");      label (tone, "TONE");        label (width, "WIDTH");     label (excite, "EXCITE");
    label (deharsh, "DE-HARSH"); label (mix, "MIX");          label (output, "OUTPUT");

    g.setFont (font (11.f * s, true, 0.22f));
    g.setColour (col::inkSoft);
    g.drawText ("LEVEL", juce::Rectangle<float> ((float) meterCell.getX() - 20.f * s, (float) meterCell.getBottom() + 2.f * s,
                                                 (float) meterCell.getWidth() + 40.f * s, 18.f * s),
                juce::Justification::centred);

    // I/O meters
    const float barH = 66.f * s, barW = 12.f * s;
    const float cx = (float) meterCell.getCentreX(), top = (float) meterCell.getY() + 4.f * s;
    auto bar = [&] (float x, float level, const char* t)
    {
        const juce::Rectangle<float> r (x, top, barW, barH);
        g.setColour (col::ink.withAlpha (0.10f));
        g.fillRoundedRectangle (r, barW * 0.5f);
        const float v = juce::jlimit (0.f, 1.f, (juce::Decibels::gainToDecibels (level, -60.f) + 60.f) / 60.f);
        if (v > 0.01f)
        {
            g.setGradientFill (juce::ColourGradient (col::neonCyan, 0.f, top + barH * (1.f - v),
                                                     col::violet, 0.f, top + barH, false));
            g.fillRoundedRectangle (r.withTop (top + barH * (1.f - v)), barW * 0.5f);
        }
        g.setFont (font (8.f * s, true, 0.2f));
        g.setColour (col::inkSoft.withAlpha (0.8f));
        g.drawText (t, juce::Rectangle<float> (x - 8.f * s, top + barH + 1.f * s, barW + 16.f * s, 9.f * s),
                    juce::Justification::centred);
    };
    bar (cx - 20.f * s, inS,  "IN");
    bar (cx + 8.f * s,  outS, "OUT");
}

void SandwaveEditor::resized()
{
    const float s = (float) getWidth() / kW;
    auto R = [s] (float x, float y, float w, float h)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * s), juce::roundToInt (y * s),
                                     juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    presetBox.setBounds (R (24, 14, 230, 30));
    powerButton.setBounds (R (712, 8, 44, 44));
    visualizer.setBounds (R (20, 58, 740, 188));

    airMid.setBounds  (R (40, 274, 60, 168));
    airHigh.setBounds (R (118, 274, 60, 168));
    sand.setBounds    (R (226, 268, 180, 180));

    grain.setBounds   (R (430, 270, 76, 76));
    tone.setBounds    (R (510, 270, 76, 76));
    width.setBounds   (R (590, 270, 76, 76));
    excite.setBounds  (R (670, 270, 76, 76));
    deharsh.setBounds (R (430, 372, 76, 76));
    mix.setBounds     (R (510, 372, 76, 76));
    output.setBounds  (R (590, 372, 76, 76));
    meterCell = R (670, 372, 76, 76);
}
