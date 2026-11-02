#pragma once

#include "PluginProcessor.h"

class VantaLook final : public juce::LookAndFeel_V4
{
public:
    VantaLook();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
};

class VantaKnob final : public juce::Component
{
public:
    void configure(juce::AudioProcessorValueTreeState&, vanta::Parameter, const juce::String& labelText);
    void setCaption(const juce::String&);
    void resized() override;
    std::function<void()> onValueChanged;

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class VantaKeyboard final : public juce::MidiKeyboardComponent
{
public:
    explicit VantaKeyboard(juce::MidiKeyboardState&);
    ~VantaKeyboard() override;
    void drawBlackNote(int, juce::Graphics&, juce::Rectangle<float>, bool, bool, juce::Colour) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool) override;
    void focusLost(FocusChangeType) override;
    // Converts a physical-key snapshot into MIDI transitions; also used by tests.
    bool updateComputerKeys(const std::array<bool, 17>&, bool shortcutHeld);

private:
    void releaseComputerKeys();
    juce::MidiKeyboardState& midiState;
    std::array<bool, 17> computerKeysDown {};
};

class VantaEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VantaEditor(VantaProcessor&);
    ~VantaEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    class Canvas;
    class ResizeOverlay;
    int baseHeight() const noexcept;
    int currentScalePercent() const noexcept;
    int fitScalePercent() const;
    void setUiScale(int percent, bool notifyHost);
    void reconcileHostResize();
    void positionStandaloneWindowOnce();
    void showResizeMenu(bool show);
    void paintCanvas(juce::Graphics&);
    void layoutCanvas();
    void timerCallback() override;
    void configureButton(juce::TextButton&, const juce::String&, const juce::String& role = {});
    void selectPage(int);
    void refresh();
    void setParameter(vanta::Parameter, float);
    void setWaveform(int);
    void changePreset(int);
    void loadFactory(int);
    void openPreset();
    void savePreset();
    void showMessage(const juce::String&, const juce::String&, bool warning);

    VantaProcessor& processor;
    VantaLook look;
    std::unique_ptr<Canvas> canvas;
    std::unique_ptr<ResizeOverlay> resizeOverlay;
    juce::TextButton logoButton;
    int lastObservedScale = vanta::defaultEditorScale, lastObservedWidth = 0, lastObservedHeight = 0;
    bool awaitingHostResize = false, resizeRequestFromUser = false, hostResizeRejected = false;
    bool needsStandalonePosition = true;
    juce::uint32 resizeRequestTime = 0;
    int page = 0, folder = 0, currentFactory = -1, selectedOsc = 0, lastOscMask = -1, selectedFx = 0;
    bool browserOpen = false;
    juce::String lastPresetName;

    juce::TextButton previous, next, browserButton, writeButton, importButton;
    std::array<juce::TextButton, 7> tabs;
    std::array<juce::TextButton, 4> folders;
    std::array<juce::TextButton, VantaProcessor::factoryPresetCount> presetButtons;
    juce::TextEditor presetSearch;
    juce::Slider levelSlider;
    // The preview's small controls are real duplicate views of these parameters.
    std::array<juce::Slider, 3> mainMiniSliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 3> mainMiniAttachments;
    juce::TextButton editEnvelope, editVibrato;
    juce::ComboBox waveform;
    std::array<juce::TextButton, 4> waveButtons;
    std::array<juce::Slider, vanta::oscillatorCount> tablePositions;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, vanta::oscillatorCount> tablePositionAttachments;
    std::array<juce::TextButton, 9> effectSelectors, effectEnabled;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 9> effectEnabledAttachments;
    std::array<std::array<VantaKnob, 4>, 9> effectKnobs;
    std::array<juce::TextButton, vanta::oscillatorCount> oscillatorTabs;
    juce::TextButton addOscillator, removeOscillator;
    std::array<std::array<VantaKnob, 4>, vanta::oscillatorCount> oscillatorKnobs;
    std::array<juce::TextButton, vanta::oscillatorCount> mixerEnabled;
    std::array<juce::Slider, vanta::oscillatorCount> mixerLevel, mixerPan;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, vanta::oscillatorCount> mixerEnabledAttachments;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, vanta::oscillatorCount> mixerLevelAttachments, mixerPanAttachments;
    std::array<VantaKnob, 8> mainKnobs;
    std::array<VantaKnob, 6> modulationKnobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    std::unique_ptr<VantaKeyboard> standaloneKeyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ThreadPool importWorkers { 1 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VantaEditor)
};
