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
    void resized() override;
    std::function<void()> onValueChanged;

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class VantaEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VantaEditor(VantaProcessor&);
    ~VantaEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void configureButton(juce::TextButton&, const juce::String&, const juce::String& role = {});
    void selectPage(int);
    void refresh();
    void changePreset(int);
    void loadFactory(int);
    void openPreset();
    void savePreset();
    void showMessage(const juce::String&, const juce::String&, bool warning);

    VantaProcessor& processor;
    VantaLook look;
    int page = 0, folder = 0, currentFactory = -1;
    bool browserOpen = false;
    juce::String lastPresetName;

    juce::TextButton previous, next, browserButton, writeButton, importButton;
    std::array<juce::TextButton, 5> tabs;
    std::array<juce::TextButton, 4> folders;
    std::array<juce::TextButton, VantaProcessor::factoryPresetCount> presetButtons;
    juce::Slider levelSlider;
    juce::ComboBox waveform;
    std::array<VantaKnob, 8> mainKnobs;
    std::array<VantaKnob, 9> effectKnobs;
    std::array<VantaKnob, 6> modulationKnobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttachment;
    std::unique_ptr<juce::MidiKeyboardComponent> standaloneKeyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VantaEditor)
};
