#include "PluginEditor.h"
#include <cmath>

namespace
{
// A neutral instrument shell with two restrained accent colours. The coloured
// graphs are parameter illustrations, not a live audio or spectrum display.
const juce::Colour background(0xff0d1017), header(0xff11151e), surface(0xff191e29);
const juce::Colour surfaceDark(0xff121720), border(0xff323a49), textPrimary(0xfff0f2f7);
const juce::Colour textSecondary(0xffa1a9b9), textMuted(0xff778194);
const juce::Colour accent(0xffb8a2ff), mint(0xff79e4d0);

void text(juce::Graphics& g, const juce::String& value, int x, int y, int w, int h,
          float size, juce::Colour colour, int style = juce::Font::plain,
          juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, style));
    g.drawFittedText(value, x, y, w, h, justification, 1);
}
void panel(juce::Graphics& g, float x, float y, float w, float h)
{
    const auto area = juce::Rectangle<float>(x, y, w, h);
    g.setGradientFill(juce::ColourGradient(surface, x, y, juce::Colour(0xff151923), x, y + h, false));
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(border.withAlpha(0.82f));
    g.drawRoundedRectangle(area.reduced(0.5f), 10.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.035f));
    g.drawLine(x + 12.0f, y + 1.0f, x + w - 12.0f, y + 1.0f);
}
void separator(juce::Graphics& g, float x1, float x2, float y)
{
    g.setColour(border.withAlpha(0.7f));
    g.drawLine(x1, y, x2, y, 1.0f);
}
void sectionHeader(juce::Graphics& g, const juce::String& number, const juce::String& title,
                   const juce::String& detail, int x, int y, int width)
{
    text(g, number, x, y, 27, 18, 10.0f, accent, juce::Font::bold);
    text(g, title, x + 31, y, width - 31, 18, 12.0f, textPrimary, juce::Font::bold);
    text(g, detail, x + 31, y + 22, width - 31, 18, 11.0f, textSecondary);
}
void graphBackground(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(surfaceDark); g.fillRoundedRectangle(area, 6.0f);
    g.setColour(border.withAlpha(0.52f)); g.drawRoundedRectangle(area.reduced(0.5f), 6.0f, 1.0f);
    g.saveState();
    g.reduceClipRegion(area.reduced(1.0f).toNearestInt());
    g.setColour(juce::Colour(0xff313a47).withAlpha(0.55f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine(juce::roundToInt(area.getX() + area.getWidth() * static_cast<float>(i) / 8.0f), area.getY(), area.getBottom());
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine(juce::roundToInt(area.getY() + area.getHeight() * static_cast<float>(i) / 4.0f), area.getX(), area.getRight());
    g.restoreState();
}
void drawWave(juce::Graphics& g, juce::Rectangle<float> area, bool saw)
{
    graphBackground(g, area);
    auto graph = area.reduced(12.0f, 14.0f);
    juce::Path wave;
    constexpr int points = 400;
    for (int i = 0; i <= points; ++i)
    {
        const float unit = static_cast<float>(i) / static_cast<float>(points);
        const auto phase = std::fmod(unit * 3.0f, 1.0f);
        const auto sample = saw ? 2.0f * phase - 1.0f : std::sin(phase * juce::MathConstants<float>::twoPi);
        const auto x = graph.getX() + unit * graph.getWidth();
        const auto y = graph.getCentreY() - sample * graph.getHeight() * 0.43f;
        if (i == 0) wave.startNewSubPath(x, y); else wave.lineTo(x, y);
    }
    g.setColour(mint.withAlpha(0.12f)); g.strokePath(wave, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(mint); g.strokePath(wave, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
void drawEnvelope(juce::Graphics& g, juce::Rectangle<float> area, const vanta::ParameterValues& p)
{
    graphBackground(g, area);
    auto r = area.reduced(16.0f, 17.0f);
    // Sustain has no fixed duration; half a second is used solely for display.
    const float total = p[vanta::attack] + p[vanta::decay] + 0.5f + p[vanta::release];
    const float x0 = r.getX();
    const float x1 = x0 + r.getWidth() * p[vanta::attack] / total;
    const float x2 = x1 + r.getWidth() * p[vanta::decay] / total;
    const float x3 = x2 + r.getWidth() * 0.5f / total;
    const float x4 = r.getRight();
    const float low = r.getBottom(), high = r.getY();
    const float sustainY = low - r.getHeight() * p[vanta::sustain];
    juce::Path fill; fill.startNewSubPath(x0, low); fill.lineTo(x1, high); fill.lineTo(x2, sustainY);
    fill.lineTo(x3, sustainY); fill.lineTo(x4, low); fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(accent.withAlpha(0.22f), r.getX(), r.getY(), accent.withAlpha(0.015f), r.getX(), r.getBottom(), false));
    g.fillPath(fill);
    juce::Path line; line.startNewSubPath(x0, low); line.lineTo(x1, high); line.lineTo(x2, sustainY);
    line.lineTo(x3, sustainY); line.lineTo(x4, low);
    g.setColour(accent); g.strokePath(line, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    for (const auto point : { juce::Point<float>(x1, high), juce::Point<float>(x2, sustainY), juce::Point<float>(x3, sustainY) })
    {
        g.setColour(surfaceDark); g.fillEllipse(point.x - 3.5f, point.y - 3.5f, 7.0f, 7.0f);
        g.setColour(accent); g.drawEllipse(point.x - 3.5f, point.y - 3.5f, 7.0f, 7.0f, 1.5f);
    }
}
void drawLfo(juce::Graphics& g, juce::Rectangle<float> area, const vanta::ParameterValues& p)
{
    graphBackground(g, area);
    auto r = area.reduced(12.0f, 15.0f);
    juce::Path line;
    // This shows the selected depth/rate, not an oscilloscope of the audio.
    const auto cycles = juce::jmap(p[vanta::vibratoRate], 0.1f, 12.0f, 1.0f, 5.0f);
    const auto depth = p[vanta::vibrato];
    for (int i = 0; i <= 180; ++i)
    {
        const float t = static_cast<float>(i) / 180.0f;
        const float x = r.getX() + t * r.getWidth();
        const float y = r.getCentreY() - std::sin(t * cycles * juce::MathConstants<float>::twoPi) * r.getHeight() * 0.43f * depth;
        if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
    }
    g.setColour(mint); g.strokePath(line, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
void effectIllustration(juce::Graphics& g, int effect, juce::Rectangle<float> area, const vanta::ParameterValues& p)
{
    graphBackground(g, area);
    const auto c = area.getCentre();
    if (effect == 0)
    {
        for (int i = 5; i >= 1; --i)
        {
            const float radius = 9.0f + static_cast<float>(i) * 12.0f;
            g.setColour(accent.withAlpha(0.10f + (5.0f - static_cast<float>(i)) * 0.055f));
            g.drawEllipse(c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);
        }
        g.setColour(accent); g.fillEllipse(c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
    }
    else if (effect == 1)
    {
        for (int i = 0; i < 6; ++i)
        {
            const float h = 60.0f * std::pow(0.69f, static_cast<float>(i));
            g.setColour(mint.withAlpha(0.95f - static_cast<float>(i) * 0.13f));
            g.fillRoundedRectangle(area.getX() + 42.0f + static_cast<float>(i) * 35.0f, c.y - h * 0.5f, 5.0f, h, 2.5f);
        }
    }
    else
    {
        auto r = area.reduced(32.0f, 25.0f);
        juce::Path line;
        const float drive = p[vanta::distortionDrive];
        const float norm = std::tanh(drive);
        for (int i = 0; i <= 100; ++i)
        {
            const float t = static_cast<float>(i) / 100.0f;
            const float y = std::tanh((t * 2.0f - 1.0f) * drive) / norm;
            if (i == 0) line.startNewSubPath(r.getX(), r.getCentreY() - y * r.getHeight() * 0.5f);
            else line.lineTo(r.getX() + t * r.getWidth(), r.getCentreY() - y * r.getHeight() * 0.5f);
        }
        g.setColour(accent); g.strokePath(line, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}
}

VantaLook::VantaLook()
{
    setColour(juce::ComboBox::backgroundColourId, surfaceDark);
    setColour(juce::ComboBox::textColourId, textPrimary);
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::ComboBox::arrowColourId, accent);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff202632));
    setColour(juce::PopupMenu::textColourId, textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff514670));
    setColour(juce::PopupMenu::highlightedTextColourId, textPrimary);
}
void VantaLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                 float position, float start, float end, juce::Slider& slider)
{
    const float size = juce::jmax(10.0f, static_cast<float>(juce::jmin(width, height)) - 12.0f);
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x) + (static_cast<float>(width) - size) * 0.5f,
                                              static_cast<float>(y) + (static_cast<float>(height) - size) * 0.5f, size, size);
    const auto centre = bounds.getCentre();
    const float radius = size * 0.45f;
    juce::Path track; track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    g.setColour(juce::Colour(0xff3b4454));
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path progress; progress.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, start, start + position * (end - start), true);
    g.setColour(accent);
    g.strokePath(progress, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto inner = bounds.reduced(size * 0.17f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff353d4b), inner.getX(), inner.getY(), juce::Colour(0xff171c26), inner.getX(), inner.getBottom(), false));
    g.fillEllipse(inner);
    g.setColour(juce::Colour(0xff4b5566)); g.drawEllipse(inner, 1.0f);
    const auto angle = start + position * (end - start);
    g.setColour(textPrimary);
    g.drawLine(centre.x + std::sin(angle) * radius * 0.58f, centre.y - std::cos(angle) * radius * 0.58f,
               centre.x + std::sin(angle) * radius * 0.73f, centre.y - std::cos(angle) * radius * 0.73f, 2.0f);
    text(g, slider.getTextFromValue(slider.getValue()), static_cast<int>(inner.getX()), static_cast<int>(centre.y - 8.0f),
         static_cast<int>(inner.getWidth()), 16, juce::jlimit(9.0f, 12.0f, size * 0.14f), textPrimary, juce::Font::bold, juce::Justification::centred);
}
void VantaLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                                 float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float mid = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float left = static_cast<float>(x) + 5.0f;
    const float right = static_cast<float>(x + width) - 5.0f;
    const float pos = juce::jlimit(left, right, position);
    g.setColour(juce::Colour(0xff394252)); g.fillRoundedRectangle(left, mid - 2.0f, right - left, 4.0f, 2.0f);
    g.setColour(accent); g.fillRoundedRectangle(left, mid - 2.0f, juce::jmax(0.0f, pos - left), 4.0f, 2.0f);
    g.setColour(textPrimary); g.fillEllipse(pos - 5.0f, mid - 5.0f, 10.0f, 10.0f);
}
void VantaLook::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool hovered, bool down)
{
    const auto role = button.getProperties().getWithDefault("role", {}).toString();
    auto area = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool selected = button.getToggleState();
    if (role == "tab")
    {
        if (selected || hovered)
        {
            g.setColour(selected ? juce::Colour(0xff2b293e) : juce::Colour(0xff202632));
            g.fillRoundedRectangle(area, 6.0f);
        }
        if (selected)
        {
            g.setColour(accent); g.fillRoundedRectangle(area.getX() + 12.0f, area.getBottom() - 3.0f, area.getWidth() - 24.0f, 2.0f, 1.0f);
        }
        return;
    }
    auto fill = selected ? juce::Colour(0xff353047) : juce::Colour(0xff222834);
    if (role == "preset") fill = juce::Colour(0xff1a202a);
    if (role == "presetRow") fill = selected ? juce::Colour(0xff302b43) : juce::Colour(0xff1d232d);
    if (role == "primary") fill = juce::Colour(0xffa995e9);
    if (hovered && button.isEnabled()) fill = fill.brighter(0.12f);
    if (down) fill = fill.brighter(0.10f);
    if (!button.isEnabled()) fill = fill.withMultipliedAlpha(0.45f);
    g.setColour(fill); g.fillRoundedRectangle(area, role == "preset" ? 7.0f : 6.0f);
    g.setColour(selected ? accent.withAlpha(0.75f) : border);
    g.drawRoundedRectangle(area, role == "preset" ? 7.0f : 6.0f, 1.0f);
}
void VantaLook::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto role = button.getProperties().getWithDefault("role", {}).toString();
    const auto area = button.getLocalBounds();
    const bool enabled = button.isEnabled();
    if (role == "preset")
    {
        text(g, "CURRENT PRESET", 12, 6, area.getWidth() - 42, 13, 9.0f, textMuted, juce::Font::bold);
        text(g, button.getButtonText(), 12, 20, area.getWidth() - 42, 22, 13.0f, textPrimary, juce::Font::bold);
        const float cx = static_cast<float>(area.getRight() - 19), cy = static_cast<float>(area.getCentreY());
        juce::Path chevron; chevron.startNewSubPath(cx - 4.0f, cy - 2.0f); chevron.lineTo(cx, cy + 2.0f); chevron.lineTo(cx + 4.0f, cy - 2.0f);
        g.setColour(accent); g.strokePath(chevron, juce::PathStrokeType(1.5f));
        return;
    }
    if (role == "presetRow")
    {
        text(g, button.getButtonText(), 15, 0, area.getWidth() - 140, area.getHeight(), 12.0f, textPrimary, juce::Font::bold);
        text(g, button.getProperties().getWithDefault("category", {}).toString(), area.getWidth() - 112, 0, 90, area.getHeight(), 10.0f,
             button.getToggleState() ? accent : textSecondary, juce::Font::bold, juce::Justification::centredRight);
        return;
    }
    const float fontSize = role == "arrow" ? 17.0f : 10.5f;
    const auto colour = role == "primary" ? juce::Colour(0xff17141f)
                        : (role == "tab" && !button.getToggleState() ? textSecondary : textPrimary);
    text(g, button.getButtonText(), 4, 0, area.getWidth() - 8, area.getHeight(), fontSize,
         colour.withMultipliedAlpha(enabled ? 1.0f : 0.4f), juce::Font::bold, juce::Justification::centred);
}
void VantaLook::drawComboBox(juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox&)
{
    const auto area = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
    g.setColour(down ? juce::Colour(0xff2c3441) : juce::Colour(0xff202733)); g.fillRoundedRectangle(area, 6.0f);
    g.setColour(border); g.drawRoundedRectangle(area, 6.0f, 1.0f);
    const float x = static_cast<float>(width) - 17.0f, y = static_cast<float>(height) * 0.5f;
    juce::Path chevron; chevron.startNewSubPath(x - 4.0f, y - 2.0f); chevron.lineTo(x, y + 2.0f); chevron.lineTo(x + 4.0f, y - 2.0f);
    g.setColour(accent); g.strokePath(chevron, juce::PathStrokeType(1.5f));
}
juce::Font VantaLook::getComboBoxFont(juce::ComboBox&) { return juce::Font(juce::FontOptions(11.0f, juce::Font::bold)); }
void VantaLook::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(11, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}
void VantaKnob::configure(juce::AudioProcessorValueTreeState& state, vanta::Parameter parameter, const juce::String& labelText)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTitle(vanta::parameterSpecs[parameter].name);
    slider.setComponentID("parameter-" + juce::String(vanta::parameterSpecs[parameter].id));
    slider.setTooltip(juce::String(vanta::parameterSpecs[parameter].name) + " - drag to adjust, double-click to reset");
    slider.onValueChange = [this] { if (onValueChanged) onValueChanged(); };
    label.setText(labelText, juce::dontSendNotification);
    label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, textSecondary);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(slider); addAndMakeVisible(label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, vanta::parameterSpecs[parameter].id, slider);
    // JUCE's attachment installs its own formatter, so set our units afterwards.
    slider.textFromValueFunction = [parameter](double value)
    {
        if (parameter == vanta::cutoff) return value >= 1000.0 ? juce::String(value / 1000.0, 1) + " kHz" : juce::String(value, 0) + " Hz";
        if (parameter == vanta::attack || parameter == vanta::decay || parameter == vanta::release || parameter == vanta::delayTime)
            return value < 1.0 ? juce::String(value * 1000.0, 0) + " ms" : juce::String(value, 2) + " s";
        if (parameter == vanta::vibratoRate) return juce::String(value, 1) + " Hz";
        if (parameter == vanta::distortionDrive) return juce::String(value, 1) + "x";
        return juce::String(value * 100.0, 0) + "%";
    };
}
void VantaKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromBottom(23));
    slider.setBounds(area);
}

VantaEditor::VantaEditor(VantaProcessor& owner) : AudioProcessorEditor(owner), processor(owner)
{
    setLookAndFeel(&look);
    setSize(1000, 700);
    configureButton(previous, "<", "arrow"); previous.setComponentID("preset-previous"); previous.setTooltip("Previous factory preset"); previous.onClick = [this] { changePreset(-1); };
    configureButton(next, ">", "arrow"); next.setComponentID("preset-next"); next.setTooltip("Next factory preset"); next.onClick = [this] { changePreset(1); };
    configureButton(browserButton, "VANTA INIT", "preset"); browserButton.setComponentID("preset-browser"); browserButton.setTooltip("Browse factory sounds");
    browserButton.onClick = [this] { browserOpen = !browserOpen; refresh(); };
    configureButton(writeButton, "SAVE"); writeButton.setComponentID("preset-save"); writeButton.setTooltip("Save a VANTA preset"); writeButton.onClick = [this] { savePreset(); };
    configureButton(importButton, "IMPORT", "primary"); importButton.setComponentID("preset-import"); importButton.setTooltip("Open a VANTA or Vital preset"); importButton.onClick = [this] { openPreset(); };

    const char* tabNames[] = { "OVERVIEW", "OSCILLATOR", "EFFECTS", "MODULATION", "SETTINGS" };
    const char* tabIds[] = { "tab-overview", "tab-oscillator", "tab-effects", "tab-modulation", "tab-settings" };
    for (int i = 0; i < 5; ++i)
    {
        auto& tab = tabs[static_cast<std::size_t>(i)]; configureButton(tab, tabNames[i], "tab");
        tab.setComponentID(tabIds[i]); tab.onClick = [this, i] { selectPage(i); };
    }
    const char* folderNames[] = { "ALL SOUNDS", "PADS", "LEADS", "SYNTH" };
    const char* folderIds[] = { "folder-all", "folder-pads", "folder-leads", "folder-synth" };
    for (int i = 0; i < 4; ++i)
    {
        auto& item = folders[static_cast<std::size_t>(i)]; configureButton(item, folderNames[i], "filter");
        item.setComponentID(folderIds[i]); item.onClick = [this, i] { folder = i; refresh(); };
    }
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
    {
        auto& item = presetButtons[static_cast<std::size_t>(i)]; configureButton(item, VantaProcessor::factoryPresetName(i), "presetRow");
        item.setComponentID("factory-preset-" + juce::String(i));
        item.getProperties().set("category", VantaProcessor::factoryPresetCategory(i));
        item.onClick = [this, i] { loadFactory(i); };
    }

    levelSlider.setSliderStyle(juce::Slider::LinearHorizontal); levelSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    levelSlider.setTitle("Master level"); levelSlider.setComponentID("parameter-level"); levelSlider.setTooltip("Master output level"); addAndMakeVisible(levelSlider);
    levelSlider.onValueChange = [this] { repaint(730, 9, 250, 60); };
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, "level", levelSlider);
    waveform.addItem("SINE", 1); waveform.addItem("SAW", 2); waveform.setTitle("Waveform"); waveform.setComponentID("parameter-wave");
    waveform.setTooltip("Choose the oscillator waveform"); waveform.onChange = [this] { repaint(); }; addAndMakeVisible(waveform);
    waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state, "wave", waveform);

    const vanta::Parameter mainIds[] = { vanta::cutoff, vanta::resonance, vanta::attack, vanta::decay, vanta::sustain, vanta::release, vanta::vibrato, vanta::vibratoRate };
    const char* mainNames[] = { "CUTOFF", "RESONANCE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "DEPTH", "RATE" };
    for (int i = 0; i < 8; ++i)
    {
        auto& knob = mainKnobs[static_cast<std::size_t>(i)]; knob.configure(processor.state, mainIds[i], mainNames[i]);
        knob.onValueChanged = [this] { repaint(); }; addAndMakeVisible(knob);
    }
    const vanta::Parameter fxIds[] = { vanta::reverbSize, vanta::reverbDecay, vanta::reverbMix, vanta::delayTime, vanta::delayFeedback, vanta::delayMix, vanta::distortionDrive, vanta::distortionTone, vanta::distortionMix };
    const char* fxNames[] = { "SIZE", "DECAY", "MIX", "TIME", "FEEDBACK", "MIX", "DRIVE", "TONE", "MIX" };
    for (int i = 0; i < 9; ++i)
    {
        auto& knob = effectKnobs[static_cast<std::size_t>(i)]; knob.configure(processor.state, fxIds[i], fxNames[i]);
        knob.onValueChanged = [this] { if (page == 2) repaint(); }; addAndMakeVisible(knob);
    }
    const vanta::Parameter modIds[] = { vanta::attack, vanta::decay, vanta::sustain, vanta::release, vanta::vibrato, vanta::vibratoRate };
    const char* modNames[] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "DEPTH", "RATE" };
    for (int i = 0; i < 6; ++i)
    {
        auto& knob = modulationKnobs[static_cast<std::size_t>(i)]; knob.configure(processor.state, modIds[i], modNames[i]);
        knob.onValueChanged = [this] { if (page == 3) repaint(); }; addAndMakeVisible(knob);
    }

    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        standaloneKeyboard = std::make_unique<juce::MidiKeyboardComponent>(processor.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard);
        standaloneKeyboard->setName("On-screen MIDI keyboard"); standaloneKeyboard->setComponentID("standalone-keyboard");
        standaloneKeyboard->setAvailableRange(48, 84); standaloneKeyboard->setKeyWidth(43.25f);
        standaloneKeyboard->setKeyPressBaseOctave(5); standaloneKeyboard->setScrollButtonsVisible(false);
        standaloneKeyboard->setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffd8dde6));
        standaloneKeyboard->setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff242b37));
        standaloneKeyboard->setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff4c5564));
        standaloneKeyboard->setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha(0.8f));
        standaloneKeyboard->setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, mint.withAlpha(0.3f));
        addAndMakeVisible(*standaloneKeyboard);
    }
    lastPresetName = processor.presetName();
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
        lastPresetName = name; currentFactory = -1;
        for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
            if (name == VantaProcessor::factoryPresetName(i)) currentFactory = i;
        refresh();
    }
}
void VantaEditor::selectPage(int p) { page = p; browserOpen = false; refresh(); }
void VantaEditor::refresh()
{
    browserButton.setButtonText(processor.presetName());
    browserButton.setToggleState(browserOpen, juce::dontSendNotification);
    for (int i = 0; i < 5; ++i) tabs[static_cast<std::size_t>(i)].setToggleState(!browserOpen && i == page, juce::dontSendNotification);
    waveform.setVisible(!browserOpen && (page == 0 || page == 1));
    for (int i = 0; i < 8; ++i) mainKnobs[static_cast<std::size_t>(i)].setVisible(!browserOpen && (page == 0 || (page == 1 && i < 2)));
    for (auto& knob : effectKnobs) knob.setVisible(!browserOpen && page == 2);
    for (auto& knob : modulationKnobs) knob.setVisible(!browserOpen && page == 3);
    const char* categoryNames[] = { "", "PADS", "LEADS", "SYNTH" };
    for (int i = 0; i < 4; ++i)
    {
        auto& item = folders[static_cast<std::size_t>(i)]; item.setVisible(browserOpen); item.setToggleState(i == folder, juce::dontSendNotification);
    }
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
    {
        const bool visible = folder == 0 || juce::String(VantaProcessor::factoryPresetCategory(i)) == categoryNames[folder];
        auto& item = presetButtons[static_cast<std::size_t>(i)]; item.setVisible(browserOpen && visible);
        item.setToggleState(i == currentFactory && processor.presetName() == VantaProcessor::factoryPresetName(i), juce::dontSendNotification);
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
    g.fillAll(background);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff191b29).withAlpha(0.55f), 1000.0f, 85.0f,
                                          background.withAlpha(0.0f), 340.0f, 580.0f, true));
    g.fillRect(0, 76, 1000, 524);
    g.setColour(header); g.fillRect(0, 0, 1000, 76);
    separator(g, 0.0f, 1000.0f, 75.5f);

    // Geometric V mark and a compact, persistent preset/output strip.
    g.setColour(juce::Colour(0xff2a253b)); g.fillRoundedRectangle(24.0f, 20.0f, 36.0f, 36.0f, 9.0f);
    juce::Path logo; logo.startNewSubPath(32.0f, 29.0f); logo.lineTo(37.0f, 29.0f); logo.lineTo(42.0f, 44.0f);
    logo.lineTo(47.0f, 29.0f); logo.lineTo(52.0f, 29.0f); logo.lineTo(45.0f, 48.0f); logo.lineTo(39.0f, 48.0f); logo.closeSubPath();
    g.setColour(accent); g.fillPath(logo);
    text(g, "VANTA", 70, 19, 106, 23, 20.0f, textPrimary, juce::Font::bold);
    text(g, "SYNTHESIZER", 72, 44, 105, 13, 9.0f, textMuted, juce::Font::bold);
    text(g, "MASTER OUTPUT", 735, 13, 190, 15, 9.5f, textSecondary, juce::Font::bold);
    text(g, juce::String(levelSlider.getValue() * 100.0, 0) + "%", 924, 32, 52, 24, 11.0f, textPrimary, juce::Font::bold, juce::Justification::centredRight);
    g.setColour(mint); g.fillEllipse(839.0f, 100.0f, 6.0f, 6.0f);
    text(g, standaloneKeyboard ? "STANDALONE" : "VST3 INSTRUMENT", 854, 90, 122, 27, 10.0f, textMuted, juce::Font::bold);
    separator(g, 24.0f, 976.0f, 130.5f);

    const auto values = processor.currentValues();
    if (browserOpen)
    {
        panel(g, 24.0f, 148.0f, 952.0f, 446.0f);
        text(g, "LIBRARY", 44, 168, 165, 18, 10.0f, accent, juce::Font::bold);
        text(g, "Factory sounds", 44, 189, 180, 25, 15.0f, textPrimary, juce::Font::bold);
        g.setColour(border); g.drawVerticalLine(238, 168.0f, 571.0f);
        text(g, "CHOOSE A SOUND", 264, 168, 420, 18, 10.0f, textMuted, juce::Font::bold);
        text(g, "VANTA collection", 264, 189, 420, 26, 17.0f, textPrimary, juce::Font::bold);
        text(g, "5 FACTORY PRESETS", 746, 190, 200, 22, 10.0f, textSecondary, juce::Font::bold, juce::Justification::centredRight);
        separator(g, 264.0f, 950.0f, 226.0f);
        text(g, "Import a .vanta preset or a partial Vital conversion using IMPORT above.", 264, 525, 680, 22, 11.0f, textSecondary);
        text(g, "Select a sound to return to the instrument.", 264, 549, 680, 18, 10.5f, textMuted);
    }
    else if (page == 0)
    {
        panel(g, 24.0f, 148.0f, 530.0f, 196.0f);
        panel(g, 566.0f, 148.0f, 410.0f, 196.0f);
        panel(g, 24.0f, 356.0f, 530.0f, 238.0f);
        panel(g, 566.0f, 356.0f, 410.0f, 238.0f);
        sectionHeader(g, "01", "OSCILLATOR", "Choose your source", 44, 166, 260);
        drawWave(g, { 44.0f, 220.0f, 490.0f, 96.0f }, waveform.getSelectedId() == 2);
        text(g, "WAVE SHAPE", 45, 319, 180, 15, 9.5f, textMuted, juce::Font::bold);
        text(g, "SINE / BAND-LIMITED SAW", 300, 319, 233, 15, 9.5f, textSecondary, juce::Font::bold, juce::Justification::centredRight);
        sectionHeader(g, "02", "FILTER", "Sculpt the tone", 586, 166, 250);
        text(g, "LOW PASS", 855, 169, 98, 17, 9.5f, mint, juce::Font::bold, juce::Justification::centredRight);
        sectionHeader(g, "03", "AMPLITUDE", "Shape the envelope", 44, 374, 270);
        text(g, "ADSR", 465, 377, 65, 16, 9.5f, accent, juce::Font::bold, juce::Justification::centredRight);
        text(g, "Amplitude envelope", 46, 567, 300, 16, 10.0f, textMuted);
        sectionHeader(g, "04", "MOTION", "Add pitch movement", 586, 374, 260);
        text(g, "VIBRATO", 846, 377, 105, 16, 9.5f, mint, juce::Font::bold, juce::Justification::centredRight);
        text(g, "Vibrato depth and speed", 588, 567, 300, 16, 10.0f, textMuted);
    }
    else if (page == 1)
    {
        panel(g, 24.0f, 148.0f, 576.0f, 446.0f);
        panel(g, 612.0f, 148.0f, 364.0f, 446.0f);
        sectionHeader(g, "01", "OSCILLATOR", "One sound source, sixteen voices", 44, 168, 330);
        text(g, "WAVE SHAPE", 44, 224, 210, 18, 10.0f, textMuted, juce::Font::bold);
        drawWave(g, { 44.0f, 252.0f, 536.0f, 205.0f }, waveform.getSelectedId() == 2);
        text(g, "A simple starting point for your sound.", 45, 477, 500, 22, 12.0f, textPrimary);
        text(g, "Select sine or band-limited saw above. Additional oscillators are not yet available.", 45, 505, 514, 38, 11.0f, textSecondary);
        sectionHeader(g, "02", "LOW-PASS FILTER", "Control brightness and resonance", 632, 168, 318);
        separator(g, 632.0f, 956.0f, 224.0f);
        text(g, "FILTER CONTROLS", 632, 246, 260, 18, 10.0f, textMuted, juce::Font::bold);
        text(g, "Cutoff controls the brightness of each voice.", 632, 480, 300, 22, 11.0f, textSecondary);
        text(g, "Resonance emphasises the cutoff region.", 632, 505, 300, 22, 11.0f, textSecondary);
    }
    else if (page == 2)
    {
        const char* titles[] = { "REVERB", "DELAY", "DISTORTION" };
        const char* details[] = { "Add space and atmosphere", "Create repeats and echoes", "Add colour and saturation" };
        const char* badges[] = { "SPACE", "ECHO", "DRIVE" };
        for (int i = 0; i < 3; ++i)
        {
            const auto x = 24 + i * 322;
            panel(g, static_cast<float>(x), 148.0f, 308.0f, 446.0f);
            sectionHeader(g, "0" + juce::String(i + 1), titles[i], details[i], x + 20, 168, 266);
            text(g, badges[i], x + 20, 224, 120, 17, 10.0f, textMuted, juce::Font::bold);
            text(g, "SIGNAL SKETCH", x + 150, 224, 136, 17, 9.5f, textMuted, juce::Font::bold, juce::Justification::centredRight);
            effectIllustration(g, i, { static_cast<float>(x + 20), 250.0f, 268.0f, 144.0f }, values);
            separator(g, static_cast<float>(x + 20), static_cast<float>(x + 288), 413.0f);
            text(g, "CONTROLS", x + 20, 419, 220, 17, 9.5f, textSecondary, juce::Font::bold);
            text(g, i == 0 ? "01 / REVERB" : i == 1 ? "02 / DELAY" : "03 / DISTORTION", x + 20, 567, 260, 16, 9.5f, textMuted);
        }
    }
    else if (page == 3)
    {
        panel(g, 24.0f, 148.0f, 616.0f, 446.0f);
        panel(g, 652.0f, 148.0f, 324.0f, 446.0f);
        sectionHeader(g, "01", "AMPLITUDE ENVELOPE", "Shape how each note starts and fades", 44, 168, 500);
        text(g, "ADSR SHAPE", 44, 223, 320, 18, 10.0f, textMuted, juce::Font::bold);
        drawEnvelope(g, { 44.0f, 249.0f, 576.0f, 139.0f }, values);
        text(g, "Sustain length is illustrative.", 44, 396, 350, 17, 10.0f, textMuted);
        sectionHeader(g, "02", "VIBRATO", "Pitch movement", 672, 168, 270);
        text(g, "LFO SHAPE", 672, 223, 255, 18, 10.0f, textMuted, juce::Font::bold);
        drawLfo(g, { 672.0f, 249.0f, 284.0f, 139.0f }, values);
        text(g, "Illustration, not a live audio display.", 672, 396, 275, 17, 10.0f, textMuted);
    }
    else
    {
        panel(g, 24.0f, 148.0f, 574.0f, 446.0f);
        panel(g, 610.0f, 148.0f, 366.0f, 446.0f);
        sectionHeader(g, "01", "AUDIO & MIDI", "Your setup", 44, 168, 500);
        separator(g, 44.0f, 578.0f, 224.0f);
        text(g, standaloneKeyboard ? "Choose your output and MIDI input" : "Your host manages audio and MIDI", 44, 245, 510, 24, 14.0f, textPrimary, juce::Font::bold);
        text(g, standaloneKeyboard ? "Open Options > Audio/MIDI Settings in the window title bar." : "Choose your device, sample rate and buffer size in your DAW.",
             44, 278, 510, 22, 11.0f, textSecondary);
        text(g, standaloneKeyboard ? "Click the piano below to play with your mouse or computer keys." : "Connect a MIDI controller or use your DAW's piano roll to play.",
             44, 306, 510, 22, 11.0f, textSecondary);
        separator(g, 44.0f, 578.0f, 354.0f);
        text(g, "ENGINE", 44, 376, 200, 18, 10.0f, accent, juce::Font::bold);
        text(g, "16 voices  /  velocity  /  sustain  /  pitch bend", 44, 404, 500, 22, 12.0f, textPrimary);
        text(g, "One oscillator  /  sine and saw  /  stereo output", 44, 432, 500, 22, 11.0f, textSecondary);
        text(g, "VANTA BETA 0.6", 44, 548, 300, 18, 10.0f, textMuted, juce::Font::bold);
        sectionHeader(g, "02", "PRESET SUPPORT", "Know what loads", 630, 168, 310);
        separator(g, 630.0f, 956.0f, 224.0f);
        text(g, ".VANTA", 630, 247, 200, 20, 12.0f, textPrimary, juce::Font::bold);
        text(g, "Save and load native presets", 630, 272, 300, 20, 11.0f, textSecondary);
        text(g, ".VITAL", 630, 320, 200, 20, 12.0f, textPrimary, juce::Font::bold);
        text(g, "Partial, approximate import only", 630, 345, 300, 20, 11.0f, textSecondary);
        text(g, "OTHER FORMATS", 630, 393, 200, 20, 12.0f, textPrimary, juce::Font::bold);
        text(g, "Serum and ZENOLOGY are not supported yet.", 630, 418, 300, 37, 11.0f, textSecondary);
    }

    if (standaloneKeyboard)
    {
        text(g, "PLAY", 25, 607, 48, 18, 10.0f, accent, juce::Font::bold);
        text(g, "CLICK KEYS OR FOCUS KEYBOARD: A W S E D F T G Y H U J", 77, 607, 600, 18, 10.0f, textSecondary, juce::Font::bold);
        text(g, "AUDIO / MIDI: WINDOW OPTIONS", 695, 607, 280, 18, 10.0f, textMuted, juce::Font::bold, juce::Justification::centredRight);
    }
    else
    {
        separator(g, 24.0f, 976.0f, 620.0f);
        text(g, "16 VOICES", 24, 633, 100, 20, 10.0f, mint, juce::Font::bold);
        text(g, "VELOCITY  /  SUSTAIN  /  PITCH BEND", 126, 633, 470, 20, 10.0f, textSecondary, juce::Font::bold);
        text(g, "VANTA  /  BETA 0.6", 756, 633, 220, 20, 10.0f, textMuted, juce::Font::bold, juce::Justification::centredRight);
        text(g, "A VANTA INSTRUMENT", 24, 660, 340, 16, 9.5f, textMuted);
    }
}
void VantaEditor::resized()
{
    previous.setBounds(190, 21, 32, 36); browserButton.setBounds(228, 12, 239, 54); next.setBounds(473, 21, 32, 36);
    writeButton.setBounds(525, 21, 70, 35); importButton.setBounds(601, 21, 88, 35);
    levelSlider.setBounds(735, 31, 178, 25);
    const int tabX[] = { 24, 146, 279, 387, 530 };
    const int tabW[] = { 118, 129, 104, 139, 112 };
    for (int i = 0; i < 5; ++i) tabs[static_cast<std::size_t>(i)].setBounds(tabX[i], 87, tabW[i], 38);
    if (standaloneKeyboard) standaloneKeyboard->setBounds(24, 630, 952, 55);

    if (page == 0)
    {
        waveform.setBounds(386, 165, 148, 32);
        mainKnobs[0].setBounds(595, 206, 151, 125); mainKnobs[1].setBounds(781, 206, 151, 125);
        for (int i = 0; i < 4; ++i) mainKnobs[static_cast<std::size_t>(i + 2)].setBounds(35 + i * 129, 429, 120, 126);
        mainKnobs[6].setBounds(595, 429, 151, 126); mainKnobs[7].setBounds(781, 429, 151, 126);
    }
    else if (page == 1)
    {
        waveform.setBounds(414, 168, 166, 32);
        mainKnobs[0].setBounds(637, 300, 139, 144); mainKnobs[1].setBounds(798, 300, 139, 144);
    }
    for (int i = 0; i < 9; ++i)
        effectKnobs[static_cast<std::size_t>(i)].setBounds(35 + (i / 3) * 322 + (i % 3) * 95, 438, 94, 116);
    for (int i = 0; i < 4; ++i) modulationKnobs[static_cast<std::size_t>(i)].setBounds(42 + i * 145, 426, 132, 127);
    modulationKnobs[4].setBounds(664, 426, 135, 127); modulationKnobs[5].setBounds(816, 426, 135, 127);
    for (int i = 0; i < 4; ++i) folders[static_cast<std::size_t>(i)].setBounds(44, 238 + i * 46, 175, 38);
    int row = 0;
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
        if (presetButtons[static_cast<std::size_t>(i)].isVisible())
            presetButtons[static_cast<std::size_t>(i)].setBounds(264, 240 + row++ * 49, 686, 41);
}
