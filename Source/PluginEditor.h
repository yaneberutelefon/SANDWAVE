#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SandLookAndFeel.h"
#include "SandVisualizer.h"

class SandwaveEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SandwaveEditor (SandwaveProcessor&);
    ~SandwaveEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setupSlider (juce::Slider&, const juce::String& name, const char* paramId,
                      std::unique_ptr<SliderAtt>& att, std::function<juce::String (double)> fmt,
                      bool vertical = false, bool hero = false);

    SandwaveProcessor& proc;
    sw::SandLookAndFeel laf;

    SandVisualizer visualizer;
    juce::ComboBox presetBox;
    juce::ToggleButton powerButton;
    juce::Slider airMid, airHigh, sand, grain, tone, width, excite, deharsh, mix, output;

    std::unique_ptr<SliderAtt> aAirMid, aAirHigh, aSand, aGrain, aTone, aWidth, aExcite, aDeh, aMix, aOut;
    std::unique_ptr<ButtonAtt> aPower;

    juce::Rectangle<int> meterCell;
    float inS = 0.f, outS = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SandwaveEditor)
};
