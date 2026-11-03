#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
// Red-and-black instrument shell based on the approved HTML study. The graphs
// illustrate settings; they are not live audio or spectrum measurements.
const juce::Colour header(0xffa92227), surface(0xff170d0e);
const juce::Colour surfaceDark(0xff0b0809), border(0xff48201f), textPrimary(0xfffce9df);
const juce::Colour textSecondary(0xffdfb7aa), textMuted(0xff987973);
const juce::Colour accent(0xffff5845), mint(0xff82e1cf);
constexpr int editorWidth = vanta::editorWidth, vstHeight = vanta::vstEditorHeight, standaloneHeight = vanta::standaloneEditorHeight;

struct EffectDefinition
{
    const char* name;
    const char* id;
    const char* description;
    vanta::Parameter enabled;
    std::array<vanta::Parameter, 4> parameters;
    std::array<const char*, 4> labels;
    int count;
};
// Selection order follows the supplied rack reference; processing order is
// fixed and shown separately so existing patches retain their original sound.
constexpr std::array<EffectDefinition, 9> effects {{
    { "CHORUS", "chorus", "STEREO MODULATED VOICES / SOFT WIDTH", vanta::chorusEnabled,
      { vanta::chorusRate, vanta::chorusDepth, vanta::chorusMix, vanta::level }, { "RATE", "DEPTH", "MIX", "" }, 3 },
    { "COMPRESSOR", "compressor", "THREE-BAND UP / DOWN DYNAMICS / HIGH BAND +3 dB", vanta::ottEnabled,
      { vanta::ottDepth, vanta::ottMix, vanta::level, vanta::level }, { "DEPTH", "MIX", "", "" }, 2 },
    { "DELAY", "delay", "STEREO ECHO / FREE TIME", vanta::delayEnabled,
      { vanta::delayTime, vanta::delayFeedback, vanta::delayMix, vanta::level }, { "TIME", "FEEDBACK", "MIX", "" }, 3 },
    { "DISTORTION", "distortion", "SATURATION / LOW-PASS TONE", vanta::distortionEnabled,
      { vanta::distortionDrive, vanta::distortionTone, vanta::distortionMix, vanta::level }, { "DRIVE", "TONE", "MIX", "" }, 3 },
    { "EQUALIZER", "equalizer", "THREE-BAND TONE / 250 Hz + 2.5 kHz CROSSOVERS", vanta::eqEnabled,
      { vanta::eqLow, vanta::eqMid, vanta::eqHigh, vanta::eqMix }, { "LOW", "MID", "HIGH", "MIX" }, 4 },
    { "FILTER", "filter", "POST-EFFECT LOW-PASS / RESONANCE", vanta::fxFilterEnabled,
      { vanta::fxFilterCutoff, vanta::fxFilterResonance, vanta::fxFilterMix, vanta::level }, { "CUTOFF", "RESO", "MIX", "" }, 3 },
    { "FLANGER", "flanger", "SHORT MODULATED DELAY / FEEDBACK", vanta::flangerEnabled,
      { vanta::flangerRate, vanta::flangerDepth, vanta::flangerFeedback, vanta::flangerMix }, { "RATE", "DEPTH", "FEEDBACK", "MIX" }, 4 },
    { "PHASER", "phaser", "FOUR-STAGE PHASE SWEEP / FEEDBACK", vanta::phaserEnabled,
      { vanta::phaserRate, vanta::phaserDepth, vanta::phaserFeedback, vanta::phaserMix }, { "RATE", "DEPTH", "FEEDBACK", "MIX" }, 4 },
    { "REVERB", "reverb", "STEREO SPACE / SIZE + DECAY", vanta::reverbEnabled,
      { vanta::reverbSize, vanta::reverbDecay, vanta::reverbMix, vanta::level }, { "SIZE", "DECAY", "MIX", "" }, 3 }
}};

void text(juce::Graphics& g, const juce::String& value, int x, int y, int w, int h,
          float size, juce::Colour colour, int style = juce::Font::plain,
          juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, style));
    g.drawFittedText(value, x, y, w, h, justification, 1);
}
void mono(juce::Graphics& g, const juce::String& value, int x, int y, int w, int h,
          float size, juce::Colour colour, juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour(colour);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain)));
    g.drawFittedText(value, x, y, w, h, justification, 1);
}
void panel(juce::Graphics& g, float x, float y, float w, float h)
{
    const auto area = juce::Rectangle<float>(x, y, w, h);
    g.setColour(surface); g.fillRect(area);
    g.setColour(border); g.drawRect(area.reduced(0.5f), 1.0f);
}
void screen(juce::Graphics& g, float x, float y, float w, float h)
{
    const auto area = juce::Rectangle<float>(x, y, w, h);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff120e0e), x, y,
                                          juce::Colour(0xff080707), x + w, y + h, false));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(juce::Colour(0xffb64135)); g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.0f);
    g.setColour(juce::Colour(0x22ffae9a)); g.drawRoundedRectangle(area.reduced(3.0f), 2.0f, 0.5f);
}
void separator(juce::Graphics& g, float x1, float x2, float y)
{
    g.setColour(border.withAlpha(0.7f));
    g.drawLine(x1, y, x2, y, 1.0f);
}
void graphBackground(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(surfaceDark); g.fillRect(area);
    g.setColour(border.withAlpha(0.80f)); g.drawRect(area.reduced(0.5f), 1.0f);
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
    const float total = p[vanta::ampDelay] + p[vanta::attack] + p[vanta::ampHold]
                      + p[vanta::decay] + 0.5f + p[vanta::release];
    const float x0 = r.getX();
    const float xDelay = x0 + r.getWidth() * p[vanta::ampDelay] / total;
    const float xAttack = xDelay + r.getWidth() * p[vanta::attack] / total;
    const float xHold = xAttack + r.getWidth() * p[vanta::ampHold] / total;
    const float xDecay = xHold + r.getWidth() * p[vanta::decay] / total;
    const float xSustain = xDecay + r.getWidth() * 0.5f / total;
    const float low = r.getBottom(), high = r.getY();
    const float sustainY = low - r.getHeight() * p[vanta::sustain];
    juce::Path line; line.startNewSubPath(x0, low); line.lineTo(xDelay, low);
    line.lineTo(xAttack, high); line.lineTo(xHold, high); line.lineTo(xDecay, sustainY);
    line.lineTo(xSustain, sustainY); line.lineTo(r.getRight(), low);
    auto fill = line; fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(accent.withAlpha(0.22f), r.getX(), r.getY(), accent.withAlpha(0.015f), r.getX(), r.getBottom(), false));
    g.fillPath(fill);
    g.setColour(accent); g.strokePath(line, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto marker = [&](float x, float y)
    {
        g.setColour(surfaceDark); g.fillEllipse(x - 3.5f, y - 3.5f, 7.0f, 7.0f);
        g.setColour(accent); g.drawEllipse(x - 3.5f, y - 3.5f, 7.0f, 7.0f, 1.5f);
    };
    if (p[vanta::ampDelay] > 0.0f) marker(xDelay, low);
    marker(xAttack, high);
    if (p[vanta::ampHold] > 0.0f) marker(xHold, high);
    marker(xDecay, sustainY); marker(xSustain, sustainY);
}
void drawLfo(juce::Graphics& g, juce::Rectangle<float> area, const vanta::ParameterValues& p)
{
    graphBackground(g, area);
    auto r = area.reduced(12.0f, 15.0f);
    juce::Path line;
    // Draw a faint full-depth guide like the approved preview, plus the actual
    // parameter curve. Neither is a live oscilloscope.
    const auto cycles = juce::jmap(p[vanta::vibratoRate], 0.1f, 12.0f, 1.0f, 5.0f);
    const auto depth = p[vanta::vibrato];
    juce::Path guide;
    for (int i = 0; i <= 180; ++i)
    {
        const float t = static_cast<float>(i) / 180.0f;
        const float x = r.getX() + t * r.getWidth();
        const float fullY = r.getCentreY() - std::sin(t * cycles * juce::MathConstants<float>::twoPi) * r.getHeight() * 0.43f;
        const float y = r.getCentreY() + (fullY - r.getCentreY()) * depth;
        if (i == 0) { line.startNewSubPath(x, y); guide.startNewSubPath(x, fullY); }
        else { line.lineTo(x, y); guide.lineTo(x, fullY); }
    }
    juce::Path dashed;
    const float lengths[] = { 3.0f, 5.0f };
    juce::PathStrokeType(0.8f).createDashedStroke(dashed, guide, lengths, 2);
    g.setColour(mint.withAlpha(0.28f)); g.fillPath(dashed);
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
    const auto r = area.reduced(8.0f, 8.0f);
    juce::Path line;
    if (effect == 0)
    {
        // A bounded, illustrative decaying reverb response, not a measurement.
        for (int i = 0; i <= 160; ++i)
        {
            const float t = static_cast<float>(i) / 160.0f;
            const float decay = 2.0f + 3.0f * (1.0f - p[vanta::reverbDecay]);
            const float sample = std::abs(std::sin(t * 9.0f * juce::MathConstants<float>::pi)) * std::exp(-decay * t);
            const float x = r.getX() + t * r.getWidth();
            const float y = r.getBottom() - sample * r.getHeight() * 0.82f;
            if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
        }
        g.setColour(accent); g.strokePath(line, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (effect == 1)
    {
        const float baseline = r.getBottom();
        g.setColour(accent.withAlpha(0.65f)); g.drawLine(r.getX(), baseline, r.getRight(), baseline, 1.0f);
        for (int i = 0; i < 6; ++i)
        {
            const float x = r.getX() + static_cast<float>(i) * (r.getWidth() / 6.0f);
            const float h = r.getHeight() * 0.8f * std::pow(0.25f + 0.7f * p[vanta::delayFeedback], static_cast<float>(i));
            g.setColour(accent.withAlpha(0.95f - static_cast<float>(i) * 0.10f));
            g.drawLine(x, baseline, x, baseline - h, 1.8f);
        }
    }
    else
    {
        const float drive = p[vanta::distortionDrive], norm = std::tanh(drive);
        for (int i = 0; i <= 100; ++i)
        {
            const float t = static_cast<float>(i) / 100.0f;
            const float y = std::tanh((t * 2.0f - 1.0f) * drive) / norm;
            if (i == 0) line.startNewSubPath(r.getX(), r.getCentreY() - y * r.getHeight() * 0.5f);
            else line.lineTo(r.getX() + t * r.getWidth(), r.getCentreY() - y * r.getHeight() * 0.5f);
        }
        g.setColour(accent); g.strokePath(line, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}
void rackIllustration(juce::Graphics& g, int effect, juce::Rectangle<float> area, const vanta::ParameterValues& p)
{
    if (effect == 8 || effect == 2 || effect == 3)
    {
        effectIllustration(g, effect == 8 ? 0 : effect == 2 ? 1 : 2, area, p);
        return;
    }
    if (effect == 5)
    {
        auto filterValues = p;
        filterValues[vanta::cutoff] = p[vanta::fxFilterCutoff];
        filterValues[vanta::resonance] = p[vanta::fxFilterResonance];
        drawFilter(g, area, filterValues);
        return;
    }
    graphBackground(g, area);
    const auto r = area.reduced(10.0f, 10.0f);
    juce::Path curve;
    for (int i = 0; i <= 180; ++i)
    {
        const auto t = static_cast<float>(i) / 180.0f;
        float y = r.getCentreY();
        if (effect == 4)
        {
            const auto gain = t < 0.5f ? juce::jmap(t * 2.0f, p[vanta::eqLow], p[vanta::eqMid])
                                       : juce::jmap((t - 0.5f) * 2.0f, p[vanta::eqMid], p[vanta::eqHigh]);
            y -= gain / 12.0f * r.getHeight() * 0.42f;
        }
        else if (effect == 1)
        {
            const auto bend = p[vanta::ottDepth] * 0.32f;
            const auto shaped = t < 0.5f ? std::sqrt(t * 0.5f) : 1.0f - std::sqrt((1.0f - t) * 0.5f);
            y = r.getBottom() - juce::jmap(bend, t, shaped) * r.getHeight();
        }
        else
        {
            const auto depth = effect == 0 ? p[vanta::chorusDepth] : effect == 6 ? p[vanta::flangerDepth] : p[vanta::phaserDepth];
            const auto cycles = effect == 0 ? 2.0f : effect == 6 ? 5.0f : 3.0f;
            y -= std::sin(t * cycles * juce::MathConstants<float>::twoPi) * r.getHeight() * (0.08f + depth * 0.34f);
        }
        const auto x = r.getX() + t * r.getWidth();
        if (i == 0) curve.startNewSubPath(x, y); else curve.lineTo(x, y);
    }
    g.setColour(accent.withAlpha(0.16f)); g.strokePath(curve, juce::PathStrokeType(5.0f));
    g.setColour(accent); g.strokePath(curve, juce::PathStrokeType(1.8f));
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
    const float size = juce::jmax(10.0f, static_cast<float>(juce::jmin(width, height)) - 6.0f);
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x) + (static_cast<float>(width) - size) * 0.5f,
                                              static_cast<float>(y) + (static_cast<float>(height) - size) * 0.5f, size, size);
    const auto centre = bounds.getCentre();
    const float radius = size * 0.42f;
    juce::Path track; track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    g.setColour(juce::Colour(0xff632325));
    g.strokePath(track, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path progress; progress.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, start, start + position * (end - start), true);
    g.setColour(accent.withAlpha(0.20f));
    g.strokePath(progress, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accent);
    g.strokePath(progress, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto inner = bounds.reduced(size * 0.17f);
    g.setColour(juce::Colour(0xff250e0f)); g.fillEllipse(inner.expanded(3.0f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff332323), centre.x, inner.getY(),
                                          juce::Colour(0xff0a0808), centre.x, inner.getBottom(), false));
    g.fillEllipse(inner);
    g.setColour(juce::Colour(0xffbd3932)); g.drawEllipse(inner, 1.0f);
    const auto angle = start + position * (end - start);
    const float markerX = centre.x + std::sin(angle) * radius, markerY = centre.y - std::cos(angle) * radius;
    g.setColour(juce::Colour(0xffffd58e)); g.fillEllipse(markerX - 2.5f, markerY - 2.5f, 5.0f, 5.0f);
    const auto label = slider.getProperties().getWithDefault("faceLabel", {}).toString();
    text(g, label, static_cast<int>(inner.getX() + 2.0f), static_cast<int>(centre.y - 16.0f),
         static_cast<int>(inner.getWidth() - 4.0f), 14, juce::jlimit(7.5f, 9.5f, size * 0.105f), textSecondary, juce::Font::bold, juce::Justification::centred);
    mono(g, slider.getTextFromValue(slider.getValue()), static_cast<int>(inner.getX()), static_cast<int>(centre.y + 1.0f),
         static_cast<int>(inner.getWidth()), 16, juce::jlimit(9.0f, 12.0f, size * 0.13f), textPrimary, juce::Justification::centred);
}
void VantaLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                                 float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const float alpha = slider.isEnabled() ? 1.0f : 0.38f;
    if (style == juce::Slider::LinearVertical)
    {
        const auto centre = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const auto top = static_cast<float>(y) + 7.0f, bottom = static_cast<float>(y + height) - 7.0f;
        const auto pos = juce::jlimit(top, bottom, position);
        g.setColour(juce::Colour(0xff321213).withAlpha(alpha)); g.fillRect(centre - 2.0f, top, 4.0f, bottom - top);
        g.setColour(accent.withAlpha(alpha)); g.fillRect(centre - 2.0f, pos, 4.0f, bottom - pos);
        g.setColour(juce::Colour(0xffffdfa3).withAlpha(alpha)); g.fillRect(centre - 8.0f, pos - 4.0f, 16.0f, 8.0f);
        return;
    }
    const float mid = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float left = static_cast<float>(x) + 5.0f, right = static_cast<float>(x + width) - 5.0f;
    const float pos = juce::jlimit(left, right, position);
    g.setColour(juce::Colour(0xff250a0c).withAlpha(alpha)); g.fillRect(left, mid - 2.5f, right - left, 5.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffb05e).withAlpha(alpha), left, mid,
                                          accent.withAlpha(alpha), right, mid, false));
    g.fillRect(left, mid - 1.5f, juce::jmax(0.0f, pos - left), 3.0f);
    g.setColour(juce::Colour(0xffffdfa3).withAlpha(alpha)); g.fillRect(pos - 3.5f, mid - 6.0f, 7.0f, 12.0f);
    g.setColour(juce::Colour(0xff6c2a1e).withAlpha(alpha)); g.drawRect(pos - 3.5f, mid - 6.0f, 7.0f, 12.0f, 1.0f);
}
void VantaLook::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool hovered, bool down)
{
    const auto role = button.getProperties().getWithDefault("role", {}).toString();
    const auto area = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool selected = button.getToggleState();
    if (role == "logo")
    {
        if (hovered || button.hasKeyboardFocus(true)) { g.setColour(accent.withAlpha(0.7f)); g.drawRect(area, 1.0f); }
        return;
    }
    if (role == "preset" || role == "arrow" || role == "link")
    {
        if (hovered && button.isEnabled() && role != "link") { g.setColour(juce::Colour(0x443e1715)); g.fillRect(area); }
        return;
    }
    if (role == "fxpower")
    {
        const auto circle = area.reduced(6.0f, 8.0f);
        g.setColour(selected ? accent : juce::Colour(0xff493333)); g.fillEllipse(circle);
        g.setColour(selected ? juce::Colour(0xffffc99a) : juce::Colour(0xff80605b)); g.drawEllipse(circle, 1.0f);
        if (hovered) { g.setColour(textPrimary.withAlpha(0.5f)); g.drawEllipse(circle.expanded(2.0f), 1.0f); }
        return;
    }
    if (role == "tab" || role == "scale")
    {
        if (selected)
        {
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffcc80), area.getX(), area.getY(),
                                                  juce::Colour(0xfff0854e), area.getX(), area.getBottom(), false));
            g.fillRect(area);
            g.setColour(juce::Colour(0xffffd9a8)); g.drawRect(area, 1.0f);
        }
        else { g.setColour(hovered ? juce::Colour(0xff481a1b) : juce::Colour(0xff231112)); g.fillRect(area); }
        return;
    }
    auto fill = selected ? juce::Colour(0xff5f1d1c) : juce::Colour(0xff211112);
    if (role == "presetRow") fill = selected ? juce::Colour(0xff5f1d1c) : juce::Colour(0xff1a0d0e);
    if (role == "primary") fill = juce::Colour(0xff491a1c);
    if (hovered && button.isEnabled()) fill = fill.brighter(0.13f);
    if (down) fill = fill.brighter(0.10f);
    if (!button.isEnabled()) fill = fill.withMultipliedAlpha(0.45f);
    g.setColour(fill); g.fillRect(area);
    g.setColour(selected ? accent.withAlpha(0.75f) : juce::Colour(0xff71302c)); g.drawRect(area, 1.0f);
}
void VantaLook::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto role = button.getProperties().getWithDefault("role", {}).toString();
    const auto area = button.getLocalBounds();
    const bool enabled = button.isEnabled();
    if (role == "logo" || role == "fxpower") return;
    if (role == "fxrow")
    {
        text(g, button.getButtonText(), 9, 0, area.getWidth() - 45, area.getHeight(), 9.5f,
             button.getToggleState() ? textPrimary : textSecondary, juce::Font::bold);
        const float x = static_cast<float>(area.getWidth() - 30), y = static_cast<float>(area.getCentreY());
        const int index = static_cast<int>(button.getProperties().getWithDefault("fxIndex", 0));
        g.setColour(button.getToggleState() ? accent : textMuted);
        if (index == 0)
        {
            for (int n = 0; n < 3; ++n) g.drawEllipse(x + n * 4.0f, y - 6.0f, 12.0f, 12.0f, 1.2f);
        }
        else if (index == 2 || index == 6)
        {
            for (int n = 0; n < 5; ++n)
            {
                const float h = index == 2 ? 13.0f - n * 2.0f : 7.0f + (n % 2) * 5.0f;
                g.drawVerticalLine(static_cast<int>(x + n * 4.0f), y - h * 0.5f, y + h * 0.5f);
            }
        }
        else if (index == 8)
        {
            g.drawRect(x, y - 7.0f, 19.0f, 14.0f, 1.0f); g.drawLine(x, y - 7.0f, x + 19.0f, y + 7.0f, 1.0f);
            g.drawLine(x + 19.0f, y - 7.0f, x, y + 7.0f, 1.0f);
        }
        else
        {
            juce::Path glyph;
            for (int n = 0; n <= 24; ++n)
            {
                const float t = static_cast<float>(n) / 24.0f;
                const float z = index == 5 ? (t < 0.52f ? 0.0f : (t - 0.52f) * 15.0f)
                              : index == 1 ? (t < 0.5f ? (0.5f - t) * 8.0f : (t - 0.5f) * 8.0f)
                              : std::sin(t * juce::MathConstants<float>::twoPi * (index == 7 ? 2.0f : 1.0f)) * 5.0f;
                if (n == 0) glyph.startNewSubPath(x, y + z); else glyph.lineTo(x + t * 19.0f, y + z);
            }
            g.strokePath(glyph, juce::PathStrokeType(1.4f));
        }
        return;
    }
    if (role == "preset")
    {
        const auto category = button.getProperties().getWithDefault("category", "USER").toString();
        const int index = static_cast<int>(button.getProperties().getWithDefault("presetIndex", -1));
        text(g, "PRESET", 0, 10, 56, 15, 10.0f, accent, juce::Font::bold);
        text(g, index >= 0 ? "VANTA FACTORY   /   " + category : "USER / IMPORTED", 62, 10, area.getWidth() - 76, 15, 10.0f, textSecondary, juce::Font::bold);
        mono(g, index >= 0 ? juce::String(index + 1).paddedLeft('0', 3) + ":" : "---:", 0, 29, 86, 38, 31.0f, accent);
        text(g, button.getButtonText(), 88, 31, area.getWidth() - 100, 36, 25.0f, textPrimary, juce::Font::bold);
        mono(g, index >= 0 ? "FACTORY COLLECTION  /  CLICK TO BROWSE" : "USER PRESET  /  CLICK TO BROWSE", 0, 74, area.getWidth() - 5, 15, 9.0f, textMuted);
        return;
    }
    if (role == "arrow")
    {
        const auto cx = static_cast<float>(area.getCentreX()), cy = static_cast<float>(area.getCentreY());
        const bool up = button.getButtonText() == "^";
        juce::Path chevron; chevron.startNewSubPath(cx - 7.0f, cy + (up ? 3.0f : -3.0f));
        chevron.lineTo(cx, cy + (up ? -3.0f : 3.0f)); chevron.lineTo(cx + 7.0f, cy + (up ? 3.0f : -3.0f));
        g.setColour(accent); g.strokePath(chevron, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }
    if (role == "presetRow")
    {
        text(g, button.getButtonText(), 15, 0, area.getWidth() - 140, area.getHeight(), 11.0f, textPrimary, juce::Font::bold);
        mono(g, button.getProperties().getWithDefault("category", {}).toString(), area.getWidth() - 112, 0, 90, area.getHeight(), 9.0f,
             button.getToggleState() ? accent : textSecondary, juce::Justification::centredRight);
        return;
    }
    const bool tabLike = role == "tab" || role == "scale";
    const auto colour = tabLike && button.getToggleState() ? juce::Colour(0xff35100c)
                        : (tabLike ? textSecondary : role == "link" ? accent : textPrimary);
    text(g, button.getButtonText(), 4, 0, area.getWidth() - 8, area.getHeight(), role == "link" ? 9.0f : 10.0f,
         colour.withMultipliedAlpha(enabled ? 1.0f : 0.4f), juce::Font::bold, juce::Justification::centred);
}
void VantaLook::drawComboBox(juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox&)
{
    const auto area = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
    g.setColour(down ? juce::Colour(0xff5c251f) : juce::Colour(0xff2b1112)); g.fillRect(area);
    g.setColour(border); g.drawRect(area, 1.0f);
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
    slider.getProperties().set("faceLabel", labelText);
    label.setText({}, juce::dontSendNotification);
    label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, textSecondary);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(slider); addAndMakeVisible(label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, vanta::parameterSpecs[parameter].id, slider);
    // JUCE's attachment installs its own formatter, so set our units afterwards.
    slider.textFromValueFunction = [parameter](double value)
    {
        if (parameter == vanta::cutoff || parameter == vanta::fxFilterCutoff) return value >= 1000.0 ? juce::String(value / 1000.0, 1) + " kHz" : juce::String(value, 0) + " Hz";
        if (parameter == vanta::ampDelay || parameter == vanta::attack || parameter == vanta::ampHold
            || parameter == vanta::decay || parameter == vanta::release || parameter == vanta::delayTime)
            return value < 1.0 ? juce::String(value * 1000.0, 0) + " ms" : juce::String(value, 2) + " s";
        if (parameter == vanta::vibratoRate || parameter == vanta::chorusRate || parameter == vanta::flangerRate || parameter == vanta::phaserRate)
            return juce::String(value, 2) + " Hz";
        if (parameter == vanta::eqLow || parameter == vanta::eqMid || parameter == vanta::eqHigh)
            return juce::String(value, 1) + " dB";
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
void VantaKnob::setCaption(const juce::String& caption) { label.setText(caption, juce::dontSendNotification); }
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

// Keep the editor itself untransformed: JUCE uses its transform for host DPI.
// Scaling a single child keeps painting, mouse coordinates and controls aligned.
class VantaEditor::Canvas final : public juce::Component
{
public:
    explicit Canvas(VantaEditor& editor) : owner(editor) { setComponentID("vanta-canvas"); }
    void paint(juce::Graphics& g) override { owner.paintCanvas(g); }
private:
    VantaEditor& owner;
};

class VantaEditor::ResizeOverlay final : public juce::Component
{
public:
    explicit ResizeOverlay(VantaEditor& editor) : owner(editor)
    {
        setComponentID("resize-overlay"); setName("VANTA interface size");
        setWantsKeyboardFocus(true);
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        const int choices[] = { 60, 75, 90, 100, 110, 125, 150 };
        for (std::size_t i = 0; i < sizes.size(); ++i)
        {
            auto& button = sizes[i]; const int percent = choices[i];
            button.setButtonText(juce::String(percent) + "%");
            button.setComponentID("resize-" + juce::String(percent));
            button.setTitle("Set interface size to " + juce::String(percent) + " percent");
            button.getProperties().set("role", "scale");
            button.onClick = [this, percent] { owner.showResizeMenu(false); owner.setUiScale(percent, true); };
            addAndMakeVisible(button);
        }
        fit.setButtonText("FIT SCREEN"); fit.setComponentID("resize-fit");
        fit.setTitle("Fit the interface to the current screen"); fit.getProperties().set("role", "scale");
        fit.onClick = [this] { const int percent = owner.fitScalePercent(); owner.showResizeMenu(false); owner.setUiScale(percent, true); };
        addAndMakeVisible(fit);
        close.setButtonText("X"); close.setComponentID("resize-close"); close.setTitle("Close resize menu");
        close.getProperties().set("role", "link"); close.onClick = [this] { owner.showResizeMenu(false); };
        addAndMakeVisible(close);
    }
    void updateSelection(int percentage)
    {
        constexpr int choices[] = { 60, 75, 90, 100, 110, 125, 150 };
        for (std::size_t i = 0; i < sizes.size(); ++i)
            sizes[i].setToggleState(choices[i] == percentage, juce::dontSendNotification);
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::black.withAlpha(0.72f)); g.fillRect(getLocalBounds());
        const auto r = dialogBounds();
        screen(g, static_cast<float>(r.getX()), static_cast<float>(r.getY()), static_cast<float>(r.getWidth()), static_cast<float>(r.getHeight()));
        g.setColour(juce::Colour(0xffd04038)); g.fillRect(r.getX() + 1, r.getY() + 1, r.getWidth() - 2, 3);
        text(g, "VANTA / DISPLAY", r.getX() + 22, r.getY() + 20, 230, 17, 10.0f, accent, juce::Font::bold);
        text(g, "INTERFACE SIZE", r.getX() + 22, r.getY() + 41, 290, 26, 19.0f, textPrimary, juce::Font::bold);
        mono(g, "CURRENT " + juce::String(owner.currentScalePercent()) + "%", r.getX() + 282, r.getY() + 46, 105, 18, 9.0f, textSecondary, juce::Justification::centredRight);
        mono(g, owner.hostResizeRejected ? "Host did not accept that size. Try another." : "Choose a size or fit VANTA to your screen.",
             r.getX() + 22, r.getY() + 75, r.getWidth() - 44, 15, 9.0f, owner.hostResizeRejected ? accent : textMuted);
        separator(g, static_cast<float>(r.getX() + 22), static_cast<float>(r.getRight() - 22), static_cast<float>(r.getY() + 99));
        mono(g, "ESC OR CLICK OUTSIDE TO CLOSE", r.getX() + 22, r.getBottom() - 30, r.getWidth() - 44, 14, 9.0f, textMuted);
    }
    void resized() override
    {
        dialog = calculateDialogBounds();
        const auto r = dialogBounds();
        close.setBounds(r.getRight() - 48, r.getY() + 15, 30, 25);
        for (std::size_t i = 0; i < sizes.size(); ++i)
            sizes[i].setBounds(r.getX() + 24 + static_cast<int>(i % 4) * 100, r.getY() + 112 + static_cast<int>(i / 4) * 47, 92, 36);
        fit.setBounds(r.getX() + 324, r.getY() + 159, 92, 36);
    }
    void mouseDown(const juce::MouseEvent& event) override
    {
        if (!dialogBounds().contains(event.getPosition())) owner.showResizeMenu(false);
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey) { owner.showResizeMenu(false); return true; }
        return false;
    }
    void updatePlacement()
    {
        if (calculateDialogBounds() != dialog) { resized(); repaint(); }
    }
private:
    juce::Rectangle<int> dialogBounds() const { return dialog; }
    juce::Rectangle<int> calculateDialogBounds() const
    {
        auto visible = getLocalBounds();
        if (getPeer() != nullptr)
        {
            const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds());
            if (display != nullptr) visible = visible.getIntersection(getLocalArea(nullptr, display->userArea));
        }
        const auto position = vanta::resizePopupPosition(getWidth(), getHeight(), visible.getX(), visible.getY(),
                                                         visible.getWidth(), visible.getHeight());
        return { position.x, position.y, vanta::resizePopupWidth, vanta::resizePopupHeight };
    }
    VantaEditor& owner;
    juce::Rectangle<int> dialog;
    std::array<juce::TextButton, 7> sizes;
    juce::TextButton fit, close;
};

VantaEditor::VantaEditor(VantaProcessor& owner) : AudioProcessorEditor(owner), processor(owner)
{
    setLookAndFeel(&look);
    canvas = std::make_unique<Canvas>(*this); canvas->setSize(editorWidth, baseHeight()); addAndMakeVisible(*canvas);
    lastObservedScale = processor.editorScalePercent();
    setSize(juce::roundToInt(editorWidth * lastObservedScale / 100.0), juce::roundToInt(baseHeight() * lastObservedScale / 100.0));
    setResizeLimits(editorWidth * vanta::minEditorScale / 100, juce::roundToInt(baseHeight() * vanta::minEditorScale / 100.0),
                    editorWidth * vanta::maxEditorScale / 100, juce::roundToInt(baseHeight() * vanta::maxEditorScale / 100.0));
    getConstrainer()->setFixedAspectRatio(static_cast<double>(editorWidth) / static_cast<double>(baseHeight()));
    setResizable(true, false);
    configureButton(logoButton, {}, "logo"); logoButton.setComponentID("resize-logo");
    logoButton.setTitle("Resize VANTA interface"); logoButton.setTooltip("Open interface size options");
    logoButton.onClick = [this] { showResizeMenu(true); };
    configureButton(previous, "^", "arrow"); previous.setComponentID("preset-previous"); previous.setTooltip("Previous factory preset"); previous.onClick = [this] { changePreset(-1); };
    configureButton(next, "v", "arrow"); next.setComponentID("preset-next"); next.setTooltip("Next factory preset"); next.onClick = [this] { changePreset(1); };
    configureButton(browserButton, "VANTA INIT", "preset"); browserButton.setComponentID("preset-browser"); browserButton.setTooltip("Browse and search factory sounds");
    browserButton.onClick = [this] { browserOpen = !browserOpen; refresh(); };
    configureButton(writeButton, "WRITE"); writeButton.setComponentID("preset-save"); writeButton.setTooltip("Save a VANTA preset"); writeButton.onClick = [this] { savePreset(); };
    configureButton(importButton, "IMPORT"); importButton.setComponentID("preset-import"); importButton.setTooltip("Open a VANTA, Vital, Serum or Serum 2 preset"); importButton.onClick = [this] { openPreset(); };

    const char* tabNames[] = { "MAIN", "OSC", "EFFECTS", "MOD", "MIXER", "SEQ", "SYSTEM" };
    const char* tabIds[] = { "tab-overview", "tab-oscillator", "tab-effects", "tab-modulation", "tab-mixer", "tab-sequencer", "tab-settings" };
    for (int i = 0; i < static_cast<int>(tabs.size()); ++i)
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
    presetSearch.onTextChange = [this] { refresh(); }; canvas->addAndMakeVisible(presetSearch);

    levelSlider.setSliderStyle(juce::Slider::LinearHorizontal); levelSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    levelSlider.setTitle("Master level"); levelSlider.setComponentID("parameter-level"); levelSlider.setTooltip("Master output level"); canvas->addAndMakeVisible(levelSlider);
    levelSlider.onValueChange = [this] { repaint(); };
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, "level", levelSlider);
    const vanta::Parameter miniIds[] = { vanta::decay, vanta::sustain, vanta::vibratoRate };
    for (int i = 0; i < 3; ++i)
    {
        auto& slider = mainMiniSliders[static_cast<std::size_t>(i)];
        const auto parameter = miniIds[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setTitle(vanta::parameterSpecs[parameter].name);
        slider.setComponentID("parameter-" + juce::String(vanta::parameterSpecs[parameter].id));
        slider.setTooltip(juce::String(vanta::parameterSpecs[parameter].name));
        slider.onValueChange = [this] { repaint(); }; canvas->addAndMakeVisible(slider);
        mainMiniAttachments[static_cast<std::size_t>(i)] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.state, vanta::parameterSpecs[parameter].id, slider);
    }
    configureButton(editEnvelope, "EDIT >", "link"); editEnvelope.setComponentID("edit-envelope");
    editEnvelope.setTooltip("Open amplitude envelope controls"); editEnvelope.onClick = [this] { selectPage(3); };
    configureButton(editVibrato, "EDIT >", "link"); editVibrato.setComponentID("edit-vibrato");
    editVibrato.setTooltip("Open vibrato controls"); editVibrato.onClick = [this] { selectPage(3); };
    waveform.addItem("SINE", 1); waveform.addItem("SAW", 2); waveform.addItem("SQUARE", 3); waveform.addItem("TRIANGLE", 4); waveform.addItem("WAVETABLE", 5); waveform.addItem("SAMPLE", 6);
    waveform.setTitle("Selected oscillator waveform"); waveform.setComponentID("parameter-wave");
    waveform.setTooltip("Choose the selected oscillator's waveform"); waveform.onChange = [this] { setWaveform(waveform.getSelectedId() - 1); }; canvas->addAndMakeVisible(waveform);
    const char* waveNames[] = { "SINE", "SAW", "SQUARE", "TRIANGLE" };
    const char* waveIds[] = { "wave-sine", "wave-saw", "wave-square", "wave-triangle" };
    for (int i = 0; i < 4; ++i)
    {
        auto& waveButton = waveButtons[static_cast<std::size_t>(i)];
        configureButton(waveButton, waveNames[i], "filter"); waveButton.setComponentID(waveIds[i]);
        waveButton.setTooltip("Select " + juce::String(waveNames[i]) + " for this oscillator");
        waveButton.onClick = [this, i] { setWaveform(i); };
    }

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
            knob.onValueChanged = [this] { repaint(); }; canvas->addAndMakeVisible(knob);
        }
        auto& enabled = mixerEnabled[i]; configureButton(enabled, "ON", "filter");
        enabled.setComponentID("mixer-osc-" + letter + "-enabled"); enabled.setTooltip("Enable oscillator " + letter);
        enabled.setClickingTogglesState(true);
        mixerEnabledAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,
                vanta::parameterSpecs[vanta::oscillatorEnabled(layer)].id, enabled);
        enabled.onClick = [this] { refresh(); };
        auto& position = tablePositions[i];
        position.setSliderStyle(juce::Slider::LinearHorizontal); position.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        position.setTitle("Oscillator " + letter + " wavetable position");
        position.setComponentID("parameter-" + juce::String(vanta::parameterSpecs[vanta::oscillatorTablePosition(layer)].id));
        position.setTooltip("Blend between imported wavetable frames"); canvas->addAndMakeVisible(position);
        tablePositionAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,
                vanta::parameterSpecs[vanta::oscillatorTablePosition(layer)].id, position);
        auto setupMixerSlider = [this, i, &letter](juce::Slider& slider, vanta::Parameter parameter, bool vertical)
        {
            slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.setTitle(vanta::parameterSpecs[parameter].name);
            slider.setComponentID("mixer-" + juce::String(vanta::parameterSpecs[parameter].id));
            slider.setTooltip(juce::String(vanta::parameterSpecs[parameter].name));
            slider.onValueChange = [this] { repaint(); }; canvas->addAndMakeVisible(slider);
            auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, vanta::parameterSpecs[parameter].id, slider);
            if (vertical) mixerLevelAttachments[i] = std::move(attachment); else mixerPanAttachments[i] = std::move(attachment);
        };
        setupMixerSlider(mixerLevel[i], ids[0], true); setupMixerSlider(mixerPan[i], ids[3], false);
    }
    configureButton(addOscillator, "+ NEW TAB", "primary"); addOscillator.setComponentID("add-oscillator");
    addOscillator.setTooltip("Enable the next oscillator"); addOscillator.onClick = [this]
    {
        const auto values = processor.currentValues();
        for (int layer = 0; layer < vanta::oscillatorCount; ++layer)
            if (!vanta::oscillatorIsEnabled(values, layer)) { setParameter(vanta::oscillatorEnabled(layer), 1.0f); selectedOsc = layer; break; }
        refresh();
    };
    configureButton(removeOscillator, "REMOVE", "filter"); removeOscillator.setComponentID("remove-oscillator");
    removeOscillator.setTooltip("Disable the selected oscillator"); removeOscillator.onClick = [this]
    {
        if (selectedOsc > 0) { setParameter(vanta::oscillatorEnabled(selectedOsc), 0.0f); selectedOsc = 0; }
        refresh();
    };

    for (std::size_t i = 0; i < effects.size(); ++i)
    {
        const auto& fx = effects[i];
        auto& selector = effectSelectors[i];
        configureButton(selector, fx.name, "fxrow");
        selector.setComponentID("fx-select-" + juce::String(fx.id));
        selector.getProperties().set("fxIndex", static_cast<int>(i));
        selector.setTooltip("Edit " + juce::String(fx.name));
        selector.onClick = [this, i] { selectedFx = static_cast<int>(i); refresh(); };
        auto& enabled = effectEnabled[i];
        configureButton(enabled, {}, "fxpower");
        enabled.setComponentID("parameter-" + juce::String(vanta::parameterSpecs[fx.enabled].id));
        enabled.setTitle("Enable " + juce::String(fx.name));
        enabled.setTooltip("Turn " + juce::String(fx.name) + " on or off");
        enabled.setClickingTogglesState(true);
        enabled.onClick = [this] { repaint(); };
        effectEnabledAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.state, vanta::parameterSpecs[fx.enabled].id, enabled);
        for (int k = 0; k < fx.count; ++k)
        {
            auto& knob = effectKnobs[i][static_cast<std::size_t>(k)];
            knob.configure(processor.state, fx.parameters[static_cast<std::size_t>(k)], fx.labels[static_cast<std::size_t>(k)]);
            knob.onValueChanged = [this] { repaint(); }; canvas->addAndMakeVisible(knob);
        }
    }
    const vanta::Parameter mainIds[] = { vanta::cutoff, vanta::resonance, vanta::attack, vanta::decay, vanta::sustain, vanta::release, vanta::vibrato, vanta::vibratoRate };
    const char* mainNames[] = { "CUTOFF", "RESO", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "VIBRATO", "RATE" };
    for (int i = 0; i < 8; ++i)
    {
        auto& knob = mainKnobs[static_cast<std::size_t>(i)]; knob.configure(processor.state, mainIds[i], mainNames[i]);
        knob.onValueChanged = [this] { repaint(); }; canvas->addAndMakeVisible(knob);
    }
    mainKnobs[0].setCaption("FILTER"); mainKnobs[1].setCaption("FILTER");
    mainKnobs[2].setCaption("ENVELOPE"); mainKnobs[5].setCaption("ENVELOPE"); mainKnobs[6].setCaption("PITCH");
    const vanta::Parameter modIds[] = { vanta::ampDelay, vanta::attack, vanta::ampHold, vanta::decay,
                                         vanta::sustain, vanta::release, vanta::vibrato, vanta::vibratoRate };
    const char* modNames[] = { "DELAY", "ATTACK", "HOLD", "DECAY", "SUSTAIN", "RELEASE", "VIBRATO", "RATE" };
    for (int i = 0; i < 8; ++i)
    {
        auto& knob = modulationKnobs[static_cast<std::size_t>(i)]; knob.configure(processor.state, modIds[i], modNames[i]);
        knob.onValueChanged = [this] { repaint(); }; canvas->addAndMakeVisible(knob);
    }

    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        standaloneKeyboard = std::make_unique<VantaKeyboard>(processor.keyboardState);
        canvas->addAndMakeVisible(*standaloneKeyboard);
    }
    resizeOverlay = std::make_unique<ResizeOverlay>(*this);
    canvas->addChildComponent(*resizeOverlay); resizeOverlay->setBounds(canvas->getLocalBounds());
    lastPresetName = processor.presetName();
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
        if (lastPresetName == VantaProcessor::factoryPresetName(i)) currentFactory = i;
    startTimerHz(10);
    refresh();
}
VantaEditor::~VantaEditor()
{
    // Don't allow a background parser to outlive the plugin/editor DLL.
    importWorkers.removeAllJobs(false, -1);
    stopTimer(); setLookAndFeel(nullptr);
}
void VantaEditor::configureButton(juce::TextButton& button, const juce::String& label, const juce::String& role)
{
    button.setButtonText(label); button.getProperties().set("role", role); canvas->addAndMakeVisible(button);
}
int VantaEditor::baseHeight() const noexcept
{
    return processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone ? standaloneHeight : vstHeight;
}
int VantaEditor::currentScalePercent() const noexcept
{
    const auto scale = juce::jmin(static_cast<double>(getWidth()) / editorWidth,
                                  static_cast<double>(getHeight()) / baseHeight());
    return juce::roundToInt(scale * 100.0);
}
int VantaEditor::fitScalePercent() const
{
    // Display work areas are in logical coordinates. The root editor may have
    // an additional host DPI transform; estimate that from its screen bounds.
    if (getPeer() == nullptr || getWidth() <= 0) return vanta::defaultEditorScale;
    const auto screenBounds = getScreenBounds();
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = displays.getDisplayForRect(screenBounds);
    if (display == nullptr || screenBounds.getWidth() <= 0) return vanta::defaultEditorScale;
    const auto hostScale = static_cast<double>(screenBounds.getWidth()) / getWidth();
    return vanta::fitEditorScaleToWorkArea(baseHeight(), display->userArea.getWidth(), display->userArea.getHeight(), hostScale);
}
void VantaEditor::setUiScale(int percent, bool notifyHost)
{
    percent = juce::jlimit(vanta::minEditorScale, vanta::maxEditorScale, percent);
    const auto oldScale = processor.editorScalePercent();
    const int width = juce::roundToInt(editorWidth * percent / 100.0);
    const int height = juce::roundToInt(baseHeight() * percent / 100.0);
    hostResizeRejected = false;
    // A VST3 host owns the surrounding view and may refuse the request. Wait a
    // little for onSize before checking the wrapper's actual bounds.
    awaitingHostResize = (width != getWidth() || height != getHeight())
                         && processor.wrapperType == juce::AudioProcessor::wrapperType_VST3
                         && getParentComponent() != nullptr && (notifyHost || getPeer() != nullptr);
    resizeRequestFromUser = notifyHost;
    resizeRequestTime = juce::Time::getMillisecondCounter();
    lastObservedScale = percent;
    processor.setEditorScalePercent(percent);
    // JUCE's VST3 wrapper forwards this size request to the host's plug frame;
    // we never modify the root transform reserved for the host's DPI scale.
    setSize(width, height);
    if (notifyHost && oldScale != processor.editorScalePercent())
        processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged(true));
}
void VantaEditor::reconcileHostResize()
{
    if (!awaitingHostResize || juce::Time::getMillisecondCounter() - resizeRequestTime < 1000) return;
    awaitingHostResize = false;
    auto* parent = getParentComponent();
    if (parent == nullptr) return;
    const auto actual = parent->getLocalBounds();
    const auto requested = parent->getLocalArea(this, getLocalBounds());
    if (std::abs(requested.getWidth() - actual.getWidth()) <= 2
        && std::abs(requested.getHeight() - actual.getHeight()) <= 2) return;
    // JUCE's VST3 content wrapper is the editor's immediate parent. Convert its
    // bounds back through the host DPI transform and keep all controls visible.
    const auto accepted = getLocalArea(parent, actual);
    if (accepted.getWidth() <= 0 || accepted.getHeight() <= 0) return;
    setSize(accepted.getWidth(), accepted.getHeight());
    if (resizeRequestFromUser)
    {
        hostResizeRejected = true;
        showResizeMenu(true);
    }
}
void VantaEditor::positionStandaloneWindowOnce()
{
    if (!needsStandalonePosition || processor.wrapperType != juce::AudioProcessor::wrapperType_Standalone || getPeer() == nullptr) return;
    auto* window = getTopLevelComponent();
    if (window == nullptr || !window->isOnDesktop()) return;
    const auto bounds = window->getScreenBounds();
    const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(bounds);
    if (display == nullptr || bounds.isEmpty()) return;
    const auto area = display->userArea;
    const auto position = vanta::standaloneWindowPosition(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                                                           area.getX(), area.getY(), area.getWidth(), area.getHeight());
    needsStandalonePosition = false;
    if (position.x != bounds.getX() || position.y != bounds.getY())
        window->setTopLeftPosition(window->getX() + position.x - bounds.getX(), window->getY() + position.y - bounds.getY());
}
void VantaEditor::showResizeMenu(bool show)
{
    if (!resizeOverlay) return;
    if (show)
    {
        resizeOverlay->setBounds(canvas->getLocalBounds());
        resizeOverlay->updateSelection(currentScalePercent());
        resizeOverlay->setVisible(true); resizeOverlay->toFront(true);
        resizeOverlay->updatePlacement(); resizeOverlay->grabKeyboardFocus();
    }
    else
    {
        resizeOverlay->setVisible(false);
        if (logoButton.isShowing()) logoButton.grabKeyboardFocus();
    }
}
void VantaEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff11090a)); }
void VantaEditor::mouseDown(const juce::MouseEvent&)
{
    // A host that ignores the aspect ratio may leave letterbox space outside
    // the canvas; treat clicks there like clicks outside the popup too.
    if (resizeOverlay && resizeOverlay->isVisible()) showResizeMenu(false);
}
void VantaEditor::resized()
{
    if (!canvas) return;
    const auto scale = juce::jmax(0.001f, juce::jmin(static_cast<float>(getWidth()) / editorWidth,
                                                    static_cast<float>(getHeight()) / baseHeight()));
    const auto x = (static_cast<float>(getWidth()) - editorWidth * scale) * 0.5f;
    const auto y = (static_cast<float>(getHeight()) - baseHeight() * scale) * 0.5f;
    canvas->setTransform(juce::AffineTransform::scale(scale).translated(x, y));
    layoutCanvas();
    // A host may resize the editor directly. Remember its actual size without
    // adding an automatable parameter or writing anything from processBlock.
    if (getWidth() != lastObservedWidth || getHeight() != lastObservedHeight)
    {
        lastObservedWidth = getWidth(); lastObservedHeight = getHeight();
        processor.setEditorScalePercent(currentScalePercent());
        lastObservedScale = processor.editorScalePercent();
        if (resizeOverlay && resizeOverlay->isVisible()) resizeOverlay->updateSelection(currentScalePercent());
    }
}
void VantaEditor::timerCallback()
{
    // A host can restore state while the editor is open. Apply on the message thread.
    if (const int restoredScale = processor.editorScalePercent(); restoredScale != lastObservedScale)
        setUiScale(restoredScale, false);
    reconcileHostResize();
    positionStandaloneWindowOnce();
    if (resizeOverlay && resizeOverlay->isVisible()) resizeOverlay->updatePlacement();
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
    if (mask != lastOscMask || waveform.getSelectedId() != (values[vanta::oscillatorSample(selectedOsc)] >= 0.5f ? 6 : values[vanta::oscillatorTable(selectedOsc)] >= 0.5f ? 5 : vanta::oscillatorShape(values, selectedOsc) + 1)) needsRefresh = true;
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
    if (shape < 0 || shape > 5) return;
    if (shape == 5)
    {
        if (!processor.hasSample(selectedOsc))
        {
            showMessage("No sample", "Import a preset with an embedded sample for this oscillator first.", true);
            refresh(); return;
        }
        setParameter(vanta::oscillatorTable(selectedOsc), 0.0f);
        setParameter(vanta::oscillatorSample(selectedOsc), 1.0f); refresh(); return;
    }
    if (shape == 4)
    {
        if (!processor.hasWaveTable(selectedOsc))
        {
            showMessage("No wavetable", "Import a preset with an embedded wavetable for this oscillator first.", true);
            refresh(); return;
        }
        setParameter(vanta::oscillatorSample(selectedOsc), 0.0f);
        setParameter(vanta::oscillatorTable(selectedOsc), 1.0f); refresh(); return;
    }
    setParameter(vanta::oscillatorTable(selectedOsc), 0.0f);
    setParameter(vanta::oscillatorSample(selectedOsc), 0.0f);
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
    if (!vanta::oscillatorIsEnabled(values, selectedOsc))
        for (int i = 0; i < vanta::oscillatorCount; ++i)
            if (vanta::oscillatorIsEnabled(values, i)) { selectedOsc = i; break; }
    int mask = 0, activeCount = 0;
    for (int i = 0; i < vanta::oscillatorCount; ++i)
        if (vanta::oscillatorIsEnabled(values, i)) { mask |= 1 << i; ++activeCount; }
    lastOscMask = mask;
    browserButton.setButtonText(processor.presetName());
    browserButton.getProperties().set("presetIndex", currentFactory);
    browserButton.getProperties().set("category", currentFactory >= 0 ? juce::String(VantaProcessor::factoryPresetCategory(currentFactory)) : juce::String("USER"));
    browserButton.setToggleState(browserOpen, juce::dontSendNotification);
    for (int i = 0; i < static_cast<int>(tabs.size()); ++i) tabs[static_cast<std::size_t>(i)].setToggleState(!browserOpen && i == page, juce::dontSendNotification);
    waveform.setVisible(!browserOpen && page == 1);
    waveform.setComponentID(selectedOsc == 0 ? "parameter-wave" : "parameter-" + juce::String(vanta::parameterSpecs[vanta::oscillatorWave(selectedOsc)].id));
    waveform.setSelectedId(values[vanta::oscillatorSample(selectedOsc)] >= 0.5f ? 6 : values[vanta::oscillatorTable(selectedOsc)] >= 0.5f ? 5 : vanta::oscillatorShape(values, selectedOsc) + 1, juce::dontSendNotification);
    for (int i = 0; i < 8; ++i)
    {
        const bool mainDial = i == 0 || i == 1 || i == 2 || i == 5 || i == 6;
        mainKnobs[static_cast<std::size_t>(i)].setVisible(!browserOpen && ((page == 0 && mainDial) || (page == 1 && i < 2)));
    }
    for (auto& slider : mainMiniSliders) slider.setVisible(!browserOpen && page == 0);
    editEnvelope.setVisible(!browserOpen && page == 0); editVibrato.setVisible(!browserOpen && page == 0);
    for (int i = 0; i < 4; ++i)
    {
        auto& item = waveButtons[static_cast<std::size_t>(i)]; item.setVisible(!browserOpen && page == 1);
        item.setToggleState(values[vanta::oscillatorSample(selectedOsc)] < 0.5f && values[vanta::oscillatorTable(selectedOsc)] < 0.5f
                            && vanta::oscillatorShape(values, selectedOsc) == i, juce::dontSendNotification);
    }
    for (std::size_t i = 0; i < effects.size(); ++i)
    {
        effectSelectors[i].setVisible(!browserOpen && page == 2);
        effectSelectors[i].setToggleState(static_cast<int>(i) == selectedFx, juce::dontSendNotification);
        effectEnabled[i].setVisible(!browserOpen && page == 2);
        for (int k = 0; k < effects[i].count; ++k)
            effectKnobs[i][static_cast<std::size_t>(k)].setVisible(!browserOpen && page == 2 && static_cast<int>(i) == selectedFx);
    }
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
        tablePositions[index].setVisible(!browserOpen && page == 1 && layer == selectedOsc && values[vanta::oscillatorTable(layer)] >= 0.5f);
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
    chooser = std::make_unique<juce::FileChooser>("Open a VANTA, Vital, Serum or Serum 2 preset", juce::File(), "*.vanta;*.vital;*.fxp;*.SerumPreset");
    juce::Component::SafePointer<VantaEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& selected)
        {
            if (safe == nullptr) return;
            const auto file = selected.getResult();
            if (file == juce::File()) return;
            safe->importButton.setEnabled(false);
            safe->importWorkers.addJob([safe, file]
            {
                try
                {
                    auto result = std::make_shared<vanta::PresetResult>(vanta::PresetCodec::loadFile(file));
                    juce::MessageManager::callAsync([safe, file, result]
                    {
                        if (safe == nullptr) return;
                        safe->importButton.setEnabled(true);
                        if (result->ok)
                        {
                            safe->processor.applyPreset(*result);
                            safe->currentFactory = -1; safe->browserOpen = false; safe->refresh();
                        }
                        if (!result->ok || !file.hasFileExtension(".vanta"))
                            safe->showMessage(result->ok ? "Preset import - approximation" : "Preset not loaded", result->message, true);
                    });
                }
                catch (...)
                {
                    // Best effort if even constructing/queuing the result ran out of memory.
                    try { juce::MessageManager::callAsync([safe]
                    {
                        if (safe != nullptr)
                        {
                            safe->importButton.setEnabled(true);
                            safe->showMessage("Preset not loaded", "The preset could not be decoded safely. Your current sound was not changed.", true);
                        }
                    }); } catch (...) {}
                }
            });
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

void VantaEditor::paintCanvas(juce::Graphics& g)
{
    // The HTML concept is the layout reference.  This is a native, functional
    // JUCE editor, so the preview's unimplemented mock controls stay labelled.
    g.setGradientFill(juce::ColourGradient(header, 0.0f, 0.0f, juce::Colour(0xff710a0e),
                                          1000.0f, static_cast<float>(baseHeight()), false));
    g.fillAll();
    g.setColour(juce::Colour(0xffdb5040)); g.fillRect(0, 0, editorWidth, 4);
    g.setColour(juce::Colour(0xffd04038)); g.drawRect(canvas->getLocalBounds(), 1);
    g.setColour(juce::Colour(0x28ffc4a3)); g.drawRect(canvas->getLocalBounds().reduced(3), 1);

    // Brand and the recessed seven-page navigation match the approved header.
    g.setColour(juce::Colour(0xff200d0e)); g.fillRoundedRectangle(20.0f, 24.0f, 36.0f, 36.0f, 2.0f);
    g.setColour(juce::Colour(0x99f08469)); g.drawRoundedRectangle(20.5f, 24.5f, 35.0f, 35.0f, 2.0f, 1.0f);
    juce::Path mark; mark.startNewSubPath(27.0f, 33.0f); mark.lineTo(32.5f, 33.0f);
    mark.lineTo(38.0f, 48.0f); mark.lineTo(43.5f, 33.0f); mark.lineTo(49.0f, 33.0f);
    mark.lineTo(41.0f, 52.0f); mark.lineTo(35.0f, 52.0f); mark.closeSubPath();
    g.setColour(juce::Colour(0xffffe0c4)); g.fillPath(mark);
    g.setColour(accent); g.fillRect(46, 51, 4, 2);
    text(g, "VANTA", 66, 19, 162, 29, 23.0f, textPrimary, juce::Font::bold);
    mono(g, "SOFTWARE INSTRUMENT / 13", 67, 48, 210, 12, 8.5f, textSecondary);
    g.setColour(juce::Colour(0xff150b0c)); g.fillRoundedRectangle(500.0f, 17.0f, 480.0f, 37.0f, 3.0f);
    g.setColour(juce::Colour(0xff451417)); g.drawRoundedRectangle(500.5f, 17.5f, 479.0f, 36.0f, 3.0f, 1.0f);
    g.setColour(juce::Colour(0xff41090b)); g.fillRect(500, 63, 350, 2);
    mono(g, "16 VOICES / STEREO", 847, 57, 132, 15, 9.0f, textSecondary, juce::Justification::centredRight);

    screen(g, 16.0f, 80.0f, 968.0f, 115.0f);
    g.setColour(juce::Colour(0xff321417)); g.drawVerticalLine(780, 81.0f, 194.0f);
    mono(g, "TUNING  A = 440 Hz", 807, 168, 157, 14, 9.0f, textSecondary, juce::Justification::centredRight);
    text(g, "LEVEL", 23, 204, 55, 17, 10.0f, textPrimary, juce::Font::bold);
    mono(g, juce::String(levelSlider.getValue() * 100.0, 0) + "%", 932, 204, 40, 17, 10.0f, textPrimary, juce::Justification::centredRight);
    screen(g, 16.0f, 240.0f, 968.0f, 437.0f);

    const auto values = processor.currentValues();
    if (browserOpen)
    {
        text(g, "FACTORY PRESET BROWSER", 32, 253, 400, 17, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "SEARCH / SELECT A SOUND", 675, 253, 292, 17, 9.0f, textMuted, juce::Justification::centredRight);
        separator(g, 32.0f, 968.0f, 275.0f);
        text(g, "LIBRARY", 43, 298, 160, 18, 10.0f, accent, juce::Font::bold);
        text(g, "FACTORY SOUNDS", 43, 319, 170, 18, 10.0f, textSecondary, juce::Font::bold);
        text(g, "SEARCH", 228, 291, 100, 18, 9.0f, textMuted, juce::Font::bold);
        const bool hasMatch = std::any_of(presetButtons.begin(), presetButtons.end(), [](const auto& button) { return button.isVisible(); });
        if (!hasMatch) mono(g, "No factory presets match this search.", 228, 365, 640, 24, 10.0f, textSecondary);
        mono(g, "Import VANTA, Vital or Serum presets above; third-party conversions are approximate.", 228, 618, 700, 18, 9.0f, textMuted);
    }
    else if (page == 0)
    {
        text(g, "01", 32, 253, 25, 17, 10.0f, accent, juce::Font::bold);
        text(g, "PERFORMANCE / SOUND", 57, 253, 400, 17, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "16 VOICES / GLOBAL CONTROLS", 660, 253, 308, 17, 9.0f, textMuted, juce::Justification::centredRight);
        separator(g, 32.0f, 968.0f, 275.0f);
        text(g, "PLAY MODE", 32, 290, 130, 14, 9.0f, textMuted, juce::Font::bold);
        const char* modes[] = { "POLY", "UNISON", "LEGATO", "PORTAMENTO" };
        for (int i = 0; i < 4; ++i)
        {
            const int y = 309 + i * 28;
            g.setColour(i == 0 ? juce::Colour(0xff501918) : juce::Colour(0xff160e0e)); g.fillRect(32, y, 124, 23);
            g.setColour(i == 0 ? juce::Colour(0xff9c3a30) : juce::Colour(0xff341617)); g.drawRect(32, y, 124, 23, 1);
            if (i == 0) { g.setColour(juce::Colour(0xffffc37a)); g.fillRect(40, y + 10, 4, 4); }
            text(g, modes[i], 49, y + 1, 96, 21, 9.0f, i == 0 ? textPrimary : textMuted.withAlpha(0.65f), juce::Font::bold);
        }
        mono(g, "Additional modes are", 32, 426, 124, 12, 8.0f, textMuted);
        mono(g, "not available yet.", 32, 438, 124, 12, 8.0f, textMuted);
        g.setColour(border); g.drawVerticalLine(169, 290.0f, 449.0f);
        panel(g, 32.0f, 457.0f, 464.0f, 206.0f); panel(g, 504.0f, 457.0f, 464.0f, 206.0f);
        text(g, "01", 44, 470, 24, 16, 10.0f, accent, juce::Font::bold);
        text(g, "AMP ENVELOPE", 69, 470, 300, 16, 10.0f, textSecondary, juce::Font::bold);
        text(g, "02", 516, 470, 24, 16, 10.0f, accent, juce::Font::bold);
        text(g, "PITCH VIBRATO", 541, 470, 300, 16, 10.0f, textSecondary, juce::Font::bold);
        drawEnvelope(g, { 44.0f, 498.0f, 440.0f, 98.0f }, values);
        drawLfo(g, { 516.0f, 498.0f, 440.0f, 98.0f }, values);
        auto seconds = [](float v) { return v < 1.0f ? juce::String(v * 1000.0f, 0) + " ms" : juce::String(v, 2) + " s"; };
        const juce::String stages[] = { "DL " + seconds(values[vanta::ampDelay]), "A " + seconds(values[vanta::attack]),
                                       "H " + seconds(values[vanta::ampHold]), "D " + seconds(values[vanta::decay]),
                                       "S " + juce::String(values[vanta::sustain] * 100.0f, 0) + "%", "R " + seconds(values[vanta::release]) };
        for (int i = 0; i < 6; ++i)
            mono(g, stages[i], 44 + i * 73, 603, 72, 15, 8.0f, textMuted);
        mono(g, "DEPTH  " + juce::String(values[vanta::vibrato] * 100.0f, 0) + "%", 516, 603, 115, 15, 9.0f, textMuted);
        mono(g, "RATE  " + juce::String(values[vanta::vibratoRate], 1) + " Hz", 638, 603, 130, 15, 9.0f, textMuted);
        mono(g, "SINE / PARAMETER VIEW", 768, 603, 188, 15, 8.5f, textMuted, juce::Justification::centredRight);
        panel(g, 44.0f, 624.0f, 216.0f, 27.0f); panel(g, 266.0f, 624.0f, 218.0f, 27.0f);
        panel(g, 516.0f, 624.0f, 440.0f, 27.0f);
        text(g, "DECAY", 51, 630, 38, 15, 8.0f, textSecondary, juce::Font::bold);
        text(g, "SUSTAIN", 272, 630, 43, 15, 8.0f, textSecondary, juce::Font::bold);
        text(g, "RATE", 523, 630, 42, 15, 8.0f, textSecondary, juce::Font::bold);
        mono(g, seconds(values[vanta::decay]), 201, 630, 52, 15, 8.0f, textPrimary, juce::Justification::centredRight);
        mono(g, juce::String(values[vanta::sustain] * 100.0f, 0) + "%", 428, 630, 49, 15, 8.0f, textPrimary, juce::Justification::centredRight);
        mono(g, juce::String(values[vanta::vibratoRate], 1) + " Hz", 870, 630, 79, 15, 8.0f, textPrimary, juce::Justification::centredRight);
    }
    else if (page == 1)
    {
        text(g, "02", 32, 253, 25, 17, 10.0f, accent, juce::Font::bold);
        text(g, "OSCILLATOR / FILTER", 57, 253, 400, 17, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "UP TO 8 OSCILLATORS / GLOBAL LOW-PASS", 610, 253, 358, 17, 9.0f, textMuted, juce::Justification::centredRight);
        separator(g, 32.0f, 968.0f, 275.0f);
        int active = 0; for (int i = 0; i < vanta::oscillatorCount; ++i) if (vanta::oscillatorIsEnabled(values, i)) ++active;
        mono(g, juce::String(active) + " / 8 TABS", 833, 292, 134, 20, 9.0f, textMuted, juce::Justification::centredRight);
        mono(g, "REAL OSCILLATOR LAYERS  /  SHARED FILTER AND EFFECTS", 32, 320, 730, 14, 9.0f, textMuted);
        panel(g, 32.0f, 338.0f, 588.0f, 324.0f); panel(g, 630.0f, 338.0f, 338.0f, 224.0f); panel(g, 630.0f, 570.0f, 338.0f, 92.0f);
        const auto letter = juce::String::charToString(static_cast<juce::juce_wchar>('A' + selectedOsc));
        text(g, "01", 44, 352, 22, 17, 10.0f, accent, juce::Font::bold);
        text(g, "OSCILLATOR " + letter, 69, 352, 142, 17, 10.0f, textSecondary, juce::Font::bold);
        drawWave(g, { 44.0f, 384.0f, 564.0f, 114.0f }, vanta::oscillatorShape(values, selectedOsc));
        mono(g, values[vanta::oscillatorSample(selectedOsc)] >= 0.5f ? "SAMPLE / SCHEMATIC" : values[vanta::oscillatorTable(selectedOsc)] >= 0.5f ? "WAVETABLE / SCHEMATIC" : "WAVE SHAPE / ILLUSTRATIVE", 44, 502, 350, 14, 9.0f, textMuted);
        mono(g, "16 POLY / GLOBAL FILTER", 423, 502, 185, 14, 9.0f, textMuted, juce::Justification::centredRight);
        if (values[vanta::oscillatorTable(selectedOsc)] >= 0.5f) text(g, "TABLE POSITION", 44, 632, 102, 18, 9.0f, textSecondary, juce::Font::bold);
        else mono(g, "SELECT AN IMPORTED SOURCE IN THE SOURCE MENU", 44, 630, 546, 18, 8.5f, textMuted);
        text(g, "02", 642, 352, 22, 17, 10.0f, accent, juce::Font::bold);
        text(g, "GLOBAL LOW-PASS", 667, 352, 205, 17, 10.0f, textSecondary, juce::Font::bold);
        drawFilter(g, { 642.0f, 384.0f, 314.0f, 62.0f }, values);
        text(g, "03", 642, 584, 22, 17, 10.0f, accent, juce::Font::bold);
        text(g, "IMPORTED SOURCES", 667, 584, 250, 17, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "WAVETABLE / SAMPLE WHEN AVAILABLE", 642, 612, 306, 16, 9.0f, textMuted);
        mono(g, "UNISON + EXTRA ENGINES / PLANNED", 642, 632, 306, 16, 9.0f, textMuted);
    }
    else if (page == 2)
    {
        text(g, "03", 32, 253, 25, 17, 10.0f, accent, juce::Font::bold);
        text(g, "EFFECTS / RACK", 57, 253, 400, 17, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "FIXED SIGNAL CHAIN / SELECT A MODULE", 620, 253, 348, 17, 9.0f, textMuted, juce::Justification::centredRight);
        separator(g, 32.0f, 968.0f, 275.0f);
        panel(g, 32.0f, 289.0f, 176.0f, 373.0f); panel(g, 218.0f, 289.0f, 750.0f, 373.0f);
        mono(g, "ON", 37, 297, 24, 16, 8.0f, textMuted, juce::Justification::centred);
        text(g, "FX / RACK", 66, 297, 100, 16, 9.0f, accent, juce::Font::bold);
        const auto& fx = effects[static_cast<std::size_t>(selectedFx)];
        text(g, fx.name, 237, 307, 430, 23, 15.0f, textPrimary, juce::Font::bold);
        mono(g, values[fx.enabled] >= 0.5f ? "MODULE ON" : "MODULE OFF", 815, 310, 135, 17, 9.0f,
             values[fx.enabled] >= 0.5f ? accent : textMuted, juce::Justification::centredRight);
        mono(g, fx.description, 237, 336, 680, 16, 9.0f, textSecondary);
        rackIllustration(g, selectedFx, { 237.0f, 365.0f, 713.0f, 102.0f }, values);
        mono(g, "ILLUSTRATIVE RESPONSE / DRAG KNOBS TO ADJUST", 237, 470, 700, 15, 8.5f, textMuted);
        mono(g, "RVB > DELAY > DIST > COMP > CHORUS > EQ > FILTER > FLANGER > PHASER", 237, 632, 710, 16, 8.0f, textMuted);
    }
    else if (page == 3)
    {
        text(g, "04", 32, 253, 25, 17, 10.0f, accent, juce::Font::bold);
        text(g, "MODULATION / ENVELOPE", 57, 253, 420, 17, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "AMP ENVELOPE / SINE PITCH VIBRATO", 630, 253, 338, 17, 9.0f, textMuted, juce::Justification::centredRight);
        separator(g, 32.0f, 968.0f, 275.0f);
        panel(g, 32.0f, 289.0f, 464.0f, 337.0f); panel(g, 504.0f, 289.0f, 464.0f, 337.0f);
        text(g, "01", 44, 302, 25, 17, 10.0f, accent, juce::Font::bold);
        text(g, "AMPLITUDE ENVELOPE", 69, 302, 310, 17, 10.0f, textSecondary, juce::Font::bold);
        text(g, "02", 516, 302, 25, 17, 10.0f, accent, juce::Font::bold);
        text(g, "PITCH VIBRATO", 541, 302, 310, 17, 10.0f, textSecondary, juce::Font::bold);
        drawEnvelope(g, { 44.0f, 335.0f, 440.0f, 122.0f }, values);
        drawLfo(g, { 516.0f, 335.0f, 440.0f, 122.0f }, values);
        mono(g, "ONE SHARED AMPLITUDE ENVELOPE", 44, 600, 420, 15, 9.0f, textMuted);
        mono(g, "IMPORTED LFO ROUTES PLAY / EDITING PLANNED", 516, 600, 438, 15, 9.0f, textMuted);
        panel(g, 32.0f, 636.0f, 936.0f, 26.0f);
        text(g, "MIDI INPUT", 44, 641, 90, 16, 9.0f, textSecondary, juce::Font::bold);
        mono(g, "VELOCITY > AMP   /   PITCH BEND +/-2 ST   /   SUSTAIN CC64", 137, 641, 800, 16, 9.0f, textMuted);
    }
    else if (page == 4)
    {
        text(g, "05", 32, 253, 25, 17, 10.0f, accent, juce::Font::bold);
        text(g, "MIXER / ROUTING", 57, 253, 400, 17, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "LAYER LEVEL + PAN / GLOBAL FILTER AND FX", 620, 253, 348, 17, 9.0f, textMuted, juce::Justification::centredRight);
        separator(g, 32.0f, 968.0f, 275.0f);
        mono(g, "EIGHT REAL OSCILLATOR CHANNELS", 32, 288, 500, 16, 9.0f, textMuted);
        for (int layer = 0; layer < vanta::oscillatorCount; ++layer)
        {
            const int x = 32 + (layer % 4) * 236, y = 312 + (layer / 4) * 146;
            panel(g, static_cast<float>(x), static_cast<float>(y), 228.0f, 138.0f);
            const auto letter = juce::String::charToString(static_cast<juce::juce_wchar>('A' + layer));
            text(g, "OSC " + letter, x + 11, y + 10, 100, 18, 10.0f, textSecondary, juce::Font::bold);
            mono(g, vanta::oscillatorIsEnabled(values, layer) ? "ACTIVE" : "OFF", x + 132, y + 10, 83, 18, 9.0f, textMuted, juce::Justification::centredRight);
            mono(g, "LEVEL", x + 11, y + 40, 55, 18, 9.0f, textMuted);
            mono(g, "PAN", x + 11, y + 68, 55, 18, 9.0f, textMuted);
            mono(g, juce::String(values[vanta::oscillatorLevel(layer)] * 100.0f, 0) + "%", x + 165, y + 104, 50, 18, 9.0f, textSecondary, juce::Justification::centredRight);
        }
        panel(g, 32.0f, 615.0f, 936.0f, 47.0f);
        text(g, "ROUTING", 44, 625, 78, 18, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "ALL LAYERS SHARE THE GLOBAL LOW-PASS, ENVELOPE AND EFFECTS", 130, 625, 810, 18, 9.0f, textMuted);
    }
    else if (page == 5)
    {
        text(g, "06", 32, 253, 25, 17, 10.0f, accent, juce::Font::bold);
        text(g, "SEQUENCING", 57, 253, 400, 17, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "DESIGN TARGETS / NOT IMPLEMENTED", 630, 253, 338, 17, 9.0f, textMuted, juce::Justification::centredRight);
        separator(g, 32.0f, 968.0f, 275.0f);
        panel(g, 32.0f, 290.0f, 464.0f, 372.0f); panel(g, 504.0f, 290.0f, 464.0f, 372.0f);
        text(g, "01 / ARPEGGIATOR", 44, 305, 300, 20, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "32 STEPS / PLANNED", 44, 331, 300, 18, 9.0f, textMuted);
        text(g, "02 / CLIP SEQUENCER", 516, 305, 300, 20, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "64 STEPS / PLANNED", 516, 331, 300, 18, 9.0f, textMuted);
        for (int i = 0; i < 32; ++i)
        {
            const float x = static_cast<float>(44 + (i % 16) * 26), y = static_cast<float>(371 + (i / 16) * 32);
            g.setColour((i % 4 == 0) ? juce::Colour(0xff8e4137) : surfaceDark); g.fillRect(x, y, 21.0f, 24.0f);
            g.setColour(border); g.drawRect(x, y, 21.0f, 24.0f, 1.0f);
        }
        for (int i = 0; i < 64; ++i)
        {
            const float x = static_cast<float>(516 + (i % 16) * 26), y = static_cast<float>(371 + (i / 16) * 32);
            g.setColour((i % 7 == 0) ? juce::Colour(0xff8e4137) : surfaceDark); g.fillRect(x, y, 21.0f, 24.0f);
            g.setColour(border); g.drawRect(x, y, 21.0f, 24.0f, 1.0f);
        }
        mono(g, "NOT PLAYABLE / PLANNED FOR A FUTURE BUILD", 44, 592, 420, 18, 9.0f, textMuted);
        mono(g, "NOT PLAYABLE / PLANNED FOR A FUTURE BUILD", 516, 592, 420, 18, 9.0f, textMuted);
    }
    else if (page == 6)
    {
        text(g, "07", 32, 253, 25, 17, 10.0f, accent, juce::Font::bold);
        text(g, "SYSTEM / INFORMATION", 57, 253, 400, 17, 10.0f, textSecondary, juce::Font::bold);
        mono(g, "WINDOWS / VST3 + STANDALONE", 650, 253, 318, 17, 9.0f, textMuted, juce::Justification::centredRight);
        separator(g, 32.0f, 968.0f, 275.0f);
        panel(g, 32.0f, 290.0f, 464.0f, 372.0f); panel(g, 504.0f, 290.0f, 464.0f, 372.0f);
        text(g, "01 / AUDIO & MIDI", 44, 307, 400, 20, 10.0f, textSecondary, juce::Font::bold);
        text(g, standaloneKeyboard ? "Options > Audio/MIDI Settings" : "Audio and MIDI are configured in your DAW.", 44, 343, 430, 23, 12.0f, textSecondary);
        mono(g, "POLY / 16 VOICES", 44, 390, 380, 20, 10.0f, textPrimary);
        mono(g, "VELOCITY / SUSTAIN / PITCH BEND +/-2", 44, 418, 400, 20, 10.0f, textMuted);
        mono(g, "UP TO 8 LAYERS / GLOBAL FILTER AND FX", 44, 446, 410, 20, 10.0f, textMuted);
        mono(g, "VANTA BETA 0.13", 44, 622, 300, 18, 10.0f, textMuted);
        text(g, "02 / PRESET SUPPORT", 516, 307, 380, 20, 10.0f, textSecondary, juce::Font::bold);
        mono(g, ".VANTA  /  SAVE + LOAD", 516, 343, 380, 22, 10.0f, textPrimary);
        mono(g, ".VITAL  /  PARTIAL IMPORT", 516, 376, 380, 22, 10.0f, textSecondary);
        mono(g, "SERUM 1 / SERUM 2  /  PARTIAL IMPORT", 516, 409, 410, 22, 10.0f, textSecondary);
        mono(g, "ZENOLOGY  /  UNSUPPORTED", 516, 442, 380, 22, 10.0f, textMuted);
    }

    if (standaloneKeyboard)
    {
        text(g, "02", 16, 688, 25, 16, 10.0f, accent, juce::Font::bold);
        text(g, "PERFORMANCE KEYBOARD", 42, 688, 250, 16, 10.0f, textPrimary, juce::Font::bold);
        mono(g, "CLICK A KEY, THEN PLAY Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P", 340, 688, 644, 16, 9.0f, textSecondary, juce::Justification::centredRight);
    }
    const int footerY = baseHeight() - 23;
    mono(g, "VANTA / SOFTWARE SYNTHESIZER", 18, footerY, 450, 16, 9.0f, textSecondary);
    mono(g, standaloneKeyboard ? "STANDALONE / BETA 0.13" : "VST3 / BETA 0.13", 750, footerY, 232, 16, 9.0f, textSecondary, juce::Justification::centredRight);
}
void VantaEditor::layoutCanvas()
{
    logoButton.setBounds(20, 24, 36, 36);
    if (resizeOverlay) resizeOverlay->setBounds(canvas->getLocalBounds());
    previous.setBounds(25, 93, 38, 38); next.setBounds(25, 143, 38, 38);
    browserButton.setBounds(74, 87, 700, 100); writeButton.setBounds(820, 96, 72, 28); importButton.setBounds(899, 96, 72, 28);
    levelSlider.setBounds(87, 203, 826, 20);
    for (int i = 0; i < 7; ++i) tabs[static_cast<std::size_t>(i)].setBounds(504 + i * 67, 21, 64, 29);
    if (standaloneKeyboard) standaloneKeyboard->setBounds(17, 710, 966, 66);

    if (page == 0)
    {
        const int indices[] = { 0, 1, 2, 5, 6 };
        for (int i = 0; i < 5; ++i) mainKnobs[static_cast<std::size_t>(indices[i])].setBounds(180 + i * 157, 292, 148, 152);
        mainMiniSliders[0].setBounds(89, 629, 105, 17); mainMiniSliders[1].setBounds(315, 629, 105, 17);
        mainMiniSliders[2].setBounds(563, 629, 298, 17);
        editEnvelope.setBounds(435, 467, 48, 20); editVibrato.setBounds(907, 467, 48, 20);
    }
    if (page == 1)
    {
        mainKnobs[0].setBounds(659, 453, 112, 100); mainKnobs[1].setBounds(828, 453, 112, 100);
        waveform.setBounds(464, 347, 144, 27);
        for (int i = 0; i < 4; ++i) waveButtons[static_cast<std::size_t>(i)].setBounds(215 + i * 61, 348, 57, 25);
    }
    int shown = 0;
    for (int layer = 0; layer < vanta::oscillatorCount; ++layer)
    {
        const auto i = static_cast<std::size_t>(layer);
        if (oscillatorTabs[i].isVisible()) oscillatorTabs[i].setBounds(32 + shown++ * 59, 289, 55, 25);
        for (int k = 0; k < 4; ++k) oscillatorKnobs[i][static_cast<std::size_t>(k)].setBounds(48 + k * 139, 520, 112, 103);
        tablePositions[i].setBounds(148, 629, 440, 22);
        const int x = 32 + (layer % 4) * 236, y = 312 + (layer / 4) * 146;
        mixerEnabled[i].setBounds(x + 11, y + 101, 65, 25);
        mixerLevel[i].setBounds(x + 63, y + 39, 150, 20);
        mixerPan[i].setBounds(x + 63, y + 67, 150, 20);
    }
    const int addX = 32 + shown * 59;
    addOscillator.setBounds(addX, 289, 103, 25); removeOscillator.setBounds(addX + 110, 289, 82, 25);
    for (std::size_t i = 0; i < effects.size(); ++i)
    {
        const int y = 318 + static_cast<int>(i) * 37;
        effectEnabled[i].setBounds(37, y, 24, 30);
        effectSelectors[i].setBounds(64, y, 138, 30);
        const int count = effects[i].count;
        for (int k = 0; k < count; ++k)
            effectKnobs[i][static_cast<std::size_t>(k)].setBounds(250 + k * 170, 490, 124, 114);
    }
    for (int i = 0; i < 6; ++i) modulationKnobs[static_cast<std::size_t>(i)].setBounds(44 + i * 73, 470, 72, 114);
    modulationKnobs[6].setBounds(590, 470, 112, 114); modulationKnobs[7].setBounds(767, 470, 112, 114);
    for (int i = 0; i < 4; ++i) folders[static_cast<std::size_t>(i)].setBounds(43, 349 + i * 42, 160, 34);
    presetSearch.setBounds(228, 313, 726, 29);
    int row = 0;
    for (int i = 0; i < VantaProcessor::factoryPresetCount; ++i)
        if (presetButtons[static_cast<std::size_t>(i)].isVisible())
            presetButtons[static_cast<std::size_t>(i)].setBounds(228, 350 + row++ * 44, 726, 36);
}
