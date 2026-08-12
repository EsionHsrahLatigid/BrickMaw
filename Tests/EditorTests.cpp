#include "TestSupport.h"
#include "ParameterIDs.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <array>
#include <string>

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    return test_support::run("brickmaw_editor_tests", [] {
        BrickMawAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        auto* custom = dynamic_cast<BrickMawAudioProcessorEditor*>(editor.get());
        test_support::check(custom != nullptr, "custom editor type, not GenericAudioProcessorEditor");
        test_support::check(dynamic_cast<juce::GenericAudioProcessorEditor*>(editor.get()) == nullptr, "not GenericAudioProcessorEditor");
        test_support::check(editor->getWidth() == BrickMawAudioProcessorEditor::defaultWidth, "default width");
        test_support::check(editor->getHeight() == BrickMawAudioProcessorEditor::defaultHeight, "default height");
        test_support::check(editor->getComponentID() == "brickmaw-editor", "component id");
        test_support::check(editor->getName().isNotEmpty(), "accessible name");
        test_support::check(custom->getTooltip().isNotEmpty(), "tooltip");
        test_support::check(editor->getWantsKeyboardFocus(), "keyboard focus");

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
        for (const auto* id : ids)
        {
            auto* control = editor->findChildWithID(juce::String("brickmaw-") + id);
            test_support::check(control != nullptr, std::string("editor exposes control for ") + id);
            test_support::check(control->getName().isNotEmpty(), std::string("control has accessible name for ") + id);
            test_support::check(control->getProperties()["brickmawTooltip"].toString().isNotEmpty(), std::string("control has tooltip for ") + id);
            test_support::check(control->getWantsKeyboardFocus(), std::string("control has keyboard focus for ") + id);
            const auto bounds = control->getBounds();
            test_support::check(! bounds.isEmpty(), std::string("initial parameter control bounds are non-empty for ") + id);
            test_support::check(bounds.getY() >= 80, std::string("initial parameter control stays below simple header for ") + id);
            test_support::check(bounds.getRight() <= editor->getWidth(), std::string("initial parameter control stays inside editor width for ") + id);
            test_support::check(bounds.getBottom() <= editor->getHeight(), std::string("initial parameter control stays inside editor height for ") + id);
        }

        juce::Image image(juce::Image::RGB, 320, 200, true);
        juce::Graphics g(image);
        editor->setBounds(0, 0, image.getWidth(), image.getHeight());
        editor->resized();
        editor->paint(g);
        const auto background = juce::Colour(0xff050505);
        const auto divider = juce::Colour(0xff2a2a2a);
        bool headerHasInk = false;
        bool separatorBandIsSimple = true;
        bool bodyIsPlain = true;
        int maxChannelSpread = 0;
        for (int y = 0; y < image.getHeight(); ++y)
        {
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const auto pixel = image.getPixelAt(x, y);
                headerHasInk = headerHasInk || (y < 64 && pixel != background);
                if (y >= 64 && y < 80)
                {
                    const bool onDivider = y == 72 && x >= 32 && x < image.getWidth() - 32;
                    separatorBandIsSimple = separatorBandIsSimple && pixel == (onDivider ? divider : background);
                }
                bodyIsPlain = bodyIsPlain && (y < 80 || pixel == background);
                const int red = pixel.getRed();
                const int green = pixel.getGreen();
                const int blue = pixel.getBlue();
                const int high = juce::jmax(juce::jmax(red, green), blue);
                const int low = juce::jmin(juce::jmin(red, green), blue);
                maxChannelSpread = juce::jmax(maxChannelSpread, high - low);
            }
        }
        test_support::check(headerHasInk, "simple header paints product text above divider");
        test_support::check(separatorBandIsSimple, "paint keeps only one divider in the 64px to 79px separator band");
        test_support::check(bodyIsPlain, "paint has no grid, motif, frame, or parameter-driven decoration below y=80");
        // The DHN9 paper/mid colours intentionally use a slight warm monochrome offset.
        test_support::check(maxChannelSpread <= 4,
                            "paint stays monochrome within DHN9 palette tolerance, max spread "
                                + std::to_string(maxChannelSpread));
    });
}
