#pragma once
#include <JuceHeader.h>
#include "SandEngine.h"

namespace ids
{
    inline constexpr const char* airMid  = "airMid";
    inline constexpr const char* airHigh = "airHigh";
    inline constexpr const char* sand    = "sand";
    inline constexpr const char* grain   = "grain";
    inline constexpr const char* tone    = "tone";
    inline constexpr const char* width   = "width";
    inline constexpr const char* excite  = "excite";
    inline constexpr const char* deharsh = "deharsh";
    inline constexpr const char* mix     = "mix";
    inline constexpr const char* output  = "output";
    inline constexpr const char* power   = "power";
}

class SandwaveProcessor : public juce::AudioProcessor
{
public:
    SandwaveProcessor();
    ~SandwaveProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // read by the GUI
    std::atomic<float> highEnergy { 0.f }, sandEnergy { 0.f }, inLevel { 0.f }, outLevel { 0.f };

private:
    SandEngine engine;
    int currentProgram = 0;

    std::atomic<float> *pAirMid = nullptr, *pAirHigh = nullptr, *pSand = nullptr, *pGrain = nullptr,
                       *pTone = nullptr, *pWidth = nullptr, *pExcite = nullptr, *pDeharsh = nullptr,
                       *pMix = nullptr, *pOutput = nullptr, *pPower = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SandwaveProcessor)
};
