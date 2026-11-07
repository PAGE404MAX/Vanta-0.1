#pragma once
#include "PluginProcessor.h"
#include <array>
#include <vector>

class SidechainLook final : public juce::LookAndFeel_V4
{
  public:
    SidechainLook();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle,
                          juce::Slider&) override;
};
class SidechainEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit SidechainEditor(SidechainProcessor&);
    ~SidechainEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

  private:
    class Canvas;
    class CurveEditor;
    class ResizeOverlay;
    class RackButton;
    struct Snapshot
    {
        std::array<float, sidechain::stepCount> points{};
        bool stepped = false;
    };
    using History = std::array<std::array<std::vector<Snapshot>, sidechain::bandCount>, sidechain::moduleCount>;
    void paintCanvas(juce::Graphics&);
    void layoutCanvas();
    void refresh();
    void bindControls();
    void selectModule(int);
    void selectBand(int);
    void reorderModule(int, int);
    void rememberCurve();
    void undoCurve(bool redo);
    void applyShape(const juce::String&);
    void showResizeMenu(bool);
    void setUiScale(int, bool);
    int currentScale() const noexcept;
    int fitScale() const;
    void reconcileHostResize();
    void timerCallback() override;
    void setupButton(juce::TextButton&, const juce::String&, const char* role, const juce::String& id);
    static constexpr int width = 1300, height = 850;
    SidechainProcessor& processor;
    SidechainLook look;
    std::unique_ptr<Canvas> canvas;
    std::unique_ptr<CurveEditor> curve;
    std::unique_ptr<ResizeOverlay> overlay;
    std::array<std::unique_ptr<RackButton>, sidechain::moduleCount> rack;
    std::array<juce::TextButton, sidechain::bandCount> bands;
    std::array<juce::TextButton, 3> sources, bankTabs;
    std::array<juce::TextButton, 8> shapes;
    juce::TextButton logo, masterEnabled, help, shaperEnabled, stepDraw, pointTool, undo, redo, reset, oneShot,
        keyOverlay;
    juce::ComboBox length, preset;
    juce::Slider masterMix, smooth, threshold, attack, release;
    std::array<juce::Slider, 3> params;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAttachment> masterMixAttachment, smoothAttachment, thresholdAttachment, attackAttachment,
        releaseAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 3> paramAttachments;
    std::unique_ptr<ComboAttachment> lengthAttachment;
    History past, future;
    juce::String selectedShape = "DEEP";
    int bank = 0, lastScale = 100, lastWidth = 0, lastHeight = 0, boundModule = -1, boundBand = -1;
    bool showKey = true, awaitingHostResize = false, resizeRequestFromUser = false, hostResizeRejected = false;
    juce::uint32 resizeRequestTime = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidechainEditor)
};
