#include "TestSupport.h"
#include "ParameterIDs.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <array>
#include <string>

namespace
{
constexpr std::array<const char*, 12> ids {{
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

bool interceptsMouseClicks(const juce::Component& component) noexcept
{
    bool allowsThis = true;
    bool allowsChildren = true;
    component.getInterceptsMouseClicks(allowsThis, allowsChildren);
    return allowsThis || allowsChildren;
}

void checkPaintContract(juce::AudioProcessorEditor& editor)
{
    juce::Image image(juce::Image::RGB, BrickMawAudioProcessorEditor::defaultWidth,
                      BrickMawAudioProcessorEditor::defaultHeight, true);
    editor.setBounds(0, 0, image.getWidth(), image.getHeight());
    {
        juce::Graphics g(image);
        editor.paint(g);
    }

    const auto background = ehl::juce_design::Palette::ink();
    const auto divider = ehl::juce_design::Palette::low();
    const auto foreground = ehl::juce_design::Palette::paper();
    bool topStripeIsPaper = true;
    bool headerHasInk = false;
    bool separatorBandIsExact = true;
    bool bodyIsPlain = true;
    int maxChannelSpread = 0;

    for (int y = 0; y < image.getHeight(); ++y)
    {
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const auto pixel = image.getPixelAt(x, y);
            if (y < 4)
                topStripeIsPaper = topStripeIsPaper && pixel == foreground;
            headerHasInk = headerHasInk || (y >= 4 && y < 48 && pixel != background);
            if (y >= 48 && y < ehl::juce_design::Metrics::headerHeight)
            {
                const bool onDivider = y == ehl::juce_design::Metrics::dividerY
                                    && x >= ehl::juce_design::Metrics::margin
                                    && x < image.getWidth() - ehl::juce_design::Metrics::margin;
                separatorBandIsExact = separatorBandIsExact && pixel == (onDivider ? divider : background);
            }
            bodyIsPlain = bodyIsPlain && (y < ehl::juce_design::Metrics::headerHeight || pixel == background);
            const int high = juce::jmax(juce::jmax(pixel.getRed(), pixel.getGreen()), pixel.getBlue());
            const int low = juce::jmin(juce::jmin(pixel.getRed(), pixel.getGreen()), pixel.getBlue());
            maxChannelSpread = juce::jmax(maxChannelSpread, high - low);
        }
    }

    test_support::check(topStripeIsPaper, "module chrome paints top paper stripe");
    test_support::check(headerHasInk, "module chrome paints header text below top stripe");
    test_support::check(separatorBandIsExact, "module chrome paints only divider at Metrics::dividerY from x=16 to w-17");
    test_support::check(bodyIsPlain, "module chrome leaves body y>=64 as ink");
    test_support::check(maxChannelSpread <= 4,
                        "paint stays monochrome within EHL palette tolerance, max spread "
                            + std::to_string(maxChannelSpread));
}

void checkLayoutContract(juce::AudioProcessorEditor& editor, int width, int height)
{
    editor.setBounds(0, 0, width, height);
    editor.resized();

    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        const auto controlId = juce::String("brickmaw-") + ids[i];
        auto* component = editor.findChildWithID(controlId);
        test_support::check(component != nullptr, std::string("editor exposes control for ") + ids[i]);
        auto* slider = dynamic_cast<juce::Slider*>(component);
        test_support::check(slider != nullptr, std::string("control is slider for ") + ids[i]);
        test_support::check(slider->getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag,
                            std::string("module slider style for ") + ids[i]);
        test_support::check(slider->getTextBoxPosition() == juce::Slider::TextBoxBelow,
                            std::string("module slider text box below for ") + ids[i]);
        test_support::check(slider->getTextBoxWidth() == ehl::juce_design::Metrics::valueWidth,
                            std::string("module slider value width for ") + ids[i]);
        test_support::check(slider->findColour(juce::Slider::trackColourId) == ehl::juce_design::Palette::mid(),
                            std::string("module slider palette for ") + ids[i]);
        test_support::check(component->getName().isNotEmpty(), std::string("control has accessible name for ") + ids[i]);
        test_support::check(component->getProperties()["brickmawTooltip"].toString().isNotEmpty(), std::string("control has tooltip for ") + ids[i]);

        const auto expected = ehl::juce_design::labelledControlBounds(
            ehl::juce_design::controlCell(editor.getLocalBounds(), i));
        const auto bounds = component->getBounds();
        test_support::check(bounds == expected.control, std::string("module control grid for ") + ids[i]);
        test_support::check(bounds.getY() >= ehl::juce_design::Metrics::headerHeight,
                            std::string("control body starts at y>=64 for ") + ids[i]);

        auto* label = editor.findChildWithID(controlId + "-label");
        test_support::check(label != nullptr, std::string("explicit label for ") + ids[i]);
        test_support::check(label->getBounds() == expected.label, std::string("module label grid for ") + ids[i]);
    }

    auto* display = dynamic_cast<ehl::juce_design::ParameterDisplay*>(
        editor.findChildWithID("brickmaw-parameter-display"));
    test_support::check(display != nullptr, "parameter display component id");
    test_support::check(display->getKind() == ehl::juce_design::DisplayKind::limiter,
                        "parameter display kind");
    test_support::check(display->getBounds() == ehl::juce_design::parameterDisplayArea(editor.getLocalBounds()),
                        "parameter display bounds");
    test_support::check(! interceptsMouseClicks(*display), "parameter display is noninteractive");
}

void checkParameterDisplayFollowsControls(juce::AudioProcessorEditor& editor)
{
    editor.setBounds(0, 0, BrickMawAudioProcessorEditor::defaultWidth,
                     BrickMawAudioProcessorEditor::defaultHeight);
    editor.resized();

    auto* display = dynamic_cast<ehl::juce_design::ParameterDisplay*>(
        editor.findChildWithID("brickmaw-parameter-display"));
    auto* ceiling = dynamic_cast<juce::Slider*>(editor.findChildWithID("brickmaw-ceiling"));
    test_support::check(display != nullptr && ceiling != nullptr, "display and ceiling slider exist");

    const auto before = display->getValues()[0];
    ceiling->setValue(ceiling->getMinimum(), juce::sendNotificationSync);
    juce::Thread::sleep(40);
    juce::Timer::callPendingTimersSynchronously();
    const auto after = display->getValues()[0];
    test_support::check(after < before - 0.10f, "parameter display follows real slider value changes");
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    return test_support::run("brickmaw_editor_tests", [] {
        BrickMawAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        auto* custom = dynamic_cast<BrickMawAudioProcessorEditor*>(editor.get());
        test_support::check(custom != nullptr, "custom editor type, not GenericAudioProcessorEditor");
        test_support::check(dynamic_cast<juce::GenericAudioProcessorEditor*>(editor.get()) == nullptr, "not GenericAudioProcessorEditor");
        test_support::check(editor->getWidth() == BrickMawAudioProcessorEditor::defaultWidth, "default width is module default");
        test_support::check(editor->getHeight() == BrickMawAudioProcessorEditor::defaultHeight, "default height is module default");
        test_support::check(BrickMawAudioProcessorEditor::minimumWidth == 512, "module minimum width");
        test_support::check(BrickMawAudioProcessorEditor::minimumHeight == 320, "module minimum height");
        test_support::check(editor->getComponentID() == "brickmaw-editor", "component id");
        test_support::check(editor->getName().isNotEmpty(), "accessible name");
        test_support::check(custom->getTooltip().isNotEmpty(), "tooltip");
        test_support::check(editor->getWantsKeyboardFocus(), "keyboard focus");

        checkLayoutContract(*editor, BrickMawAudioProcessorEditor::defaultWidth,
                            BrickMawAudioProcessorEditor::defaultHeight);
        checkLayoutContract(*editor, BrickMawAudioProcessorEditor::minimumWidth,
                            BrickMawAudioProcessorEditor::minimumHeight);
        checkLayoutContract(*editor, ehl::juce_design::Metrics::maximumWidth,
                            ehl::juce_design::Metrics::maximumHeight);
        checkParameterDisplayFollowsControls(*editor);
        checkPaintContract(*editor);
    });
}
