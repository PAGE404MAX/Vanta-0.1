#include "PluginEditor.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>

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
    auto* canvas = findVisible(editor, "vanta-canvas");
    check(canvas != nullptr, "Scaled editor canvas is visible");
    const auto mapped = editor.getLocalArea(canvas, canvas->getLocalBounds());
    bool withinBounds = editor.getLocalBounds().expanded(1).contains(mapped), noOverlaps = true;
    const auto children = canvas->getChildren();
    for (int i = 0; i < children.size(); ++i)
    {
        const auto* a = children[i];
        // The open overlay is intentionally in front of all of the controls.
        if (!a->isVisible() || a->getComponentID() == "resize-overlay") continue;
        withinBounds &= canvas->getLocalBounds().contains(a->getBounds());
        for (int j = i + 1; j < children.size(); ++j)
        {
            const auto* b = children[j];
            if (b->isVisible() && b->getComponentID() != "resize-overlay" && a->getBounds().intersects(b->getBounds()))
            {
                std::cerr << "Overlapping controls: " << a->getComponentID() << " and " << b->getComponentID() << '\n';
                noOverlaps = false;
            }
        }
    }
    if (auto* overlay = findVisible(editor, "resize-overlay"))
    {
        const auto buttons = overlay->getChildren();
        for (int i = 0; i < buttons.size(); ++i)
        {
            withinBounds &= overlay->getLocalBounds().contains(buttons[i]->getBounds());
            for (int j = i + 1; j < buttons.size(); ++j)
                noOverlaps &= !buttons[i]->getBounds().intersects(buttons[j]->getBounds());
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
    check(image.isValid() && image.getWidth() == 1000 && (image.getHeight() == 720 || image.getHeight() == 820), "Editor snapshot renders at the expected size");
    juce::MemoryOutputStream stream;
    check(juce::PNGImageFormat().writeImageToStream(image, stream)
              && directory.getChildFile(name).replaceWithData(stream.getData(), stream.getDataSize()),
          "Editor preview PNG saved");
}
void checkHit(juce::Component& editor, const juce::String& id, const char* message)
{
    auto* target = findVisible(editor, id);
    check(target != nullptr, message);
    const auto point = editor.getLocalPoint(target, target->getLocalBounds().getCentre());
    auto* hit = editor.getComponentAt(point);
    check(hit == target || (hit != nullptr && target->isParentOf(hit)), message);
}
void snapshotResized(juce::Component& editor, const juce::File& directory, const char* name)
{
    const auto image = editor.createComponentSnapshot(editor.getLocalBounds());
    check(image.isValid() && image.getWidth() == editor.getWidth() && image.getHeight() == editor.getHeight(),
          "Scaled editor renders at the actual size");
    juce::MemoryOutputStream stream;
    check(juce::PNGImageFormat().writeImageToStream(image, stream)
          && directory.getChildFile(name).replaceWithData(stream.getData(), stream.getDataSize()), "Scaled preview PNG saved");
}
void sendOutsideMouseDown(juce::Component& component, juce::Point<float> point)
{
    const auto now = juce::Time::getCurrentTime();
    const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(), point,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        &component, &component, now, point, now, 1, false);
    component.mouseDown(event);
}
void sendDoubleClick(juce::Component& component)
{
    const auto now = juce::Time::getCurrentTime();
    const auto point = component.getLocalBounds().getCentre().toFloat();
    const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(), point,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        &component, &component, now, point, now, 2, false);
    component.mouseDoubleClick(event);
}
struct UiScaleListener final : juce::AudioProcessorListener
{
    int changed = 0;
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details) override
    { if (details.nonParameterStateChanged) ++changed; }
};
void testResizeMenu(juce::AudioProcessor::WrapperType type, const juce::File& previews)
{
    juce::AudioProcessor::setTypeOfNextNewPlugin(type);
    VantaProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    const bool standalone = type == juce::AudioProcessor::wrapperType_Standalone;
    const int baseHeight = standalone ? 820 : 720;
    const auto originalParameters = processor.currentValues();
    const int originalParameterCount = processor.getParameters().size();
    UiScaleListener listener; processor.addListener(&listener);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor()); editor->setVisible(true);
    check(editor->isResizable() && editor->getConstrainer() != nullptr
          && std::abs(editor->getConstrainer()->getFixedAspectRatio() - 1000.0 / baseHeight) < 0.0001,
          "Host resizing uses the correct fixed aspect ratio");
    checkHit(*editor, "resize-logo", "The V logo is a clickable hit target");
    click(*editor, "resize-logo");
    auto* overlay = findVisible(*editor, "resize-overlay");
    check(overlay != nullptr, "V logo opens the size overlay");
    const auto* current = dynamic_cast<juce::TextButton*>(findVisible(*editor, "resize-100"));
    check(current != nullptr && current->getToggleState(), "Current size is highlighted in the menu");
    auto* first = findVisible(*editor, "resize-60"); auto* last = findVisible(*editor, "resize-fit");
    check(first && last && (first->getBounds().getCentreX() + last->getBounds().getCentreX()) / 2 == 500,
          "Resize choices are horizontally centred");
    checkLayout(*editor);
    checkHit(*editor, "resize-75", "Resize choices receive transformed hit testing");
    auto* logoBehindOverlay = findVisible(*editor, "resize-logo");
    const auto logoPoint = editor->getLocalPoint(logoBehindOverlay, logoBehindOverlay->getLocalBounds().getCentre());
    check(editor->getComponentAt(logoPoint) == overlay, "Modal overlay intercepts background clicks");
    if (!standalone) snapshotResized(*editor, previews, "vst-resize-menu.png");
    check(overlay->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)), "Escape dismisses the resize menu");
    check(findVisible(*editor, "resize-overlay") == nullptr, "Resize overlay is hidden after Escape");
    click(*editor, "resize-logo"); overlay = findVisible(*editor, "resize-overlay");
    sendOutsideMouseDown(*overlay, { 0.0f, 0.0f });
    check(findVisible(*editor, "resize-overlay") == nullptr, "Clicking outside dismisses the resize menu");
    click(*editor, "resize-logo"); click(*editor, "resize-close");
    check(findVisible(*editor, "resize-overlay") == nullptr, "Close button dismisses the resize menu");
    editor->setSize(1200, baseHeight); // A non-conforming host may leave letterbox margins.
    click(*editor, "resize-logo");
    check(editor->getComponentAt(5, 5) == editor.get(), "Letterbox belongs to the editor, not the canvas");
    sendOutsideMouseDown(*editor, { 5.0f, 5.0f });
    check(findVisible(*editor, "resize-overlay") == nullptr, "Clicking a letterbox margin dismisses the menu");
    editor->setSize(1000, baseHeight);

    constexpr int percentages[] = { 60, 75, 90, 100, 110, 125, 150 };
    for (const int percent : percentages)
    {
        const auto parametersBeforeResize = processor.currentValues();
        click(*editor, "resize-logo");
        const auto id = "resize-" + juce::String(percent);
        checkHit(*editor, id, "Scale button is clickable at the current scale");
        auto* option = dynamic_cast<juce::TextButton*>(findVisible(*editor, id));
        option->onClick();
        check(findVisible(*editor, "resize-overlay") == nullptr, "Choosing a size closes the menu");
        check(editor->getWidth() == juce::roundToInt(1000 * percent / 100.0)
              && editor->getHeight() == juce::roundToInt(baseHeight * percent / 100.0)
              && processor.editorScalePercent() == percent, "Selected size updates editor geometry and state");
        check(parametersBeforeResize == processor.currentValues(), "Selecting a size leaves all synth parameters unchanged");
        checkLayout(*editor);
        checkHit(*editor, "resize-logo", "V logo remains clickable after resizing");
        checkHit(*editor, "tab-oscillator", "Navigation remains clickable after resizing");
        checkHit(*editor, "parameter-level", "Parameter slider hit target follows its scaled drawing");
        if (percent == 60 || percent == 150)
        {
            for (const auto* page : { "tab-oscillator", "tab-effects", "tab-modulation", "tab-mixer", "tab-sequencer", "tab-settings", "tab-overview" })
            {
                checkHit(*editor, page, "Every navigation tab remains hittable at the minimum and maximum sizes");
                click(*editor, page); checkLayout(*editor);
            }
        }
        if (percent == 75)
        {
            click(*editor, "tab-oscillator"); checkLayout(*editor);
            checkHit(*editor, "wave-saw", "Oscillator controls remain clickable at 75 percent");
            click(*editor, "wave-saw");
            check(processor.currentValues()[vanta::wave] == 1.0f, "Scaled oscillator button still changes its parameter");
            click(*editor, "preset-browser"); checkLayout(*editor);
            checkHit(*editor, "preset-search", "Preset browser search remains clickable at 75 percent");
            click(*editor, "tab-overview");
        }
        if (percent == 125)
        {
            changeSlider(*editor, processor, vanta::cutoff);
            if (standalone)
            {
                checkHit(*editor, "standalone-keyboard", "Standalone keyboard remains clickable at 125 percent");
                auto* keyboard = dynamic_cast<VantaKeyboard*>(findVisible(*editor, "standalone-keyboard"));
                const auto blackKey = keyboard->getRectangleForKey(61).reduced(2.0f);
                const auto scaledBlackKey = editor->getLocalArea(keyboard, blackKey).toNearestInt();
                check(editor->getComponentAt(scaledBlackKey.getCentre()) == keyboard,
                      "Black-key hit target follows its scaled drawing");
                std::array<bool, 17> held {}; held[1] = true; keyboard->updateComputerKeys(held, false);
                const auto pressed = editor->createComponentSnapshot(editor->getLocalBounds());
                check(processor.keyboardState.isNoteOn(1, 61)
                      && pressed.getPixelAt(scaledBlackKey.getX(), scaledBlackKey.getY()) == juce::Colour(0xffb73929)
                      && pressed.getPixelAt(scaledBlackKey.getCentreX(), scaledBlackKey.getCentreY()) == juce::Colour(0xffb73929)
                      && pressed.getPixelAt(scaledBlackKey.getRight() - 1, scaledBlackKey.getBottom() - 1) == juce::Colour(0xffb73929),
                      "Scaled standalone black key plays and is fully red");
                held[1] = false; keyboard->updateComputerKeys(held, false);
                snapshotResized(*editor, previews, "standalone-resize-125.png");
            }
            else snapshotResized(*editor, previews, "vst-resize-125.png");
        }
    }
    check(processor.getParameters().size() == originalParameterCount && listener.changed > 0,
          "UI size is non-automatable state and tells the host to save changes");
    check(originalParameters[vanta::level] == processor.currentValues()[vanta::level], "Resizing preserves synth controls");
    processor.loadFactoryPreset(0);
    check(processor.editorScalePercent() == 150, "Changing presets does not reset the UI size");
    click(*editor, "resize-logo"); click(*editor, "resize-fit");
    check(editor->getWidth() == 1000 && editor->getHeight() == baseHeight && processor.editorScalePercent() == 100,
          "Fit Screen has a safe 100 percent fallback without a desktop peer");
    // JUCE uses the root transform for host DPI: changing UI size must not replace it.
    const auto hostDpi = juce::AffineTransform::scale(1.25f);
    editor->setTransform(hostDpi);
    click(*editor, "resize-logo"); click(*editor, "resize-75");
    check(editor->getTransform() == hostDpi && editor->getWidth() == 750,
          "UI scaling preserves the separate host DPI transform");
    checkHit(*editor, "resize-logo", "Logo remains clickable with UI and host scaling");
    processor.removeListener(&listener);
}
void testFitScreenCalculation()
{
    const auto normalWindow = vanta::standaloneWindowPosition(100, 80, 1000, 820, 0, 0, 1920, 1080);
    check(normalWindow.x == 100 && normalWindow.y == 80, "A visible standalone window keeps its position");
    const auto oversized = vanta::standaloneWindowPosition(-140, -200, 1500, 1230, 0, 0, 1280, 720);
    check(oversized.x == 0 && oversized.y == 0, "An oversized standalone window keeps its logo on-screen at startup");
    const auto moved = vanta::standaloneWindowPosition(2500, 800, 1000, 820, 1920, 0, 1920, 1080);
    check(moved.x == 2500 && moved.y == 260, "Standalone startup respects the chosen monitor's work area");
    const auto noDisplay = vanta::standaloneWindowPosition(200, 300, 1000, 820, 0, 0, 0, 0);
    check(noDisplay.x == 200 && noDisplay.y == 300, "Standalone startup leaves position unchanged without display data");
    const auto centre = vanta::resizePopupPosition(1000, 820, 0, 0, 1000, 820);
    check(centre.x == 280 && centre.y == 296, "Resize popup is centred when the editor fits the screen");
    const auto clipped = vanta::resizePopupPosition(1000, 820, 0, 0, 700, 500);
    check(clipped.x == 130 && clipped.y == 136, "Resize popup centres on the visible part of an oversized editor");
    const auto edge = vanta::resizePopupPosition(1000, 820, 900, 750, 100, 70);
    check(edge.x == 560 && edge.y == 592, "Resize popup stays within the editor at screen edges");
    const auto unknown = vanta::resizePopupPosition(1000, 820, 0, 0, 0, 0);
    check(unknown.x == centre.x && unknown.y == centre.y, "Resize popup defaults to centre without display information");
    using vanta::fitEditorScaleToWorkArea;
    check(fitEditorScaleToWorkArea(720, 1920, 1080, 1.0) == 135, "Fit Screen chooses a bounded 1080p VST size");
    check(fitEditorScaleToWorkArea(820, 1920, 1080, 1.0) == 120, "Fit Screen accounts for the standalone keyboard");
    check(fitEditorScaleToWorkArea(720, 1366, 768, 1.0) == 90
          && fitEditorScaleToWorkArea(820, 1366, 768, 1.0) == 80, "Fit Screen fits a smaller display");
    check(fitEditorScaleToWorkArea(720, 3840, 2160, 2.0) == 140, "Fit Screen accounts for host DPI scaling");
    check(fitEditorScaleToWorkArea(720, 640, 480, 1.0) == 60
          && fitEditorScaleToWorkArea(820, 8000, 8000, 1.0) == 150, "Fit Screen respects minimum and maximum sizes");
    check(fitEditorScaleToWorkArea(720, 0, 1080, 1.0) == 100
          && fitEditorScaleToWorkArea(720, 1920, 1080, 0.0) == 100
          && fitEditorScaleToWorkArea(720, 1920, 1080, std::numeric_limits<double>::quiet_NaN()) == 100
          && fitEditorScaleToWorkArea(720, 1920, 1080, std::numeric_limits<double>::infinity()) == 100,
          "Fit Screen falls back safely for invalid display information");
}
void testResizeState()
{
    VantaProcessor processor;
    processor.setEditorScalePercent(125);
    juce::MemoryBlock saved; processor.getStateInformation(saved);
    processor.setEditorScalePercent(60);
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    check(processor.editorScalePercent() == 125, "Host state saves and restores UI scale");
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor()); editor->setVisible(true);
    check(editor->getWidth() == 1250 && editor->getHeight() == 900, "Reopened editor uses the saved size");

    auto tree = processor.state.copyState();
    auto applyScaleProperty = [&](const juce::String& value)
    {
        auto copy = tree.createCopy(); copy.setProperty("vantaEditorScale", value, nullptr);
        auto xml = copy.createXml(); juce::MemoryBlock data; juce::AudioProcessor::copyXmlToBinary(*xml, data);
        processor.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    };
    for (const auto& bad : { "", "0", "59", "151", "999", "-1", "1e9", "1000000000000000000000000", "abc" })
    {
        processor.setEditorScalePercent(75); applyScaleProperty(bad);
        check(processor.editorScalePercent() == 100, "Invalid saved sizes fall back to 100 percent");
    }
    auto oldXml = tree.createXml(); juce::MemoryBlock old; juce::AudioProcessor::copyXmlToBinary(*oldXml, old);
    processor.setEditorScalePercent(150);
    processor.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
    check(processor.editorScalePercent() == 100, "Old projects without UI scale open at 100 percent");

    // A rejected asset must not partially apply a scale from the same state.
    auto malformed = tree.createCopy(); malformed.setProperty("vantaEditorScale", "150", nullptr);
    malformed.setProperty("vantaWavetables", "not valid JSON", nullptr);
    auto badXml = malformed.createXml(); juce::MemoryBlock bad; juce::AudioProcessor::copyXmlToBinary(*badXml, bad);
    processor.setEditorScalePercent(90); processor.setStateInformation(bad.getData(), static_cast<int>(bad.getSize()));
    check(processor.editorScalePercent() == 90, "Malformed host state cannot partially change UI scale");

    applyScaleProperty("75");
    click(*editor, "tab-oscillator"); // Refresh before the timer must not overwrite restored state.
    check(processor.editorScalePercent() == 75, "Layout refresh preserves a pending restored size");
    for (int i = 0; i < 30 && editor->getWidth() != 750; ++i)
    {
        juce::Thread::sleep(20); juce::Timer::callPendingTimersSynchronously();
    }
    check(editor->getWidth() == 750 && editor->getHeight() == 540 && processor.editorScalePercent() == 75,
          "Open editor applies a restored size on the message-thread timer");
}

struct FakeVstFrame final : juce::Component
{
    bool acceptResize = false;
    void childBoundsChanged(juce::Component* child) override
    {
        if (acceptResize)
        {
            const auto bounds = getLocalArea(child, child->getLocalBounds());
            setSize(bounds.getWidth(), bounds.getHeight());
        }
    }
};
void dispatchTimersFor(int milliseconds)
{
    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(milliseconds);
    while (static_cast<juce::int32>(deadline - juce::Time::getMillisecondCounter()) > 0)
    {
        juce::Thread::sleep(20);
        juce::Timer::callPendingTimersSynchronously();
    }
}
void testHostResizeNegotiation()
{
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_VST3);
    VantaProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    FakeVstFrame frame; frame.setVisible(true); frame.setSize(1000, 720); frame.addAndMakeVisible(editor.get());
    // Model a host that ignores a request: the component grows, but its parent
    // does not. The editor must revert and expose the warning instead of clipping.
    click(*editor, "resize-logo"); click(*editor, "resize-125");
    check(editor->getWidth() == 1250 && frame.getWidth() == 1000, "Simulated VST3 host refuses resize");
    dispatchTimersFor(1250);
    check(editor->getWidth() == 1000 && editor->getHeight() == 720 && processor.editorScalePercent() == 100,
          "Rejected host resize rolls back to the visible view");
    check(findVisible(*editor, "resize-overlay") != nullptr, "Rejected resize reopens the options with a warning");
    auto* active = dynamic_cast<juce::TextButton*>(findVisible(*editor, "resize-100"));
    check(active != nullptr && active->getToggleState(), "Rejected resize highlights the actual size");
    // Model a cooperating host, including a separate 125% host DPI transform.
    frame.acceptResize = true; editor->setTransform(juce::AffineTransform::scale(1.25f));
    frame.setSize(1250, 900);
    click(*editor, "resize-125"); dispatchTimersFor(1250);
    check(std::abs(frame.getWidth() - juce::roundToInt(1250 * 1.25)) <= 1
          && std::abs(frame.getHeight() - juce::roundToInt(900 * 1.25)) <= 1
          && editor->getWidth() == 1250 && editor->getHeight() == 900
          && processor.editorScalePercent() == 125 && findVisible(*editor, "resize-overlay") == nullptr,
          "Accepted host resize and host DPI scaling remain separate");
    frame.acceptResize = false;
    click(*editor, "resize-logo"); click(*editor, "resize-60"); dispatchTimersFor(1250);
    check(std::abs(editor->getWidth() - 1250) <= 1 && std::abs(editor->getHeight() - 900) <= 1
          && processor.editorScalePercent() == 125 && findVisible(*editor, "resize-overlay") != nullptr,
          "Rejected resize falls back correctly with host DPI scaling");
}

void testHostState()
{
    VantaProcessor processor;
    auto* legacyWave = processor.state.getParameter("wave");
    check(legacyWave != nullptr && legacyWave->getNumSteps() == 2
          && std::abs(legacyWave->convertTo0to1(1.0f) - 1.0f) < 0.0001f,
          "Legacy waveform automation remains a two-choice 0/1 parameter");
    auto* delay = processor.state.getParameter("ampDelay");
    auto* hold = processor.state.getParameter("ampHold");
    check(delay != nullptr && hold != nullptr && delay != processor.state.getParameter("delayTime")
          && delay->getDefaultValue() == 0.0f && hold->getDefaultValue() == 0.0f
          && delay->isAutomatable() && hold->isAutomatable(),
          "New amp delay/hold are separate, automatable zero-default host parameters");
    auto original = vanta::defaultValues();
    original[vanta::ampDelay] = 0.456f; original[vanta::ampHold] = 1.23f;
    original[vanta::wave] = 1.0f; original[vanta::cutoff] = 2400.0f;
    original[vanta::oscBEnabled] = 1.0f; original[vanta::oscBWave] = 2.0f;
    original[vanta::oscBFine] = -17.0f; original[vanta::oscBPan] = 0.5f;
    original[vanta::oscDEnabled] = 1.0f; original[vanta::oscHEnabled] = 1.0f;
    original[vanta::chorusEnabled] = 1.0f; original[vanta::eqLow] = 5.0f;
    processor.applyValues(original, "Layered");
    juce::MemoryBlock saved; processor.getStateInformation(saved);
    processor.applyValues(vanta::defaultValues(), "Reset");
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    auto restored = processor.currentValues();
    check(processor.presetName() == "Layered" && restored[vanta::oscBEnabled] == 1.0f
          && restored[vanta::oscBWave] == 2.0f && restored[vanta::oscDEnabled] == 1.0f
          && restored[vanta::oscHEnabled] == 1.0f && std::abs(restored[vanta::oscBFine] + 17.0f) < 0.01f,
          "Host state saves and restores oscillator layers and settings");
    check(restored[vanta::chorusEnabled] == 1.0f && restored[vanta::eqLow] == 5.0f,
          "Host state saves and restores the effects rack");
    check(std::abs(restored[vanta::ampDelay] - 0.456f) < 0.001f && std::abs(restored[vanta::ampHold] - 1.23f) < 0.001f,
          "Host state saves and restores amp delay and hold");

    // Beta 0.12 states do not contain the two new parameters. Restoring one
    // must reset them even if the current patch had nonzero values.
    auto beta12 = processor.state.copyState();
    for (int n = beta12.getNumChildren() - 1; n >= 0; --n)
    {
        const auto id = beta12.getChild(n).getProperty("id").toString();
        if (id == "ampDelay" || id == "ampHold") beta12.removeChild(n, nullptr);
    }
    auto old12Xml = beta12.createXml(); juce::MemoryBlock beta12Bytes;
    juce::AudioProcessor::copyXmlToBinary(*old12Xml, beta12Bytes);
    processor.setStateInformation(beta12Bytes.getData(), static_cast<int>(beta12Bytes.getSize()));
    auto from12 = processor.currentValues();
    check(from12[vanta::ampDelay] == 0.0f && from12[vanta::ampHold] == 0.0f
          && from12[vanta::chorusEnabled] == 1.0f && from12[vanta::eqLow] == 5.0f,
          "Beta 0.12 state preserves effects and resets missing envelope stages to zero");

    // A Beta 0.11 host state contains none of these appended parameters. Its
    // original nonzero wet mixes must remain enabled, while new effects stay off.
    auto beta11 = processor.state.copyState();
    for (int n = beta11.getNumChildren() - 1; n >= 0; --n)
    {
        const auto id = beta11.getChild(n).getProperty("id").toString();
        bool newParameter = false;
        for (std::size_t i = vanta::reverbEnabled; i < vanta::parameterCount; ++i)
            newParameter |= id == vanta::parameterSpecs[i].id;
        if (newParameter) beta11.removeChild(n, nullptr);
    }
    auto old11Xml = beta11.createXml(); juce::MemoryBlock beta11Bytes; juce::AudioProcessor::copyXmlToBinary(*old11Xml, beta11Bytes);
    processor.setStateInformation(beta11Bytes.getData(), static_cast<int>(beta11Bytes.getSize()));
    auto from11 = processor.currentValues();
    check(from11[vanta::reverbEnabled] == 1.0f && from11[vanta::delayEnabled] == 1.0f && from11[vanta::distortionEnabled] == 1.0f
          && from11[vanta::chorusEnabled] == 0.0f,
          "Beta 0.11 states keep legacy effects enabled and default new effects safely");

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

void testEnvelopeCompatibility()
{
    VantaProcessor processor;
    const float minima[] { 0.001f, 0.001f, 0.01f };
    const float maxima[] { 2.0f, 4.0f, 4.0f };
    for (std::size_t stage = 0; stage < vanta::legacyEnvelopeTimes.size(); ++stage)
    {
        auto* legacy = processor.state.getParameter(vanta::parameterSpecs[vanta::legacyEnvelopeTimes[stage]].id);
        auto* full = processor.state.getParameter(vanta::parameterSpecs[vanta::wideEnvelopeTimes[stage]].id);
        const juce::NormalisableRange<float> oldRange(minima[stage], maxima[stage], 0.001f, 0.4f);
        check(legacy && full && full->isAutomatable()
              && std::abs(legacy->convertFrom0to1(0.37f) - oldRange.snapToLegalValue(oldRange.convertFrom0to1(0.37f))) < 0.00001f
              && full->convertFrom0to1(0.0f) == 0.0f && full->convertFrom0to1(1.0f) == 32.0f,
              "Legacy ADSR automation mapping is unchanged while full-range automation is available");
    }
    auto legacyValues = vanta::defaultValues();
    legacyValues[vanta::attack] = 0.75f; legacyValues[vanta::decay] = 0.5f; legacyValues[vanta::release] = 0.85f;
    processor.applyValues(legacyValues, "Old automation");
    auto oldState = processor.state.copyState();
    for (int n = oldState.getNumChildren() - 1; n >= 0; --n)
    {
        const auto id = oldState.getChild(n).getProperty("id").toString();
        bool newParameter = false;
        for (std::size_t i = vanta::ampAttackWide; i < vanta::parameterCount; ++i)
            newParameter |= id == vanta::parameterSpecs[i].id;
        if (newParameter) oldState.removeChild(n, nullptr);
    }
    auto xml = oldState.createXml(); juce::MemoryBlock bytes; juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
    processor.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    auto old = processor.currentValues();
    check(old[vanta::ampAttackWideActive] == 0.0f && std::abs(vanta::envelopeTime(old, 0) - 0.75f) < 0.001f
          && std::abs(old[vanta::ampAttackWide] - 0.75f) < 0.001f,
          "A 0.13 host state uses its original attack and seeds the new control with that value");
    auto* oldAttack = processor.state.getParameter("attack");
    oldAttack->setValueNotifyingHost(0.5f);
    const auto automatedOld = processor.currentValues();
    check(automatedOld[vanta::ampAttackWideActive] == 0.0f
          && std::abs(vanta::envelopeTime(automatedOld, 0) - oldAttack->convertFrom0to1(0.5f)) < 0.001f,
          "Automation of the old attack ID continues controlling the restored project");

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor()); editor->setVisible(true);
    click(*editor, "tab-modulation");
    auto* fullAttack = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-ampAttackWide"));
    check(fullAttack && std::abs(fullAttack->getValue() - vanta::envelopeTime(automatedOld, 0)) < 0.001,
          "New attack knob displays the current legacy-automated value");
    // The wide parameter starts at 0.02 while this legacy knob displays 0.354.
    // A direct edit to the wide default must still take control of the stage.
    sendDoubleClick(*fullAttack);
    check(processor.currentValues()[vanta::ampAttackWideActive] == 1.0f
          && std::abs(vanta::envelopeTime(processor.currentValues(), 0) - 0.02f) < 0.001f,
          "Editing a displayed legacy value to the full-range default takes over the stage");
    fullAttack->setValue(12.5, juce::sendNotificationSync);
    auto edited = processor.currentValues();
    check(edited[vanta::ampAttackWideActive] == 1.0f && std::abs(vanta::envelopeTime(edited, 0) - 12.5f) < 0.001f,
          "Editing the full-range knob takes over that one stage without changing other legacy stages");
    check(edited[vanta::ampDecayWideActive] == 0.0f && edited[vanta::ampReleaseWideActive] == 0.0f,
          "Wide-range activation is independent per envelope stage");
    oldAttack->setValueNotifyingHost(0.7f);
    check(std::abs(vanta::envelopeTime(processor.currentValues(), 0) - 12.5f) < 0.001f,
          "The selected full-range stage stays authoritative after an intentional edit");
    juce::MemoryBlock saved; processor.getStateInformation(saved);
    VantaProcessor restored; restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    auto reloaded = restored.currentValues();
    check(reloaded[vanta::ampAttackWideActive] == 1.0f && std::abs(vanta::envelopeTime(reloaded, 0) - 12.5f) < 0.001f
          && reloaded[vanta::ampDecayWideActive] == 0.0f,
          "Host state persists edited full-range and untouched legacy stages");
    restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    reloaded = restored.currentValues();
    check(reloaded[vanta::ampAttackWideActive] == 0.0f && std::abs(vanta::envelopeTime(reloaded, 0) - 0.75f) < 0.001f,
          "Loading an older host state after a newer one clears full-range takeover");
    const auto native = vanta::PresetCodec::decode(vanta::PresetCodec::encode("Range", edited), ".vanta");
    check(native.ok && native.values[vanta::ampAttackWideActive] == 1.0f && std::abs(native.values[vanta::ampAttackWide] - 12.5f) < 0.001f,
          "Native presets persist the full-range compatibility mode");
    const auto older = vanta::PresetCodec::decode(R"({"format":"vanta-preset","version":1,"parameters":{"attack":1.2,"decay":0.8,"release":1.1}})", ".vanta");
    check(older.ok && older.values[vanta::ampAttackWideActive] == 0.0f
          && std::abs(older.values[vanta::ampAttackWide] - 1.2f) < 0.001f
          && std::abs(vanta::envelopeTime(older.values, 0) - 1.2f) < 0.001f,
          "Older native presets use the original values and safely initialize new controls");
    const auto partial = vanta::PresetCodec::decode(R"({"format":"vanta-preset","version":1,"parameters":{"ampDecayWide":21}})", ".vanta");
    check(partial.ok && partial.values[vanta::ampDecayWideActive] == 1.0f
          && vanta::envelopeTime(partial.values, 1) == 21.0f && partial.values[vanta::ampAttackWideActive] == 0.0f,
          "A partial native preset with a new wide value activates only that stage");
    // Resetting to a value that is already in the wide parameter must work
    // after legacy automation has moved the displayed value away from it.
    auto defaultState = oldState.createCopy();
    for (auto child : defaultState)
        if (child.getProperty("id").toString() == "attack") child.setProperty("value", 0.02f, nullptr);
    auto defaultXml = defaultState.createXml(); juce::MemoryBlock defaultBytes;
    juce::AudioProcessor::copyXmlToBinary(*defaultXml, defaultBytes);
    VantaProcessor resetDefault; resetDefault.setStateInformation(defaultBytes.getData(), static_cast<int>(defaultBytes.getSize()));
    auto* legacyDefaultAttack = resetDefault.state.getParameter("attack");
    legacyDefaultAttack->setValueNotifyingHost(0.5f);
    std::unique_ptr<juce::AudioProcessorEditor> resetEditor(resetDefault.createEditor()); resetEditor->setVisible(true);
    click(*resetEditor, "tab-modulation");
    auto* resetSlider = dynamic_cast<juce::Slider*>(findVisible(*resetEditor, "parameter-ampAttackWide"));
    check(resetSlider && std::abs(resetDefault.currentValues()[vanta::ampAttackWide] - 0.02f) < 0.001f
          && std::abs(resetSlider->getValue() - 0.02) > 0.01,
          "Legacy automation can move the displayed value while the wide parameter remains at its default");
    UiScaleListener dirty; resetDefault.addListener(&dirty);
    sendDoubleClick(*resetSlider);
    check(resetDefault.currentValues()[vanta::ampAttackWideActive] == 1.0f
          && std::abs(vanta::envelopeTime(resetDefault.currentValues(), 0) - 0.02f) < 0.001f && dirty.changed > 0,
          "Double-clicking the same wide default takes over and marks the host state dirty");
    resetDefault.removeListener(&dirty);
    VantaProcessor automated; automated.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    auto* wide = automated.state.getParameter("ampDecayWide");
    wide->setValueNotifyingHost(wide->convertTo0to1(24.0f));
    check(automated.currentValues()[vanta::ampDecayWideActive] == 1.0f
          && std::abs(vanta::envelopeTime(automated.currentValues(), 1) - 24.0f) < 0.002f,
          "Host automation of the new full-range ID activates it even without an editor");
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
    check(processor.currentValues()[vanta::wave] == 1.0f && processor.currentValues()[vanta::oscAExtraWave] == 0.0f
          && vanta::oscillatorShape(processor.currentValues(), 0) == 1,
          "VANTA INIT factory preset uses a saw wave");
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


void testImportedEditorControls()
{
    VantaProcessor processor;
    vanta::PresetResult preset; preset.ok = true; preset.name = "Imported editor controls";
    preset.values = vanta::defaultValues(); preset.values[vanta::oscATable] = 1;
    preset.values[vanta::oscDEnabled] = 1; preset.values[vanta::oscDSample] = 1;
    preset.values[vanta::ottEnabled] = 1;
    preset.waveTables = std::make_shared<vanta::WaveTableBank>();
    preset.waveTables->tables[0].frameCount = 2; preset.waveTables->tables[0].positions[1] = 1.0f;
    preset.waveTables->sample.slot = 3; preset.waveTables->sample.frames = 4; preset.waveTables->sample.channels = 1;
    processor.applyPreset(preset);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor()); editor->setVisible(true);
    click(*editor, "tab-oscillator");
    auto* wave = dynamic_cast<juce::ComboBox*>(findVisible(*editor, "parameter-wave"));
    check(wave != nullptr && wave->getSelectedId() == 5, "Imported wavetable appears in OSC source selector");
    checkLayout(*editor);
    changeSlider(*editor, processor, vanta::oscATablePosition);
    wave->setSelectedId(1, juce::sendNotificationSync);
    check(processor.currentValues()[vanta::oscATable] == 0.0f, "Selecting a basic shape disables wavetable playback");
    wave->setSelectedId(5, juce::sendNotificationSync);
    check(processor.currentValues()[vanta::oscATable] == 1.0f, "Selecting an available wavetable enables playback");
    click(*editor, "osc-tab-D");
    auto* sample = dynamic_cast<juce::ComboBox*>(findVisible(*editor, "parameter-oscDWave"));
    check(sample != nullptr && sample->getSelectedId() == 6, "Imported sample appears in OSC source selector");
    sample->setSelectedId(1, juce::sendNotificationSync);
    check(processor.currentValues()[vanta::oscDSample] == 0.0f, "Selecting a basic shape disables sample playback");
    sample->setSelectedId(6, juce::sendNotificationSync);
    check(processor.currentValues()[vanta::oscDSample] == 1.0f, "Selecting an available sample enables playback");
    click(*editor, "tab-effects"); click(*editor, "fx-select-compressor"); checkLayout(*editor);
    auto* multiband = dynamic_cast<juce::TextButton*>(findVisible(*editor, "parameter-ottEnabled"));
    check(multiband != nullptr && multiband->getToggleState(), "Imported multiband enable is visible on effects page");
    multiband->setToggleState(false, juce::sendNotificationSync);
    check(processor.currentValues()[vanta::ottEnabled] == 0.0f, "Multiband switch controls bypass");
    changeSlider(*editor, processor, vanta::ottDepth); changeSlider(*editor, processor, vanta::ottMix);
    click(*editor, "tab-mixer");
    auto* first = dynamic_cast<juce::TextButton*>(findVisible(*editor, "mixer-osc-A-enabled"));
    check(first != nullptr && first->getToggleState(), "Mixer exposes OSC A enable");
    first->setToggleState(false, juce::sendNotificationSync);
    check(processor.currentValues()[vanta::oscAEnabled] == 0.0f, "Mixer can disable OSC A for imported patches");
}

void testStandalone(const juce::File& previews)
{
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    VantaProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setVisible(true);
    check(editor->getWidth() == 1000 && editor->getHeight() == 820, "Standalone editor opens at 1000 x 820");
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
    for (const auto parameter : { vanta::level, vanta::cutoff, vanta::resonance, vanta::ampAttackWide, vanta::ampDecayWide,
                                  vanta::sustain, vanta::ampReleaseWide, vanta::vibrato, vanta::vibratoRate })
        changeSlider(*editor, processor, parameter);

    click(*editor, "edit-envelope");
    check(findVisible(*editor, "parameter-ampAttackWide") != nullptr && findVisible(*editor, "edit-envelope") == nullptr,
          "Envelope EDIT opens the modulation page");
    click(*editor, "tab-overview"); click(*editor, "edit-vibrato");
    check(findVisible(*editor, "parameter-vibratoRate") != nullptr && findVisible(*editor, "edit-vibrato") == nullptr,
          "Vibrato EDIT opens the modulation page");
    click(*editor, "tab-overview");
    click(*editor, "tab-oscillator");
    checkLayout(*editor); snapshot(*editor, previews, "oscillator.png");
    auto* waveform = dynamic_cast<juce::ComboBox*>(findVisible(*editor, "parameter-wave"));
    check(waveform != nullptr, "OSC A waveform selector is visible");
    const auto sineView = editor->createComponentSnapshot(editor->getLocalBounds());
    click(*editor, "wave-saw");
    check(waveform->getSelectedId() == 2, "Preview-style SAW button updates the source selector");
    const auto sawView = editor->createComponentSnapshot(editor->getLocalBounds());
    check(processor.currentValues()[vanta::wave] == 1.0f, "OSC A saw updates the original parameter");
    check(changedPixels(sineView, sawView, { 44, 384, 564, 114 }) > 100, "Waveform preview follows the selected waveform");
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
    changeSlider(*editor, processor, vanta::oscDUnison);
    changeSlider(*editor, processor, vanta::oscDPhase);
    auto* level = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-oscDLevel"));
    auto* pan = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-oscDPan"));
    auto* unison = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-oscDUnison"));
    auto* phase = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-oscDPhase"));
    check(level && pan && unison && phase && level->getTextFromValue(1.0) == "1.000"
          && pan->getTextFromValue(-1.0) == "-100.0" && pan->getTextFromValue(1.0) == "100.0"
          && unison->getTextFromValue(0.5) == "50.0%" && phase->getTextFromValue(1.0) == "100.0%",
          "Oscillator controls display the requested level, pan and percentage units");
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
    const struct { const char* selector; vanta::Parameter enabled; std::array<vanta::Parameter, 4> knobs; int count; } rack[] = {
        { "fx-select-chorus", vanta::chorusEnabled, { vanta::chorusRate, vanta::chorusDepth, vanta::chorusMix, vanta::level }, 3 },
        { "fx-select-compressor", vanta::ottEnabled, { vanta::ottDepth, vanta::ottMix, vanta::level, vanta::level }, 2 },
        { "fx-select-delay", vanta::delayEnabled, { vanta::delayTime, vanta::delayFeedback, vanta::delayMix, vanta::level }, 3 },
        { "fx-select-distortion", vanta::distortionEnabled, { vanta::distortionDrive, vanta::distortionTone, vanta::distortionMix, vanta::level }, 3 },
        { "fx-select-equalizer", vanta::eqEnabled, { vanta::eqLow, vanta::eqMid, vanta::eqHigh, vanta::eqMix }, 4 },
        { "fx-select-filter", vanta::fxFilterEnabled, { vanta::fxFilterCutoff, vanta::fxFilterResonance, vanta::fxFilterMix, vanta::level }, 3 },
        { "fx-select-flanger", vanta::flangerEnabled, { vanta::flangerRate, vanta::flangerDepth, vanta::flangerFeedback, vanta::flangerMix }, 4 },
        { "fx-select-phaser", vanta::phaserEnabled, { vanta::phaserRate, vanta::phaserDepth, vanta::phaserFeedback, vanta::phaserMix }, 4 },
        { "fx-select-reverb", vanta::reverbEnabled, { vanta::reverbSize, vanta::reverbDecay, vanta::reverbMix, vanta::level }, 3 }
    };
    for (const auto& fx : rack)
    {
        click(*editor, fx.selector); checkLayout(*editor);
        auto* enable = dynamic_cast<juce::TextButton*>(findVisible(*editor, "parameter-" + juce::String(vanta::parameterSpecs[fx.enabled].id)));
        check(enable != nullptr, "Every effect has a power switch");
        enable->setToggleState(!enable->getToggleState(), juce::sendNotificationSync);
        check(processor.currentValues()[fx.enabled] == (enable->getToggleState() ? 1.0f : 0.0f), "Effect power switch controls its parameter");
        for (int i = 0; i < fx.count; ++i) changeSlider(*editor, processor, fx.knobs[static_cast<std::size_t>(i)]);
    }
    const auto reverbBefore = editor->createComponentSnapshot(editor->getLocalBounds());
    auto* reverbDecay = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-reverbDecay"));
    check(reverbDecay != nullptr, "Reverb decay control is visible");
    reverbDecay->setValue(1.0, juce::sendNotificationSync);
    const auto reverbAfter = editor->createComponentSnapshot(editor->getLocalBounds());
    check(changedPixels(reverbBefore, reverbAfter, { 237, 365, 713, 102 }) > 20, "Reverb sketch follows decay");
    check(changedPixels(reverbBefore, reverbAfter, { 237, 350, 713, 15 }) == 0
          && changedPixels(reverbBefore, reverbAfter, { 237, 468, 713, 15 }) == 0,
          "Reverb illustration stays inside its graph frame");
    click(*editor, "fx-select-distortion");
    const auto distortionBefore = editor->createComponentSnapshot(editor->getLocalBounds());
    auto* drive = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-distortionDrive"));
    check(drive != nullptr, "Distortion drive control is visible");
    drive->setValue(8.0, juce::sendNotificationSync);
    const auto distortionAfter = editor->createComponentSnapshot(editor->getLocalBounds());
    check(changedPixels(distortionBefore, distortionAfter, { 237, 365, 713, 102 }) > 30, "Distortion sketch follows drive");
    click(*editor, "tab-modulation"); checkLayout(*editor); snapshot(*editor, previews, "modulation.png");
    const auto envelopeBefore = editor->createComponentSnapshot(editor->getLocalBounds());
    auto* attack = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-ampAttackWide"));
    auto* smooth = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-lfoSmooth"));
    auto* lfoDelay = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-lfoDelay"));
    auto* lfoStereo = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-lfoStereo"));
    auto* sustain = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-sustain"));
    check(attack && smooth && lfoDelay && lfoStereo && sustain
          && attack->getMinimum() == 0.0 && attack->getMaximum() == 32.0
          && smooth->getMaximum() == 16.0 && lfoDelay->getMaximum() == 4.0
          && lfoStereo->getMinimum() == -0.5 && lfoStereo->getMaximum() == 0.5
          && sustain->getTextFromValue(0.5) == "0.500", "Modulation controls expose the requested ranges and units");
    checkHit(*editor, "parameter-lfoSmooth", "LFO smooth knob is clickable");
    checkHit(*editor, "parameter-lfoDelay", "LFO delay knob is clickable");
    checkHit(*editor, "parameter-lfoStereo", "LFO stereo knob is clickable");
    attack->setValue(1.9, juce::sendNotificationSync);
    const auto envelopeAfter = editor->createComponentSnapshot(editor->getLocalBounds());
    check(changedPixels(envelopeBefore, envelopeAfter, { 44, 335, 440, 122 }) > 30, "Envelope preview follows the controls");
    auto* ampDelay = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-ampDelay"));
    auto* ampHold = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-ampHold"));
    check(ampDelay != nullptr && ampHold != nullptr && ampDelay->getTextFromValue(0.0) == "0.000 s"
          && ampHold->getTextFromValue(1.5) == "1.500 s", "New amp stages show time units");
    checkHit(*editor, "parameter-ampDelay", "Amp delay knob is independently clickable");
    checkHit(*editor, "parameter-ampHold", "Amp hold knob is independently clickable");
    ampDelay->setValue(0.5, juce::sendNotificationSync);
    const auto delayView = editor->createComponentSnapshot(editor->getLocalBounds());
    ampHold->setValue(0.75, juce::sendNotificationSync);
    const auto holdView = editor->createComponentSnapshot(editor->getLocalBounds());
    check(changedPixels(envelopeAfter, delayView, { 44, 335, 440, 122 }) > 30
          && changedPixels(delayView, holdView, { 44, 335, 440, 122 }) > 30,
          "Envelope graph displays both the delay and peak hold segments");
    snapshot(*editor, previews, "modulation-dahdsr.png");
    for (const auto parameter : { vanta::ampDelay, vanta::ampAttackWide, vanta::ampHold, vanta::ampDecayWide, vanta::sustain, vanta::ampReleaseWide, vanta::vibrato, vanta::vibratoRate, vanta::lfoSmooth, vanta::lfoDelay, vanta::lfoStereo })
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
void testVstEditor(const juce::File& previews)
{
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_VST3);
    VantaProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor()); editor->setVisible(true);
    check(editor->getWidth() == 1000 && editor->getHeight() == 720, "VST3 editor opens at 1000 x 720");
    check(findVisible(*editor, "standalone-keyboard") == nullptr, "VST3 does not add the standalone keyboard");
    check(findVisible(*editor, "edit-envelope") != nullptr && findVisible(*editor, "edit-vibrato") != nullptr,
          "Preview-style visual cards expose functional edit links");
    auto* preset = findVisible(*editor, "preset-browser");
    check(preset != nullptr && preset->getBounds() == juce::Rectangle<int>(74, 87, 700, 100),
          "VST3 preset display uses the approved large header layout");
    checkLayout(*editor); snapshot(*editor, previews, "vst-main.png");
    const std::pair<const char*, const char*> pages[] = {
        { "tab-oscillator", "vst-oscillator.png" }, { "tab-effects", "vst-effects.png" },
        { "tab-modulation", "vst-modulation.png" }, { "tab-mixer", "vst-mixer.png" },
        { "tab-sequencer", "vst-sequencer.png" }, { "tab-settings", "vst-settings.png" }
    };
    for (const auto& [tab, file] : pages) { click(*editor, tab); checkLayout(*editor); snapshot(*editor, previews, file); }
    click(*editor, "tab-modulation");
    auto* delay = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-ampDelay"));
    auto* hold = dynamic_cast<juce::Slider*>(findVisible(*editor, "parameter-ampHold"));
    check(delay && hold, "VST3 exposes both amp-envelope stages");
    delay->setValue(0.25, juce::sendNotificationSync); hold->setValue(0.4, juce::sendNotificationSync);
    checkLayout(*editor); snapshot(*editor, previews, "vst-modulation-dahdsr.png");
    delay->setValue(0.0, juce::sendNotificationSync); hold->setValue(0.0, juce::sendNotificationSync);
    click(*editor, "preset-browser"); checkLayout(*editor); snapshot(*editor, previews, "vst-browser.png");
    click(*editor, "tab-overview");
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
    testHostState(); testEnvelopeCompatibility(); testImportedState(); testImportedEditorControls(); testStandalone(previewDirectory); testVstEditor(previewDirectory);
    testFitScreenCalculation(); testResizeState(); testHostResizeNegotiation();
    testResizeMenu(juce::AudioProcessor::wrapperType_VST3, previewDirectory);
    testResizeMenu(juce::AudioProcessor::wrapperType_Standalone, previewDirectory);
    std::cout << "All VANTA editor checks passed.\n";
    return 0;
}
