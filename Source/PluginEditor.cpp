#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
// Red-and-black instrument shell based on the approved HTML study. The graphs
// illustrate settings; they are not live audio or spectrum measurements.
const juce::Colour background(0xff851018), header(0xffa31c24), surface(0xff120c0d);
const juce::Colour surfaceDark(0xff0b0909), border(0xff572b27), textPrimary(0xffffe5d1);
const juce::Colour textSecondary(0xffd0a89b), textMuted(0xffa0796f);
const juce::Colour accent(0xffff634b), mint(0xff82e1cf);

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
    g.setGradientFill(juce::ColourGradient(surface, x, y, juce::Colour(0xff180d0e), x, y + h, false));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(border.withAlpha(0.82f));
    g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.0f);
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
    g.setColour(juce::Colour(0xff512725).withAlpha(0.55f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine(juce::roundToInt(area.getX() + area.getWidth() * static_cast<float>(i) / 8.0f), area.getY(), area.getBottom());
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine(juce::roundToInt(area.getY() + area.getHeight() * static_cast<float>(i) / 4.0f), area.getX(), area.getRight());
    g.restoreState();
}
void drawWave(juce::Graphics& g, juce::Rectangle<float> area, int shape)
{
    graphBackground(g, area);
    auto graph = area.reduced(12.0f, 14.0f);
    juce::Path wave;
    constexpr int points = 400;
    for (int i = 0; i <= points; ++i)
    {
        const float unit = static_cast<float>(i) / static_cast<float>(points);
        const auto phase = std::fmod(unit * 3.0f, 1.0f);
        const auto sample = shape == 1 ? 2.0f * phase - 1.0f
                          : shape == 2 ? (phase < 0.5f ? 1.0f : -1.0f)
                          : shape == 3 ? 1.0f - 4.0f * std::abs(phase - 0.5f)
                          : std::sin(phase * juce::MathConstants<float>::twoPi);
        const auto x = graph.getX() + unit * graph.getWidth();
        const auto y = graph.getCentreY() - sample * graph.getHeight() * 0.43f;
        if (i == 0) wave.startNewSubPath(x, y); else wave.lineTo(x, y);
    }
    g.setColour(accent.withAlpha(0.10f)); g.strokePath(wave, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accent); g.strokePath(wave, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
void drawFilter(juce::Graphics& g, juce::Rectangle<float> area, const vanta::ParameterValues& p)
{
    graphBackground(g, area);
    const auto r = area.reduced(8.0f, 8.0f);
    const double q = 0.707 + 5.0 * static_cast<double>(p[vanta::resonance]);
    juce::Path curve;
    for (int i = 0; i <= 160; ++i)
    {
        const double t = static_cast<double>(i) / 160.0;
        const double frequency = 30.0 * std::pow(600.0, t), ratio = frequency / p[vanta::cutoff];
        const double magnitude = 1.0 / std::sqrt(std::pow(1.0 - ratio * ratio, 2.0) + std::pow(ratio / q, 2.0));
        const auto db = juce::jlimit(-48.0, 12.0, 20.0 * std::log10(magnitude));
        const auto x = r.getX() + static_cast<float>(t) * r.getWidth();
        const auto y = r.getBottom() - static_cast<float>((db + 48.0) / 60.0) * r.getHeight();
        if (i == 0) curve.startNewSubPath(x, y); else curve.lineTo(x, y);
    }
    g.setColour(accent); g.strokePath(curve, juce::PathStrokeType(1.8f));
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
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff2b1212));
    setColour(juce::PopupMenu::textColourId, textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff713026));
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
    g.setColour(juce::Colour(0xff572725));
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path progress; progress.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, start, start + position * (end - start), true);
    g.setColour(accent);
    g.strokePath(progress, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto inner = bounds.reduced(size * 0.17f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff542722), inner.getX(), inner.getY(), juce::Colour(0xff1a1010), inner.getX(), inner.getBottom(), false));
    g.fillEllipse(inner);
    g.setColour(juce::Colour(0xff8f4436)); g.drawEllipse(inner, 1.0f);
    const auto angle = start + position * (end - start);
    g.setColour(textPrimary);
    g.drawLine(centre.x + std::sin(angle) * radius * 0.58f, centre.y - std::cos(angle) * radius * 0.58f,
               centre.x + std::sin(angle) * radius * 0.73f, centre.y - std::cos(angle) * radius * 0.73f, 2.0f);
    text(g, slider.getTextFromValue(slider.getValue()), static_cast<int>(inner.getX()), static_cast<int>(centre.y - 8.0f),
         static_cast<int>(inner.getWidth()), 16, juce::jlimit(9.0f, 12.0f, size * 0.14f), textPrimary, juce::Font::bold, juce::Justification::centred);
}
void VantaLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                                 float, float, juce::Slider::SliderStyle style, juce::Slider&)
{
    if (style == juce::Slider::LinearVertical)
    {
        const auto centre = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const auto top = static_cast<float>(y) + 7.0f, bottom = static_cast<float>(y + height) - 7.0f;
        const auto pos = juce::jlimit(top, bottom, position);
        g.setColour(juce::Colour(0xff632e28)); g.fillRoundedRectangle(centre - 2.0f, top, 4.0f, bottom - top, 2.0f);
        g.setColour(accent); g.fillRoundedRectangle(centre - 2.0f, pos, 4.0f, bottom - pos, 2.0f);
        g.setColour(textPrimary); g.fillRoundedRectangle(centre - 11.0f, pos - 4.0f, 22.0f, 8.0f, 2.0f);
        return;
    }
    const float mid = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float left = static_cast<float>(x) + 5.0f;
    const float right = static_cast<float>(x + width) - 5.0f;
    const float pos = juce::jlimit(left, right, position);
    g.setColour(juce::Colour(0xff632e28)); g.fillRoundedRectangle(left, mid - 2.0f, right - left, 4.0f, 2.0f);
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
            g.setColour(selected ? juce::Colour(0xffffa176) : juce::Colour(0xff2b1212));
            g.fillRoundedRectangle(area, 6.0f);
        }
        if (selected)
        {
            g.setColour(accent); g.fillRoundedRectangle(area.getX() + 12.0f, area.getBottom() - 3.0f, area.getWidth() - 24.0f, 2.0f, 1.0f);
        }
        return;
    }
    auto fill = selected ? juce::Colour(0xff71271f) : juce::Colour(0xff2b1112);
    if (role == "preset") fill = juce::Colour(0xff100a0a);
    if (role == "presetRow") fill = selected ? juce::Colour(0xff53201b) : juce::Colour(0xff1a0d0e);
    if (role == "primary") fill = juce::Colour(0xff7d2b24);
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
    const auto colour = role == "tab" && button.getToggleState() ? juce::Colour(0xff42120e)
                        : (role == "tab" ? textSecondary : textPrimary);
    text(g, button.getButtonText(), 4, 0, area.getWidth() - 8, area.getHeight(), fontSize,
         colour.withMultipliedAlpha(enabled ? 1.0f : 0.4f), juce::Font::bold, juce::Justification::centred);
}
void VantaLook::drawComboBox(juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox&)
{
    const auto area = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
    g.setColour(down ? juce::Colour(0xff5c251f) : juce::Colour(0xff2b1112)); g.fillRoundedRectangle(area, 6.0f);
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
        for (int layer = 0; layer < vanta::oscillatorCount; ++layer)
        {
            if (parameter == vanta::oscillatorTune(layer)) return juce::String(value, 0) + " st";
            if (parameter == vanta::oscillatorFine(layer)) return juce::String(value, 0) + " ct";
            if (parameter == vanta::oscillatorPan(layer))
                return std::abs(value) < 0.005 ? juce::String("C") : juce::String(value < 0 ? "L " : "R ") + juce::String(std::abs(value) * 100.0, 0);
        }
        return juce::String(value * 100.0, 0) + "%";
    };
}
void VantaKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromBottom(23));
    slider.setBounds(area);
}

VantaKeyboard::VantaKeyboard(juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent(state, juce::MidiKeyboardComponent::horizontalKeyboard), midiState(state)
{
    setName("On-screen MIDI keyboard"); setComponentID("standalone-keyboard");
    setAvailableRange(48, 84); setKeyWidth(43.25f);
    setOctaveForMiddleC(4); // MIDI 60 is C4, as in the approved preview.
    setScrollButtonsVisible(false);
    // Computer keys are handled below so Shift does not alter digit keys.
    clearKeyMappings();
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffeee7dc));
    setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff181517));
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff888077));
    setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0xffb73929));
    setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x22ffbda0));
}
VantaKeyboard::~VantaKeyboard() { releaseComputerKeys(); }
void VantaKeyboard::drawBlackNote(int note, juce::Graphics& g, juce::Rectangle<float> area,
                                  bool isDown, bool isOver, juce::Colour noteFill)
{
    if (isDown) { g.setColour(juce::Colour(0xffb73929)); g.fillRect(area); return; }
    juce::MidiKeyboardComponent::drawBlackNote(note, g, area, false, isOver, noteFill);
}
bool VantaKeyboard::keyPressed(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCtrlDown() || mods.isAltDown() || mods.isCommandDown()) return false;
    const auto code = juce::CharacterFunctions::toLowerCase(static_cast<juce::juce_wchar>(key.getKeyCode()));
    constexpr char keys[] = "q2w3er5t6y7ui9o0p";
    for (int i = 0; i < 17; ++i) if (code == keys[i]) return true;
    // Some keyboard layouts report shifted US digits as their symbol.
    return mods.isShiftDown() && juce::String("@#%^&()").containsChar(code);
}
bool VantaKeyboard::keyStateChanged(bool)
{
    const auto mods = juce::ModifierKeys::getCurrentModifiers();
    const bool shortcuts = mods.isCtrlDown() || mods.isAltDown() || mods.isCommandDown();
    constexpr char keys[] = "q2w3er5t6y7ui9o0p";
    std::array<bool, 17> held {};
    for (int i = 0; i < 17; ++i) held[static_cast<std::size_t>(i)] = juce::KeyPress::isKeyCurrentlyDown(keys[i]);
    return updateComputerKeys(held, shortcuts);
}
bool VantaKeyboard::updateComputerKeys(const std::array<bool, 17>& held, bool shortcutHeld)
{
    bool changed = false;
    for (int i = 0; i < 17; ++i)
    {
        const auto index = static_cast<std::size_t>(i);
        const bool down = !shortcutHeld && held[index];
        if (down == computerKeysDown[index]) continue;
        computerKeysDown[index] = down; changed = true;
        if (down) midiState.noteOn(1, 60 + i, 0.8f); else midiState.noteOff(1, 60 + i, 0.0f);
    }
    return changed;
}
void VantaKeyboard::releaseComputerKeys()
{
    for (int i = 0; i < 17; ++i)
        if (computerKeysDown[static_cast<std::size_t>(i)])
        {
            midiState.noteOff(1, 60 + i, 0.0f); computerKeysDown[static_cast<std::size_t>(i)] = false;
        }
}
void VantaKeyboard::focusLost(FocusChangeType type)
{
    releaseComputerKeys();
    juce::MidiKeyboardComponent::focusLost(type);
}

VantaEditor::VantaEditor(VantaProcessor& owner) : AudioProcessorEditor(owner), processor(owner)
{
    setLookAndFeel(&look);
    setSize(1000, 700);
    configureButton(previous, "<", "arrow"); previous.setComponentID("preset-previous"); previous.setTooltip("Previous factory preset"); previous.onClick = [this] { changePreset(-1); };
    configureButton(next, ">", "arrow"); next.setComponentID("preset-next"); next.setTooltip("Next factory preset"); next.onClick = [this] { changePreset(1); };
    configureButton(browserButton, "VANTA INIT", "preset"); browserButton.setComponentID("preset-browser"); browserButton.setTooltip("Browse and search factory sounds");
    browserButton.onClick = [this] { browserOpen = !browserOpen; refresh(); };
    configureButton(writeButton, "WRITE"); writeButton.setComponentID("preset-save"); writeButton.setTooltip("Save a VANTA preset"); writeButton.onClick = [this] { savePreset(); };
    configureButton(importButton, "IMPORT"); importButton.setComponentID("preset-import"); importButton.setTooltip("Open a VANTA or Vital preset"); importButton.onClick = [this] { openPreset(); };

    const char* tabNames[] = { "MAIN", "OSC", "EFFECTS", "MOD", "MIXER", "SEQ", "SYSTEM" };
    const char* tabIds[] = { "tab-overview", "tab-oscillator", "tab-effects", "tab-modulation", "tab-mixer", "tab-sequencer", "tab-settings" };
    for (int i = 0; i < 7; ++i)
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
    presetSearch.setComponentID("preset-search"); presetSearch.setTitle("Search factory preset names or categories");
    presetSearch.setTextToShowWhenEmpty("Search name or category", textMuted);
    presetSearch.setColour(juce::TextEditor::backgroundColourId, surfaceDark);
    presetSearch.setColour(juce::TextEditor::textColourId, textPrimary);
    presetSearch.setColour(juce::TextEditor::outlineColourId, border);
    presetSearch.setColour(juce::TextEditor::focusedOutlineColourId, accent);
    presetSearch.setFont(juce::FontOptions(12.0f));
    presetSearch.onTextChange = [this] { refresh(); }; addAndMakeVisible(presetSearch);

    levelSlider.setSliderStyle(juce::Slider::LinearHorizontal); levelSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    levelSlider.setTitle("Master level"); levelSlider.setComponentID("parameter-level"); levelSlider.setTooltip("Master output level"); addAndMakeVisible(levelSlider);
    levelSlider.onValueChange = [this] { repaint(); };
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, "level", levelSlider);
    waveform.addItem("SINE", 1); waveform.addItem("SAW", 2); waveform.addItem("SQUARE", 3); waveform.addItem("TRIANGLE", 4);
    waveform.setTitle("Selected oscillator waveform"); waveform.setComponentID("parameter-wave");
    waveform.setTooltip("Choose the selected oscillator's waveform"); waveform.onChange = [this] { setWaveform(waveform.getSelectedId() - 1); }; addAndMakeVisible(waveform);

    for (int layer = 0; layer < vanta::oscillatorCount; ++layer)
    {
        const auto i = static_cast<std::size_t>(layer);
        const auto letter = juce::String::charToString(static_cast<juce::juce_wchar>('A' + layer));
        auto& tab = oscillatorTabs[i]; configureButton(tab, "OSC " + letter, "filter");
        tab.setComponentID("osc-tab-" + letter); tab.setTooltip("Edit oscillator " + letter);
        tab.onClick = [this, layer] { selectedOsc = layer; refresh(); };
        const vanta::Parameter ids[] = { vanta::oscillatorLevel(layer), vanta::oscillatorTune(layer), vanta::oscillatorFine(layer), vanta::oscillatorPan(layer) };
        const char* labels[] = { "LEVEL", "SEMITONE", "FINE", "PAN" };
        for (int k = 0; k < 4; ++k)
        {
            auto& knob = oscillatorKnobs[i][static_cast<std::size_t>(k)]; knob.configure(processor.state, ids[k], labels[k]);
            knob.onValueChanged = [this] { repaint(); }; addAndMakeVisible(knob);
        }
        auto& enabled = mixerEnabled[i]; configureButton(enabled, "ON", "filter");
        enabled.setComponentID("mixer-osc-" + letter + "-enabled"); enabled.setTooltip("Enable oscillator " + letter);
        if (layer == 0) { enabled.setToggleState(true, juce::dontSendNotification); enabled.setEnabled(false); }
        else
        {
            enabled.setClickingTogglesState(true);
            mixerEnabledAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,
                    vanta::parameterSpecs[vanta::oscillatorEnabled(layer)].id, enabled);
            enabled.onClick = [this] { refresh(); };
        }
        auto setupMixerSlider = [this, i, &letter](juce::Slider& slider, vanta::Parameter parameter, bool vertical)
        {
            slider.setSliderStyle(vertical ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.setTitle(vanta::parameterSpecs[parameter].name);
            slider.setComponentID("mixer-" + juce::String(vanta::parameterSpecs[parameter].id));
            slider.setTooltip(juce::String(vanta::parameterSpecs[parameter].name));
            slider.onValueChange = [this] { repaint(); }; addAndMakeVisible(slider);
            auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, vanta::parameterSpecs[parameter].id, slider);
            if (vertical) mixerLevelAttachments[i] = std::move(attachment); else mixerPanAttachments[i] = std::move(attachment);
        };
        setupMixerSlider(mixerLevel[i], ids[0], true); setupMixerSlider(mixerPan[i], ids[3], false);
    }
    configureButton(addOscillator, "+ NEW TAB", "primary"); addOscillator.setComponentID("add-oscillator");
    addOscillator.setTooltip("Enable the next oscillator"); addOscillator.onClick = [this]
    {
        const auto values = processor.currentValues();
        for (int layer = 1; layer < vanta::oscillatorCount; ++layer)
            if (!vanta::oscillatorIsEnabled(values, layer)) { setParameter(vanta::oscillatorEnabled(layer), 1.0f); selectedOsc = layer; break; }
        refresh();
    };
    configureButton(removeOscillator, "REMOVE", "filter"); removeOscillator.setComponentID("remove-oscillator");
    removeOscillator.setTooltip("Disable the selected oscillator"); removeOscillator.onClick = [this]
    {
        if (selectedOsc > 0) { setParameter(vanta::oscillatorEnabled(selectedOsc), 0.0f); selectedOsc = 0; }
        refresh();
    };

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
        knob.onValueChanged = [this] { repaint(); }; addAndMakeVisible(knob);
    }
    const vanta::Parameter modIds[] = { vanta::attack, vanta::decay, vanta::sustain, vanta::release, vanta::vibrato, vanta::vibratoRate };
    const char* modNames[] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "DEPTH", "RATE" };
    for (int i = 0; i < 6; ++i)
    {
        auto& knob = modulationKnobs[static_cast<std::size_t>(i)]; knob.configure(processor.state, modIds[i], modNames[i]);
        knob.onValueChanged = [this] { repaint(); }; addAndMakeVisible(knob);
    }

    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        standaloneKeyboard = std::make_unique<VantaKeyboard>(processor.keyboardState);
        addAndMakeVisible(*standaloneKeyboard);
    }
    lastPresetName = processor.presetName();
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
        if (lastPresetName == VantaProcessor::factoryPresetName(i)) currentFactory = i;
    startTimerHz(10);
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
    bool needsRefresh = false;
    if (name != lastPresetName)
    {
        lastPresetName = name; currentFactory = -1;
        for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
            if (name == VantaProcessor::factoryPresetName(i)) currentFactory = i;
        needsRefresh = true;
    }
    const auto values = processor.currentValues();
    int mask = 0;
    for (int i = 0; i < vanta::oscillatorCount; ++i) if (vanta::oscillatorIsEnabled(values, i)) mask |= 1 << i;
    if (mask != lastOscMask || waveform.getSelectedId() != vanta::oscillatorShape(values, selectedOsc) + 1) needsRefresh = true;
    if (needsRefresh) refresh(); else repaint();
}
void VantaEditor::selectPage(int p) { page = p; browserOpen = false; refresh(); }
void VantaEditor::setParameter(vanta::Parameter id, float value)
{
    if (auto* parameter = processor.state.getParameter(vanta::parameterSpecs[id].id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}
void VantaEditor::setWaveform(int shape)
{
    if (shape < 0 || shape > 3) return;
    if (selectedOsc == 0)
    {
        // The original two-choice parameter remains unchanged for old automation.
        if (shape < 2) { setParameter(vanta::wave, static_cast<float>(shape)); setParameter(vanta::oscAExtraWave, 0.0f); }
        else setParameter(vanta::oscAExtraWave, static_cast<float>(shape - 1));
    }
    else setParameter(vanta::oscillatorWave(selectedOsc), static_cast<float>(shape));
    refresh();
}
void VantaEditor::refresh()
{
    const auto values = processor.currentValues();
    if (!vanta::oscillatorIsEnabled(values, selectedOsc)) selectedOsc = 0;
    int mask = 0, activeCount = 0;
    for (int i = 0; i < vanta::oscillatorCount; ++i)
        if (vanta::oscillatorIsEnabled(values, i)) { mask |= 1 << i; ++activeCount; }
    lastOscMask = mask;
    browserButton.setButtonText(processor.presetName());
    browserButton.setToggleState(browserOpen, juce::dontSendNotification);
    for (int i = 0; i < 7; ++i) tabs[static_cast<std::size_t>(i)].setToggleState(!browserOpen && i == page, juce::dontSendNotification);
    waveform.setVisible(!browserOpen && page == 1);
    waveform.setComponentID(selectedOsc == 0 ? "parameter-wave" : "parameter-" + juce::String(vanta::parameterSpecs[vanta::oscillatorWave(selectedOsc)].id));
    waveform.setSelectedId(vanta::oscillatorShape(values, selectedOsc) + 1, juce::dontSendNotification);
    for (int i = 0; i < 8; ++i) mainKnobs[static_cast<std::size_t>(i)].setVisible(!browserOpen && (page == 0 || (page == 1 && i < 2)));
    for (auto& knob : effectKnobs) knob.setVisible(!browserOpen && page == 2);
    for (auto& knob : modulationKnobs) knob.setVisible(!browserOpen && page == 3);
    addOscillator.setVisible(!browserOpen && page == 1); addOscillator.setEnabled(activeCount < vanta::oscillatorCount);
    removeOscillator.setVisible(!browserOpen && page == 1 && selectedOsc > 0);
    for (int layer = 0; layer < vanta::oscillatorCount; ++layer)
    {
        const auto index = static_cast<std::size_t>(layer);
        const bool enabled = vanta::oscillatorIsEnabled(values, layer);
        oscillatorTabs[index].setVisible(!browserOpen && page == 1 && enabled);
        oscillatorTabs[index].setToggleState(layer == selectedOsc, juce::dontSendNotification);
        for (auto& knob : oscillatorKnobs[index]) knob.setVisible(!browserOpen && page == 1 && layer == selectedOsc);
        mixerEnabled[index].setVisible(!browserOpen && page == 4);
        mixerEnabled[index].setButtonText(enabled ? "ON" : "OFF");
        mixerLevel[index].setVisible(!browserOpen && page == 4); mixerLevel[index].setEnabled(enabled);
        mixerPan[index].setVisible(!browserOpen && page == 4); mixerPan[index].setEnabled(enabled);
    }
    const char* categoryNames[] = { "", "PADS", "LEADS", "SYNTH" };
    for (int i = 0; i < 4; ++i)
    {
        auto& item = folders[static_cast<std::size_t>(i)]; item.setVisible(browserOpen); item.setToggleState(i == folder, juce::dontSendNotification);
    }
    presetSearch.setVisible(browserOpen);
    const auto query = presetSearch.getText().trim();
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
    {
        const juce::String name(VantaProcessor::factoryPresetName(i)), category(VantaProcessor::factoryPresetCategory(i));
        const bool visible = (folder == 0 || category == categoryNames[folder])
                             && (query.isEmpty() || name.containsIgnoreCase(query) || category.containsIgnoreCase(query));
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
    g.setGradientFill(juce::ColourGradient(header, 0.0f, 0.0f, juce::Colour(0xff6b0b11), 1000.0f, 700.0f, false));
    g.fillRect(getLocalBounds());
    g.setColour(juce::Colour(0xffff7255)); g.fillRect(0, 0, 1000, 3);
    g.setColour(juce::Colour(0xff461313)); g.drawRect(getLocalBounds().reduced(3), 1);

    g.setColour(surfaceDark); g.fillRoundedRectangle(24.0f, 17.0f, 34.0f, 34.0f, 3.0f);
    text(g, "V.", 27, 16, 29, 35, 24.0f, textPrimary, juce::Font::bold, juce::Justification::centred);
    text(g, "VANTA", 69, 12, 160, 29, 22.0f, textPrimary, juce::Font::bold);
    text(g, "SOFTWARE INSTRUMENT / 07", 70, 41, 210, 14, 9.0f, textSecondary, juce::Font::bold);
    text(g, standaloneKeyboard ? "STANDALONE  /  16 VOICES" : "VST3  /  16 VOICES", 784, 52, 180, 12, 9.0f, textSecondary, juce::Font::plain, juce::Justification::centredRight);
    panel(g, 24.0f, 66.0f, 952.0f, 72.0f);
    text(g, "A = 440 Hz", 847, 113, 98, 14, 9.0f, textSecondary, juce::Font::plain, juce::Justification::centredRight);
    text(g, "LEVEL", 25, 146, 60, 20, 10.0f, textPrimary, juce::Font::bold);
    text(g, juce::String(levelSlider.getValue() * 100.0, 0) + "%", 920, 146, 53, 20, 10.0f, textPrimary, juce::Font::bold, juce::Justification::centredRight);
    panel(g, 24.0f, 175.0f, 952.0f, 420.0f);

    const auto values = processor.currentValues();
    if (browserOpen)
    {
        text(g, "LIBRARY", 44, 188, 160, 18, 10.0f, accent, juce::Font::bold);
        text(g, "Factory sounds", 44, 210, 180, 23, 15.0f, textPrimary, juce::Font::bold);
        g.setColour(border); g.drawVerticalLine(238, 192.0f, 569.0f);
        text(g, "SEARCH PRESETS", 264, 190, 350, 19, 11.0f, textPrimary, juce::Font::bold);
        text(g, "5 FACTORY SOUNDS", 741, 190, 200, 19, 10.0f, textSecondary, juce::Font::bold, juce::Justification::centredRight);
        const bool hasMatch = std::any_of(presetButtons.begin(), presetButtons.end(), [](const auto& button) { return button.isVisible(); });
        if (!hasMatch) text(g, "No factory presets match this search.", 264, 278, 640, 24, 11.0f, textSecondary);
        text(g, "Search applies to factory names and categories.", 264, 535, 640, 19, 10.0f, textMuted);
        text(g, "Import VANTA or an approximate Vital conversion above.", 264, 557, 640, 18, 10.0f, textMuted);
    }
    else if (page == 0)
    {
        text(g, "01  PERFORMANCE / SOUND", 42, 187, 450, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, "16 VOICES  /  GLOBAL CONTROLS", 655, 187, 300, 20, 9.0f, textMuted, juce::Font::bold, juce::Justification::centredRight);
        separator(g, 42.0f, 958.0f, 210.0f);
        text(g, "01  AMP ENVELOPE", 43, 344, 300, 18, 10.0f, textPrimary, juce::Font::bold);
        text(g, "02  PITCH VIBRATO", 518, 344, 300, 18, 10.0f, textPrimary, juce::Font::bold);
        drawEnvelope(g, { 42.0f, 368.0f, 442.0f, 154.0f }, values);
        drawLfo(g, { 516.0f, 368.0f, 442.0f, 154.0f }, values);
        text(g, "ADSR  /  SUSTAIN LENGTH IS ILLUSTRATIVE", 43, 531, 430, 17, 9.0f, textMuted);
        text(g, "SINE VIBRATO  /  NOT A LIVE AUDIO DISPLAY", 517, 531, 440, 17, 9.0f, textMuted);
    }
    else if (page == 1)
    {
        text(g, "02  OSCILLATOR / FILTER", 42, 187, 460, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, "UP TO 8 OSCILLATORS  /  GLOBAL LOW-PASS", 640, 187, 318, 20, 9.0f, textMuted, juce::Font::bold, juce::Justification::centredRight);
        separator(g, 42.0f, 958.0f, 210.0f);
        panel(g, 38.0f, 257.0f, 565.0f, 319.0f);
        panel(g, 615.0f, 257.0f, 342.0f, 319.0f);
        const auto letter = juce::String::charToString(static_cast<juce::juce_wchar>('A' + selectedOsc));
        text(g, "OSCILLATOR " + letter, 54, 271, 305, 18, 11.0f, textPrimary, juce::Font::bold);
        text(g, "WAVE", 386, 270, 48, 22, 9.0f, textMuted, juce::Font::bold);
        drawWave(g, { 54.0f, 304.0f, 532.0f, 105.0f }, vanta::oscillatorShape(values, selectedOsc));
        text(g, "WAVE SHAPE / ILLUSTRATIVE", 55, 413, 400, 15, 9.0f, textMuted);
        text(g, "GLOBAL LOW-PASS", 632, 271, 285, 18, 11.0f, textPrimary, juce::Font::bold);
        drawFilter(g, { 632.0f, 305.0f, 308.0f, 88.0f }, values);
        text(g, "ALL ENABLED OSCILLATORS SHARE THIS FILTER", 632, 548, 307, 16, 9.0f, textMuted);
    }
    else if (page == 2)
    {
        text(g, "03  EFFECTS / SIGNAL CHAIN", 42, 187, 450, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, "REVERB  >  DELAY  >  DISTORTION", 650, 187, 308, 20, 9.0f, textMuted, juce::Font::bold, juce::Justification::centredRight);
        separator(g, 42.0f, 958.0f, 210.0f);
        const char* titles[] = { "01 / REVERB", "02 / DELAY", "03 / DISTORTION" };
        for (int i = 0; i < 3; ++i)
        {
            const int x = 38 + i * 306;
            panel(g, static_cast<float>(x), 225.0f, 296.0f, 350.0f);
            text(g, titles[i], x + 14, 239, 260, 20, 11.0f, textPrimary, juce::Font::bold);
            effectIllustration(g, i, { static_cast<float>(x + 14), 273.0f, 268.0f, 135.0f }, values);
            text(g, "ILLUSTRATIVE RESPONSE", x + 14, 414, 260, 14, 9.0f, textMuted);
        }
        text(g, "MORE PROCESSORS AND REORDERING ARE PLANNED", 42, 578, 600, 14, 9.0f, textMuted);
    }
    else if (page == 3)
    {
        text(g, "04  MODULATION / ENVELOPE", 42, 187, 460, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, "AMP ENVELOPE  /  SINE PITCH VIBRATO", 640, 187, 318, 20, 9.0f, textMuted, juce::Font::bold, juce::Justification::centredRight);
        separator(g, 42.0f, 958.0f, 210.0f);
        panel(g, 38.0f, 225.0f, 565.0f, 350.0f); panel(g, 615.0f, 225.0f, 342.0f, 350.0f);
        text(g, "01 / AMPLITUDE ENVELOPE", 54, 239, 480, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, "02 / PITCH VIBRATO", 632, 239, 300, 20, 11.0f, textPrimary, juce::Font::bold);
        drawEnvelope(g, { 54.0f, 272.0f, 532.0f, 120.0f }, values);
        drawLfo(g, { 632.0f, 272.0f, 308.0f, 120.0f }, values);
        text(g, "MORE ENVELOPES AND THE MOD MATRIX ARE PLANNED", 54, 550, 520, 15, 9.0f, textMuted);
        text(g, "GENERAL-PURPOSE LFOS ARE PLANNED", 632, 550, 308, 15, 9.0f, textMuted);
    }
    else if (page == 4)
    {
        text(g, "05  MIXER / ROUTING", 42, 187, 450, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, "LAYER LEVEL + PAN  /  GLOBAL FILTER AND FX", 630, 187, 328, 20, 9.0f, textMuted, juce::Font::bold, juce::Justification::centredRight);
        separator(g, 42.0f, 958.0f, 210.0f);
        for (int layer = 0; layer < vanta::oscillatorCount; ++layer)
        {
            const int x = 38 + layer * 115;
            panel(g, static_cast<float>(x), 225.0f, 106.0f, 350.0f);
            const auto letter = juce::String::charToString(static_cast<juce::juce_wchar>('A' + layer));
            text(g, "OSC " + letter, x + 9, 237, 88, 18, 10.0f, textPrimary, juce::Font::bold, juce::Justification::centred);
            text(g, juce::String(values[vanta::oscillatorLevel(layer)] * 100.0f, 0) + "%", x + 9, 464, 88, 18, 10.0f, textSecondary, juce::Font::bold, juce::Justification::centred);
            text(g, "PAN", x + 9, 490, 88, 15, 9.0f, textMuted, juce::Font::bold, juce::Justification::centred);
            text(g, layer == 0 ? "ALWAYS ON" : "LAYER", x + 9, 553, 88, 14, 8.0f, textMuted, juce::Font::plain, juce::Justification::centred);
        }
    }
    else if (page == 5)
    {
        text(g, "06  SEQUENCING", 42, 187, 450, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, "DESIGN TARGETS / NOT IMPLEMENTED", 630, 187, 328, 20, 9.0f, textMuted, juce::Font::bold, juce::Justification::centredRight);
        separator(g, 42.0f, 958.0f, 210.0f);
        panel(g, 38.0f, 225.0f, 450.0f, 350.0f); panel(g, 508.0f, 225.0f, 449.0f, 350.0f);
        text(g, "01 / ARPEGGIATOR", 54, 240, 300, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, "32 STEPS / PLANNED", 54, 265, 300, 18, 9.0f, textSecondary);
        text(g, "02 / CLIP + AUTOMATION", 524, 240, 360, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, "64 STEPS / PLANNED", 524, 265, 300, 18, 9.0f, textSecondary);
        for (int i = 0; i < 32; ++i)
        {
            const float x = static_cast<float>(54 + (i % 16) * 25), y = static_cast<float>(303 + (i / 16) * 30);
            g.setColour((i % 4 == 0) ? juce::Colour(0xff8e4137) : surfaceDark); g.fillRect(x, y, 20.0f, 23.0f);
            g.setColour(border); g.drawRect(x, y, 20.0f, 23.0f, 1.0f);
        }
        for (int i = 0; i < 64; ++i)
        {
            const float x = static_cast<float>(524 + (i % 16) * 25), y = static_cast<float>(303 + (i / 16) * 30);
            g.setColour((i % 7 == 0) ? juce::Colour(0xff8e4137) : surfaceDark); g.fillRect(x, y, 20.0f, 23.0f);
            g.setColour(border); g.drawRect(x, y, 20.0f, 23.0f, 1.0f);
        }
        text(g, "Not playable yet; planned for a future build.", 54, 499, 400, 26, 11.0f, textSecondary);
        text(g, "Not playable yet; planned for a future build.", 524, 499, 400, 26, 11.0f, textSecondary);
    }
    else
    {
        text(g, "07  SYSTEM / INFORMATION", 42, 187, 450, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, "WINDOWS  /  VST3 + STANDALONE", 650, 187, 308, 20, 9.0f, textMuted, juce::Font::bold, juce::Justification::centredRight);
        separator(g, 42.0f, 958.0f, 210.0f);
        panel(g, 38.0f, 225.0f, 565.0f, 350.0f); panel(g, 615.0f, 225.0f, 342.0f, 350.0f);
        text(g, "01 / AUDIO & MIDI", 54, 242, 500, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, standaloneKeyboard ? "Options > Audio/MIDI Settings" : "Audio and MIDI are configured in your DAW.", 54, 278, 520, 23, 13.0f, textSecondary);
        text(g, "16 voices  /  velocity  /  sustain  /  pitch bend", 54, 315, 520, 23, 12.0f, textPrimary);
        text(g, "Up to 8 analog oscillators  /  shared filter and effects", 54, 344, 520, 23, 11.0f, textSecondary);
        text(g, "More synthesis, modulation and effects are planned.", 54, 373, 520, 23, 11.0f, textSecondary);
        text(g, "VANTA BETA 0.7", 54, 538, 300, 18, 10.0f, textMuted, juce::Font::bold);
        text(g, "02 / PRESET SUPPORT", 632, 242, 300, 20, 11.0f, textPrimary, juce::Font::bold);
        text(g, ".VANTA  /  SAVE + LOAD", 632, 278, 300, 22, 11.0f, textPrimary);
        text(g, ".VITAL  /  PARTIAL IMPORT", 632, 311, 300, 22, 11.0f, textSecondary);
        text(g, "SERUM / ZENOLOGY  /  UNSUPPORTED", 632, 344, 300, 32, 10.0f, textSecondary);
    }

    if (standaloneKeyboard)
    {
        text(g, "02  PERFORMANCE KEYBOARD", 25, 605, 250, 18, 10.0f, textPrimary, juce::Font::bold);
        text(g, "CLICK KEYS OR FOCUS: Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P", 282, 605, 690, 18, 9.5f, textSecondary, juce::Font::bold, juce::Justification::centredRight);
    }
    else
    {
        text(g, "VANTA  /  SOFTWARE SYNTHESIZER", 25, 617, 450, 20, 10.0f, textSecondary, juce::Font::bold);
        text(g, "VST3  /  BETA 0.7", 770, 617, 200, 20, 10.0f, textSecondary, juce::Font::bold, juce::Justification::centredRight);
    }
}
void VantaEditor::resized()
{
    previous.setBounds(31, 76, 27, 24); next.setBounds(31, 104, 27, 24);
    browserButton.setBounds(65, 72, 645, 60); writeButton.setBounds(771, 78, 78, 31); importButton.setBounds(855, 78, 90, 31);
    levelSlider.setBounds(84, 145, 828, 22);
    for (int i = 0; i < 7; ++i) tabs[static_cast<std::size_t>(i)].setBounds(420 + i * 78, 17, 76, 30);
    if (standaloneKeyboard) standaloneKeyboard->setBounds(24, 631, 952, 55);

    for (int i = 0; i < 8; ++i)
    {
        if (page == 0) mainKnobs[static_cast<std::size_t>(i)].setBounds(42 + i * 113, 217, 108, 113);
    }
    if (page == 1)
    {
        mainKnobs[0].setBounds(636, 414, 135, 123); mainKnobs[1].setBounds(796, 414, 135, 123);
        waveform.setBounds(433, 266, 152, 28);
    }
    int shown = 0;
    for (int layer = 0; layer < vanta::oscillatorCount; ++layer)
    {
        const auto i = static_cast<std::size_t>(layer);
        if (oscillatorTabs[i].isVisible()) oscillatorTabs[i].setBounds(42 + shown++ * 62, 220, 58, 27);
        for (int k = 0; k < 4; ++k) oscillatorKnobs[i][static_cast<std::size_t>(k)].setBounds(55 + k * 130, 428, 120, 120);
        const int x = 38 + layer * 115;
        mixerEnabled[i].setBounds(x + 20, 260, 66, 26);
        mixerLevel[i].setBounds(x + 33, 297, 40, 160);
        mixerPan[i].setBounds(x + 12, 510, 82, 22);
    }
    addOscillator.setBounds(565, 220, 112, 27); removeOscillator.setBounds(684, 220, 84, 27);
    for (int i = 0; i < 9; ++i)
        effectKnobs[static_cast<std::size_t>(i)].setBounds(52 + (i / 3) * 306 + (i % 3) * 92, 434, 88, 110);
    for (int i = 0; i < 4; ++i) modulationKnobs[static_cast<std::size_t>(i)].setBounds(55 + i * 130, 422, 120, 122);
    modulationKnobs[4].setBounds(636, 422, 135, 122); modulationKnobs[5].setBounds(796, 422, 135, 122);
    for (int i = 0; i < 4; ++i) folders[static_cast<std::size_t>(i)].setBounds(44, 266 + i * 44, 175, 36);
    presetSearch.setBounds(264, 220, 680, 30);
    int row = 0;
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
        if (presetButtons[static_cast<std::size_t>(i)].isVisible())
            presetButtons[static_cast<std::size_t>(i)].setBounds(264, 266 + row++ * 45, 680, 38);
}
