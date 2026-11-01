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
    for (auto* child : root.getChildren()) if (auto* found = findVisible(*child, id)) return found;
    return nullptr;
}
void click(juce::Component& root, const char* id)
{
    auto* button = dynamic_cast<juce::TextButton*>(findVisible(root, id));
    check(button != nullptr && button->isEnabled() && static_cast<bool>(button->onClick), id);
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
void changeSlider(juce::Component& editor, VantaProcessor& processor, vanta::Parameter parameter, const juce::String& prefix = "parameter-")
{
    const auto& spec = vanta::parameterSpecs[parameter];
    auto* slider = dynamic_cast<juce::Slider*>(findVisible(editor, prefix + spec.id));
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
    check(juce::PNGImageFormat().writeImageToStream(image, stream)
              && directory.getChildFile(name).replaceWithData(stream.getData(), stream.getDataSize()),
          "Editor preview PNG saved");
}
void testHostState()
{
    VantaProcessor processor;
    auto* legacyWave = processor.state.getParameter("wave");
    check(legacyWave != nullptr && legacyWave->getNumSteps() == 2
          && std::abs(legacyWave->convertTo0to1(1.0f) - 1.0f) < 0.0001f,
          "Legacy waveform automation remains a two-choice 0/1 parameter");
    auto original = vanta::defaultValues();
    original[vanta::wave] = 1.0f; original[vanta::cutoff] = 2400.0f;
    original[vanta::oscBEnabled] = 1.0f; original[vanta::oscBWave] = 2.0f;
    original[vanta::oscBFine] = -17.0f; original[vanta::oscBPan] = 0.5f;
    original[vanta::oscDEnabled] = 1.0f; original[vanta::oscHEnabled] = 1.0f;
    processor.applyValues(original, "Layered");
    juce::MemoryBlock saved; processor.getStateInformation(saved);
    processor.applyValues(vanta::defaultValues(), "Reset");
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    auto restored = processor.currentValues();
    check(processor.presetName() == "Layered" && restored[vanta::oscBEnabled] == 1.0f
          && restored[vanta::oscBWave] == 2.0f && restored[vanta::oscDEnabled] == 1.0f
          && restored[vanta::oscHEnabled] == 1.0f && std::abs(restored[vanta::oscBFine] + 17.0f) < 0.01f,
          "Host state saves and restores oscillator layers and settings");

    // Synthesize an old host state without any of the new oscillator parameters.
    auto old = processor.state.copyState();
    for (int i = old.getNumChildren() - 1; i >= 0; --i)
    {
        const auto child = old.getChild(i);
        if (child.getProperty("id").toString().startsWith("osc")) old.removeChild(i, nullptr);
    }
    auto xml = old.createXml(); juce::MemoryBlock oldBytes;
    juce::AudioProcessor::copyXmlToBinary(*xml, oldBytes);
    original[vanta::oscAExtraWave] = 2.0f; original[vanta::oscBEnabled] = 1.0f;
    processor.applyValues(original, "New state before old load");
    processor.setStateInformation(oldBytes.getData(), static_cast<int>(oldBytes.getSize()));
    restored = processor.currentValues();
    check(restored[vanta::wave] == 1.0f && restored[vanta::oscAExtraWave] == 0.0f
          && restored[vanta::oscALevel] == 1.0f && restored[vanta::oscBEnabled] == 0.0f
          && restored[vanta::oscDEnabled] == 0.0f && restored[vanta::oscHEnabled] == 0.0f,
          "Old host state restores legacy wave and defaults missing layers even after a newer state");

    // Beta 0.3 had only these four parameters and no preset-name property.
    auto beta03 = old.createCopy();
    beta03.removeProperty("presetName", nullptr);
    for (int i = beta03.getNumChildren() - 1; i >= 0; --i)
    {
        const auto id = beta03.getChild(i).getProperty("id").toString();
        if (id != "level" && id != "attack" && id != "release" && id != "wave")
            beta03.removeChild(i, nullptr);
    }
    auto beta03Xml = beta03.createXml(); juce::MemoryBlock beta03Bytes;
    juce::AudioProcessor::copyXmlToBinary(*beta03Xml, beta03Bytes);
    processor.applyValues(original, "A later patch");
    processor.setStateInformation(beta03Bytes.getData(), static_cast<int>(beta03Bytes.getSize()));
    restored = processor.currentValues();
    check(restored[vanta::wave] == 1.0f && restored[vanta::cutoff] == vanta::parameterSpecs[vanta::cutoff].defaultValue
          && restored[vanta::oscAExtraWave] == 0.0f && restored[vanta::oscBEnabled] == 0.0f
          && processor.presetName() == "VANTA INIT",
          "Beta 0.3-style host state restores four parameters and defaults all later additions");
}

void testImportedState()
{
    VantaProcessor processor;
    vanta::PresetResult preset; preset.ok = true; preset.name = "State with imported assets";
    preset.values = vanta::defaultValues(); preset.values[vanta::attack] = 0.001f;
    preset.values[vanta::oscATable] = 1; preset.values[vanta::oscDEnabled] = 1;
    preset.values[vanta::oscDSample] = 1; preset.values[vanta::oscDLevel] = 0.4f;
    preset.values[vanta::ottEnabled] = 1; preset.values[vanta::ottMix] = 0.4f;
    preset.waveTables = std::make_shared<vanta::WaveTableBank>();
    auto& bank = *preset.waveTables;
    bank.tables[0].frameCount = 1;
    for (int i = 0; i < vanta::wavetableFrameSize; ++i) bank.tables[0].samples[static_cast<std::size_t>(i)] = i < 1024 ? 0.7f : -0.7f;
    bank.sample.slot = 3; bank.sample.channels = 2; bank.sample.frames = 4096; bank.sample.sampleRate = 44100; bank.sample.loop = true;
    for (int i = 0; i < bank.sample.frames; ++i)
    {
        bank.sample.left[static_cast<std::size_t>(i)] = static_cast<float>(0.3 * std::sin(i * 0.03));
        bank.sample.right[static_cast<std::size_t>(i)] = -bank.sample.left[static_cast<std::size_t>(i)];
    }
    auto& mod = bank.modulation; mod.routeCount = 1; mod.routes[0] = { 0, 0, vanta::ModTarget::level, -0.5f, false };
    mod.lfos[0].rateHz = 0.01f; // exercise the lowest representable serialized rate
    for (auto& lfo : mod.lfos)
        for (int i = 0; i < vanta::importedLfoPoints; ++i) lfo.shape[static_cast<std::size_t>(i)] = static_cast<float>(i) / vanta::importedLfoPoints;
    processor.applyPreset(preset);
    auto render = [](VantaProcessor& synth)
    {
        synth.prepareToPlay(44100, 4096);
        juce::AudioBuffer<float> out(2, 4096); juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0);
        synth.processBlock(out, midi); return out;
    };
    const auto before = render(processor);
    juce::MemoryBlock stateBytes; processor.getStateInformation(stateBytes);
    const auto temporary = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("vanta-state-" + juce::Uuid().toString() + ".vanta");
    check(processor.savePresetFile(temporary).wasOk(), "Processor saves imported assets to a VANTA preset");
    const auto native = vanta::PresetCodec::loadFile(temporary);
    check(native.ok && native.waveTables != nullptr && native.waveTables->sample.frames == 4096 && native.waveTables->modulation.routeCount == 1,
          "Processor-saved VANTA file contains sample and modulation");
    temporary.deleteFile();
    processor.loadFactoryPreset(0);
    check(!processor.hasWaveTable(0) && !processor.hasSample(3), "Factory patch clears previous imported audio assets");
    processor.setStateInformation(stateBytes.getData(), static_cast<int>(stateBytes.getSize()));
    check(processor.hasWaveTable(0) && processor.hasSample(3) && processor.currentValues()[vanta::ottEnabled] == 1,
          "Host state restores imported wavetable, sample and multiband state");
    // A fresh instance avoids comparing the reverb's parameter-smoothing
    // startup transient before/after reusing an already-running engine.
    VantaProcessor fresh;
    fresh.setStateInformation(stateBytes.getData(), static_cast<int>(stateBytes.getSize()));
    const auto after = render(fresh);
    double delta = 0;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 4096; ++i) delta += std::abs(before.getSample(c, i) - after.getSample(c, i));
    if (delta >= 0.05) std::cerr << "Host audio delta: " << delta << "\n";
    check(delta < 0.05, "Host state restores the same rendered imported sound");

    auto broken = processor.state.copyState(); broken.setProperty("vantaSample", "{\"frames\":-1}", nullptr);
    if (auto xml = broken.createXml())
    {
        juce::MemoryBlock badState; juce::AudioProcessor::copyXmlToBinary(*xml, badState);
        const auto oldName = processor.presetName(); const auto oldValues = processor.currentValues();
        processor.setStateInformation(badState.getData(), static_cast<int>(badState.getSize()));
        check(processor.presetName() == oldName && processor.currentValues() == oldValues && processor.hasWaveTable(0) && processor.hasSample(3),
              "Malformed host asset data leaves the current patch intact");
    }
    const auto preservedName = processor.presetName(); const auto preservedValues = processor.currentValues();
    auto invalidXml = stateBytes;
    static_cast<std::uint8_t*>(invalidXml.getData())[8] = 0xff;
    processor.setStateInformation(invalidXml.getData(), static_cast<int>(invalidXml.getSize()));
    check(processor.presetName() == preservedName && processor.currentValues() == preservedValues && processor.hasWaveTable(0),
          "Invalid UTF-8 host state is rejected without changing the patch");
    const auto deepXml = juce::String::repeatedString("<a>", 100) + juce::String::repeatedString("</a>", 100);
    juce::MemoryOutputStream rawState;
    rawState.writeInt(0x21324356); rawState.writeInt(static_cast<int>(deepXml.getNumBytesAsUTF8()));
    rawState.write(deepXml.toRawUTF8(), deepXml.getNumBytesAsUTF8()); rawState.writeByte(0);
    processor.setStateInformation(rawState.getData(), static_cast<int>(rawState.getDataSize()));
    check(processor.presetName() == preservedName && processor.currentValues() == preservedValues && processor.hasWaveTable(0),
          "Excessively nested host XML is rejected without changing the patch");
    const auto invalid = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("vanta-bad-" + juce::Uuid().toString() + ".fxp");
    invalid.replaceWithText("broken"); const auto oldName = processor.presetName(); const auto oldValues = processor.currentValues();
    const auto error = processor.loadPresetFile(invalid); invalid.deleteFile();
    check(!error.ok && processor.presetName() == oldName && processor.currentValues() == oldValues && processor.hasWaveTable(0),
          "Malformed preset file leaves the current patch intact");
}

void testStandalone(const juce::File& previews)
{
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    VantaProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setVisible(true);
    check(editor->getWidth() == 1000 && editor->getHeight() == 700, "Standalone editor opens at 1000 x 700");
    auto* keyboard = dynamic_cast<VantaKeyboard*>(findVisible(*editor, "standalone-keyboard"));
    check(keyboard != nullptr, "Standalone keyboard is visible");
    check(keyboard->getOctaveForMiddleC() == 4, "Keyboard labels MIDI 60 as C4");
    const char* keys = "q2w3er5t6y7ui9o0p";
    bool mapped = true;
    for (int i = 0; i < 17; ++i) mapped &= keyboard->keyPressed(juce::KeyPress(keys[i], 0, 0));
    check(mapped && !keyboard->keyPressed(juce::KeyPress('a', 0, 0)), "Standalone uses the requested FL Studio key mapping");
    check(keyboard->keyPressed(juce::KeyPress('2', juce::ModifierKeys::shiftModifier, '@'))
          && !keyboard->keyPressed(juce::KeyPress('q', juce::ModifierKeys::ctrlModifier, 'q')),
          "Shifted digits remain playable while Ctrl shortcuts are not consumed");
    std::array<bool, 17> held {};
    bool correctNotes = true;
    for (int i = 0; i < 17; ++i)
    {
        held[static_cast<std::size_t>(i)] = true; keyboard->updateComputerKeys(held, false);
        correctNotes &= processor.keyboardState.isNoteOn(1, 60 + i);
        held[static_cast<std::size_t>(i)] = false; keyboard->updateComputerKeys(held, false);
        correctNotes &= !processor.keyboardState.isNoteOn(1, 60 + i);
    }
    held[0] = held[1] = true; keyboard->updateComputerKeys(held, false);
    correctNotes &= processor.keyboardState.isNoteOn(1, 60) && processor.keyboardState.isNoteOn(1, 61);
    held[0] = false; keyboard->updateComputerKeys(held, false);
    correctNotes &= !processor.keyboardState.isNoteOn(1, 60) && processor.keyboardState.isNoteOn(1, 61);
    keyboard->updateComputerKeys(held, true);
    correctNotes &= !processor.keyboardState.isNoteOn(1, 61);
    held[0] = true; keyboard->updateComputerKeys(held, false);
    keyboard->focusLost(juce::Component::focusChangedDirectly);
    correctNotes &= !processor.keyboardState.isNoteOn(1, 60) && !processor.keyboardState.isNoteOn(1, 61);
    check(correctNotes, "All 17 keys, chords, releases, shortcuts and focus loss produce correct MIDI transitions");
    juce::MidiBuffer discarded; processor.keyboardState.processNextMidiBuffer(discarded, 0, 4096, true);
    juce::Image pressedKey(juce::Image::ARGB, 30, 30, true);
    {
        // End the graphics context before reading pixels. Windows may defer
        // drawing until its Direct2D image context has been flushed.
        juce::Graphics keyGraphics(pressedKey);
        keyboard->drawBlackNote(61, keyGraphics, { 0.0f, 0.0f, 30.0f, 30.0f }, true, true, juce::Colours::black);
    }
    const auto topLeft = pressedKey.getPixelAt(0, 0);
    const auto centre = pressedKey.getPixelAt(15, 15);
    const auto bottomRight = pressedKey.getPixelAt(29, 29);
    if (topLeft != juce::Colour(0xffb73929) || centre != juce::Colour(0xffb73929) || bottomRight != juce::Colour(0xffb73929))
        std::cerr << "Black-key pixels: " << topLeft.toString() << ", " << centre.toString() << ", " << bottomRight.toString() << '\n';
    check(topLeft == juce::Colour(0xffb73929) && centre == juce::Colour(0xffb73929) && bottomRight == juce::Colour(0xffb73929),
          "Pressed black key is fully red, including edges");
    checkLayout(*editor); snapshot(*editor, previews, "overview.png");
    for (const auto parameter : { vanta::level, vanta::cutoff, vanta::resonance, vanta::attack, vanta::decay,
                                  vanta::sustain, vanta::release, vanta::vibrato, vanta::vibratoRate })
        changeSlider(*editor, processor, parameter);

    click(*editor, "tab-oscillator");
    checkLayout(*editor); snapshot(*editor, previews, "oscillator.png");
    auto* waveform = dynamic_cast<juce::ComboBox*>(findVisible(*editor, "parameter-wave"));
    check(waveform != nullptr, "OSC A waveform selector is visible");
    const auto sineView = editor->createComponentSnapshot(editor->getLocalBounds());
    waveform->setSelectedId(2, juce::sendNotificationSync);
    const auto sawView = editor->createComponentSnapshot(editor->getLocalBounds());
    check(processor.currentValues()[vanta::wave] == 1.0f, "OSC A saw updates the original parameter");
    check(changedPixels(sineView, sawView, { 54, 304, 532, 105 }) > 100, "Waveform preview follows the selected waveform");
    waveform->setSelectedId(3, juce::sendNotificationSync);
    check(processor.currentValues()[vanta::oscAExtraWave] == 1.0f, "OSC A square uses the appended shape parameter");
    waveform->setSelectedId(4, juce::sendNotificationSync);
    check(processor.currentValues()[vanta::oscAExtraWave] == 2.0f, "OSC A triangle uses the appended shape parameter");
    for (int layer = 1; layer < 4; ++layer) click(*editor, "add-oscillator");
    check(processor.currentValues()[vanta::oscBEnabled] == 1.0f && processor.currentValues()[vanta::oscCEnabled] == 1.0f
          && processor.currentValues()[vanta::oscDEnabled] == 1.0f, "Add tab creates four real oscillators");
    waveform = dynamic_cast<juce::ComboBox*>(findVisible(*editor, "parameter-oscDWave"));
    check(waveform != nullptr, "Selected OSC D has its own waveform selector");
    waveform->setSelectedId(3, juce::sendNotificationSync);
    changeSlider(*editor, processor, vanta::oscDLevel);
    changeSlider(*editor, processor, vanta::oscDTune);
    changeSlider(*editor, processor, vanta::oscDFine);
    changeSlider(*editor, processor, vanta::oscDPan);
    check(processor.currentValues()[vanta::oscDWave] == 2.0f && processor.currentValues()[vanta::oscAExtraWave] == 2.0f,
          "Each oscillator retains its own waveform and settings");
    checkLayout(*editor); snapshot(*editor, previews, "oscillator-four.png");
    click(*editor, "remove-oscillator");
    check(processor.currentValues()[vanta::oscDEnabled] == 0.0f && findVisible(*editor, "osc-tab-D") == nullptr,
          "Remove disables the selected oscillator");
    for (int layer = 3; layer < vanta::oscillatorCount; ++layer) click(*editor, "add-oscillator");
    auto* addButton = dynamic_cast<juce::TextButton*>(findVisible(*editor, "add-oscillator"));
    check(addButton != nullptr && !addButton->isEnabled() && processor.currentValues()[vanta::oscHEnabled] == 1.0f,
          "Eight-oscillator limit disables add");
    checkLayout(*editor);
    click(*editor, "osc-tab-A");
    check(findVisible(*editor, "remove-oscillator") == nullptr, "OSC A cannot be removed");

    click(*editor, "tab-effects"); checkLayout(*editor); snapshot(*editor, previews, "effects.png");
    const auto distortionBefore = editor->createComponentSnapshot(editor->getLocalBounds());
    auto* drive = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-distortionDrive"));
    check(drive != nullptr, "Distortion drive control is visible");
    drive->setValue(8.0, juce::sendNotificationSync);
    const auto distortionAfter = editor->createComponentSnapshot(editor->getLocalBounds());
    check(changedPixels(distortionBefore, distortionAfter, { 668, 273, 268, 135 }) > 30, "Distortion sketch follows drive");
    for (const auto parameter : { vanta::reverbSize, vanta::reverbDecay, vanta::reverbMix, vanta::delayTime,
                                  vanta::delayFeedback, vanta::delayMix, vanta::distortionDrive, vanta::distortionTone, vanta::distortionMix })
        changeSlider(*editor, processor, parameter);
    click(*editor, "tab-modulation"); checkLayout(*editor); snapshot(*editor, previews, "modulation.png");
    const auto envelopeBefore = editor->createComponentSnapshot(editor->getLocalBounds());
    auto* attack = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-attack"));
    check(attack != nullptr, "Modulation attack control is visible");
    attack->setValue(1.9, juce::sendNotificationSync);
    const auto envelopeAfter = editor->createComponentSnapshot(editor->getLocalBounds());
    check(changedPixels(envelopeBefore, envelopeAfter, { 54, 272, 532, 120 }) > 30, "Envelope preview follows the controls");
    for (const auto parameter : { vanta::attack, vanta::decay, vanta::sustain, vanta::release, vanta::vibrato, vanta::vibratoRate })
        changeSlider(*editor, processor, parameter);
    click(*editor, "tab-mixer"); checkLayout(*editor); snapshot(*editor, previews, "mixer.png");
    changeSlider(*editor, processor, vanta::oscBLevel, "mixer-");
    changeSlider(*editor, processor, vanta::oscBPan, "mixer-");
    auto* bToggle = dynamic_cast<juce::TextButton*>(findVisible(*editor, "mixer-osc-B-enabled"));
    check(bToggle != nullptr && bToggle->getToggleState(), "Mixer shows enabled layer");
    bToggle->setToggleState(false, juce::sendNotificationSync);
    check(processor.currentValues()[vanta::oscBEnabled] == 0.0f, "Mixer enable switch changes the audio parameter");
    click(*editor, "tab-sequencer"); checkLayout(*editor); snapshot(*editor, previews, "sequencer-planned.png");
    click(*editor, "tab-settings"); checkLayout(*editor); snapshot(*editor, previews, "settings.png");

    click(*editor, "preset-browser"); checkLayout(*editor); snapshot(*editor, previews, "browser.png");
    click(*editor, "folder-pads");
    check(findVisible(*editor, "factory-preset-2") != nullptr && findVisible(*editor, "factory-preset-3") == nullptr,
          "Preset browser filters categories");
    auto* search = dynamic_cast<juce::TextEditor*>(findVisible(*editor, "preset-search"));
    check(search != nullptr, "Preset search is visible");
    // TextEditor normally posts this callback to the message loop. Invoke it
    // explicitly here because the off-screen test does not run a dispatch loop.
    search->setText("night", false); search->onTextChange();
    check(findVisible(*editor, "factory-preset-2") == nullptr, "Search and category filters combine");
    search->setText("red", false); search->onTextChange();
    check(findVisible(*editor, "factory-preset-2") != nullptr, "Preset search matches names");
    click(*editor, "factory-preset-2");
    check(processor.presetName() == "RED HORIZON" && findVisible(*editor, "folder-pads") == nullptr
          && processor.currentValues()[vanta::oscBEnabled] == 0.0f,
          "Selecting a factory preset loads it, closes the browser and resets layer enables");
    click(*editor, "preset-next"); check(processor.presetName() == "NIGHT CIRCUIT", "Next preset button loads the next sound");
    click(*editor, "preset-previous"); check(processor.presetName() == "RED HORIZON", "Previous preset button loads the previous sound");

    processor.prepareToPlay(44100.0, 4096);
    processor.keyboardState.noteOn(1, 69, 0.8f);
    juce::MidiBuffer midi; juce::AudioBuffer<float> buffer(2, 4096);
    processor.processBlock(buffer, midi);
    check(buffer.getRMSLevel(0, 2048, 1024) > 0.0001f, "Standalone virtual keyboard reaches the audio engine");
    processor.keyboardState.noteOff(1, 69, 0.0f); midi.clear(); processor.processBlock(buffer, midi); processor.releaseResources();
}
void testVstEditor()
{
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_VST3);
    VantaProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor()); editor->setVisible(true);
    check(findVisible(*editor, "standalone-keyboard") == nullptr, "VST3 does not add the standalone keyboard");
    checkLayout(*editor);
    processor.prepareToPlay(44100.0, 4096);
    processor.keyboardState.noteOn(1, 69, 0.8f);
    juce::MidiBuffer midi; juce::AudioBuffer<float> buffer(2, 4096);
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
    testHostState(); testImportedState(); testStandalone(previewDirectory); testVstEditor();
    std::cout << "All VANTA editor checks passed.\n";
    return 0;
}
