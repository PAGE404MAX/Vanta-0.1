#include "PluginProcessor.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <set>

namespace
{
void check(bool ok, const char* text)
{
    if (!ok)
    {
        std::cerr << "FAIL: " << text << '\n';
        std::exit(1);
    }
    std::cout << "PASS: " << text << '\n';
}
bool near(float a, float b, float eps = .002f)
{
    return std::abs(a - b) < eps;
}
struct GestureListener final : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0, changes = 0;
    void parameterValueChanged(int, float) override
    {
        ++changes;
    }
    void parameterGestureChanged(int, bool starting) override
    {
        if (starting)
            ++begins;
        else
            ++ends;
    }
};
juce::Component* find(juce::Component& root, const juce::String& id)
{
    if (!root.isVisible())
        return nullptr;
    if (root.getComponentID() == id)
        return &root;
    for (auto* child : root.getChildren())
        if (auto* found = find(*child, id))
            return found;
    return nullptr;
}
void click(juce::Component& root, const juce::String& id)
{
    auto* button = dynamic_cast<juce::TextButton*>(find(root, id));
    check(button != nullptr && static_cast<bool>(button->onClick), id.toRawUTF8());
    button->onClick();
}
void snapshot(juce::Component& editor, const juce::File& dir, const char* name)
{
    const auto image = editor.createComponentSnapshot(editor.getLocalBounds());
    juce::MemoryOutputStream stream;
    check(image.isValid() && juce::PNGImageFormat().writeImageToStream(image, stream) &&
              dir.getChildFile(name).replaceWithData(stream.getData(), stream.getDataSize()),
          "Native editor snapshot saved");
}
void checkLayout(juce::Component& editor)
{
    auto* canvas = find(editor, "sidechain-canvas");
    check(canvas != nullptr, "Native canvas is visible");
    bool fits = editor.getLocalBounds().expanded(1).contains(editor.getLocalArea(canvas, canvas->getLocalBounds())),
         overlap = false;
    auto children = canvas->getChildren();
    for (int i = 0; i < children.size(); ++i)
    {
        auto* a = children[i];
        if (!a->isVisible() || a->getComponentID() == "resize-overlay")
            continue;
        fits &= canvas->getLocalBounds().contains(a->getBounds());
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
    check(fits, "Controls fit the native editor");
    check(!overlap, "Native controls do not overlap");
}
void testProcessor()
{
    SidechainProcessor p;
    check(p.acceptsMidi() && !p.producesMidi() && !p.isMidiEffect(), "Sidechain is a MIDI-triggerable audio effect");
    check(p.getBusCount(true) == 2 && p.getBusCount(false) == 1 && p.getBus(true, 1)->getName() == "Sidechain",
          "Plugin exposes a separate named sidechain input");
    auto layout = p.getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::mono());
    layout.outputBuses.set(0, juce::AudioChannelSet::mono());
    layout.inputBuses.set(1, juce::AudioChannelSet::mono());
    check(p.isBusesLayoutSupported(layout), "Mono main and mono key buses are supported");
    layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
    check(!p.isBusesLayoutSupported(layout), "Mismatched main input and output are rejected");
    layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
    layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
    layout.inputBuses.set(1, juce::AudioChannelSet::stereo());
    check(p.setBusesLayout(layout), "Host can activate the stereo sidechain bus");
    std::set<juce::String> ids;
    bool unique = true;
    for (auto* item : p.getParameters())
    {
        auto* id = dynamic_cast<juce::AudioProcessorParameterWithID*>(item);
        unique &= id && ids.insert(id->paramID).second;
    }
    check(unique && ids.size() > 900, "Every host parameter has a distinct stable identifier");
    p.setParameterValue(SidechainProcessor::moduleId(0, "attack"), 0);
    p.setParameterValue(SidechainProcessor::moduleId(0, "release"), 0);
    p.setParameterValue(SidechainProcessor::moduleId(0, "smooth"), 0);
    p.setParameterValue(SidechainProcessor::bandId(0, 0, "amount"), 1);
    std::array<float, 16> points{};
    points[15] = 1;
    p.applyCurve(0, 0, points, true);
    p.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> audio(4, 256);
    audio.clear();
    for (int i = 0; i < 256; ++i)
    {
        audio.setSample(0, i, .8f);
        audio.setSample(1, i, -.4f);
    }
    audio.setSample(2, 17, 1);
    juce::MidiBuffer midi;
    p.processBlock(audio, midi);
    check(near(audio.getSample(0, 16), .8f) && near(audio.getSample(0, 17), 0) && near(audio.getSample(1, 17), 0),
          "Processor ducks main audio only after the external sidechain trigger");
    check(audio.getSample(2, 17) == 1, "Input-only sidechain buffer is not overwritten");
    p.releaseResources();
    p.prepareToPlay(48000, 256);
    p.setParameterValue(SidechainProcessor::moduleId(0, "source"), 2);
    audio.clear();
    for (int i = 0; i < 256; ++i)
    {
        audio.setSample(0, i, .8f);
        audio.setSample(1, i, -.4f);
    }
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 63);
    p.processBlock(audio, midi);
    check(near(audio.getSample(0, 62), .8f) && near(audio.getSample(0, 63), 0),
          "MIDI input retriggers at the exact sample");
    p.setParameterValue(SidechainProcessor::moduleId(1, "on"), 1);
    p.setParameterValue(SidechainProcessor::moduleId(1, "multi"), 1);
    p.setParameterValue(SidechainProcessor::bandId(1, 2, "amount"), .217f);
    p.setEditorScalePercent(125);
    p.setSelectedModule(1);
    p.setSelectedBand(2);
    p.setStepDraw(true);
    juce::MemoryBlock saved;
    p.getStateInformation(saved);
    SidechainProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    auto a = p.currentSettings(), b = restored.currentSettings();
    check(saved.getSize() > 1000 && b.module[1].enabled && b.module[1].multiband &&
              near(b.module[1].band[2].amount, .217f) && near(b.module[0].band[0].curve[15], 1) &&
              b.module[0].source == 2,
          "Project state round-trips shapers, band controls, curves and trigger");
    check(restored.editorScalePercent() == 125 && restored.selectedModule() == 1 && restored.selectedBand() == 2 &&
              restored.stepDraw(),
          "Project state restores editor selection and draw mode");
    auto tree = restored.state.copyState();
    auto child = tree.getChild(0);
    child.setProperty("value", "NaN", nullptr);
    juce::MemoryBlock bad;
    if (auto xml = tree.createXml())
        juce::AudioProcessor::copyXmlToBinary(*xml, bad);
    restored.setStateInformation(bad.getData(), static_cast<int>(bad.getSize()));
    check(restored.currentSettings().module[1].enabled &&
              near(restored.currentSettings().module[1].band[2].amount, .217f),
          "Invalid state is rejected without changing existing settings");
    juce::AudioBuffer<float> dry(4, 128);
    for (int c = 0; c < 4; ++c)
        for (int i = 0; i < 128; ++i)
            dry.setSample(c, i, c == 0 ? 1.5f : c == 1 ? -.7f : 0);
    p.processBlockBypassed(dry, midi);
    check(dry.getSample(0, 10) == 1.5f && dry.getSample(1, 10) == -.7f, "Host bypass is exactly dry");
}
void testEditor(const juce::File& dir)
{
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    SidechainProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    editor->setVisible(true);
    check(editor->getWidth() == 1300 && editor->getHeight() == 850 && editor->isResizable(),
          "Native sidechain editor has the intended layout and scaling");
    checkLayout(*editor);
    snapshot(*editor, dir, "sidechain-native.png");
    auto* preset = dynamic_cast<juce::ComboBox*>(find(*editor, "shape-preset"));
    check(preset && preset->getText() == "DEEP", "Displayed preset matches the default curve");
    click(*editor, "step-draw");
    check(p.stepDraw(), "STEP DRAW turns on");
    click(*editor, "step-draw");
    check(!p.stepDraw(), "STEP DRAW turns off on the next click");
    click(*editor, "step-draw");
    click(*editor, "point-tool");
    check(!p.stepDraw(), "Point tool exits step draw");
    click(*editor, "shaper-1");
    click(*editor, "shaper-enabled");
    check(p.selectedModule() == 1 && p.currentSettings().module[1].enabled,
          "Shaper selection and power change real parameters");
    click(*editor, "band-2");
    check(p.selectedBand() == 2 && p.currentSettings().module[1].multiband,
          "Selecting a frequency band enables multiband processing");
    const auto before = p.currentSettings().module[1].band[2].curve;
    click(*editor, "shape-0");
    const auto after = p.currentSettings().module[1].band[2].curve;
    check(before != after && preset->getText() == "TIGHT", "Quick shape changes the selected curve and preset label");
    click(*editor, "curve-undo");
    check(p.currentSettings().module[1].band[2].curve == before, "Curve undo restores the previous shape");
    click(*editor, "curve-redo");
    check(p.currentSettings().module[1].band[2].curve == after, "Curve redo restores the changed shape");
    click(*editor, "source-1");
    check(p.currentSettings().module[1].source == 1, "Sync button changes the trigger source");
    click(*editor, "source-2");
    check(p.currentSettings().module[1].source == 2, "MIDI button changes the trigger source");
    auto* graph = find(*editor, "sidechain-curve");
    check(graph != nullptr && graph->keyPressed(juce::KeyPress(juce::KeyPress::upKey)),
          "Shape graph supports keyboard edits");
    click(*editor, "step-draw");
    const auto now = juce::Time::getCurrentTime();
    auto mouse = [&](juce::Component* component, juce::Point<float> at, juce::Point<float> start)
    {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at,
                                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, component, component, now, start, now, 1, false);
    };
    const juce::Point<float> start(graph->getWidth() * .27f, graph->getHeight() * .8f),
        end(graph->getWidth() * .52f, graph->getHeight() * .3f);
    auto* point = p.pointParameter(1, 2, 4);
    GestureListener gestures;
    point->addListener(&gestures);
    graph->mouseDown(mouse(graph, start, start));
    graph->mouseDrag(mouse(graph, end, start));
    graph->mouseUp(mouse(graph, end, start));
    point->removeListener(&gestures);
    auto drawn = p.currentSettings().module[1].band[2];
    check(preset->getSelectedId() == 0 && preset->getTextWhenNothingSelected() == "CUSTOM",
          "Editing a curve displays a custom preset label");
    check(drawn.stepped && drawn.curve[4] < .3f && drawn.curve[6] > .3f && drawn.curve[6] < .7f && drawn.curve[8] > .6f,
          "STEP DRAW interpolates a drag across multiple held steps");
    check(gestures.begins == 1 && gestures.ends == 1 && gestures.changes > 0,
          "Step dragging sends balanced automation gestures");
    click(*editor, "step-draw");
    check(!p.stepDraw() && p.currentSettings().module[1].band[2].curve == drawn.curve &&
              p.currentSettings().module[1].band[2].stepped,
          "Turning STEP DRAW off preserves the drawn staircase");
    const juce::Point<float> edit(graph->getWidth() * .39f, graph->getHeight() * .12f);
    graph->mouseDown(mouse(graph, edit, edit));
    graph->mouseUp(mouse(graph, edit, edit));
    const auto pointEdit = p.currentSettings().module[1].band[2];
    check(pointEdit.curve[6] > .8f && near(pointEdit.curve[4], drawn.curve[4]),
          "Point mode edits one point without erasing the drawn shape");
    juce::MemoryBlock drawnState;
    p.getStateInformation(drawnState);
    SidechainProcessor reopened;
    reopened.setStateInformation(drawnState.getData(), static_cast<int>(drawnState.getSize()));
    check(reopened.currentSettings().module[1].band[2].curve == pointEdit.curve &&
              reopened.currentSettings().module[1].band[2].stepped,
          "Drawn shape survives project state restoration");
    auto* first = find(*editor, "shaper-1");
    auto* last = find(*editor, "shaper-8");
    const auto from = first->getLocalBounds().getCentre().toFloat();
    const auto to = first->getLocalPoint(last, last->getLocalBounds().getCentre()).toFloat();
    first->mouseDown(mouse(first, from, from));
    first->mouseDrag(mouse(first, to, from));
    first->mouseUp(mouse(first, to, from));
    check(p.currentSettings().module[1].order > p.currentSettings().module[8].order,
          "Dragging a rack shaper changes the actual effect order");
    click(*editor, "resize-logo");
    check(find(*editor, "resize-overlay") != nullptr, "V logo opens the centered resize menu");
    snapshot(*editor, dir, "sidechain-resize.png");
    click(*editor, "resize-75");
    check(p.editorScalePercent() == 75 && editor->getWidth() == 975, "Resize menu changes the real editor size");
    checkLayout(*editor);
    snapshot(*editor, dir, "sidechain-75.png");
}
} // namespace
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])
                                    : juce::File::getCurrentWorkingDirectory();
    check(dir.createDirectory().wasOk(), "Snapshot directory is available");
    testProcessor();
    testEditor(dir);
    std::cout << "All sidechain processor/editor tests passed.\n";
}
