#include "PluginEditor.h"
#include <cmath>

namespace
{
const juce::Colour red(0xfff52d36), softRed(0xffeaa0a5), panelRed(0xff9e292f);
void text(juce::Graphics& g, const juce::String& value, int x, int y, int w, int h,
          float size, juce::Colour colour, int style = juce::Font::plain,
          juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, style));
    g.drawText(value, x, y, w, h, justification, true);
}
void panel(juce::Graphics& g, float x, float y, float w, float h)
{
    auto rect = juce::Rectangle<float>(x, y, w, h);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff282224), x, y,
                                          juce::Colour(0xff070607), x + w, y + h, false));
    g.fillRoundedRectangle(rect, 8.0f);
    g.setColour(juce::Colour(0xffe9626a));
    g.drawRoundedRectangle(rect.reduced(0.7f), 8.0f, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawLine(x + 10.0f, y + 1.5f, x + w - 10.0f, y + 1.5f);
}
}

VantaLook::VantaLook()
{
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff140b0c));
    setColour(juce::ComboBox::textColourId, juce::Colours::white);
    setColour(juce::ComboBox::outlineColourId, panelRed);
    setColour(juce::ComboBox::arrowColourId, red);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1a0b0d));
    setColour(juce::PopupMenu::textColourId, juce::Colours::white);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff8e171e));
}
void VantaLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                 float position, float start, float end, juce::Slider& slider)
{
    const auto size = static_cast<float>(juce::jmin(width, height)) - 12.0f;
    auto bounds = juce::Rectangle<float>(static_cast<float>(x) + (static_cast<float>(width) - size) * 0.5f,
                                         static_cast<float>(y) + (static_cast<float>(height) - size) * 0.5f, size, size);
    const auto centre = bounds.getCentre();
    const auto radius = size * 0.46f;
    juce::Path track; track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    g.setColour(juce::Colour(0xff66141a));
    g.strokePath(track, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path progress; progress.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, start, start + position * (end - start), true);
    g.setColour(red);
    g.strokePath(progress, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto inner = bounds.reduced(size * 0.15f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff494142), inner.getX(), inner.getY(),
                                          juce::Colour(0xff090808), inner.getRight(), inner.getBottom(), false));
    g.fillEllipse(inner);
    g.setColour(juce::Colour(0xffd02c35)); g.drawEllipse(inner, 1.5f);
    const auto angle = start + position * (end - start);
    g.setColour(juce::Colours::white);
    g.drawLine(centre.x + std::sin(angle) * radius * 0.45f, centre.y - std::cos(angle) * radius * 0.45f,
               centre.x + std::sin(angle) * radius * 0.70f, centre.y - std::cos(angle) * radius * 0.70f, 2.2f);
    text(g, slider.getTextFromValue(slider.getValue()), static_cast<int>(inner.getX()), static_cast<int>(centre.y - 6.0f),
         static_cast<int>(inner.getWidth()), 17, 11.0f, juce::Colours::white, juce::Font::bold, juce::Justification::centred);
}
void VantaLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                                 float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float mid = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    g.setColour(juce::Colour(0xff4c1519)); g.fillRoundedRectangle(static_cast<float>(x), mid - 3.0f, static_cast<float>(width), 6.0f, 3.0f);
    g.setColour(red); g.fillRoundedRectangle(static_cast<float>(x), mid - 3.0f, juce::jmax(0.0f, position - static_cast<float>(x)), 6.0f, 3.0f);
    g.setColour(juce::Colours::white); g.fillEllipse(position - 6.0f, mid - 6.0f, 12.0f, 12.0f);
}
void VantaLook::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool hovered, bool down)
{
    const auto role = button.getProperties().getWithDefault("role", {}).toString();
    auto colour = role == "arrow" ? juce::Colour(0xff190a0b) : juce::Colour(0xff300a0d);
    if (button.getToggleState()) colour = juce::Colour(0xffd42730);
    if (hovered && button.isEnabled()) colour = colour.brighter(0.25f);
    if (down) colour = colour.brighter(0.15f);
    if (!button.isEnabled()) colour = colour.withMultipliedAlpha(0.45f);
    auto area = button.getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(colour); g.fillRoundedRectangle(area, 3.0f);
    g.setColour(button.getToggleState() ? juce::Colour(0xffff868b) : juce::Colour(0xff823038));
    g.drawRoundedRectangle(area, 3.0f, 1.0f);
}
void VantaLook::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont(juce::FontOptions(button.getProperties().getWithDefault("role", {}).toString() == "arrow" ? 19.0f : 10.0f,
                                juce::Font::bold));
    g.setColour(juce::Colours::white.withAlpha(button.isEnabled() ? 1.0f : 0.35f));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(3), juce::Justification::centred, 1);
}
void VantaKnob::configure(juce::AudioProcessorValueTreeState& state, vanta::Parameter parameter, const juce::String& labelText)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTitle(vanta::parameterSpecs[parameter].name);
    slider.setTooltip(juce::String(vanta::parameterSpecs[parameter].name) + " - drag or use arrow keys");
    slider.textFromValueFunction = [parameter](double value)
    {
        if (parameter == vanta::cutoff) return value >= 1000.0 ? juce::String(value / 1000.0, 1) + " kHz" : juce::String(value, 0) + " Hz";
        if (parameter == vanta::attack || parameter == vanta::decay || parameter == vanta::release || parameter == vanta::delayTime)
            return value < 1.0 ? juce::String(value * 1000.0, 0) + " ms" : juce::String(value, 2) + " s";
        if (parameter == vanta::vibratoRate) return juce::String(value, 1) + " Hz";
        if (parameter == vanta::distortionDrive) return juce::String(value, 1) + "x";
        return juce::String(value * 100.0, 0) + "%";
    };
    label.setText(labelText, juce::dontSendNotification);
    label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(slider); addAndMakeVisible(label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, vanta::parameterSpecs[parameter].id, slider);
}
void VantaKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromBottom(22));
    slider.setBounds(area);
}

VantaEditor::VantaEditor(VantaProcessor& owner) : AudioProcessorEditor(owner), processor(owner)
{
    setLookAndFeel(&look);
    setSize(1000, 700);
    configureButton(previous, "^", "arrow"); previous.setTooltip("Previous factory preset"); previous.onClick = [this] { changePreset(-1); };
    configureButton(next, "v", "arrow"); next.setTooltip("Next factory preset"); next.onClick = [this] { changePreset(1); };
    configureButton(browserButton, "<> ", "arrow"); browserButton.setTooltip("Open preset browser");
    browserButton.onClick = [this] { browserOpen = !browserOpen; refresh(); };
    configureButton(writeButton, "WRITE .VANTA"); writeButton.onClick = [this] { savePreset(); };
    configureButton(importButton, "IMPORT PRESET"); importButton.onClick = [this] { openPreset(); };
    const char* tabNames[] = { "MAIN", "OSC", "EFFECTS", "MODULATION", "SETTINGS" };
    for (int i = 0; i < 5; ++i) { configureButton(tabs[static_cast<std::size_t>(i)], tabNames[i], "tab"); tabs[static_cast<std::size_t>(i)].onClick = [this, i] { selectPage(i); }; }
    const char* modeNames[] = { "POLY  /  16 VOICES", "UNISON  /  LATER", "LEGATO  /  LATER", "PORTAMENTO  /  LATER" };
    for (int i = 0; i < 4; ++i) { configureButton(modes[static_cast<std::size_t>(i)], modeNames[i]); modes[static_cast<std::size_t>(i)].setEnabled(i == 0); }
    modes[0].setToggleState(true, juce::dontSendNotification);
    const char* folderNames[] = { "ALL", "PADS", "LEADS", "SYNTH" };
    for (int i = 0; i < 4; ++i) { configureButton(folders[static_cast<std::size_t>(i)], folderNames[i]); folders[static_cast<std::size_t>(i)].onClick = [this, i] { folder = i; refresh(); }; }
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i) { configureButton(presetButtons[static_cast<std::size_t>(i)], VantaProcessor::factoryPresetName(i)); presetButtons[static_cast<std::size_t>(i)].onClick = [this, i] { loadFactory(i); }; }
    for (int i = 0; i < 4; ++i) { configureButton(partials[static_cast<std::size_t>(i)], "PARTIAL " + juce::String(i + 1)); partials[static_cast<std::size_t>(i)].setEnabled(i == 0); }
    partials[0].setToggleState(true, juce::dontSendNotification);

    levelSlider.setSliderStyle(juce::Slider::LinearHorizontal); levelSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    levelSlider.setTitle("Level"); levelSlider.setTooltip("Master output level"); addAndMakeVisible(levelSlider);
    levelSlider.onValueChange = [this] { repaint(920, 207, 55, 27); };
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, "level", levelSlider);
    waveform.addItem("SINE", 1); waveform.addItem("SAW", 2); waveform.setTitle("Waveform"); addAndMakeVisible(waveform);
    waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state, "wave", waveform);

    const vanta::Parameter mainIds[] = { vanta::cutoff, vanta::resonance, vanta::attack, vanta::release, vanta::vibrato };
    const char* mainNames[] = { "CUTOFF", "RESONANCE", "ATTACK", "RELEASE", "VIBRATO" };
    for (int i = 0; i < 5; ++i) { mainKnobs[static_cast<std::size_t>(i)].configure(processor.state, mainIds[i], mainNames[i]); addAndMakeVisible(mainKnobs[static_cast<std::size_t>(i)]); }
    const vanta::Parameter fxIds[] = { vanta::reverbSize, vanta::reverbDecay, vanta::reverbMix, vanta::delayTime, vanta::delayFeedback, vanta::delayMix, vanta::distortionDrive, vanta::distortionTone, vanta::distortionMix };
    const char* fxNames[] = { "SIZE", "DECAY", "MIX", "TIME", "FEEDBACK", "MIX", "DRIVE", "TONE", "MIX" };
    for (int i = 0; i < 9; ++i) { effectKnobs[static_cast<std::size_t>(i)].configure(processor.state, fxIds[i], fxNames[i]); addAndMakeVisible(effectKnobs[static_cast<std::size_t>(i)]); }
    const vanta::Parameter modIds[] = { vanta::attack, vanta::decay, vanta::sustain, vanta::release, vanta::vibrato, vanta::vibratoRate };
    const char* modNames[] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "VIBRATO", "RATE" };
    for (int i = 0; i < 6; ++i) { modulationKnobs[static_cast<std::size_t>(i)].configure(processor.state, modIds[i], modNames[i]); addAndMakeVisible(modulationKnobs[static_cast<std::size_t>(i)]); }
    lastPresetName = processor.presetName();
    currentFactory = -1;
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
        if (lastPresetName == VantaProcessor::factoryPresetName(i)) currentFactory = i;
    startTimerHz(5);
    refresh();
}
VantaEditor::~VantaEditor() { stopTimer(); setLookAndFeel(nullptr); }
void VantaEditor::configureButton(juce::TextButton& button, const juce::String& label, const juce::String& role)
{
    button.setButtonText(label); button.getProperties().set("role", role); addAndMakeVisible(button);
}
void VantaEditor::timerCallback()
{
    const auto name = processor.presetName();
    if (name != lastPresetName)
    {
        lastPresetName = name;
        currentFactory = -1;
        for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
            if (name == VantaProcessor::factoryPresetName(i)) currentFactory = i;
        refresh();
    }
}
void VantaEditor::selectPage(int p) { page = p; browserOpen = false; refresh(); }
void VantaEditor::refresh()
{
    for (int i = 0; i < 5; ++i) { tabs[static_cast<std::size_t>(i)].setVisible(!browserOpen); tabs[static_cast<std::size_t>(i)].setToggleState(i == page, juce::dontSendNotification); }
    levelSlider.setVisible(!browserOpen);
    for (auto& knob : mainKnobs) knob.setVisible(!browserOpen && page == 0);
    for (auto& mode : modes) mode.setVisible(!browserOpen && page == 0);
    for (auto& partial : partials) partial.setVisible(!browserOpen && page == 1);
    waveform.setVisible(!browserOpen && page == 1);
    for (auto& knob : effectKnobs) knob.setVisible(!browserOpen && page == 2);
    for (auto& knob : modulationKnobs) knob.setVisible(!browserOpen && page == 3);
    const char* folderNames[] = { "ALL", "PADS", "LEADS", "SYNTH" };
    for (int i = 0; i < 4; ++i) { folders[static_cast<std::size_t>(i)].setVisible(browserOpen); folders[static_cast<std::size_t>(i)].setToggleState(i == folder, juce::dontSendNotification); }
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
    {
        const bool visible = folder == 0 || juce::String(VantaProcessor::factoryPresetCategory(i)) == folderNames[folder];
        presetButtons[static_cast<std::size_t>(i)].setVisible(browserOpen && visible);
        presetButtons[static_cast<std::size_t>(i)].setToggleState(i == currentFactory && processor.presetName() == VantaProcessor::factoryPresetName(i), juce::dontSendNotification);
    }
    resized(); repaint();
}
void VantaEditor::changePreset(int direction)
{
    currentFactory = currentFactory < 0 ? (direction < 0 ? VantaProcessor::factoryPresetCount - 1 : 0)
                                        : (currentFactory + direction + VantaProcessor::factoryPresetCount) % VantaProcessor::factoryPresetCount;
    loadFactory(currentFactory);
}
void VantaEditor::loadFactory(int index)
{
    currentFactory = index; processor.loadFactoryPreset(index); browserOpen = false; refresh();
}
void VantaEditor::showMessage(const juce::String& title, const juce::String& message, bool warning)
{
    juce::AlertWindow::showMessageBoxAsync(warning ? juce::MessageBoxIconType::WarningIcon : juce::MessageBoxIconType::InfoIcon,
                                           title, message, "OK", this);
}
void VantaEditor::openPreset()
{
    chooser = std::make_unique<juce::FileChooser>("Open a VANTA or Vital preset", juce::File(), "*.vanta;*.vital;*.fxp;*.SerumPreset;*.svz;*.svd;*.bin");
    juce::Component::SafePointer<VantaEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& selected)
        {
            if (safe == nullptr) return;
            const auto file = selected.getResult();
            if (file == juce::File()) return;
            const auto result = safe->processor.loadPresetFile(file);
            if (result.ok) { safe->currentFactory = -1; safe->browserOpen = false; safe->refresh(); }
            if (!result.ok || file.hasFileExtension(".vital")) safe->showMessage(result.ok ? "Vital import - approximation" : "Preset not loaded", result.message, true);
        });
}
void VantaEditor::savePreset()
{
    chooser = std::make_unique<juce::FileChooser>("Save a VANTA preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Vanta Preset.vanta"), "*.vanta");
    juce::Component::SafePointer<VantaEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe](const juce::FileChooser& selected)
        {
            if (safe == nullptr) return;
            auto file = selected.getResult();
            if (file == juce::File()) return;
            if (!file.hasFileExtension(".vanta"))
            {
                safe->showMessage("Preset not saved", "Please choose a filename ending in .vanta.", true);
                return;
            }
            const auto result = safe->processor.savePresetFile(file);
            if (result.failed()) safe->showMessage("Preset not saved", result.getErrorMessage(), true);
            else { safe->currentFactory = -1; safe->refresh(); }
        });
}
void VantaEditor::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffbe353d), 0.0f, 0.0f, juce::Colour(0xff58060a), 1000.0f, 700.0f, false));
    g.fillAll();
    g.setColour(juce::Colours::white.withAlpha(0.38f)); g.drawRect(getLocalBounds().reduced(1), 1);
    g.setColour(red); g.fillRect(0, 0, getWidth(), 4);
    juce::Path mark; mark.startNewSubPath(25.0f, 22.0f); mark.lineTo(33.0f, 22.0f); mark.lineTo(39.0f, 37.0f);
    mark.lineTo(45.0f, 22.0f); mark.lineTo(53.0f, 22.0f); mark.lineTo(42.0f, 49.0f); mark.lineTo(36.0f, 49.0f); mark.closeSubPath();
    g.setColour(juce::Colours::white); g.fillPath(mark);
    text(g, "VANTA", 64, 18, 215, 31, 25.0f, juce::Colours::white, juce::Font::bold);
    text(g, "SOFTWARE SYNTHESIZER  /  BETA 0.4", 66, 48, 370, 14, 9.0f, softRed, juce::Font::bold);
    g.setColour(juce::Colour(0xff11090a)); g.fillRoundedRectangle(710.0f, 18.0f, 270.0f, 35.0f, 5.0f);
    text(g, "INSTRUMENT", 724, 20, 92, 29, 10.0f, juce::Colours::white, juce::Font::bold, juce::Justification::centred);
    text(g, "STEREO", 817, 20, 65, 29, 10.0f, softRed, juce::Font::bold, juce::Justification::centred);
    text(g, "ENGINE READY", 884, 20, 90, 29, 9.0f, juce::Colour(0xffd2bbbb), juce::Font::plain, juce::Justification::centred);

    panel(g, 15.0f, 72.0f, 970.0f, 125.0f);
    text(g, "PRESET  /  VANTA COLLECTION", 86, 82, 600, 21, 10.0f, red, juce::Font::bold);
    const auto number = currentFactory >= 0 ? juce::String(94 + currentFactory).paddedLeft('0', 3) : juce::String("USER");
    text(g, number, 85, 112, 84, 38, 29.0f, red);
    text(g, processor.presetName(), 173, 112, 602, 38, 23.0f, juce::Colours::white, juce::Font::bold);
    text(g, currentFactory >= 0 ? juce::String(VantaProcessor::factoryPresetCategory(currentFactory)) + "  /  FACTORY SOUND" : "USER PRESET  /  VANTA PATCH",
         88, 157, 620, 18, 10.0f, juce::Colour(0xffaaa0a2));
    text(g, "TUNING    A = 440 Hz", 811, 165, 145, 18, 9.0f, softRed, juce::Font::bold, juce::Justification::centred);

    if (!browserOpen)
    {
        text(g, "LEVEL", 27, 207, 60, 27, 11.0f, juce::Colours::white, juce::Font::bold);
        text(g, juce::String(levelSlider.getValue() * 100.0, 0) + "%", 924, 207, 50, 27, 10.0f, juce::Colours::white, juce::Font::bold, juce::Justification::centredRight);
        panel(g, 15.0f, 300.0f, 970.0f, 330.0f);
        if (page == 0)
        {
            text(g, "PERFORMANCE & SOUND", 32, 310, 360, 24, 12.0f, juce::Colours::white, juce::Font::bold);
            text(g, "PLAY MODE", 32, 338, 160, 18, 9.0f, softRed, juce::Font::bold);
            text(g, "QUICK CONTROLS", 226, 338, 230, 18, 9.0f, softRed, juce::Font::bold);
            text(g, "DRAG TO ADJUST", 750, 338, 200, 18, 9.0f, juce::Colour(0xffa59395), juce::Font::plain, juce::Justification::centredRight);
            text(g, "Waveform: OSC tab   /   16 voices   /   MIDI velocity, sustain & pitch bend", 220, 535, 725, 35, 10.0f, juce::Colour(0xffbba5a7));
        }
        else if (page == 1)
        {
            text(g, "STRUCTURE  /  PARTIAL", 32, 310, 260, 20, 10.0f, softRed, juce::Font::bold);
            text(g, "ONE OSCILLATOR IN BETA 0.4", 680, 310, 270, 20, 10.0f, softRed, juce::Font::bold, juce::Justification::centredRight);
            for (int i = 0; i < 3; ++i)
            {
                const float x = static_cast<float>(31 + i * 314);
                g.setColour(juce::Colour(0xff180b0d)); g.fillRoundedRectangle(x, 390.0f, 300.0f, 210.0f, 4.0f);
                g.setColour(juce::Colour(0xff6c2228)); g.drawRoundedRectangle(x, 390.0f, 300.0f, 210.0f, 4.0f, 1.0f);
                text(g, "0" + juce::String(i + 1) + "   OSC " + juce::String(i + 1), static_cast<int>(x) + 15, 402, 240, 23, 12.0f, juce::Colours::white, juce::Font::bold);
            }
            text(g, "WAVEFORM", 47, 442, 230, 20, 10.0f, softRed, juce::Font::bold);
            text(g, "SINE  /  BAND-LIMITED SAW", 47, 515, 250, 23, 10.0f, juce::Colour(0xffc9b6b8));
            text(g, "Not implemented yet", 360, 460, 250, 30, 11.0f, juce::Colour(0xff9b8184));
            text(g, "Not implemented yet", 674, 460, 250, 30, 11.0f, juce::Colour(0xff9b8184));
        }
        else if (page == 2)
        {
            text(g, "SIGNAL PROCESSING  /  EFFECTS RACK", 32, 310, 450, 24, 12.0f, juce::Colours::white, juce::Font::bold);
            text(g, "INPUT  >  REVERB  >  DELAY  >  DISTORTION  >  OUTPUT", 32, 342, 800, 20, 10.0f, softRed, juce::Font::bold);
            const char* names[] = { "01    REVERB", "02    DELAY", "03    DISTORTION" };
            for (int i = 0; i < 3; ++i)
            {
                const float x = static_cast<float>(31 + i * 314);
                g.setColour(juce::Colour(0xff180b0d)); g.fillRoundedRectangle(x, 376.0f, 300.0f, 220.0f, 4.0f);
                g.setColour(juce::Colour(0xff6c2228)); g.drawRoundedRectangle(x, 376.0f, 300.0f, 220.0f, 4.0f, 1.0f);
                text(g, names[i], static_cast<int>(x) + 14, 386, 260, 22, 11.0f, juce::Colours::white, juce::Font::bold);
            }
        }
        else if (page == 3)
        {
            text(g, "MOVEMENT & CONTROL  /  MODULATION", 32, 310, 550, 24, 12.0f, juce::Colours::white, juce::Font::bold);
            text(g, "AMPLITUDE ENVELOPE", 40, 358, 530, 22, 10.0f, softRed, juce::Font::bold);
            text(g, "VIBRATO LFO", 668, 358, 250, 22, 10.0f, softRed, juce::Font::bold);
            text(g, "These controls affect the current sound. Additional routing is planned for later.", 40, 557, 850, 25, 10.0f, juce::Colour(0xffbba5a7));
        }
        else
        {
            text(g, "VANTA PREFERENCES  /  SETTINGS", 32, 310, 550, 24, 12.0f, juce::Colours::white, juce::Font::bold);
            text(g, "01   AUDIO & MIDI", 40, 365, 280, 23, 11.0f, softRed, juce::Font::bold);
            text(g, "FL Studio controls the device, sample rate and buffer size.", 40, 392, 850, 22, 11.0f, juce::Colours::white);
            text(g, "16 voices  /  MIDI velocity  /  sustain pedal  /  pitch bend +/- 2 semitones", 40, 416, 850, 22, 11.0f, juce::Colour(0xffcdbbbd));
            text(g, "02   PRESETS & COMPATIBILITY", 40, 466, 450, 23, 11.0f, softRed, juce::Font::bold);
            text(g, "VANTA .vanta: save and load    /    Vital .vital: partial, approximate import", 40, 493, 850, 22, 11.0f, juce::Colours::white);
            text(g, "Serum, Serum 2 and ZENOLOGY preset import is not implemented yet.", 40, 518, 850, 22, 11.0f, juce::Colour(0xffcdbbbd));
            text(g, "UI scale and other preferences are planned for a later beta.", 40, 552, 850, 22, 10.0f, juce::Colour(0xffac9396));
        }
    }
    else
    {
        panel(g, 15.0f, 215.0f, 970.0f, 415.0f);
        text(g, "PRESET BROWSER  /  FACTORY SOUNDS", 32, 230, 500, 24, 12.0f, juce::Colours::white, juce::Font::bold);
        text(g, "Select a sound, or use IMPORT PRESET above", 224, 257, 650, 22, 10.0f, softRed);
        g.setColour(juce::Colour(0xff6c2228)); g.drawVerticalLine(207, 270.0f, 600.0f);
    }
    g.setColour(juce::Colour(0xff43070a)); g.fillRect(15, 649, 970, 35);
    text(g, "VANTA  /  BETA 0.4", 29, 651, 250, 30, 9.0f, juce::Colours::white, juce::Font::bold);
    text(g, "NATIVE PRESETS + PARTIAL VITAL IMPORT  /  OTHER FORMATS NOT YET SUPPORTED", 278, 651, 683, 30, 9.0f, softRed, juce::Font::plain, juce::Justification::centredRight);
}
void VantaEditor::resized()
{
    previous.setBounds(26, 80, 42, 31); browserButton.setBounds(26, 117, 42, 31); next.setBounds(26, 154, 42, 31);
    writeButton.setBounds(817, 85, 142, 30); importButton.setBounds(817, 124, 142, 30);
    levelSlider.setBounds(91, 207, 814, 27);
    for (int i = 0; i < 5; ++i) tabs[static_cast<std::size_t>(i)].setBounds(16 + i * 145, 252, 137, 33);
    for (int i = 0; i < 4; ++i) modes[static_cast<std::size_t>(i)].setBounds(32, 365 + i * 38, 160, 30);
    for (int i = 0; i < 5; ++i) mainKnobs[static_cast<std::size_t>(i)].setBounds(218 + i * 146, 381, 125, 135);
    for (int i = 0; i < 4; ++i) partials[static_cast<std::size_t>(i)].setBounds(32 + i * 118, 342, 108, 29);
    waveform.setBounds(47, 473, 250, 32);
    for (int i = 0; i < 9; ++i) effectKnobs[static_cast<std::size_t>(i)].setBounds(39 + (i / 3) * 314 + (i % 3) * 94, 419, 86, 132);
    for (int i = 0; i < 6; ++i) modulationKnobs[static_cast<std::size_t>(i)].setBounds(38 + i * 155, 394, 125, 135);
    for (int i = 0; i < 4; ++i) folders[static_cast<std::size_t>(i)].setBounds(32, 280 + i * 42, 158, 34);
    int row = 0;
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
        if (presetButtons[static_cast<std::size_t>(i)].isVisible()) presetButtons[static_cast<std::size_t>(i)].setBounds(226, 288 + row++ * 46, 720, 38);
}
