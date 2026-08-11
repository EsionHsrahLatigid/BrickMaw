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
        }

        juce::Image image(juce::Image::RGB, 320, 200, true);
        juce::Graphics g(image);
        editor->setBounds(0, 0, image.getWidth(), image.getHeight());
        editor->resized();
        editor->paint(g);
        const auto first = image.getPixelAt(0, 0);
        bool varied = false;
        for (int y = 0; y < image.getHeight(); y += 16)
            for (int x = 0; x < image.getWidth(); x += 16)
                varied = varied || image.getPixelAt(x, y) != first;
        test_support::check(varied, "software paint uses monochrome palette and procedural motif");
    });
}
