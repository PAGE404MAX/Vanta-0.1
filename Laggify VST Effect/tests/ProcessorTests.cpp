#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
    std::cout << "PASS: " << message << '\n';
}
juce::Component* find(juce::Component& root, const juce::String& id)
{
    if (!root.isVisible())
        return nullptr;
    if (root.getComponentID() == id)
        return &root;
    for (auto* c : root.getChildren())
        if (auto* r = find(*c, id))
            return r;
    return nullptr;
}
void click(juce::Component& root, const juce::String& id)
{
    auto* b = dynamic_cast<juce::TextButton*>(find(root, id));
    check(b != nullptr && static_cast<bool>(b->onClick), id.toRawUTF8());
    b->onClick();
}
void checkHit(juce::Component& editor, const juce::String& id)
{
    auto* target = find(editor, id);
    check(target != nullptr, "Scaled target is visible");
    const auto position = editor.getLocalPoint(target, target->getLocalBounds().getCentre());
    check(editor.getComponentAt(position) == target, "Scaled target receives mouse hits");
}
void layout(juce::Component& editor)
{
    auto* canvas = find(editor, "laggify-canvas");
    check(canvas != nullptr, "Canvas is visible");
    bool inside = editor.getLocalBounds().expanded(1).contains(editor.getLocalArea(canvas, canvas->getLocalBounds())),
         overlap = false;
    auto children = canvas->getChildren();
    for (int i = 0; i < children.size(); ++i)
    {
        auto* a = children[i];
        if (!a->isVisible() || a->getComponentID() == "resize-overlay")
            continue;
        inside &= canvas->getLocalBounds().contains(a->getBounds());
        for (int j = i + 1; j < children.size(); ++j)
        {
            auto* b = children[j];
            if (b->isVisible() && b->getComponentID() != "resize-overlay" && a->getBounds().intersects(b->getBounds()))
            {
                std::cerr << "Overlap: " << a->getComponentID() << " / " << b->getComponentID() << '\n';
                overlap = true;
            }
        }
    }
    check(inside, "Controls fit within editor");
    check(!overlap, "Controls do not overlap");
}
void snapshot(juce::Component& editor, const juce::File& dir, const char* filename)
{
    const auto image = editor.createComponentSnapshot(editor.getLocalBounds());
    juce::MemoryOutputStream stream;
    check(image.isValid() && juce::PNGImageFormat().writeImageToStream(image, stream) &&
              dir.getChildFile(filename).replaceWithData(stream.getData(), stream.getDataSize()),
          "Native editor snapshot saved");
}
struct PlayHead final : juce::AudioPlayHead
{
    PositionInfo p;
    juce::Optional<PositionInfo> getPosition() const override
    {
        return p;
    }
};
struct GestureListener final : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0, changes = 0;
    void parameterValueChanged(int, float) override { ++changes; }
    void parameterGestureChanged(int, bool starting) override
    {
        if (starting)
            ++begins;
        else
            ++ends;
    }
};
void testProcessor()
{
    LaggifyProcessor p;
    check(!p.acceptsMidi() && !p.producesMidi() && !p.isMidiEffect() && p.getMainBusNumInputChannels() == 2 &&
              p.getMainBusNumOutputChannels() == 2,
          "Laggify is a stereo audio insert, not a synth or MIDI effect");
    auto layout = p.getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::mono());
    layout.outputBuses.set(0, juce::AudioChannelSet::mono());
    check(p.isBusesLayoutSupported(layout), "Mono input/output is supported");
    layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
    check(!p.isBusesLayoutSupported(layout), "Mismatched input/output layouts are rejected");
    check(p.getParameters().size() == 38 && p.currentPattern(true) == 15 && p.currentPattern(false) == 0,
          "Stable independent parameters and default reference pattern are present");
    p.applyPattern(true, 1);
    p.applyPattern(false, 35);
    auto s = p.currentSettings();
    check(std::abs(s.time[8] - .25f) < .001f && s.volume[2] == 0 && p.currentPattern(true) == 1 &&
              p.currentPattern(false) == 35,
          "Preset buttons load real independent time and volume parameters");
    p.setParameterValue(LaggifyProcessor::pointId(true, 5), .777f);
    check(p.currentPattern(true) == -1 && p.currentPattern(false) == 35,
          "Host-automated point values show custom without disturbing the other bank");
    p.setParameterValue("mix", .63f);
    p.setParameterValue("enabled", 1);
    p.setEditorScalePercent(125);
    p.setEditTime(false);
    juce::MemoryBlock state;
    p.getStateInformation(state);
    LaggifyProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    auto rs = restored.currentSettings();
    check(std::abs(rs.time[5] - .777f) < .002f && rs.volume[2] == 0 && std::abs(rs.mix - .63f) < .002f && rs.enabled &&
              restored.currentPattern(true) == -1 && restored.currentPattern(false) == 35 &&
              restored.editorScalePercent() == 125 && !restored.editTime(),
          "Host state round-trips curves, mix, selected bank and UI size");
    const auto before = restored.currentSettings();
    char broken[] = {1, 2, 3, 4};
    restored.setStateInformation(broken, 4);
    check(restored.currentSettings().time == before.time, "Malformed state is ignored without altering the patch");
    auto tree = restored.state.copyState();
    auto child = tree.getChild(0);
    child.setProperty("value", "NaN", nullptr);
    juce::MemoryBlock invalid;
    if (auto xml = tree.createXml())
        juce::AudioProcessor::copyXmlToBinary(*xml, invalid);
    restored.setStateInformation(invalid.getData(), static_cast<int>(invalid.getSize()));
    check(restored.currentSettings().time == before.time && restored.currentSettings().enabled == before.enabled,
          "Non-finite host state is rejected atomically");
    LaggifyProcessor host;
    host.applyPattern(true, 0);
    host.applyPattern(false, 0);
    host.setParameterValue("enabled", 1);
    host.setParameterValue(LaggifyProcessor::pointId(false, 1), 0);
    PlayHead clock;
    clock.p.setBpm(120);
    clock.p.setPpqPosition(.25);
    clock.p.setIsPlaying(true);
    host.setPlayHead(&clock);
    host.prepareToPlay(44100, 8192);
    juce::AudioBuffer<float> audio(2, 8192);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < audio.getNumSamples(); ++i)
            audio.setSample(c, i, c == 0 ? .5f : .3f);
    juce::MidiBuffer midi;
    host.processBlock(audio, midi);
    check(audio.getRMSLevel(0, 2500, 1500) < .01f && audio.getRMSLevel(1, 2500, 1500) < .01f,
          "Real processor gates both input channels at the supplied host PPQ");
    juce::AudioBuffer<float> bypass(2, 1000);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < bypass.getNumSamples(); ++i)
            bypass.setSample(c, i, c == 0 ? 1.5f : -.7f);
    host.processBlockBypassed(bypass, midi);
    check(bypass.getSample(0, 10) == 1.5f && bypass.getSample(1, 10) == -.7f,
          "Host bypass passes mixer audio unchanged even after active processing");
    host.setPlayHead(nullptr);
}
void testEditor(const juce::File& dir)
{
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    LaggifyProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    editor->setVisible(true);
    check(editor->getWidth() == 1000 && editor->getHeight() == 820 && editor->isResizable(),
          "Native editor has the approved aspect and resize support");
    layout(*editor);
    bool bankNames = true;
    for (int i = 0; i < laggify::bankSize; ++i)
    {
        auto* t = dynamic_cast<juce::TextButton*>(find(*editor, "time-preset-" + juce::String(i)));
        auto* v = dynamic_cast<juce::TextButton*>(find(*editor, "volume-preset-" + juce::String(i)));
        bankNames &= t != nullptr && v != nullptr && t->getButtonText() == laggify::timeNames[i] &&
                     v->getButtonText() == laggify::volumeNames[i];
    }
    check(bankNames && find(*editor, "laggify-curve"), "All 72 named bank buttons and the large graph are visible");
    snapshot(*editor, dir, "laggify-time.png");
    click(*editor, "volume-preset-35");
    check(!p.editTime() && p.currentPattern(false) == 35,
          "Selecting Mute switches to volume and updates the processor");
    snapshot(*editor, dir, "laggify-volume.png");
    click(*editor, "time-preset-1");
    check(p.editTime() && p.currentPattern(true) == 1 && std::abs(p.currentSettings().time[8] - .25f) < .001f,
          "Selecting Basic 1 loads the half-speed time curve");
    auto* curve = find(*editor, "laggify-curve");
    check(curve && curve->keyPressed(juce::KeyPress(juce::KeyPress::downKey)) && p.currentPattern(true) == -1,
          "Arrow keys edit the active time curve");
    click(*editor, "tool-line");
    check(p.currentSettings().timeSmooth, "Line tool interpolates the active time lane");
    click(*editor, "tool-step");
    check(!p.currentSettings().timeSmooth, "Step tool holds the active time lane");
    const auto now = juce::Time::getCurrentTime();
    auto mouse = [&](juce::Point<float> pt)
    {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), pt,
                                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, curve, curve, now, pt, now, 1, false);
    };
    const auto start = juce::Point<float>(curve->getWidth() * .27f, curve->getHeight() * .8f);
    const auto end = juce::Point<float>(curve->getWidth() * .52f, curve->getHeight() * .3f);
    GestureListener gestures;
    auto* point = p.pointParameter(true, 4);
    check(point != nullptr, "Editable time point exists");
    point->addListener(&gestures);
    curve->mouseDown(mouse(start));
    curve->mouseDrag(mouse(end));
    curve->mouseUp(mouse(end));
    point->removeListener(&gestures);
    check(gestures.begins == 1 && gestures.ends == 1 && gestures.changes > 0,
          "Curve dragging emits balanced host automation gestures");
    auto drawn = p.currentSettings();
    check(drawn.time[4] > .7f && drawn.time[8] < .4f && drawn.time[6] > .4f,
          "Dragging across time edits and interpolates multiple points");
    click(*editor, "mode-volume");
    const auto volumePoint = juce::Point<float>(curve->getWidth() * .1f, curve->getHeight() * .8f);
    curve->mouseDown(mouse(volumePoint));
    curve->mouseUp(mouse(volumePoint));
    check(p.currentSettings().volume[1] < .3f && p.currentSettings().time == drawn.time,
          "Drawing volume changes only the volume lane");
    auto* power = dynamic_cast<juce::TextButton*>(find(*editor, "parameter-enabled"));
    check(power != nullptr, "Power control is visible");
    power->setToggleState(true, juce::sendNotificationSync);
    check(p.currentSettings().enabled, "Power button updates the processor");
    auto* mix = dynamic_cast<juce::Slider*>(find(*editor, "parameter-mix"));
    auto* bpm = dynamic_cast<juce::Slider*>(find(*editor, "parameter-bpm"));
    check(mix && bpm, "Mix and fallback BPM controls are visible");
    mix->setValue(.44, juce::sendNotificationSync);
    bpm->setValue(95, juce::sendNotificationSync);
    auto* cycle = dynamic_cast<juce::ComboBox*>(find(*editor, "parameter-cycle"));
    check(cycle != nullptr, "Cycle control is visible");
    cycle->setSelectedId(3, juce::sendNotificationSync);
    check(std::abs(p.currentSettings().mix - .44f) < .002f && p.currentSettings().fallbackBpm == 95 &&
              p.currentSettings().cycle == 2,
          "Mix, BPM and cycle controls update their parameters");
    juce::MemoryBlock edited;
    p.getStateInformation(edited);
    LaggifyProcessor reopened;
    reopened.setStateInformation(edited.getData(), static_cast<int>(edited.getSize()));
    check(reopened.currentSettings().time == p.currentSettings().time &&
              reopened.currentSettings().volume == p.currentSettings().volume,
          "Both drawn curves survive host-state restoration");
    click(*editor, "mode-time");
    click(*editor, "tool-reset");
    check(p.currentPattern(true) == 0, "Reset restores Empty for the active lane");
    click(*editor, "resize-logo");
    check(find(*editor, "resize-overlay") != nullptr, "V logo opens the size menu");
    snapshot(*editor, dir, "laggify-resize-menu.png");
    click(*editor, "resize-75");
    check(editor->getWidth() == 750 && editor->getHeight() == 615 && p.editorScalePercent() == 75,
          "75% size resizes the host view");
    layout(*editor);
    checkHit(*editor, "resize-logo");
    checkHit(*editor, "time-preset-35");
    checkHit(*editor, "laggify-curve");
    click(*editor, "resize-logo");
    auto* overlay = find(*editor, "resize-overlay");
    check(overlay && overlay->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) &&
              find(*editor, "resize-overlay") == nullptr,
          "Escape dismisses resize menu");
    click(*editor, "resize-logo");
    click(*editor, "resize-60");
    check(editor->getWidth() == 600 && editor->getHeight() == 492, "60% sets the minimum size");
    layout(*editor);
    checkHit(*editor, "time-preset-35");
    checkHit(*editor, "laggify-curve");
    click(*editor, "resize-logo");
    click(*editor, "resize-150");
    check(editor->getWidth() == 1500 && editor->getHeight() == 1230, "150% sets the maximum size");
    layout(*editor);
    checkHit(*editor, "time-preset-35");
    checkHit(*editor, "laggify-curve");
    click(*editor, "resize-logo");
    click(*editor, "resize-100");
    check(editor->getWidth() == 1000 && editor->getHeight() == 820, "100% restores the default view");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI app;
    testProcessor();
    const auto dir = argc > 1
                         ? juce::File(juce::String(argv[1]))
                         : juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("laggify-preview");
    dir.createDirectory();
    testEditor(dir);
    std::cout << "Laggify processor and editor tests passed.\n";
}
