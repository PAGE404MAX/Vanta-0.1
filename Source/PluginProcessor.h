#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "EditorSizing.h"
#include "Parameters.h"
#include "PresetCodec.h"
#include "SynthEngine.h"

class VantaProcessor final : public juce::AudioProcessor
{
public:
    VantaProcessor();
    ~VantaProcessor() override = default;

    const juce::String getName() const override { return "Vanta Beta 0.11"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 60.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    vanta::ParameterValues currentValues() const;
    void applyValues(const vanta::ParameterValues&, const juce::String& name);
    void loadFactoryPreset(int index);
    void applyPreset(const vanta::PresetResult&);
    bool hasWaveTable(int oscillator) const;
    bool hasSample(int oscillator) const;
    vanta::PresetResult loadPresetFile(const juce::File&);
    juce::Result savePresetFile(const juce::File&);
    juce::String presetName() const;
    // UI-only size, kept separately from parameters and never read by the audio callback.
    int editorScalePercent() const noexcept { return uiScale.load(std::memory_order_relaxed); }
    void setEditorScalePercent(int percent) noexcept { uiScale.store(juce::jlimit(vanta::minEditorScale, vanta::maxEditorScale, percent), std::memory_order_relaxed); }
    static constexpr int factoryPresetCount = 5;
    static const char* factoryPresetName(int index);
    static const char* factoryPresetCategory(int index);

    juce::AudioProcessorValueTreeState state;
    // The standalone editor injects notes from its on-screen/computer keyboard.
    // VST3 instances continue to use the host's MIDI input unchanged.
    juce::MidiKeyboardState keyboardState;

private:
    std::atomic<int> uiScale { vanta::defaultEditorScale };
    vanta::SynthEngine engine;
    std::array<std::atomic<float>*, vanta::parameterCount> rawParameters {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VantaProcessor)
};
