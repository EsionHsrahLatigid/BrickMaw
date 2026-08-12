#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ParameterIDs.h"

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
} // namespace

BrickMawAudioProcessorEditor::BrickMawAudioProcessorEditor(BrickMawAudioProcessor& p)
    : AudioProcessorEditor(&p), ownerProcessor(p),
      tooltipText("BrickMaw: destructive lookahead limiter with ceiling, release, predrive, 4x detector, link, mix, and output controls.")
{
    setResizeLimits(minimumWidth, minimumHeight, defaultWidth * 2, defaultHeight * 2);
    setResizable(true, true);
    setName("BrickMaw editor");
    setComponentID("brickmaw-editor");
    setTitle("BrickMaw");
    setDescription("BrickMaw monochrome 8-bit limiter editor");
    setWantsKeyboardFocus(true);

    for (std::size_t i = 0; i < controls.size(); ++i)
    {
        auto& slider = sliders[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 24);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff8a8a86));
        slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff2a2a2a));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff2f2f0));
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff2f2f0));
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff050505));
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff8a8a86));
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
        label.setName(juce::String(controls[i].label) + " label");
        label.setComponentID(juce::String("brickmaw-") + controls[i].id + "-label");
        label.setTooltip(controls[i].tooltip);
        label.setColour(juce::Label::textColourId, juce::Colour(0xfff2f2f0));
        label.attachToComponent(&slider, true);
        addAndMakeVisible(label);

        attachments[i] = std::make_unique<SliderAttachment>(ownerProcessor.parameters, controls[i].id, slider);
    }

    setSize(defaultWidth, defaultHeight);
}

void BrickMawAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds();
    g.fillAll(juce::Colour(0xff050505));

    g.setColour(juce::Colour(0xfff2f2f0));
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("BrickMaw", 32, 16, area.getWidth() - 64, 32, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff8a8a86));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("LIMITER", 32, 48, area.getWidth() - 64, 16, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawHorizontalLine(72, 32.0f, static_cast<float>(area.getWidth() - 32));
}

void BrickMawAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(32);
    area.removeFromTop(48);

    const int rowHeight = 32;
    const int rowGap = 8;
    const int columns = 2;
    const int labelWidth = 92;
    const int columnGap = 24;
    const int columnWidth = (area.getWidth() - columnGap) / columns;

    for (int i = 0; i < controlCount; ++i)
    {
        const int column = i / 6;
        const int row = i % 6;
        juce::Rectangle<int> rowBounds(area.getX() + column * (columnWidth + columnGap),
                                       area.getY() + row * (rowHeight + rowGap),
                                       columnWidth,
                                       rowHeight);
        labels[static_cast<std::size_t>(i)].setBounds(rowBounds.removeFromLeft(labelWidth));
        rowBounds.removeFromLeft(8);
        sliders[static_cast<std::size_t>(i)].setBounds(rowBounds);
    }
}
