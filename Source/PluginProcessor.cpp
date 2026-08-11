#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"

#include <cmath>

namespace
{
float readParam(const juce::AudioProcessorValueTreeState& parameters, const char* id)
{
    if (auto* value = parameters.getRawParameterValue(id))
        return value->load();
    return 0.0f;
}

brickmaw::dsp::LimiterParameters readLimiterParameters(const juce::AudioProcessorValueTreeState& parameters)
{
    brickmaw::dsp::LimiterParameters values;
    values.ceilingDb = readParam(parameters, brickmaw::parameters::ceiling);
    values.lookaheadMs = readParam(parameters, brickmaw::parameters::lookahead);
    values.releaseMs = readParam(parameters, brickmaw::parameters::release);
    values.adaptive = readParam(parameters, brickmaw::parameters::adaptive);
    values.preDriveDb = readParam(parameters, brickmaw::parameters::preDrive);
    values.clipShape = readParam(parameters, brickmaw::parameters::clipShape);
    values.oversampleDetect = readParam(parameters, brickmaw::parameters::oversampleDetect);
    values.mawBite = readParam(parameters, brickmaw::parameters::mawBite);
    values.recovery = readParam(parameters, brickmaw::parameters::recovery);
    values.link = readParam(parameters, brickmaw::parameters::link);
    values.mix = readParam(parameters, brickmaw::parameters::mix);
    values.outputDb = readParam(parameters, brickmaw::parameters::output);
    return values;
}
} // namespace

BrickMawAudioProcessor::BrickMawAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BrickMawAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>(brickmaw::parameters::ceiling, "Ceiling", juce::NormalisableRange<float>(-24.0f, 0.0f, 0.1f), -3.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(brickmaw::parameters::lookahead, "Lookahead", juce::NormalisableRange<float>(0.5f, 10.0f, 0.1f), 3.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(brickmaw::parameters::release, "Release", juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f), 85.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(brickmaw::parameters::adaptive, "Adaptive Release", 0.0f, 1.0f, 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(brickmaw::parameters::preDrive, "Predrive", juce::NormalisableRange<float>(0.0f, 36.0f, 0.1f), 9.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(brickmaw::parameters::clipShape, "Clip Shape", 0.0f, 1.0f, 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(brickmaw::parameters::oversampleDetect, "4x Detector", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(brickmaw::parameters::mawBite, "Maw Bite", 0.0f, 1.0f, 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(brickmaw::parameters::recovery, "Recovery", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(brickmaw::parameters::link, "Stereo Link", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(brickmaw::parameters::mix, "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(brickmaw::parameters::output, "Output", juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), 0.0f));
    return { params.begin(), params.end() };
}

void BrickMawAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    limiter.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    limiter.setParameters(readLimiterParameters(parameters));
    syncLatencyToLookahead();
}

void BrickMawAudioProcessor::reset()
{
    limiter.reset();
}

bool BrickMawAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != mainOut)
        return false;
    return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
}

void BrickMawAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalIn = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int channel = totalIn; channel < totalOut; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    limiter.setParameters(readLimiterParameters(parameters));
    if (totalOut > 0)
    {
        float* channelData[2] { buffer.getWritePointer(0), totalOut > 1 ? buffer.getWritePointer(1) : nullptr };
        limiter.processBlock(channelData, totalOut, buffer.getNumSamples());
    }
}

juce::AudioProcessorEditor* BrickMawAudioProcessor::createEditor()
{
    return new BrickMawAudioProcessorEditor(*this);
}

void BrickMawAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState().createXml())
        copyXmlToBinary(*state, destData);
}

void BrickMawAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
        {
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
            limiter.setParameters(readLimiterParameters(parameters));
            syncLatencyToLookahead();
        }
}

void BrickMawAudioProcessor::syncLatencyToLookahead()
{
    limiter.setParameters(readLimiterParameters(parameters));
    setLatencySamples(juce::jmax(1, limiter.latencySamples()));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BrickMawAudioProcessor();
}
