#pragma once

#include <ehl/juce_design/EhlDesign.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>

class BrickMawAudioProcessor;

class BrickMawAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit BrickMawAudioProcessorEditor(BrickMawAudioProcessor&);
    ~BrickMawAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    juce::String getTooltip() { return tooltipText; }

    static constexpr int defaultWidth = ehl::juce_design::Metrics::defaultWidth;
    static constexpr int defaultHeight = ehl::juce_design::Metrics::defaultHeight;
    static constexpr int minimumWidth = ehl::juce_design::Metrics::minimumWidth;
    static constexpr int minimumHeight = ehl::juce_design::Metrics::minimumHeight;

private:
    static constexpr int controlCount = 12;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void updateParameterDisplay();

    BrickMawAudioProcessor& ownerProcessor;
    ehl::juce_design::LookAndFeel ehlLookAndFeel;
    ehl::juce_design::ParameterDisplay parameterDisplay { ehl::juce_design::DisplayKind::limiter };
    juce::TooltipWindow tooltipWindow { this, 700 };
    juce::String tooltipText;
    std::array<juce::Slider, controlCount> sliders;
    std::array<juce::Label, controlCount> labels;
    std::array<std::unique_ptr<SliderAttachment>, controlCount> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrickMawAudioProcessorEditor)
};
