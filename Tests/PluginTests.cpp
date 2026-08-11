#include "TestSupport.h"
#include "ParameterIDs.h"
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <array>
#include <cmath>
#include <string>

namespace
{
constexpr std::array<const char*, 12> parameterIds {{
    brickmaw::parameters::ceiling,
    brickmaw::parameters::lookahead,
    brickmaw::parameters::release,
    brickmaw::parameters::adaptive,
    brickmaw::parameters::preDrive,
    brickmaw::parameters::clipShape,
    brickmaw::parameters::oversampleDetect,
    brickmaw::parameters::mawBite,
    brickmaw::parameters::recovery,
    brickmaw::parameters::link,
    brickmaw::parameters::mix,
    brickmaw::parameters::output,
}};

float maxAbs(const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            peak = std::max(peak, std::abs(buffer.getSample(ch, i)));
    return peak;
}

int firstAudibleSample(const juce::AudioBuffer<float>& buffer)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            if (std::abs(buffer.getSample(ch, i)) > 0.0001f)
                return i;
    return -1;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    return test_support::run("brickmaw_plugin_tests", [] {
        BrickMawAudioProcessor processor;
        test_support::check(processor.getName() == "BrickMaw", "product name");
        test_support::check(!processor.acceptsMidi(), "limiter does not require MIDI");
        test_support::check(!processor.isMidiEffect(), "audio effect");
        test_support::check(processor.getTailLengthSeconds() == 0.0, "zero tail");

        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add(juce::AudioChannelSet::stereo());
        stereo.outputBuses.add(juce::AudioChannelSet::stereo());
        test_support::check(processor.isBusesLayoutSupported(stereo), "stereo bus supported");

        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add(juce::AudioChannelSet::mono());
        mono.outputBuses.add(juce::AudioChannelSet::mono());
        test_support::check(processor.isBusesLayoutSupported(mono), "mono bus supported");

        juce::AudioProcessor::BusesLayout monoToStereo;
        monoToStereo.inputBuses.add(juce::AudioChannelSet::mono());
        monoToStereo.outputBuses.add(juce::AudioChannelSet::stereo());
        test_support::check(!processor.isBusesLayoutSupported(monoToStereo), "mono-to-stereo bus rejected");

        juce::AudioProcessor::BusesLayout stereoToMono;
        stereoToMono.inputBuses.add(juce::AudioChannelSet::stereo());
        stereoToMono.outputBuses.add(juce::AudioChannelSet::mono());
        test_support::check(!processor.isBusesLayoutSupported(stereoToMono), "stereo-to-mono bus rejected");

        for (const auto* id : parameterIds)
        {
            auto* parameter = processor.parameters.getParameter(id);
            test_support::check(parameter != nullptr, std::string("APVTS parameter exists: ") + id);
            test_support::check(parameter->getName(64).isNotEmpty(), std::string("parameter has accessible name: ") + id);
        }

        auto* ceiling = processor.parameters.getParameter(brickmaw::parameters::ceiling);
        auto* lookahead = processor.parameters.getParameter(brickmaw::parameters::lookahead);
        auto* preDrive = processor.parameters.getParameter(brickmaw::parameters::preDrive);
        auto* mix = processor.parameters.getParameter(brickmaw::parameters::mix);
        test_support::check(ceiling != nullptr && lookahead != nullptr && preDrive != nullptr && mix != nullptr, "key limiter parameters exist");
        ceiling->setValueNotifyingHost(ceiling->convertTo0to1(-6.0f));
        lookahead->setValueNotifyingHost(lookahead->convertTo0to1(2.0f));
        preDrive->setValueNotifyingHost(preDrive->convertTo0to1(24.0f));
        mix->setValueNotifyingHost(mix->convertTo0to1(1.0f));

        juce::MemoryBlock state;
        processor.getStateInformation(state);
        ceiling->setValueNotifyingHost(ceiling->convertTo0to1(-1.0f));
        processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        test_support::check(std::abs(processor.parameters.getRawParameterValue(brickmaw::parameters::ceiling)->load() + 6.0f) < 0.01f, "state round-trip");

        const char invalid[] = "not xml";
        processor.setStateInformation(invalid, static_cast<int>(sizeof(invalid)));
        test_support::check(std::isfinite(processor.parameters.getRawParameterValue(brickmaw::parameters::ceiling)->load()), "invalid state ignored safely");

        processor.prepareToPlay(48000.0, 128);
        test_support::check(processor.getLatencySamples() == 480, "latency reports fixed max lookahead at 48 kHz");

        juce::AudioBuffer<float> buffer(2, 640);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            buffer.setSample(0, i, i == 0 ? 4.0f : 1.5f);
            buffer.setSample(1, i, i % 19 == 0 ? -3.0f : 0.25f);
        }
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);
        const auto ceilingGain = brickmaw::dsp::BrickMawLimiter::dbToGain(-6.0f);
        test_support::check(maxAbs(buffer) <= ceilingGain + 0.00001f, "plugin path holds configured sample ceiling");
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                test_support::check(std::isfinite(buffer.getSample(ch, i)), "processed samples finite");

        for (float testMix : { 0.0f, 0.5f })
        {
            processor.reset();
            mix->setValueNotifyingHost(mix->convertTo0to1(testMix));
            lookahead->setValueNotifyingHost(lookahead->convertTo0to1(testMix == 0.0f ? 0.5f : 8.0f));
            test_support::check(processor.getLatencySamples() == 480, "lookahead automation does not change host latency");

            juce::AudioBuffer<float> impulse(2, 640);
            impulse.clear();
            impulse.setSample(0, 0, 2.0f);
            impulse.setSample(1, 0, -2.0f);
            processor.processBlock(impulse, midi);
            test_support::check(firstAudibleSample(impulse) == processor.getLatencySamples(), "plugin first audible sample matches fixed latency for mixed dry/wet path");
            test_support::check(maxAbs(impulse) <= ceilingGain + 0.00001f, "mixed plugin impulse obeys ceiling");
        }
    });
}
