#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset { const char* name; float airMid, airHigh, sand, grain, tone, width, excite, deharsh, mix, out; };

    const Preset kPresets[] =
    {
        { "SANDWAVE Default",   40, 50, 35, 50,  9000, 55, 30, 40, 100,  0.0f },
        { "Silk Air",           35, 65, 10, 30, 12000, 60, 25, 55, 100,  0.0f },
        { "Desert Vocal",       45, 55, 55, 60,  8500, 60, 35, 45, 100, -1.0f },
        { "Neon Dust",          50, 70, 70, 75, 10500, 80, 40, 50, 100, -1.5f },
        { "Sandstorm",          60, 75, 95, 90,  7500, 90, 55, 60, 100, -2.0f },
        { "Fine Grain",         30, 45, 45, 95, 13000, 50, 20, 50,  85,  0.0f },
        { "Master Sparkle",     20, 30, 15, 40, 11000, 50, 15, 30,  60,  0.0f },
    };
    constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}

juce::AudioProcessorValueTreeState::ParameterLayout SandwaveProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto pct = [&] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.f, 100.f, 0.1f), def));
    };

    pct (ids::airMid,  "Mid Air",   40.f);
    pct (ids::airHigh, "High Air",  50.f);
    pct (ids::sand,    "Sand",      35.f);
    pct (ids::grain,   "Grain",     50.f);

    NormalisableRange<float> toneRange (3000.f, 16000.f, 1.f);
    toneRange.setSkewForCentre (8500.f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::tone, 1 }, "Tone", toneRange, 9000.f));

    pct (ids::width,   "Width",     55.f);
    pct (ids::excite,  "Excite",    30.f);
    pct (ids::deharsh, "De-Harsh",  40.f);
    pct (ids::mix,     "Mix",      100.f);

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, 1 }, "Output",
                                                  NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::power, 1 }, "Power", true));
    return l;
}

SandwaveProcessor::SandwaveProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SANDWAVE", createLayout())
{
    pAirMid  = apvts.getRawParameterValue (ids::airMid);
    pAirHigh = apvts.getRawParameterValue (ids::airHigh);
    pSand    = apvts.getRawParameterValue (ids::sand);
    pGrain   = apvts.getRawParameterValue (ids::grain);
    pTone    = apvts.getRawParameterValue (ids::tone);
    pWidth   = apvts.getRawParameterValue (ids::width);
    pExcite  = apvts.getRawParameterValue (ids::excite);
    pDeharsh = apvts.getRawParameterValue (ids::deharsh);
    pMix     = apvts.getRawParameterValue (ids::mix);
    pOutput  = apvts.getRawParameterValue (ids::output);
    pPower   = apvts.getRawParameterValue (ids::power);
}

void SandwaveProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumInputChannels()));
}

bool SandwaveProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

void SandwaveProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    SandEngine::Params p;
    p.airMid   = pAirMid->load()  * 0.01f;
    p.airHigh  = pAirHigh->load() * 0.01f;
    p.sand     = pSand->load()    * 0.01f;
    p.grain    = pGrain->load()   * 0.01f;
    p.tone     = pTone->load();
    p.width    = pWidth->load()   * 0.01f;
    p.excite   = pExcite->load()  * 0.01f;
    p.deharsh  = pDeharsh->load() * 0.01f;
    p.mix      = pMix->load()     * 0.01f;
    p.outputDb = pOutput->load();
    p.power    = pPower->load() > 0.5f;

    inLevel.store (buffer.getMagnitude (0, n));
    engine.setParams (p);
    engine.process (buffer);
    outLevel.store (buffer.getMagnitude (0, n));
    highEnergy.store (engine.getHighEnergy());
    sandEnergy.store (engine.getSandEnergy());
}

juce::AudioProcessorEditor* SandwaveProcessor::createEditor() { return new SandwaveEditor (*this); }

int SandwaveProcessor::getNumPrograms() { return kNumPresets; }

const juce::String SandwaveProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, kNumPresets) ? juce::String (kPresets[index].name) : juce::String();
}

void SandwaveProcessor::setCurrentProgram (int index)
{
    currentProgram = juce::jlimit (0, kNumPresets - 1, index);
    const auto& p = kPresets[currentProgram];

    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set (ids::airMid, p.airMid);   set (ids::airHigh, p.airHigh); set (ids::sand, p.sand);
    set (ids::grain, p.grain);     set (ids::tone, p.tone);       set (ids::width, p.width);
    set (ids::excite, p.excite);   set (ids::deharsh, p.deharsh); set (ids::mix, p.mix);
    set (ids::output, p.out);
}

void SandwaveProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void SandwaveProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SandwaveProcessor(); }
