#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
const juce::Colour red(0xff8e171b), border(0xff61302a), surface(0xff1a1010), dark(0xff120d0e);
const juce::Colour primary(0xffffe5d3), secondary(0xffd3aa9b), muted(0xff9b796e), accent(0xffff7658), gold(0xffffc36a);
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
LaggifyLook::LaggifyLook()
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
void LaggifyLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
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
void LaggifyLook::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    const auto role = b.getProperties()["role"].toString();
    if (role == "logo" || role == "line" || role == "step")
        return;
    const bool bank = role == "time" || role == "volume";
    const float size = bank ? 8.0f : role == "close" ? 14.0f : role == "mode" || role == "volmode" ? 10.0f : 9.0f;
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
void LaggifyLook::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                   juce::Slider::SliderStyle, juce::Slider&)
{
    const float centre = static_cast<float>(y) + h * .5f;
    g.setColour(juce::Colour(0xff51312a));
    g.fillRoundedRectangle(static_cast<float>(x), centre - 2, static_cast<float>(w), 4, 2);
    g.setColour(accent);
    g.fillRoundedRectangle(static_cast<float>(x), centre - 2, std::max(0.0f, pos - x), 4, 2);
    g.fillEllipse(pos - 5, centre - 5, 10, 10);
}

class LaggifyEditor::Canvas final : public juce::Component
{
  public:
    explicit Canvas(LaggifyEditor& e) : owner(e)
    {
        setComponentID("laggify-canvas");
    }
    void paint(juce::Graphics& g) override
    {
        owner.paintCanvas(g);
    }

  private:
    LaggifyEditor& owner;
};
class LaggifyEditor::CurveEditor final : public juce::Component
{
  public:
    explicit CurveEditor(LaggifyEditor& e) : owner(e)
    {
        setComponentID("laggify-curve");
        setTitle("Time or volume curve: drag or use arrow keys");
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
    void paint(juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        g.setColour(dark);
        g.fillRect(area);
        g.setColour(border);
        g.drawRect(area.reduced(.5f), 1);
        const auto r = area.reduced(9.0f);
        const auto values = owner.processor.currentSettings();
        const bool time = owner.processor.editTime();
        const auto& points = time ? values.time : values.volume;
        const bool smooth = time ? values.timeSmooth : values.volumeSmooth;
        g.saveState();
        g.reduceClipRegion(area.reduced(1).toNearestInt());
        for (int i = 0; i <= 16; ++i)
        {
            const float x = r.getX() + r.getWidth() * i / 16.0f;
            g.setColour(i % 4 == 0 ? juce::Colour(0xffaa6753) : juce::Colour(0xff70443d));
            g.drawLine(x, r.getY(), x, r.getBottom(), i % 4 == 0 ? .8f : .6f);
        }
        for (int i = 1; i < 8; ++i)
        {
            const float y = r.getY() + r.getHeight() * i / 8.0f;
            g.setColour(i % 2 == 0 ? juce::Colour(0xffaa6753) : juce::Colour(0xff70443d));
            g.drawLine(r.getX(), y, r.getRight(), y, i % 2 == 0 ? .8f : .6f);
        }
        g.setColour(juce::Colour(0xff84594b).withAlpha(.45f));
        if (time)
            g.drawLine(r.getX(), r.getY(), r.getRight(), r.getBottom(), .7f);
        else
            g.drawLine(r.getX(), r.getY(), r.getRight(), r.getY(), .7f);
        const auto play = std::clamp(owner.processor.playheadPhase(), 0.0f, 1.0f);
        g.setColour(gold.withAlpha(.48f));
        g.drawLine(r.getX() + r.getWidth() * play, r.getY(), r.getX() + r.getWidth() * play, r.getBottom(), .8f);
        if (hasKeyboardFocus(false))
        {
            g.setColour(primary.withAlpha(.07f));
            g.fillRect(r.getX() + r.getWidth() * selectedStep / 16.0f, r.getY(), r.getWidth() / 16.0f, r.getHeight());
        }
        auto px = [&](int s) { return r.getX() + r.getWidth() * s / 16.0f; };
        auto py = [&](float v) { return r.getY() + r.getHeight() * (time ? v : 1.0f - v); };
        juce::Path path;
        path.startNewSubPath(px(0), py(points[0]));
        for (int i = 1; i < 16; ++i)
        {
            const bool line = smooth && !(points[i - 1] - points[i] > .5f);
            if (!line)
                path.lineTo(px(i), py(points[i - 1]));
            path.lineTo(px(i), py(points[i]));
        }
        path.lineTo(px(16), py(points[15]));
        g.setColour(time ? accent : gold);
        g.strokePath(path, juce::PathStrokeType(.9f));
        g.setColour(time ? juce::Colour(0xffffc39c) : juce::Colour(0xffffe2a4));
        for (int i = 0; i < 16; ++i)
        {
            const float x = px(i), y = py(points[i]);
            g.setColour(dark);
            g.fillEllipse(x - 3, y - 3, 6, 6);
            g.setColour(time ? juce::Colour(0xffffc39c) : juce::Colour(0xffffe2a4));
            g.drawEllipse(x - 3, y - 3, 6, 6, 1.0f);
        }
        g.restoreState();
        mono(g, time ? "LIVE" : "100%", 13, 10, 80, 13, 8, secondary);
        mono(g, time ? "1 CYCLE BACK" : "0%", 13, getHeight() - 22, 120, 13, 8, secondary);
        mono(g, "16 STEPS", getWidth() - 90, 10, 76, 13, 8, muted, juce::Justification::centredRight);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        dragStep = -1;
        change(e.position);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        change(e.position);
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        endGestures();
        dragStep = -1;
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
        if (k.getKeyCode() == juce::KeyPress::homeKey)
        {
            selectedStep = 0;
            repaint();
            return true;
        }
        if (k.getKeyCode() == juce::KeyPress::endKey)
        {
            selectedStep = 15;
            repaint();
            return true;
        }
        if (k.getKeyCode() != juce::KeyPress::upKey && k.getKeyCode() != juce::KeyPress::downKey)
            return false;
        const bool time = owner.processor.editTime();
        const auto s = owner.processor.currentSettings();
        const float current = (time ? s.time : s.volume)[selectedStep];
        const float direction = k.getKeyCode() == juce::KeyPress::upKey ? 1.0f : -1.0f;
        if (auto* p = owner.processor.pointParameter(time, selectedStep))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(
                p->convertTo0to1(std::clamp(current + direction * (time ? -.05f : .05f), 0.0f, 1.0f)));
            p->endChangeGesture();
        }
        owner.processor.markCustom(time);
        owner.refresh();
        return true;
    }

  private:
    void change(juce::Point<float> point)
    {
        const auto r = getLocalBounds().toFloat().reduced(9.0f);
        if (r.getWidth() <= 0 || r.getHeight() <= 0)
            return;
        const float x = std::clamp((point.x - r.getX()) / r.getWidth(), 0.0f, .999999f),
                    y = std::clamp((point.y - r.getY()) / r.getHeight(), 0.0f, 1.0f);
        const bool time = owner.processor.editTime();
        const int to = std::clamp(static_cast<int>(x * 16), 0, 15);
        const float value = time ? y : 1.0f - y;
        const int from = dragStep < 0 ? to : dragStep, distance = std::abs(to - from), direction = to >= from ? 1 : -1;
        for (int i = 0; i <= distance; ++i)
        {
            const int step = from + i * direction;
            const float fraction = distance ? static_cast<float>(i) / distance : 1.0f;
            const float next = dragStep < 0 ? value : dragValue + (value - dragValue) * fraction;
            if (auto* p = owner.processor.pointParameter(time, step))
            {
                if (!gestures[step])
                {
                    p->beginChangeGesture();
                    gestures[step] = true;
                    gestureTime = time;
                }
                p->setValueNotifyingHost(p->convertTo0to1(next));
            }
        }
        dragStep = to;
        dragValue = value;
        selectedStep = to;
        owner.processor.markCustom(time);
        owner.refresh();
    }
    void endGestures()
    {
        for (int i = 0; i < 16; ++i)
            if (gestures[i])
            {
                if (auto* p = owner.processor.pointParameter(gestureTime, i))
                    p->endChangeGesture();
                gestures[i] = false;
            }
    }
    LaggifyEditor& owner;
    int selectedStep = 0, dragStep = -1;
    float dragValue = 0;
    bool gestureTime = true;
    std::array<bool, 16> gestures{};
};
class LaggifyEditor::ResizeOverlay final : public juce::Component
{
  public:
    explicit ResizeOverlay(LaggifyEditor& e) : owner(e)
    {
        setComponentID("resize-overlay");
        setTitle("Laggify interface size");
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
        label(g, "LAGGIFY / DISPLAY", r.getX() + 22, r.getY() + 20, 230, 17, 10, accent,
              juce::Justification::centredLeft, true);
        label(g, "INTERFACE SIZE", r.getX() + 22, r.getY() + 41, 290, 26, 19, primary, juce::Justification::centredLeft,
              true);
        mono(g, "CURRENT " + juce::String(owner.currentScale()) + "%", r.getX() + 282, r.getY() + 46, 105, 18, 9,
             secondary, juce::Justification::centredRight);
        mono(g,
             owner.hostResizeRejected ? "Host did not accept that size. Try another."
                                      : "Choose a size or fit Laggify to your screen.",
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
    LaggifyEditor& owner;
    juce::Rectangle<int> dialog;
    std::array<juce::TextButton, 7> sizes;
    juce::TextButton fit, close;
};

LaggifyEditor::LaggifyEditor(LaggifyProcessor& p) : AudioProcessorEditor(p), processor(p)
{
    setLookAndFeel(&look);
    canvas = std::make_unique<Canvas>(*this);
    canvas->setSize(width, height);
    addAndMakeVisible(*canvas);
    auto add = [this](juce::TextButton& b, const juce::String& text, const juce::String& role, const char* id)
    {
        setupButton(b, text, role);
        b.setComponentID(id);
    };
    add(logo, "", "logo", "resize-logo");
    logo.setTitle("Resize Laggify interface");
    logo.setTooltip("Open interface size options");
    logo.onClick = [this] { showResizeMenu(true); };
    add(timeMode, "TIME", "mode", "mode-time");
    timeMode.setTitle("Edit time curve");
    timeMode.onClick = [this] { selectMode(true); };
    add(volumeMode, "VOLUME", "volmode", "mode-volume");
    volumeMode.setTitle("Edit volume curve");
    volumeMode.onClick = [this] { selectMode(false); };
    add(enabled, "LAG OFF", "power", "parameter-enabled");
    enabled.setClickingTogglesState(true);
    enabled.setTitle("Enable Laggify");
    enabledAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state, "enabled", enabled);
    add(timeTool, "T", "tool", "tool-time");
    timeTool.setTooltip("Edit time curve");
    timeTool.onClick = [this] { selectMode(true); };
    add(volumeTool, "V", "voltool", "tool-volume");
    volumeTool.setTooltip("Edit volume curve");
    volumeTool.onClick = [this] { selectMode(false); };
    add(lineTool, "", "line", "tool-line");
    lineTool.setTitle("Interpolate points");
    lineTool.setTooltip("Connect points with lines");
    lineTool.onClick = [this]
    {
        processor.setParameterValue(processor.editTime() ? "timeSmooth" : "volumeSmooth", 1);
        refresh();
    };
    add(stepTool, "", "step", "tool-step");
    stepTool.setTitle("Hold each step");
    stepTool.setTooltip("Hold each step");
    stepTool.onClick = [this]
    {
        processor.setParameterValue(processor.editTime() ? "timeSmooth" : "volumeSmooth", 0);
        refresh();
    };
    add(reset, juce::String::fromUTF8("\xe2\x86\xb6"), "tool", "tool-reset");
    reset.setTitle("Reset active curve");
    reset.onClick = [this]
    {
        processor.applyPattern(processor.editTime(), 0);
        refresh();
    };
    for (int i = 0; i < laggify::bankSize; ++i)
    {
        auto& t = timePresets[static_cast<std::size_t>(i)];
        setupButton(t, laggify::timeNames[i], "time");
        t.setComponentID("time-preset-" + juce::String(i));
        t.setTitle("Time: " + juce::String(laggify::timeNames[i]));
        t.onClick = [this, i]
        {
            processor.applyPattern(true, i);
            refresh();
        };
        auto& v = volumePresets[static_cast<std::size_t>(i)];
        setupButton(v, laggify::volumeNames[i], "volume");
        v.setComponentID("volume-preset-" + juce::String(i));
        v.setTitle("Volume: " + juce::String(laggify::volumeNames[i]));
        v.onClick = [this, i]
        {
            processor.applyPattern(false, i);
            refresh();
        };
    }
    cycle.setComponentID("parameter-cycle");
    cycle.setTitle("Cycle length");
    cycle.addItem("1/2 BAR", 1);
    cycle.addItem("1 BAR", 2);
    cycle.addItem("2 BARS", 3);
    canvas->addAndMakeVisible(cycle);
    cycleAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state, "cycle", cycle);
    bpm.setComponentID("parameter-bpm");
    bpm.setTitle("Fallback BPM");
    bpm.setTooltip("Tempo used if the host does not provide one");
    bpm.setSliderStyle(juce::Slider::LinearHorizontal);
    bpm.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 22);
    canvas->addAndMakeVisible(bpm);
    bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, "bpm", bpm);
    mix.setComponentID("parameter-mix");
    mix.setTitle("Wet mix");
    mix.setSliderStyle(juce::Slider::LinearHorizontal);
    mix.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    canvas->addAndMakeVisible(mix);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, "mix", mix);
    curve = std::make_unique<CurveEditor>(*this);
    canvas->addAndMakeVisible(*curve);
    overlay = std::make_unique<ResizeOverlay>(*this);
    canvas->addChildComponent(*overlay);
    lastScale = processor.editorScalePercent();
    setSize(juce::roundToInt(width * lastScale / 100.0), juce::roundToInt(height * lastScale / 100.0));
    setResizeLimits(width * 60 / 100, height * 60 / 100, width * 150 / 100, height * 150 / 100);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(width) / height);
    setResizable(true, false);
    refresh();
    startTimerHz(20);
}
LaggifyEditor::~LaggifyEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}
void LaggifyEditor::setupButton(juce::TextButton& b, const juce::String& text, const juce::String& role)
{
    b.setButtonText(text);
    b.getProperties().set("role", role);
    canvas->addAndMakeVisible(b);
}
void LaggifyEditor::selectMode(bool time)
{
    processor.setEditTime(time);
    processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged(true));
    refresh();
}
void LaggifyEditor::refresh()
{
    const auto s = processor.currentSettings();
    const bool time = processor.editTime();
    const int ti = processor.currentPattern(true), vi = processor.currentPattern(false);
    timeMode.setToggleState(time, juce::dontSendNotification);
    volumeMode.setToggleState(!time, juce::dontSendNotification);
    timeTool.setToggleState(time, juce::dontSendNotification);
    volumeTool.setToggleState(!time, juce::dontSendNotification);
    const bool line = time ? s.timeSmooth : s.volumeSmooth;
    lineTool.setToggleState(line, juce::dontSendNotification);
    stepTool.setToggleState(!line, juce::dontSendNotification);
    enabled.setButtonText(s.enabled ? "LAG ON" : "LAG OFF");
    for (int i = 0; i < laggify::bankSize; ++i)
    {
        timePresets[i].setToggleState(i == ti, juce::dontSendNotification);
        volumePresets[i].setToggleState(i == vi, juce::dontSendNotification);
    }
    if (curve)
        curve->repaint();
    if (canvas)
        canvas->repaint();
}
void LaggifyEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff11090a));
}
void LaggifyEditor::paintCanvas(juce::Graphics& g)
{
    g.fillAll(red);
    g.setColour(juce::Colour(0xffd14b3d));
    g.fillRect(0, 0, width, 4);
    label(g, "LAGGIFY", 70, 19, 270, 25, 23, primary, juce::Justification::centredLeft, true);
    mono(g, "VANTA AUDIO / TIME + VOLUME FX", 70, 48, 300, 13, 9, secondary);
    mono(g, "STEREO INPUT / OUTPUT", 663, 30, 165, 17, 9, secondary, juce::Justification::centredRight);
    panel(g, 838, 21, 141, 28, juce::Colour(0xff451413), juce::Colour(0xffb94837));
    mono(g, "VST3 INSERT EFFECT", 845, 22, 128, 26, 8, primary, juce::Justification::centred);
    panel(g, 16, 72, 968, 45, juce::Colour(0xff170d0e), juce::Colour(0xffbd493d));
    mono(g, "TRACK AUDIO IN  >  TIME + VOLUME  >  PROCESSED OUT", 28, 82, 630, 23, 9, primary);
    mono(g, "HOST TEMPO / POSITION", 746, 82, 226, 23, 9, secondary, juce::Justification::centredRight);
    panel(g, 16, 126, 968, 650, juce::Colour(0xff0c0909), juce::Colour(0xffd45a49));
    label(g, "01   TIME + VOLUME / CURVE EDITOR", 32, 136, 450, 21, 10, secondary, juce::Justification::centredLeft,
          true);
    mono(g, "PRESET BANKS / ORIGINAL CURVES", 700, 136, 268, 21, 8, muted, juce::Justification::centredRight);
    g.setColour(juce::Colour(0xff4b1d1d));
    g.drawHorizontalLine(164, 32, 968);
    panel(g, 32, 174, 260, 556, surface);
    panel(g, 292, 174, 32, 556, juce::Colour(0xff160e0e));
    mono(g, "01 / TIME PRESETS", 39, 216, 190, 16, 9, accent);
    mono(g, "36 SLOTS", 233, 216, 52, 16, 8, muted, juce::Justification::centredRight);
    mono(g, "02 / VOLUME PRESETS", 39, 408, 190, 16, 9, gold);
    mono(g, "36 SLOTS", 233, 408, 52, 16, 8, muted, juce::Justification::centredRight);
    panel(g, 39, 602, 246, 68, dark);
    mono(g, "CYCLE", 45, 610, 39, 20, 8, muted);
    mono(g, "BPM*", 174, 610, 34, 20, 8, muted);
    mono(g, "MIX", 45, 638, 37, 20, 8, muted);
    mono(g, juce::String(processor.currentSettings().mix * 100.0f, 0) + "%", 250, 638, 30, 20, 8, secondary,
         juce::Justification::centredRight);
    panel(g, 324, 695, 644, 35, juce::Colour(0xff201011));
    const bool time = processor.editTime();
    const int index = processor.currentPattern(time);
    const auto title = juce::String(time ? "TIME / " : "VOLUME / ") + laggify::patternName(time, index);
    mono(g, title, 333, 702, 230, 20, 10, time ? accent : gold);
    const auto values = processor.currentSettings();
    const int step = curve ? curve->selected() : 0;
    const float value = (time ? values.time : values.volume)[step];
    mono(g,
         "STEP " + juce::String(step + 1).paddedLeft('0', 2) + " / " + juce::String(value * 100.0f, 0) + "%" +
             (time ? " BACK" : ""),
         580, 702, 180, 20, 8, secondary);
    mono(g,
         values.cycle == 0   ? "SYNC 1/2 BAR"
         : values.cycle == 1 ? "SYNC 1 BAR"
                             : "SYNC 2 BARS",
         763, 702, 96, 20, 8, secondary);
    mono(g, "HOST / BPM*", 864, 702, 94, 20, 8, muted, juce::Justification::centredRight);
    mono(g, "DRAW OR SELECT A PRESET / USE ARROW KEYS TO EDIT", 32, 745, 570, 15, 8, muted);
    mono(g, "HOST TEMPO + POSITION / BPM* FALLBACK", 625, 745, 343, 15, 8, muted, juce::Justification::centredRight);
    mono(g, "VANTA AUDIO / LAGGIFY", 18, 790, 390, 16, 9, secondary);
    mono(g, "VST3 MIXER INSERT", 736, 790, 246, 16, 9, secondary, juce::Justification::centredRight);
}
void LaggifyEditor::layoutCanvas()
{
    logo.setBounds(20, 19, 38, 38);
    timeMode.setBounds(39, 181, 183, 30);
    enabled.setBounds(228, 181, 57, 30);
    for (int i = 0; i < laggify::bankSize; ++i)
    {
        const int x = 39 + (i % 4) * 62, y = 236 + (i / 4) * 19;
        timePresets[i].setBounds(x, y, 60, 18);
        volumePresets[i].setBounds(x, y + 192, 60, 18);
    }
    cycle.setBounds(83, 609, 86, 22);
    bpm.setBounds(207, 609, 72, 22);
    mix.setBounds(84, 638, 158, 20);
    volumeMode.setBounds(39, 682, 246, 31);
    timeTool.setBounds(296, 181, 24, 25);
    volumeTool.setBounds(296, 210, 24, 25);
    lineTool.setBounds(296, 245, 24, 25);
    stepTool.setBounds(296, 274, 24, 25);
    reset.setBounds(296, 663, 24, 25);
    if (curve)
        curve->setBounds(324, 174, 644, 521);
    if (overlay)
        overlay->setBounds(canvas->getLocalBounds());
}
int LaggifyEditor::currentScale() const noexcept
{
    return juce::roundToInt(
        100.0 * std::min(static_cast<double>(getWidth()) / width, static_cast<double>(getHeight()) / height));
}
int LaggifyEditor::fitScale() const
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
void LaggifyEditor::setUiScale(int percent, bool notify)
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
void LaggifyEditor::reconcileHostResize()
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
void LaggifyEditor::showResizeMenu(bool show)
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
void LaggifyEditor::mouseDown(const juce::MouseEvent&)
{
    if (overlay && overlay->isVisible())
        showResizeMenu(false);
}
void LaggifyEditor::resized()
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
void LaggifyEditor::timerCallback()
{
    if (const int restored = processor.editorScalePercent(); restored != lastScale)
        setUiScale(restored, false);
    reconcileHostResize();
    if (overlay && overlay->isVisible())
        overlay->updatePlacement();
    refresh();
}
