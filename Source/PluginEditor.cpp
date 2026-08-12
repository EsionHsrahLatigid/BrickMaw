#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ParameterIDs.h"

#include <cmath>

namespace
{
struct ControlSpec
{
    const char* id;
    const char* label;
    const char* tooltip;
};

constexpr std::array<ControlSpec, 12> controls {{
    { brickmaw::parameters::ceiling, "Ceiling", "Final sample ceiling in dBFS. BrickMaw clamps output samples to this digital ceiling." },
    { brickmaw::parameters::lookahead, "Lookahead", "Limiter lookahead in milliseconds. Reported latency is set from this value when prepared." },
    { brickmaw::parameters::release, "Release", "Base limiter release time in milliseconds." },
    { brickmaw::parameters::adaptive, "Adapt", "Deep gain reduction releases faster as adaptive release increases." },
    { brickmaw::parameters::preDrive, "Predrive", "Input gain into the destructive pre-limiter clip character." },
    { brickmaw::parameters::clipShape, "Clip", "Blend between hard jaw clipping and rounded brick clipping." },
    { brickmaw::parameters::oversampleDetect, "4x Detect", "Enables the four-times oversampled peak detector estimate." },
    { brickmaw::parameters::mawBite, "Bite", "Increases detector aggression and clip density." },
    { brickmaw::parameters::recovery, "Recover", "Moves release recovery from slow and heavy to fast and snapping." },
    { brickmaw::parameters::link, "Link", "Blends independent channel limiting with linked stereo limiting." },
    { brickmaw::parameters::mix, "Mix", "Blends the delayed limited path with the dry path before the final ceiling guard." },
    { brickmaw::parameters::output, "Output", "Post-limiter output trim before the final ceiling guard." },
}};

float normalizedSliderValue(juce::Slider& slider) noexcept
{
    const auto normalized = static_cast<float>(slider.valueToProportionOfLength(slider.getValue()));
    return std::isfinite(normalized) ? juce::jlimit(0.0f, 1.0f, normalized) : 0.0f;
}
} // namespace

BrickMawAudioProcessorEditor::BrickMawAudioProcessorEditor(BrickMawAudioProcessor& p)
    : AudioProcessorEditor(&p), ownerProcessor(p),
      tooltipText("BrickMaw: destructive lookahead limiter with ceiling, release, predrive, 4x detector, link, mix, and output controls.")
{
    setLookAndFeel(&ehlLookAndFeel);
    setResizeLimits(minimumWidth, minimumHeight,
                    ehl::juce_design::Metrics::maximumWidth,
                    ehl::juce_design::Metrics::maximumHeight);
    setResizable(true, true);
    setName("BrickMaw editor");
    setComponentID("brickmaw-editor");
    setTitle("BrickMaw");
    setDescription("BrickMaw monochrome 8-bit limiter editor");
    setWantsKeyboardFocus(true);

    parameterDisplay.setComponentID("brickmaw-parameter-display");
    parameterDisplay.setName("BrickMaw parameter display");
    parameterDisplay.setInterceptsMouseClicks(false, false);
    parameterDisplay.setWantsKeyboardFocus(false);
    addAndMakeVisible(parameterDisplay);

    for (std::size_t i = 0; i < controls.size(); ++i)
    {
        auto& slider = sliders[i];
        ehl::juce_design::styleSlider(slider);
        slider.setName(controls[i].label);
        slider.setComponentID(juce::String("brickmaw-") + controls[i].id);
        slider.setTitle(controls[i].label);
        slider.setDescription(controls[i].tooltip);
        slider.setTooltip(controls[i].tooltip);
        slider.getProperties().set("brickmawTooltip", controls[i].tooltip);
        slider.setWantsKeyboardFocus(true);
        addAndMakeVisible(slider);

        auto& label = labels[i];
        label.setText(controls[i].label, juce::dontSendNotification);
        ehl::juce_design::styleLabel(label);
        label.setName(juce::String(controls[i].label) + " label");
        label.setComponentID(juce::String("brickmaw-") + controls[i].id + "-label");
        label.setTooltip(controls[i].tooltip);
        addAndMakeVisible(label);

        attachments[i] = std::make_unique<SliderAttachment>(ownerProcessor.parameters, controls[i].id, slider);
    }

    updateParameterDisplay();
    startTimerHz(30);
    setSize(defaultWidth, defaultHeight);
}

BrickMawAudioProcessorEditor::~BrickMawAudioProcessorEditor()
{
    stopTimer();
    for (auto& slider : sliders)
        slider.setLookAndFeel(nullptr);
    for (auto& label : labels)
        label.setLookAndFeel(nullptr);
    tooltipWindow.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void BrickMawAudioProcessorEditor::paint(juce::Graphics& g)
{
    ehl::juce_design::paintEditorChrome(g, getLocalBounds(), "BrickMaw", "LIMITER");
}

void BrickMawAudioProcessorEditor::resized()
{
    parameterDisplay.setBounds(ehl::juce_design::parameterDisplayArea(getLocalBounds()));

    for (int i = 0; i < controlCount; ++i)
        ehl::juce_design::layoutLabelledControl(labels[static_cast<std::size_t>(i)],
                                                sliders[static_cast<std::size_t>(i)],
                                                ehl::juce_design::controlCell(getLocalBounds(), static_cast<std::size_t>(i)));
}

void BrickMawAudioProcessorEditor::timerCallback()
{
    updateParameterDisplay();
}

void BrickMawAudioProcessorEditor::updateParameterDisplay()
{
    parameterDisplay.setValues({ normalizedSliderValue(sliders[0]),
                                 normalizedSliderValue(sliders[1]),
                                 normalizedSliderValue(sliders[2]),
                                 normalizedSliderValue(sliders[4]) });
}
