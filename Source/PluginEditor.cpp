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
    setSize(defaultWidth, defaultHeight);
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
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xffd8d8d8));
        slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff222222));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffffff));
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff0f0f0));
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff080808));
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
        label.setColour(juce::Label::textColourId, juce::Colour(0xfff0f0f0));
        label.attachToComponent(&slider, true);
        addAndMakeVisible(label);

        attachments[i] = std::make_unique<SliderAttachment>(ownerProcessor.parameters, controls[i].id, slider);
    }
}

void BrickMawAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds();
    g.fillAll(juce::Colour(0xff050505));

    constexpr int grid = 8;
    g.setColour(juce::Colour(0xff202020));
    for (int x = 0; x < area.getWidth(); x += grid)
        g.drawVerticalLine(x, 0.0f, static_cast<float>(area.getHeight()));
    for (int y = 0; y < area.getHeight(); y += grid)
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(area.getWidth()));

    g.setColour(juce::Colour(0xfff0f0f0));
    g.setFont(juce::FontOptions(32.0f, juce::Font::bold));
    g.drawText("BrickMaw", 32, 22, area.getWidth() - 64, 44, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(15.0f));
    g.drawText("jp.ehl.brickmaw / BrMw / exact sample ceiling, not BS.1770 certified", 34, 66, area.getWidth() - 68, 24, juce::Justification::centredLeft);

    const auto bounds = area.reduced(32);
    const int motifTop = bounds.getBottom() - 104;
    const int centre = bounds.getCentreX();
    g.setColour(juce::Colour(0xffcfcfcf));
    for (int x = bounds.getX(); x < bounds.getRight(); x += 24)
    {
        const int distance = std::abs(x - centre);
        const int jaw = juce::jlimit(16, 88, 96 - distance / 7);
        const bool upper = ((x / 24) % 2) == 0;
        g.fillRect(x, upper ? motifTop : motifTop + 96 - jaw, 12, jaw);
    }

    const float reduction = juce::jlimit(0.0f, 36.0f, ownerProcessor.limiter.lastGainReductionDb());
    const int bricks = static_cast<int>(reduction / 3.0f);
    g.setColour(juce::Colour(0xffffffff));
    for (int i = 0; i < bricks; ++i)
        g.fillRect(bounds.getRight() - 18 - i * 18, motifTop - 24, 10, 16);
}

void BrickMawAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(32);
    area.removeFromTop(96);
    area.removeFromBottom(112);

    const int rowHeight = 31;
    const int rowGap = 7;
    const int columns = getWidth() >= 860 ? 2 : 1;
    const int labelWidth = 92;
    const int columnGap = 34;
    const int columnWidth = columns == 2 ? (area.getWidth() - columnGap) / 2 : area.getWidth();

    for (int i = 0; i < controlCount; ++i)
    {
        const int column = columns == 2 ? i % 2 : 0;
        const int row = columns == 2 ? i / 2 : i;
        juce::Rectangle<int> rowBounds(area.getX() + column * (columnWidth + columnGap),
                                       area.getY() + row * (rowHeight + rowGap),
                                       columnWidth,
                                       rowHeight);
        labels[static_cast<std::size_t>(i)].setBounds(rowBounds.removeFromLeft(labelWidth));
        rowBounds.removeFromLeft(8);
        sliders[static_cast<std::size_t>(i)].setBounds(rowBounds);
    }
}
