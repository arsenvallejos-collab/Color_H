#include "PluginProcessor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout BomBapProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                        StringArray { "Tape", "Cassette" }, 0));

    auto percent = [] (const char* id, const char* name, float defaultValue)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                                      defaultValue,
                                                      AudioParameterFloatAttributes().withLabel ("%"));
    };

    layout.add (percent ("drive",   "Drive",   30.0f));
    layout.add (percent ("wow",     "Wow",     30.0f));
    layout.add (percent ("flutter", "Flutter", 30.0f));
    layout.add (percent ("noise",   "Noise",   20.0f));
    layout.add (percent ("tone",    "Tone",    50.0f));
    layout.add (percent ("mix",     "Mix",    100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                                                       NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

BomBapProcessor::BomBapProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pMode    = apvts.getRawParameterValue ("mode");
    pDrive   = apvts.getRawParameterValue ("drive");
    pWow     = apvts.getRawParameterValue ("wow");
    pFlutter = apvts.getRawParameterValue ("flutter");
    pNoise   = apvts.getRawParameterValue ("noise");
    pTone    = apvts.getRawParameterValue ("tone");
    pMix     = apvts.getRawParameterValue ("mix");
    pOutput  = apvts.getRawParameterValue ("output");
}

void BomBapProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, getTotalNumOutputChannels());
    setLatencySamples (engine.getLatencySamples());   // el host compensa la latencia automaticamente
}

bool BomBapProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void BomBapProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    bombap::Params p;
    p.mode     = (int) pMode->load();
    p.drive    = pDrive->load()   * 0.01f;
    p.wow      = pWow->load()     * 0.01f;
    p.flutter  = pFlutter->load() * 0.01f;
    p.noise    = pNoise->load()   * 0.01f;
    p.tone     = pTone->load()    * 0.01f;
    p.mix      = pMix->load()     * 0.01f;
    p.outputDb = pOutput->load();

    engine.setParams (p);
    engine.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), numSamples);
}

AudioProcessorEditor* BomBapProcessor::createEditor()
{
    // Version 1: panel generico con todos los controles. Mas adelante se cambia por un diseno propio.
    return new GenericAudioProcessorEditor (*this);
}

void BomBapProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BomBapProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BomBapProcessor();
}
