#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
const juce::Colour red(0xff8f1c20), border(0xff61302a), surface(0xff1a1010), dark(0xff120d0e);
const juce::Colour primary(0xffffe5d3), secondary(0xffd3aa9b), muted(0xff9b796e), accent(0xffff7658), gold(0xffffc36a);
std::array<float, sidechain::stepCount> makeShape(const juce::String& name);
void label(juce::Graphics& g, const juce::String& text, int x, int y, int w, int h, float size, juce::Colour colour,
           juce::Justification align = juce::Justification::centredLeft, bool bold = false)
{
    g.setColour(colour);
    g.setFont(juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)));
    g.drawFittedText(text, x, y, w, h, align, 1);
}
void mono(juce::Graphics& g, const juce::String& text, int x, int y, int w, int h, float size, juce::Colour colour,
          juce::Justification align = juce::Justification::centredLeft)
{
    g.setColour(colour);
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain)));
    g.drawFittedText(text, x, y, w, h, align, 1);
}
void panel(juce::Graphics& g, int x, int y, int w, int h, juce::Colour fill, juce::Colour edge = border)
{
    g.setColour(fill);
    g.fillRect(x, y, w, h);
    g.setColour(edge);
    g.drawRect(x, y, w, h, 1);
}
} // namespace
SidechainLook::SidechainLook()
{
    setColour(juce::ComboBox::backgroundColourId, dark);
    setColour(juce::ComboBox::textColourId, primary);
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::ComboBox::arrowColourId, secondary);
    setColour(juce::PopupMenu::backgroundColourId, dark);
    setColour(juce::PopupMenu::textColourId, primary);
    setColour(juce::Slider::textBoxBackgroundColourId, dark);
    setColour(juce::Slider::textBoxTextColourId, primary);
    setColour(juce::Slider::textBoxOutlineColourId, border);
}
void SidechainLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto role = b.getProperties()["role"].toString();
    const auto r = b.getLocalBounds().toFloat().reduced(.5f);
    if (role == "logo")
    {
        g.setColour(juce::Colour(0xff200d0e));
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(over ? primary : juce::Colour(0xffa84e40));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);
        juce::Path p;
        p.startNewSubPath(r.getX() + 8, r.getY() + 9);
        p.lineTo(r.getX() + 14, r.getY() + 9);
        p.lineTo(r.getCentreX(), r.getY() + 25);
        p.lineTo(r.getRight() - 12, r.getY() + 9);
        p.lineTo(r.getRight() - 6, r.getY() + 9);
        p.lineTo(r.getCentreX() + 3, r.getBottom() - 7);
        p.lineTo(r.getCentreX() - 3, r.getBottom() - 7);
        p.closeSubPath();
        g.setColour(primary);
        g.fillPath(p);
        return;
    }
    if (role == "close")
        return;
    const bool on = b.getToggleState(), volume = role == "volume" || role == "volmode" || role == "voltool";
    juce::Colour base = juce::Colour(0xff211314), edge = juce::Colour(0xff482623);
    if (role == "mode" || role == "volmode")
        base = juce::Colour(0xff160e0e);
    if (on)
    {
        base = volume ? juce::Colour(0xff674326) : juce::Colour(0xff6b271f);
        edge = volume ? juce::Colour(0xffe5b26e) : juce::Colour(0xffff8561);
    }
    else if (over || down)
    {
        base = juce::Colour(0xff3c1b19);
        edge = juce::Colour(0xffb25542);
    }
    g.setColour(base);
    g.fillRect(r);
    g.setColour(edge);
    g.drawRect(r, 1.0f);
    if (role == "rack")
    {
        const bool enabled = static_cast<bool>(b.getProperties()["enabled"]);
        g.setColour(enabled ? accent : juce::Colour(0xff58342d));
        g.fillEllipse(r.getRight() - 9.0f, r.getY() + 5.0f, 4.0f, 4.0f);
    }
    if (role == "line" || role == "step")
    {
        g.setColour(on ? (volume ? gold : accent) : secondary);
        juce::Path p;
        if (role == "line")
        {
            p.startNewSubPath(5.0f, 19.0f);
            p.lineTo(11.0f, 6.0f);
            p.lineTo(21.0f, 15.0f);
        }
        else
        {
            p.startNewSubPath(4.0f, 19.0f);
            p.lineTo(10.0f, 19.0f);
            p.lineTo(10.0f, 12.0f);
            p.lineTo(16.0f, 12.0f);
            p.lineTo(16.0f, 6.0f);
            p.lineTo(22.0f, 6.0f);
        }
        g.strokePath(p, juce::PathStrokeType(1.1f));
    }
}
void SidechainLook::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    const auto role = b.getProperties()["role"].toString();
    if (role == "logo" || role == "line" || role == "step")
        return;
    if (role == "shape")
    {
        const auto values = makeShape(b.getButtonText());
        const float left = 10.0f, right = static_cast<float>(b.getWidth()) - 10.0f;
        const float top = 7.0f, bottom = static_cast<float>(b.getHeight()) - 20.0f;
        juce::Path path;
        path.startNewSubPath(left, bottom - values[0] * (bottom - top));
        const bool held = b.getButtonText() == "CHOP" || b.getButtonText() == "STAIR" || b.getButtonText() == "PULSE";
        for (int i = 1; i < sidechain::stepCount; ++i)
        {
            const float x = left + (right - left) * i / (sidechain::stepCount - 1.0f);
            if (held)
                path.lineTo(x, bottom - values[static_cast<std::size_t>(i - 1)] * (bottom - top));
            path.lineTo(x, bottom - values[static_cast<std::size_t>(i)] * (bottom - top));
        }
        g.setColour(b.getToggleState() || over ? juce::Colour(0xffffc39b) : accent.withAlpha(.8f));
        g.strokePath(path, juce::PathStrokeType(1.35f));
        label(g, b.getButtonText(), 6, b.getHeight() - 18, b.getWidth() - 12, 14, 9.0f,
              b.getToggleState() || over ? primary : secondary, juce::Justification::centred, true);
        return;
    }
    const bool bank = role == "time" || role == "volume";
    const float size = bank                                  ? (b.getHeight() > 25 ? 11.0f : 8.0f)
                       : role == "close"                     ? 14.0f
                       : role == "mode" || role == "volmode" ? 10.0f
                                                             : 9.0f;
    auto colour = b.getToggleState() ? primary : secondary;
    if (over)
        colour = primary;
    if (role == "close")
        colour = accent;
    const auto alignment =
        bank || role == "mode" || role == "volmode" ? juce::Justification::centredLeft : juce::Justification::centred;
    label(g, b.getButtonText(), bank ? 4 : 8, 0, b.getWidth() - (bank ? 7 : 16), b.getHeight(), size, colour, alignment,
          !bank);
}
void SidechainLook::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                     juce::Slider::SliderStyle, juce::Slider&)
{
    const float centre = static_cast<float>(y) + h * .5f;
    g.setColour(juce::Colour(0xff51312a));
    g.fillRoundedRectangle(static_cast<float>(x), centre - 2, static_cast<float>(w), 4, 2);
    g.setColour(accent);
    g.fillRoundedRectangle(static_cast<float>(x), centre - 2, std::max(0.0f, pos - x), 4, 2);
    g.fillEllipse(pos - 5, centre - 5, 10, 10);
}
namespace
{
const char* moduleNames[] = {"VOLUME", "FILTER", "DRIVE",  "CRUSH",  "TIME",  "PAN",
                             "WIDTH",  "NOISE",  "LIQUID", "REVERB", "PITCH", "COMP"};
const char* descriptions[] = {
    "Rhythmic gain and ducking.",   "Shape a resonant low-pass filter.", "Move distortion and tone.",
    "Animate bit and sample rate.", "Modulate a short delay.",           "Move across the stereo field.",
    "Shape stereo width.",          "Add moving noise texture.",         "Animate a phase-like effect.",
    "Sculpt a short stereo space.", "Shift pitch with moving grains.",   "Shape a linked compressor."};
const char* controlNames[][3] = {
    {"AMOUNT", "TRIM", "MIX"},   {"DEPTH", "RESONANCE", "MIX"}, {"DRIVE", "TONE", "MIX"},
    {"BITS", "RATE", "MIX"},     {"OFFSET", "DETAIL", "MIX"},   {"AMOUNT", "DETAIL", "MIX"},
    {"AMOUNT", "DETAIL", "MIX"}, {"LEVEL", "COLOR", "MIX"},     {"DEPTH", "DETAIL", "MIX"},
    {"DEPTH", "DECAY", "MIX"},   {"PITCH", "DETAIL", "MIX"},    {"THRESHOLD", "RATIO", "MIX"}};
const char* bankNames[][8] = {{"TIGHT", "DEEP", "SOFT", "PULSE", "LIFT", "DOUBLE", "BOUNCE", "FLAT"},
                              {"WAVE", "BOUNCE", "RAMP", "FALL", "CHOP", "STAIR", "TRI", "DOUBLE"},
                              {"FLAT", "CENTER", "FLOOR", "RAMP", "FALL", "TRI", "WAVE", "PULSE"}};
std::array<float, sidechain::stepCount> makeShape(const juce::String& name)
{
    std::array<float, sidechain::stepCount> out{};
    for (int i = 0; i < sidechain::stepCount; ++i)
    {
        const float x = static_cast<float>(i) / sidechain::stepCount;
        auto ease = [](float v)
        {
            v = std::clamp(v, 0.0f, 1.0f);
            return v * v * (3 - 2 * v);
        };
        float v = 1;
        if (name == "TIGHT")
            v = .03f + .97f * ease((x - .03f) / .25f);
        else if (name == "DEEP")
            v = .03f + .97f * ease((x - .05f) / .42f);
        else if (name == "SOFT")
            v = .15f + .85f * ease((x - .03f) / .65f);
        else if (name == "PULSE")
            v = (x > .15f && x < .65f) ? 1.0f : .05f;
        else if (name == "LIFT")
            v = .15f + .85f * ease(x);
        else if (name == "DOUBLE")
            v = x < .5f ? ease(x * 2) : ease((x - .5f) * 2);
        else if (name == "BOUNCE")
            v = std::abs(2.0f * std::fmod(x * 2.0f, 1.0f) - 1.0f);
        else if (name == "WAVE")
            v = .5f - .5f * std::cos(x * 2.0f * juce::MathConstants<float>::pi);
        else if (name == "RAMP")
            v = x;
        else if (name == "FALL")
            v = 1 - x;
        else if (name == "CHOP")
            v = (i % 4 < 2) ? 1.0f : 0.0f;
        else if (name == "STAIR")
            v = std::floor(x * 4) / 3;
        else if (name == "TRI")
            v = 1.0f - std::abs(2 * x - 1);
        else if (name == "CENTER")
            v = .5f;
        else if (name == "FLOOR")
            v = 0;
        out[static_cast<std::size_t>(i)] = std::clamp(v, 0.0f, 1.0f);
    }
    return out;
}
} // namespace
class SidechainEditor::Canvas final : public juce::Component
{
  public:
    explicit Canvas(SidechainEditor& e) : owner(e)
    {
        setComponentID("sidechain-canvas");
    }
    void paint(juce::Graphics& g) override
    {
        owner.paintCanvas(g);
    }

  private:
    SidechainEditor& owner;
};
class SidechainEditor::RackButton final : public juce::TextButton
{
  public:
    RackButton(SidechainEditor& e, int m) : owner(e), module(m) {}
    void mouseDown(const juce::MouseEvent& e) override
    {
        dragged = false;
        juce::TextButton::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() > 6)
            dragged = true;
        if (!dragged)
            juce::TextButton::mouseDrag(e);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!dragged)
        {
            juce::TextButton::mouseUp(e);
            return;
        }
        setState(juce::Button::buttonNormal);
        const auto p = e.getEventRelativeTo(owner.canvas.get()).getPosition();
        for (const auto& b : owner.rack)
            if (b && b->getBounds().contains(p))
            {
                owner.reorderModule(module, b->module);
                break;
            }
        dragged = false;
    }

  private:
    SidechainEditor& owner;
    int module;
    bool dragged = false;
};
class SidechainEditor::CurveEditor final : public juce::Component
{
  public:
    explicit CurveEditor(SidechainEditor& e) : owner(e)
    {
        setComponentID("sidechain-curve");
        setTitle("Shape editor. Use arrow keys to select and adjust points.");
        setWantsKeyboardFocus(true);
    }
    ~CurveEditor() override
    {
        endGestures();
    }
    int selected() const noexcept
    {
        return selectedStep;
    }
    void addMeterSample(float sample)
    {
        meter[static_cast<std::size_t>(meterPosition)] = std::clamp(sample, 0.0f, 1.0f);
        meterPosition = (meterPosition + 1) % static_cast<int>(meter.size());
    }
    void paint(juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        const auto r = area.reduced(9.0f);
        const auto settings = owner.processor.currentSettings();
        const int m = owner.processor.selectedModule(), b = owner.processor.selectedBand();
        const auto& lane = settings.module[m].band[b];
        g.setColour(dark);
        g.fillRect(area);
        g.setColour(border);
        g.drawRect(area.reduced(.5f), 1.0f);
        g.saveState();
        g.reduceClipRegion(area.reduced(1).toNearestInt());
        for (int i = 0; i <= 32; ++i)
        {
            const float x = r.getX() + r.getWidth() * i / 32.0f;
            g.setColour(i % 8 == 0 ? juce::Colour(0xff744338) : juce::Colour(0xff34211e));
            g.drawLine(x, r.getY(), x, r.getBottom(), i % 8 == 0 ? 1.0f : .6f);
        }
        for (int i = 0; i <= 8; ++i)
        {
            const float y = r.getY() + r.getHeight() * i / 8.0f;
            g.setColour(i % 2 == 0 ? juce::Colour(0xff5e352c) : juce::Colour(0xff30201c));
            g.drawLine(r.getX(), y, r.getRight(), y, .7f);
        }
        if (owner.showKey)
        {
            juce::Path wave;
            for (int i = 0; i < static_cast<int>(meter.size()); ++i)
            {
                const auto v = meter[static_cast<std::size_t>((meterPosition + i) % static_cast<int>(meter.size()))];
                const float x = r.getX() + r.getWidth() * i / (meter.size() - 1),
                            y = r.getBottom() - r.getHeight() * v * .55f;
                if (i == 0)
                    wave.startNewSubPath(x, y);
                else
                    wave.lineTo(x, y);
            }
            g.setColour(juce::Colour(0xffc3614d).withAlpha(.65f));
            g.strokePath(wave, juce::PathStrokeType(1.0f));
        }
        const float phase = std::clamp(owner.processor.playheadPhase(m), 0.0f, 1.0f);
        g.setColour(gold.withAlpha(.45f));
        g.drawLine(r.getX() + r.getWidth() * phase, r.getY(), r.getX() + r.getWidth() * phase, r.getBottom(), 1.0f);
        if (owner.processor.stepDraw())
        {
            g.setColour(primary.withAlpha(.06f));
            g.fillRect(r.getX() + r.getWidth() * selectedStep / 16.0f, r.getY(), r.getWidth() / 16.0f, r.getHeight());
        }
        auto px = [&](int i) { return r.getX() + r.getWidth() * i / 16.0f; };
        auto py = [&](float v) { return r.getBottom() - r.getHeight() * std::clamp(v, 0.0f, 1.0f); };
        juce::Path line;
        line.startNewSubPath(px(0), py(lane.curve[0]));
        for (int i = 1; i < 16; ++i)
        {
            if (lane.stepped)
                line.lineTo(px(i), py(lane.curve[i - 1]));
            line.lineTo(px(i), py(lane.curve[i]));
        }
        line.lineTo(px(16), py(lane.curve[15]));
        juce::Path fill = line;
        fill.lineTo(px(16), r.getBottom());
        fill.lineTo(px(0), r.getBottom());
        fill.closeSubPath();
        g.setColour(accent.withAlpha(.12f));
        g.fillPath(fill);
        g.setColour(juce::Colour(0xffffb17b));
        g.strokePath(line, juce::PathStrokeType(2.0f));
        for (int i = 0; i < 16; ++i)
        {
            const float x = px(i), y = py(lane.curve[i]);
            g.setColour(i == selectedStep && hasKeyboardFocus(false) ? primary : dark);
            g.fillEllipse(x - 4, y - 4, 8, 8);
            g.setColour(juce::Colour(0xffffc39b));
            g.drawEllipse(x - 4, y - 4, 8, 8, 1.4f);
        }
        g.restoreState();
        mono(g, "0 dB", 13, 12, 60, 14, 9, secondary);
        mono(g, "-INF", 13, getHeight() - 26, 60, 14, 9, secondary);
        mono(g, owner.showKey ? "KEY INPUT" : "KEY HIDDEN", getWidth() - 110, 12, 95, 14, 9, muted,
             juce::Justification::centredRight);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!e.mods.isLeftButtonDown())
            return;
        endGestures();
        grabKeyboardFocus();
        owner.rememberCurve();
        gestureModule = owner.processor.selectedModule();
        gestureBand = owner.processor.selectedBand();
        dragStep = -1;
        if (owner.processor.stepDraw())
            owner.processor.setParameterValue(SidechainProcessor::bandId(gestureModule, gestureBand, "stepped"), 1);
        change(e.position, true);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (gestureModule >= 0)
            change(e.position, false);
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        endGestures();
    }
    bool keyPressed(const juce::KeyPress& k) override
    {
        if (k.getKeyCode() == juce::KeyPress::leftKey)
        {
            selectedStep = std::max(0, selectedStep - 1);
            repaint();
            return true;
        }
        if (k.getKeyCode() == juce::KeyPress::rightKey)
        {
            selectedStep = std::min(15, selectedStep + 1);
            repaint();
            return true;
        }
        if (k.getKeyCode() != juce::KeyPress::upKey && k.getKeyCode() != juce::KeyPress::downKey)
            return false;
        owner.rememberCurve();
        const int m = owner.processor.selectedModule(), b = owner.processor.selectedBand();
        const auto s = owner.processor.currentSettings();
        if (owner.processor.stepDraw())
            owner.processor.setParameterValue(SidechainProcessor::bandId(m, b, "stepped"), 1);
        const float delta =
            (k.getKeyCode() == juce::KeyPress::upKey ? 1.0f : -1.0f) * (k.getModifiers().isShiftDown() ? .05f : .01f);
        owner.processor.setParameterValue(SidechainProcessor::pointId(m, b, selectedStep),
                                          std::clamp(s.module[m].band[b].curve[selectedStep] + delta, 0.0f, 1.0f));
        owner.selectedShape.clear();
        owner.refresh();
        return true;
    }
    void endGestures()
    {
        for (int i = 0; i < 16; ++i)
            if (gestures[i])
            {
                if (auto* p = owner.processor.pointParameter(gestureModule, gestureBand, i))
                    p->endChangeGesture();
                gestures[i] = false;
            }
        gestureModule = gestureBand = -1;
        dragStep = -1;
    }

  private:
    void change(juce::Point<float> point, bool first)
    {
        const auto r = getLocalBounds().toFloat().reduced(9.0f);
        if (r.getWidth() <= 0 || r.getHeight() <= 0)
            return;
        const float x = std::clamp((point.x - r.getX()) / r.getWidth(), 0.0f, .999999f),
                    value = 1.0f - std::clamp((point.y - r.getY()) / r.getHeight(), 0.0f, 1.0f);
        int to = std::clamp(static_cast<int>(x * 16), 0, 15);
        if (!owner.processor.stepDraw())
        {
            if (first)
                selectedStep = to;
            to = selectedStep;
        }
        const int from = dragStep < 0 ? to : dragStep, distance = owner.processor.stepDraw() ? std::abs(to - from) : 0,
                  direction = to >= from ? 1 : -1;
        for (int i = 0; i <= distance; ++i)
        {
            const int step = from + i * direction;
            const float t = distance ? static_cast<float>(i) / distance : 1.0f,
                        next = dragStep < 0 ? value : dragValue + (value - dragValue) * t;
            if (auto* p = owner.processor.pointParameter(gestureModule, gestureBand, step))
            {
                if (!gestures[step])
                {
                    p->beginChangeGesture();
                    gestures[step] = true;
                }
                p->setValueNotifyingHost(p->convertTo0to1(next));
            }
        }
        dragStep = to;
        dragValue = value;
        selectedStep = to;
        owner.selectedShape.clear();
        owner.refresh();
    }
    SidechainEditor& owner;
    int selectedStep = 0, dragStep = -1, gestureModule = -1, gestureBand = -1, meterPosition = 0;
    float dragValue = 0;
    std::array<bool, 16> gestures{};
    std::array<float, 128> meter{};
};
class SidechainEditor::ResizeOverlay final : public juce::Component
{
  public:
    explicit ResizeOverlay(SidechainEditor& e) : owner(e)
    {
        setComponentID("resize-overlay");
        setTitle("Sidechain interface size");
        setWantsKeyboardFocus(true);
        setFocusContainerType(FocusContainerType::focusContainer);
        constexpr int sizesValue[] = {60, 75, 90, 100, 110, 125, 150};
        for (std::size_t i = 0; i < sizes.size(); ++i)
        {
            auto& b = sizes[i];
            const int v = sizesValue[i];
            b.setButtonText(juce::String(v) + "%");
            b.setComponentID("resize-" + juce::String(v));
            b.getProperties().set("role", "size");
            b.onClick = [this, v]
            {
                owner.showResizeMenu(false);
                owner.setUiScale(v, true);
            };
            addAndMakeVisible(b);
        }
        fit.setButtonText("FIT SCREEN");
        fit.setComponentID("resize-fit");
        fit.getProperties().set("role", "size");
        fit.onClick = [this]
        {
            const int v = owner.fitScale();
            owner.showResizeMenu(false);
            owner.setUiScale(v, true);
        };
        addAndMakeVisible(fit);
        close.setButtonText("X");
        close.setComponentID("resize-close");
        close.setTitle("Close size menu");
        close.getProperties().set("role", "close");
        close.onClick = [this] { owner.showResizeMenu(false); };
        addAndMakeVisible(close);
    }
    void updateSelection(int percent)
    {
        constexpr int v[] = {60, 75, 90, 100, 110, 125, 150};
        for (std::size_t i = 0; i < sizes.size(); ++i)
            sizes[i].setToggleState(v[i] == percent, juce::dontSendNotification);
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::black.withAlpha(.72f));
        g.fillRect(getLocalBounds());
        const auto r = dialog;
        panel(g, r.getX(), r.getY(), r.getWidth(), r.getHeight(), juce::Colour(0xff0c0909), juce::Colour(0xffd04038));
        g.setColour(juce::Colour(0xffd04038));
        g.fillRect(r.getX() + 1, r.getY() + 1, r.getWidth() - 2, 3);
        label(g, "SIDECHAIN / DISPLAY", r.getX() + 22, r.getY() + 20, 230, 17, 10, accent,
              juce::Justification::centredLeft, true);
        label(g, "INTERFACE SIZE", r.getX() + 22, r.getY() + 41, 290, 26, 19, primary, juce::Justification::centredLeft,
              true);
        mono(g, "CURRENT " + juce::String(owner.currentScale()) + "%", r.getX() + 282, r.getY() + 46, 105, 18, 9,
             secondary, juce::Justification::centredRight);
        mono(g,
             owner.hostResizeRejected ? "Host did not accept that size. Try another."
                                      : "Choose a size or fit Sidechain to your screen.",
             r.getX() + 22, r.getY() + 75, r.getWidth() - 44, 15, 9, owner.hostResizeRejected ? accent : muted);
        g.setColour(juce::Colour(0xff44201e));
        g.drawHorizontalLine(r.getY() + 99, static_cast<float>(r.getX() + 22), static_cast<float>(r.getRight() - 22));
        mono(g, "ESC OR CLICK OUTSIDE TO CLOSE", r.getX() + 22, r.getBottom() - 30, r.getWidth() - 44, 14, 9, muted);
    }
    void resized() override
    {
        dialog = calculate();
        const auto r = dialog;
        close.setBounds(r.getRight() - 48, r.getY() + 15, 30, 25);
        for (std::size_t i = 0; i < sizes.size(); ++i)
            sizes[i].setBounds(r.getX() + 24 + static_cast<int>(i % 4) * 100,
                               r.getY() + 112 + static_cast<int>(i / 4) * 47, 92, 36);
        fit.setBounds(r.getX() + 324, r.getY() + 159, 92, 36);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!dialog.contains(e.getPosition()))
            owner.showResizeMenu(false);
    }
    bool keyPressed(const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey)
        {
            owner.showResizeMenu(false);
            return true;
        }
        return false;
    }
    void updatePlacement()
    {
        if (calculate() != dialog)
        {
            resized();
            repaint();
        }
    }

  private:
    juce::Rectangle<int> calculate() const
    {
        auto visible = getLocalBounds();
        if (getPeer() != nullptr)
            if (const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds()))
                visible = visible.getIntersection(getLocalArea(nullptr, d->userArea));
        const int cx = visible.isEmpty() ? getWidth() / 2 : visible.getCentreX(),
                  cy = visible.isEmpty() ? getHeight() / 2 : visible.getCentreY();
        return {std::clamp(cx - 220, 0, std::max(0, getWidth() - 440)),
                std::clamp(cy - 114, 0, std::max(0, getHeight() - 228)), 440, 228};
    }
    SidechainEditor& owner;
    juce::Rectangle<int> dialog;
    std::array<juce::TextButton, 7> sizes;
    juce::TextButton fit, close;
};

SidechainEditor::SidechainEditor(SidechainProcessor& p) : AudioProcessorEditor(p), processor(p)
{
    setLookAndFeel(&look);
    canvas = std::make_unique<Canvas>(*this);
    canvas->setSize(width, height);
    addAndMakeVisible(*canvas);
    setupButton(logo, "", "logo", "resize-logo");
    logo.setTitle("Resize VANTA Sidechain");
    logo.onClick = [this] { showResizeMenu(true); };
    setupButton(masterEnabled, "ACTIVE", "power", "master-enabled");
    masterEnabled.onClick = [this]
    {
        processor.setParameterValue("enabled", processor.currentSettings().enabled ? 0 : 1);
        refresh();
    };
    setupButton(help, "?", "tool", "routing-help");
    help.onClick = []
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon, "Link a sidechain in FL Studio",
            "Put VANTA Sidechain on the bass mixer track. Select the kick track, right-click the send arrow to the "
            "bass track, and choose 'Sidechain to this track'. In the plugin wrapper's Processing page, assign the "
            "external sidechain input if needed. Select SIDECHAIN here, then adjust the threshold.\n\nSYNC follows "
            "host tempo; MIDI retriggers on note-on.");
    };
    setupButton(shaperEnabled, "SHAPER ON", "power", "shaper-enabled");
    shaperEnabled.onClick = [this]
    {
        const int m = processor.selectedModule();
        processor.setParameterValue(SidechainProcessor::moduleId(m, "on"),
                                    processor.currentSettings().module[m].enabled ? 0 : 1);
        refresh();
    };
    setupButton(stepDraw, "STEP DRAW OFF", "tool", "step-draw");
    stepDraw.setTitle("Toggle step drawing");
    stepDraw.onClick = [this]
    {
        if (curve)
            curve->endGestures();
        processor.setStepDraw(!processor.stepDraw());
        processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged(true));
        refresh();
    };
    setupButton(pointTool, "POINT", "tool", "point-tool");
    pointTool.onClick = [this]
    {
        if (curve)
            curve->endGestures();
        processor.setStepDraw(false);
        processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged(true));
        refresh();
    };
    setupButton(undo, "UNDO", "tool", "curve-undo");
    undo.onClick = [this] { undoCurve(false); };
    setupButton(redo, "REDO", "tool", "curve-redo");
    redo.onClick = [this] { undoCurve(true); };
    setupButton(reset, "RESET", "tool", "curve-reset");
    reset.onClick = [this]
    {
        rememberCurve();
        const int m = processor.selectedModule(), b = processor.selectedBand();
        std::array<float, 16> points{};
        for (int i = 0; i < 16; ++i)
            points[i] = sidechain::defaultCurveValue(m, i);
        processor.applyCurve(m, b, points, false);
        selectedShape.clear();
        refresh();
    };
    setupButton(oneShot, "ONE SHOT", "tool", "one-shot");
    oneShot.onClick = [this]
    {
        const int m = processor.selectedModule();
        processor.setParameterValue(SidechainProcessor::moduleId(m, "oneshot"),
                                    processor.currentSettings().module[m].oneShot ? 0 : 1);
        refresh();
    };
    setupButton(keyOverlay, "KEY OVERLAY", "tool", "key-overlay");
    keyOverlay.onClick = [this]
    {
        showKey = !showKey;
        refresh();
    };
    for (int i = 0; i < sidechain::moduleCount; ++i)
    {
        rack[i] = std::make_unique<RackButton>(*this, i);
        auto& button = *rack[i];
        setupButton(button, moduleNames[i], "rack", "shaper-" + juce::String(i));
        button.setTitle(juce::String("Select ") + moduleNames[i] + "; drag to reorder");
        button.onClick = [this, i] { selectModule(i); };
    }
    const char* bandNames[] = {"FULL", "LOW", "MID", "HIGH"};
    for (int i = 0; i < 4; ++i)
    {
        setupButton(bands[i], bandNames[i], "tool", "band-" + juce::String(i));
        bands[i].onClick = [this, i] { selectBand(i); };
    }
    const char* sourceNames[] = {"SIDECHAIN", "SYNC", "MIDI"};
    for (int i = 0; i < 3; ++i)
    {
        setupButton(sources[i], sourceNames[i], "tool", "source-" + juce::String(i));
        sources[i].onClick = [this, i]
        {
            const int m = processor.selectedModule();
            processor.setParameterValue(SidechainProcessor::moduleId(m, "source"), static_cast<float>(i));
            refresh();
        };
    }
    const char* bankLabels[] = {"DUCKS", "MOTION", "UTILITY"};
    for (int i = 0; i < 3; ++i)
    {
        setupButton(bankTabs[i], bankLabels[i], "tool", "bank-" + juce::String(i));
        bankTabs[i].onClick = [this, i]
        {
            bank = i;
            refresh();
        };
    }
    for (int i = 0; i < 8; ++i)
    {
        setupButton(shapes[i], bankNames[0][i], "shape", "shape-" + juce::String(i));
        shapes[i].onClick = [this, i] { applyShape(bankNames[bank][i]); };
    }
    auto slider = [this](juce::Slider& s, const char* id, const char* title)
    {
        s.setComponentID(id);
        s.setTitle(title);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setWantsKeyboardFocus(true);
        canvas->addAndMakeVisible(s);
    };
    slider(masterMix, "master-mix", "Master mix");
    slider(smooth, "curve-smooth", "Curve smoothing");
    slider(threshold, "trigger-threshold", "Sidechain trigger threshold");
    slider(attack, "trigger-attack", "Sidechain trigger attack");
    slider(release, "trigger-release", "Sidechain trigger release");
    for (int i = 0; i < 3; ++i)
        slider(params[i], ("band-param-" + juce::String(i)).toRawUTF8(), "Selected shaper parameter");
    masterMixAttachment = std::make_unique<SliderAttachment>(processor.state, "masterMix", masterMix);
    length.setComponentID("curve-length");
    length.setTitle("Curve length");
    const char* lengths[] = {"1/4 NOTE", "1/8 NOTE", "1/16 NOTE", "1/2 NOTE", "1 BAR", "2 BARS", "4 BARS"};
    for (int i = 0; i < 7; ++i)
        length.addItem(lengths[i], i + 1);
    canvas->addAndMakeVisible(length);
    preset.setComponentID("shape-preset");
    preset.setTitle("Load a shape for the selected band");
    preset.setTextWhenNothingSelected("CUSTOM");
    int id = 1;
    juce::StringArray all;
    for (const auto& names : bankNames)
        for (auto* n : names)
            if (!all.contains(n))
            {
                all.add(n);
                preset.addItem(n, id++);
            }
    preset.setSelectedId(2, juce::dontSendNotification);
    preset.onChange = [this]
    {
        if (preset.getSelectedId() > 0)
            applyShape(preset.getText());
    };
    canvas->addAndMakeVisible(preset);
    curve = std::make_unique<CurveEditor>(*this);
    canvas->addAndMakeVisible(*curve);
    overlay = std::make_unique<ResizeOverlay>(*this);
    canvas->addChildComponent(*overlay);
    bindControls();
    lastScale = processor.editorScalePercent();
    setSize(juce::roundToInt(width * lastScale / 100.0), juce::roundToInt(height * lastScale / 100.0));
    setResizeLimits(width * 60 / 100, height * 60 / 100, width * 150 / 100, height * 150 / 100);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(width) / height);
    setResizable(true, false);
    refresh();
    startTimerHz(20);
}
SidechainEditor::~SidechainEditor()
{
    stopTimer();
    if (curve)
        curve->endGestures();
    setLookAndFeel(nullptr);
}
void SidechainEditor::setupButton(juce::TextButton& b, const juce::String& text, const char* role,
                                  const juce::String& id)
{
    b.setButtonText(text);
    b.setComponentID(id);
    b.getProperties().set("role", role);
    canvas->addAndMakeVisible(b);
}
void SidechainEditor::bindControls()
{
    const int m = processor.selectedModule(), b = processor.selectedBand();
    if (m == boundModule && b == boundBand)
        return;
    for (auto& a : paramAttachments)
        a.reset();
    smoothAttachment.reset();
    thresholdAttachment.reset();
    attackAttachment.reset();
    releaseAttachment.reset();
    lengthAttachment.reset();
    for (int i = 0; i < 3; ++i)
    {
        const char* suffix[] = {"amount", "detail", "mix"};
        params[i].setTitle(juce::String(moduleNames[m]) + " " + controlNames[m][i]);
        paramAttachments[i] =
            std::make_unique<SliderAttachment>(processor.state, SidechainProcessor::bandId(m, b, suffix[i]), params[i]);
    }
    smoothAttachment =
        std::make_unique<SliderAttachment>(processor.state, SidechainProcessor::moduleId(m, "smooth"), smooth);
    thresholdAttachment =
        std::make_unique<SliderAttachment>(processor.state, SidechainProcessor::moduleId(m, "threshold"), threshold);
    attackAttachment =
        std::make_unique<SliderAttachment>(processor.state, SidechainProcessor::moduleId(m, "attack"), attack);
    releaseAttachment =
        std::make_unique<SliderAttachment>(processor.state, SidechainProcessor::moduleId(m, "release"), release);
    lengthAttachment =
        std::make_unique<ComboAttachment>(processor.state, SidechainProcessor::moduleId(m, "length"), length);
    boundModule = m;
    boundBand = b;
}
void SidechainEditor::selectModule(int m)
{
    if (curve)
        curve->endGestures();
    processor.setSelectedModule(m);
    const auto s = processor.currentSettings();
    processor.setSelectedBand(s.module[m].multiband ? 1 : 0);
    selectedShape.clear();
    processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged(true));
    bindControls();
    refresh();
}
void SidechainEditor::selectBand(int b)
{
    if (curve)
        curve->endGestures();
    const int m = processor.selectedModule();
    processor.setSelectedBand(b);
    processor.setParameterValue(SidechainProcessor::moduleId(m, "multi"), b == 0 ? 0 : 1);
    selectedShape.clear();
    processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged(true));
    bindControls();
    refresh();
}
void SidechainEditor::reorderModule(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= sidechain::moduleCount || to >= sidechain::moduleCount)
        return;
    const auto s = processor.currentSettings();
    std::array<int, sidechain::moduleCount> order{};
    for (int i = 0; i < sidechain::moduleCount; ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return s.module[a].order < s.module[b].order; });
    const auto first = std::find(order.begin(), order.end(), from), last = std::find(order.begin(), order.end(), to);
    if (first < last)
        std::rotate(first, first + 1, last + 1);
    else
        std::rotate(last, first, first + 1);
    for (int i = 0; i < sidechain::moduleCount; ++i)
        processor.setParameterValue(SidechainProcessor::moduleId(order[i], "order"), static_cast<float>(i));
    layoutCanvas();
    refresh();
}
void SidechainEditor::rememberCurve()
{
    const int m = processor.selectedModule(), b = processor.selectedBand();
    const auto s = processor.currentSettings();
    auto& list = past[m][b];
    list.push_back({s.module[m].band[b].curve, s.module[m].band[b].stepped});
    if (list.size() > 40)
        list.erase(list.begin());
    future[m][b].clear();
}
void SidechainEditor::undoCurve(bool isRedo)
{
    const int m = processor.selectedModule(), b = processor.selectedBand();
    auto& source = isRedo ? future[m][b] : past[m][b];
    auto& destination = isRedo ? past[m][b] : future[m][b];
    if (source.empty())
        return;
    const auto s = processor.currentSettings();
    destination.push_back({s.module[m].band[b].curve, s.module[m].band[b].stepped});
    const auto restore = source.back();
    source.pop_back();
    processor.applyCurve(m, b, restore.points, restore.stepped);
    selectedShape.clear();
    refresh();
}
void SidechainEditor::applyShape(const juce::String& name)
{
    rememberCurve();
    const bool held = name == "CHOP" || name == "STAIR" || name == "PULSE";
    processor.applyCurve(processor.selectedModule(), processor.selectedBand(), makeShape(name), held);
    selectedShape = name;
    refresh();
}
void SidechainEditor::refresh()
{
    const auto s = processor.currentSettings();
    const int m = processor.selectedModule();
    int b = processor.selectedBand();
    if (s.module[m].multiband && b == 0)
    {
        b = 1;
        processor.setSelectedBand(b);
    }
    else if (!s.module[m].multiband && b != 0)
    {
        b = 0;
        processor.setSelectedBand(b);
    }
    bindControls();
    masterEnabled.setToggleState(s.enabled, juce::dontSendNotification);
    masterEnabled.setButtonText(s.enabled ? "ACTIVE" : "BYPASSED");
    shaperEnabled.setToggleState(s.module[m].enabled, juce::dontSendNotification);
    shaperEnabled.setButtonText(s.module[m].enabled ? "SHAPER ON" : "SHAPER OFF");
    stepDraw.setToggleState(processor.stepDraw(), juce::dontSendNotification);
    stepDraw.setButtonText(processor.stepDraw() ? "STEP DRAW  ON" : "STEP DRAW  OFF");
    pointTool.setToggleState(!processor.stepDraw(), juce::dontSendNotification);
    oneShot.setToggleState(s.module[m].oneShot, juce::dontSendNotification);
    keyOverlay.setToggleState(showKey, juce::dontSendNotification);
    for (int i = 0; i < sidechain::moduleCount; ++i)
    {
        rack[i]->setToggleState(i == m, juce::dontSendNotification);
        rack[i]->getProperties().set("enabled", s.module[i].enabled);
    }
    for (int i = 0; i < 4; ++i)
        bands[i].setToggleState(i == b, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i)
    {
        sources[i].setToggleState(i == s.module[m].source, juce::dontSendNotification);
        bankTabs[i].setToggleState(i == bank, juce::dontSendNotification);
    }
    if (selectedShape.isNotEmpty())
    {
        const auto expected = makeShape(selectedShape);
        const auto& lane = s.module[m].band[b];
        const bool held = selectedShape == "CHOP" || selectedShape == "STAIR" || selectedShape == "PULSE";
        if (lane.stepped != held)
            selectedShape.clear();
        else
            for (int i = 0; i < sidechain::stepCount; ++i)
                if (std::abs(lane.curve[static_cast<std::size_t>(i)] - expected[static_cast<std::size_t>(i)]) > .0011f)
                {
                    selectedShape.clear();
                    break;
                }
    }
    int presetId = 0;
    for (int i = 0; i < preset.getNumItems(); ++i)
        if (preset.getItemText(i) == selectedShape)
        {
            presetId = preset.getItemId(i);
            break;
        }
    preset.setSelectedId(presetId, juce::dontSendNotification);
    for (int i = 0; i < 8; ++i)
    {
        shapes[i].setButtonText(bankNames[bank][i]);
        shapes[i].setToggleState(selectedShape == bankNames[bank][i], juce::dontSendNotification);
    }
    if (curve)
        curve->repaint();
    if (canvas)
        canvas->repaint();
}
void SidechainEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff100a0b));
}
void SidechainEditor::paintCanvas(juce::Graphics& g)
{
    const auto s = processor.currentSettings();
    const int m = processor.selectedModule(), b = processor.selectedBand();
    const auto& ms = s.module[m];
    const auto& bs = ms.band[b];
    g.fillAll(red);
    g.setColour(juce::Colour(0xfff76650));
    g.fillRect(0, 0, width, 4);
    label(g, "SIDECHAIN", 69, 15, 205, 27, 24, primary, juce::Justification::centredLeft, true);
    mono(g, "VANTA / MULTIBAND SHAPER", 69, 43, 205, 13, 9, secondary);
    mono(g, "SHAPE PRESET", 300, 13, 200, 14, 9, secondary);
    mono(g, "MASTER MIX", 925, 13, 170, 14, 9, secondary);
    mono(g, juce::String(s.masterMix * 100, 0) + "%", 1082, 38, 45, 17, 9, primary, juce::Justification::centredRight);
    panel(g, 14, 81, 1272, 94, juce::Colour(0xff0e090a), juce::Colour(0xffc04d3e));
    mono(g, "01 / SHAPER RACK", 27, 88, 290, 17, 10, primary);
    mono(g, "CLICK TO EDIT  /  DRAG TO REORDER", 950, 88, 321, 17, 9, muted, juce::Justification::centredRight);
    panel(g, 14, 182, 340, 126, surface);
    mono(g, "FREQUENCY BANDS", 25, 190, 200, 17, 10, primary);
    mono(g, "3-BAND FOCUS", 235, 190, 106, 17, 9, muted, juce::Justification::centredRight);
    panel(g, 25, 213, 318, 46, dark);
    const int starts[] = {26, 126, 236}, widths[] = {99, 109, 106};
    for (int bandIndex = 0; bandIndex < 3; ++bandIndex)
    {
        const bool selected = b == 0 || b == bandIndex + 1;
        g.setColour(selected ? accent.withAlpha(.16f) : juce::Colours::transparentBlack);
        g.fillRect(starts[bandIndex], 214, widths[bandIndex], 44);
        g.setColour(selected ? accent.withAlpha(.8f) : muted.withAlpha(.3f));
        g.fillRect(starts[bandIndex] + 5, 252, widths[bandIndex] - 10, 2);
    }
    mono(g, "LOW", 34, 217, 60, 14, 9, muted);
    mono(g, "MID", 175, 217, 60, 14, 9, muted);
    mono(g, "HIGH", 278, 217, 54, 14, 9, muted, juce::Justification::centredRight);
    g.setColour(juce::Colour(0xff673d32));
    g.drawVerticalLine(125, 213, 259);
    g.drawVerticalLine(235, 213, 259);
    mono(g, "180 Hz / 2.5 kHz", 235, 279, 108, 14, 8, muted, juce::Justification::centredRight);
    panel(g, 361, 182, 925, 126, surface);
    label(g, moduleNames[m], 374, 194, 200, 23, 15, primary, juce::Justification::centredLeft, true);
    label(g, descriptions[m], 374, 220, 210, 16, 10, secondary);
    for (int i = 0; i < 3; ++i)
    {
        const int x = 598 + i * 224;
        panel(g, x, 193, 216, 105, dark);
        mono(g, controlNames[m][i], x + 10, 201, 130, 15, 9, muted);
        float v = i == 0 ? bs.amount : i == 1 ? bs.detail : bs.mix;
        juce::String text = juce::String(v * 100, 0) + "%";
        if (m == sidechain::Volume && i == 1)
            text = juce::String((v - .5f) * 48, 0) + " dB";
        if (m == sidechain::Compressor && i == 0)
            text = juce::String(-48 + v * 48, 0) + " dB";
        if (m == sidechain::Compressor && i == 1)
            text = juce::String(1 + v * 19, 1) + ":1";
        mono(g, text, x + 138, 201, 68, 15, 9, primary, juce::Justification::centredRight);
        mono(g, "PER BAND", x + 10, 277, 110, 12, 8, muted);
    }
    panel(g, 14, 315, 1005, 392, surface);
    g.setColour(juce::Colour(0xff211213));
    g.fillRect(15, 316, 1003, 39);
    mono(g, "02 / SHAPE EDITOR", 26, 326, 220, 16, 10, primary);
    mono(g,
         b == 0   ? "FULL BAND"
         : b == 1 ? "LOW BAND"
         : b == 2 ? "MID BAND"
                  : "HIGH BAND",
         248, 326, 110, 16, 9, secondary);
    mono(g, processor.stepDraw() ? "DRAG UP / DOWN ACROSS STEPS" : "POINT EDIT / DRAG A HANDLE", 560, 326, 300, 16, 9,
         muted, juce::Justification::centredRight);
    mono(g, "LENGTH", 25, 662, 49, 14, 9, muted);
    mono(g, "SMOOTH", 770, 662, 56, 14, 9, muted);
    mono(g, juce::String(ms.smooth * 100, 0) + "%", 974, 662, 35, 14, 9, secondary, juce::Justification::centredRight);
    const int selected = curve ? curve->selected() : 0;
    mono(g,
         (processor.stepDraw() ? "STEP DRAW" : "POINT EDIT") + juce::String("  /  ") +
             juce::String(selected + 1).paddedLeft('0', 2) + "  /  " + juce::String(bs.curve[selected] * 100, 0) + "%",
         420, 664, 275, 14, 9, secondary);
    panel(g, 1027, 315, 259, 392, surface);
    mono(g, "03 / TRIGGER", 1041, 326, 142, 16, 10, primary);
    mono(g, "PER SHAPER", 1180, 326, 91, 16, 9, muted, juce::Justification::centredRight);
    panel(g, 1041, 397, 230, 53, dark);
    mono(g,
         ms.source == 0   ? "EXTERNAL INPUT"
         : ms.source == 1 ? "HOST TEMPO"
                          : "MIDI NOTES",
         1052, 403, 185, 15, 10, primary);
    mono(g,
         ms.source == 0   ? "LINK A KICK OR OTHER TRACK"
         : ms.source == 1 ? "FOLLOWS THE HOST CLOCK"
                          : "NOTE-ON RETRIGGERS",
         1052, 424, 200, 13, 9, muted);
    mono(g, "THRESHOLD", 1042, 461, 120, 14, 9, muted);
    mono(g, juce::String(ms.thresholdDb, 0) + " dB", 1199, 461, 72, 14, 9, primary, juce::Justification::centredRight);
    mono(g, "ATTACK", 1042, 511, 120, 14, 9, muted);
    mono(g, juce::String(ms.attackMs, 0) + " ms", 1199, 511, 72, 14, 9, primary, juce::Justification::centredRight);
    mono(g, "RELEASE", 1042, 561, 120, 14, 9, muted);
    mono(g, juce::String(ms.releaseMs, 0) + " ms", 1199, 561, 72, 14, 9, primary, juce::Justification::centredRight);
    mono(g, "KEY LEVEL", 1042, 612, 100, 14, 9, muted);
    panel(g, 1121, 614, 150, 8, dark);
    g.setColour(accent);
    g.fillRect(1122, 615, static_cast<int>(148 * std::clamp(processor.keyLevel(), 0.0f, 1.0f)), 6);
    panel(g, 14, 714, 1272, 104, surface);
    mono(g, "04 / QUICK SHAPES", 25, 724, 185, 15, 10, primary);
    mono(g, "ORIGINAL EXAMPLE CURVES", 1001, 724, 270, 15, 9, muted, juce::Justification::centredRight);
    mono(g, "VANTA / SIDECHAIN", 19, 827, 350, 15, 9, secondary);
    mono(g, "EXTERNAL KEY  /  MULTIBAND  /  MOTION FX", 850, 827, 430, 15, 9, secondary,
         juce::Justification::centredRight);
}
void SidechainEditor::layoutCanvas()
{
    logo.setBounds(20, 15, 38, 38);
    preset.setBounds(300, 31, 320, 30);
    masterMix.setBounds(925, 32, 150, 26);
    masterEnabled.setBounds(1135, 21, 98, 34);
    help.setBounds(1240, 21, 34, 34);
    const auto s = processor.currentSettings();
    std::array<int, sidechain::moduleCount> order{};
    for (int i = 0; i < sidechain::moduleCount; ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return s.module[a].order < s.module[b].order; });
    for (int i = 0; i < sidechain::moduleCount; ++i)
        rack[order[i]]->setBounds(26 + i * 104, 118, 99, 44);
    for (int i = 0; i < 4; ++i)
        bands[i].setBounds(25 + i * 51, 272, 48, 26);
    shaperEnabled.setBounds(374, 255, 112, 25);
    for (int i = 0; i < 3; ++i)
        params[i].setBounds(608 + i * 224, 236, 196, 25);
    stepDraw.setBounds(870, 321, 134, 27);
    pointTool.setBounds(20, 363, 39, 26);
    undo.setBounds(20, 393, 39, 26);
    redo.setBounds(20, 423, 39, 26);
    reset.setBounds(20, 453, 39, 26);
    if (curve)
        curve->setBounds(64, 360, 940, 280);
    length.setBounds(77, 653, 115, 29);
    oneShot.setBounds(199, 653, 87, 29);
    keyOverlay.setBounds(293, 653, 112, 29);
    smooth.setBounds(827, 653, 142, 29);
    for (int i = 0; i < 3; ++i)
        sources[i].setBounds(1041 + i * 78, 357, 74, 29);
    threshold.setBounds(1041, 478, 230, 23);
    attack.setBounds(1041, 528, 230, 23);
    release.setBounds(1041, 578, 230, 23);
    for (int i = 0; i < 3; ++i)
        bankTabs[i].setBounds(208 + i * 78, 720, 73, 23);
    for (int i = 0; i < 8; ++i)
        shapes[i].setBounds(25 + i * 156, 750, 150, 55);
    if (overlay)
        overlay->setBounds(canvas->getLocalBounds());
}
int SidechainEditor::currentScale() const noexcept
{
    return juce::roundToInt(
        100.0 * std::min(static_cast<double>(getWidth()) / width, static_cast<double>(getHeight()) / height));
}
int SidechainEditor::fitScale() const
{
    if (getPeer() == nullptr || getWidth() <= 0)
        return 100;
    const auto bounds = getScreenBounds();
    const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(bounds);
    if (display == nullptr || bounds.getWidth() <= 0)
        return 100;
    const double hostScale = static_cast<double>(bounds.getWidth()) / getWidth();
    const auto area = display->userArea;
    const double fit = std::min(std::max(1, area.getWidth() - 32) / (width * hostScale),
                                std::max(1, area.getHeight() - 96) / (height * hostScale));
    return std::clamp(static_cast<int>(std::floor(std::clamp(fit, .6, 1.5) * 20)) * 5, 60, 150);
}
void SidechainEditor::setUiScale(int percent, bool notify)
{
    percent = std::clamp(percent, 60, 150);
    const int old = processor.editorScalePercent(), w = juce::roundToInt(width * percent / 100.0),
              h = juce::roundToInt(height * percent / 100.0);
    hostResizeRejected = false;
    awaitingHostResize = (w != getWidth() || h != getHeight()) &&
                         processor.wrapperType == juce::AudioProcessor::wrapperType_VST3 &&
                         getParentComponent() != nullptr && (notify || getPeer() != nullptr);
    resizeRequestFromUser = notify;
    resizeRequestTime = juce::Time::getMillisecondCounter();
    lastScale = percent;
    processor.setEditorScalePercent(percent);
    setSize(w, h);
    if (notify && old != processor.editorScalePercent())
        processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged(true));
}
void SidechainEditor::reconcileHostResize()
{
    if (!awaitingHostResize || juce::Time::getMillisecondCounter() - resizeRequestTime < 1000)
        return;
    awaitingHostResize = false;
    auto* parent = getParentComponent();
    if (parent == nullptr)
        return;
    const auto actual = parent->getLocalBounds(), requested = parent->getLocalArea(this, getLocalBounds());
    if (std::abs(requested.getWidth() - actual.getWidth()) <= 2 &&
        std::abs(requested.getHeight() - actual.getHeight()) <= 2)
        return;
    const auto accepted = getLocalArea(parent, actual);
    if (accepted.getWidth() <= 0 || accepted.getHeight() <= 0)
        return;
    setSize(accepted.getWidth(), accepted.getHeight());
    if (resizeRequestFromUser)
    {
        hostResizeRejected = true;
        showResizeMenu(true);
    }
}
void SidechainEditor::showResizeMenu(bool show)
{
    if (!overlay)
        return;
    if (show)
    {
        overlay->setBounds(canvas->getLocalBounds());
        overlay->updateSelection(currentScale());
        overlay->setVisible(true);
        overlay->toFront(true);
        overlay->updatePlacement();
        overlay->grabKeyboardFocus();
    }
    else
    {
        overlay->setVisible(false);
        if (logo.isShowing())
            logo.grabKeyboardFocus();
    }
}
void SidechainEditor::resized()
{
    if (!canvas)
        return;
    const float scale =
        std::max(.001f, std::min(static_cast<float>(getWidth()) / width, static_cast<float>(getHeight()) / height));
    canvas->setTransform(juce::AffineTransform::scale(scale).translated((getWidth() - width * scale) * .5f,
                                                                        (getHeight() - height * scale) * .5f));
    layoutCanvas();
    if (getWidth() != lastWidth || getHeight() != lastHeight)
    {
        lastWidth = getWidth();
        lastHeight = getHeight();
        processor.setEditorScalePercent(currentScale());
        lastScale = processor.editorScalePercent();
        if (overlay && overlay->isVisible())
            overlay->updateSelection(currentScale());
    }
}
void SidechainEditor::timerCallback()
{
    if (const int restored = processor.editorScalePercent(); restored != lastScale)
        setUiScale(restored, false);
    reconcileHostResize();
    if (overlay && overlay->isVisible())
        overlay->updatePlacement();
    if (curve)
        curve->addMeterSample(processor.keyLevel());
    refresh();
}
