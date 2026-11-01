#include "PluginEditor.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
    std::cout << "PASS: " << message << '\n';
}
juce::Component* findVisible(juce::Component& root, const juce::String& id)
{
    if (!root.isVisible()) return nullptr;
    if (root.getComponentID() == id) return &root;
    for (auto* child : root.getChildren())
        if (auto* found = findVisible(*child, id)) return found;
    return nullptr;
}
void click(juce::Component& root, const char* id)
{
    auto* button = dynamic_cast<juce::TextButton*>(findVisible(root, id));
    check(button != nullptr && static_cast<bool>(button->onClick), id);
    button->onClick();
}
void checkLayout(juce::Component& editor)
{
    const auto children = editor.getChildren();
    bool withinBounds = true, noOverlaps = true;
    for (int i = 0; i < children.size(); ++i)
    {
        const auto* a = children[i];
        if (!a->isVisible()) continue;
        withinBounds &= editor.getLocalBounds().contains(a->getBounds());
        for (int j = i + 1; j < children.size(); ++j)
        {
            const auto* b = children[j];
            if (b->isVisible() && a->getBounds().intersects(b->getBounds()))
            {
                std::cerr << "Overlapping controls: " << a->getComponentID() << " and " << b->getComponentID() << '\n';
                noOverlaps = false;
            }
        }
    }
    check(withinBounds, "Visible controls stay within the editor");
    check(noOverlaps, "Visible controls do not overlap");
}
void changeSlider(juce::Component& editor, VantaProcessor& processor, vanta::Parameter parameter)
{
    const auto& spec = vanta::parameterSpecs[parameter];
    auto* slider = dynamic_cast<juce::Slider*>(findVisible(editor, "parameter-" + juce::String(spec.id)));
    check(slider != nullptr, spec.name);
    const auto* raw = processor.state.getRawParameterValue(spec.id);
    check(raw != nullptr && std::abs(raw->load() - static_cast<float>(slider->getValue())) < 0.01f,
          "Slider reflects the current synth parameter");
    slider->setValue(spec.minValue + (spec.maxValue - spec.minValue) * 0.37, juce::sendNotificationSync);
    check(std::abs(raw->load() - static_cast<float>(slider->getValue())) < 0.01f,
          "Slider updates the synth parameter");
}
int changedPixels(const juce::Image& before, const juce::Image& after, juce::Rectangle<int> area)
{
    int changed = 0;
    for (int y = area.getY(); y < area.getBottom(); y += 2)
        for (int x = area.getX(); x < area.getRight(); x += 2)
            if (before.getPixelAt(x, y) != after.getPixelAt(x, y)) ++changed;
    return changed;
}
void snapshot(juce::Component& editor, const juce::File& directory, const char* name)
{
    const auto image = editor.createComponentSnapshot(editor.getLocalBounds());
    check(image.isValid() && image.getWidth() == 1000 && image.getHeight() == 700, "Editor snapshot renders at the expected size");
    juce::MemoryOutputStream stream;
    const auto file = directory.getChildFile(name);
    check(juce::PNGImageFormat().writeImageToStream(image, stream)
              && file.replaceWithData(stream.getData(), stream.getDataSize()),
          "Editor preview PNG saved");
}
void testStandalone(const juce::File& previews)
{
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    VantaProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setVisible(true);
    check(editor->getWidth() == 1000 && editor->getHeight() == 700, "Standalone editor opens at 1000 x 700");
    check(findVisible(*editor, "standalone-keyboard") != nullptr, "Standalone keyboard is visible");
    checkLayout(*editor);
    snapshot(*editor, previews, "overview.png");

    for (const auto parameter : { vanta::level, vanta::cutoff, vanta::resonance, vanta::attack, vanta::decay,
                                  vanta::sustain, vanta::release, vanta::vibrato, vanta::vibratoRate })
        changeSlider(*editor, processor, parameter);
    auto* waveform = dynamic_cast<juce::ComboBox*>(findVisible(*editor, "parameter-wave"));
    check(waveform != nullptr, "Waveform selector is visible on overview");
    const auto sineView = editor->createComponentSnapshot(editor->getLocalBounds());
    waveform->setSelectedId(2, juce::sendNotificationSync);
    const auto sawView = editor->createComponentSnapshot(editor->getLocalBounds());
    check(std::abs(processor.currentValues()[vanta::wave] - 1.0f) < 0.01f, "Waveform selector changes the synth");
    check(changedPixels(sineView, sawView, { 44, 220, 490, 96 }) > 100, "Waveform preview follows the selected waveform");

    click(*editor, "tab-oscillator");
    checkLayout(*editor); snapshot(*editor, previews, "oscillator.png");
    check(findVisible(*editor, "parameter-wave") != nullptr && findVisible(*editor, "parameter-cutoff") != nullptr,
          "Oscillator page exposes waveform and filter");
    click(*editor, "tab-effects");
    checkLayout(*editor); snapshot(*editor, previews, "effects.png");
    const auto distortionBefore = editor->createComponentSnapshot(editor->getLocalBounds());
    auto* drive = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-distortionDrive"));
    check(drive != nullptr, "Distortion drive control is visible");
    drive->setValue(8.0, juce::sendNotificationSync);
    const auto distortionAfter = editor->createComponentSnapshot(editor->getLocalBounds());
    check(changedPixels(distortionBefore, distortionAfter, { 688, 250, 268, 144 }) > 30, "Distortion sketch follows drive");
    for (const auto parameter : { vanta::reverbSize, vanta::reverbDecay, vanta::reverbMix, vanta::delayTime,
                                  vanta::delayFeedback, vanta::delayMix, vanta::distortionDrive, vanta::distortionTone, vanta::distortionMix })
        changeSlider(*editor, processor, parameter);
    click(*editor, "tab-modulation");
    checkLayout(*editor); snapshot(*editor, previews, "modulation.png");
    const auto envelopeBefore = editor->createComponentSnapshot(editor->getLocalBounds());
    auto* attack = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-attack"));
    check(attack != nullptr, "Modulation attack control is visible");
    attack->setValue(1.9, juce::sendNotificationSync);
    const auto envelopeAfter = editor->createComponentSnapshot(editor->getLocalBounds());
    check(changedPixels(envelopeBefore, envelopeAfter, { 44, 249, 576, 139 }) > 30, "Envelope preview follows the controls");
    for (const auto parameter : { vanta::attack, vanta::decay, vanta::sustain, vanta::release, vanta::vibrato, vanta::vibratoRate })
        changeSlider(*editor, processor, parameter);
    click(*editor, "tab-settings"); checkLayout(*editor); snapshot(*editor, previews, "settings.png");

    click(*editor, "preset-browser"); checkLayout(*editor); snapshot(*editor, previews, "browser.png");
    click(*editor, "folder-pads");
    check(findVisible(*editor, "factory-preset-2") != nullptr && findVisible(*editor, "factory-preset-3") == nullptr,
          "Preset browser filters categories");
    click(*editor, "factory-preset-2");
    check(processor.presetName() == "RED HORIZON" && findVisible(*editor, "folder-pads") == nullptr,
          "Selecting a factory preset loads it and closes the browser");
    click(*editor, "preset-next");
    check(processor.presetName() == "NIGHT CIRCUIT", "Next preset button loads the next sound");
    click(*editor, "preset-previous");
    check(processor.presetName() == "RED HORIZON", "Previous preset button loads the previous sound");

    processor.prepareToPlay(44100.0, 4096);
    processor.keyboardState.noteOn(1, 69, 0.8f);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(2, 4096);
    processor.processBlock(buffer, midi);
    check(buffer.getRMSLevel(0, 2048, 1024) > 0.0001f, "Standalone virtual keyboard reaches the processor's audio engine");
    processor.keyboardState.noteOff(1, 69, 0.0f);
    midi.clear();
    processor.processBlock(buffer, midi);
    processor.releaseResources();
}
void testVstEditor()
{
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_VST3);
    VantaProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setVisible(true);
    check(findVisible(*editor, "standalone-keyboard") == nullptr, "VST3 does not add the standalone keyboard");
    checkLayout(*editor);
    processor.prepareToPlay(44100.0, 4096);
    processor.keyboardState.noteOn(1, 69, 0.8f);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(2, 4096);
    processor.processBlock(buffer, midi);
    check(buffer.getRMSLevel(0, 0, 4096) == 0.0f, "Standalone keyboard state does not inject MIDI into the VST3");
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0);
    processor.processBlock(buffer, midi);
    check(buffer.getRMSLevel(0, 2048, 1024) > 0.001f, "VST3 still responds to host MIDI");
    processor.releaseResources();
}
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    const auto previewDirectory = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])
                                           : juce::File::getCurrentWorkingDirectory().getChildFile("ui-preview");
    check(previewDirectory.createDirectory().wasOk(), "Preview directory created");
    testStandalone(previewDirectory);
    testVstEditor();
    std::cout << "All VANTA editor checks passed.\n";
    return 0;
}
