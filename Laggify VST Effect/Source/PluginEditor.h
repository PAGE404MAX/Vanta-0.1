#pragma once
#include "PluginProcessor.h"

class LaggifyLook final : public juce::LookAndFeel_V4
{
  public:
    LaggifyLook();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle,
                          juce::Slider&) override;
};

class LaggifyEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit LaggifyEditor(LaggifyProcessor&);
    ~LaggifyEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

  private:
    class Canvas;
    class CurveEditor;
    class ResizeOverlay;
    void paintCanvas(juce::Graphics&);
    void layoutCanvas();
    void refresh();
    void selectMode(bool time);
    void showResizeMenu(bool);
    void setUiScale(int, bool);
    int currentScale() const noexcept;
    int fitScale() const;
    void reconcileHostResize();
    void timerCallback() override;
    void setupButton(juce::TextButton&, const juce::String&, const juce::String& role);

    static constexpr int width = 1000, height = 820;
    LaggifyProcessor& processor;
    LaggifyLook look;
    std::unique_ptr<Canvas> canvas;
    std::unique_ptr<CurveEditor> curve;
    std::unique_ptr<ResizeOverlay> overlay;
    juce::TextButton logo, timeMode, volumeMode, enabled, timeTool, volumeTool, lineTool, stepTool, reset;
    std::array<juce::TextButton, laggify::bankSize> timePresets, volumePresets;
    juce::ComboBox cycle;
    juce::Slider bpm, mix;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cycleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment, mixAttachment;
    int lastScale = 100, lastWidth = 0, lastHeight = 0;
    bool awaitingHostResize = false, resizeRequestFromUser = false, hostResizeRejected = false;
    juce::uint32 resizeRequestTime = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LaggifyEditor)
};
