#pragma once
#include <JuceHeader.h>
#include "TimeVolumeEngine.h"
#include <array>
#include <atomic>
#include <limits>

class LaggifyProcessor final : public juce::AudioProcessor
{
  public:
    LaggifyProcessor();
    ~LaggifyProcessor() override = default;
    const juce::String getName() const override
    {
        return "Laggify";
    }
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override
    {
        return true;
    }
    double getTailLengthSeconds() const override
    {
        return hold != nullptr && enabled != nullptr && hold->load(std::memory_order_relaxed) >= .5f &&
                       enabled->load(std::memory_order_relaxed) >= .5f
                   ? std::numeric_limits<double>::infinity()
                   : 16.0;
    }
    bool acceptsMidi() const override
    {
        return false;
    }
    bool producesMidi() const override
    {
        return false;
    }
    bool isMidiEffect() const override
    {
        return false;
    }
    int getNumPrograms() override
    {
        return 1;
    }
    int getCurrentProgram() override
    {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override
    {
        return {};
    }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    static juce::String pointId(bool time, int step);
    laggify::Settings currentSettings() const noexcept;
    void applyPattern(bool time, int index);
    int currentPattern(bool time) const noexcept;
    void markCustom(bool time) noexcept;
    juce::RangedAudioParameter* pointParameter(bool time, int step) const noexcept;
    void setParameterValue(const juce::String& id, float value);
    int editorScalePercent() const noexcept
    {
        return uiScale.load(std::memory_order_relaxed);
    }
    void setEditorScalePercent(int percent) noexcept
    {
        uiScale.store(juce::jlimit(60, 150, percent), std::memory_order_relaxed);
    }
    float playheadPhase() const noexcept
    {
        return engine.currentPhase();
    }
    bool editTime() const noexcept
    {
        return editingTime.load(std::memory_order_relaxed);
    }
    void setEditTime(bool time) noexcept
    {
        editingTime.store(time, std::memory_order_relaxed);
    }
    bool showBanksOnly() const noexcept
    {
        return banksOnly.load(std::memory_order_relaxed);
    }
    void setShowBanksOnly(bool show) noexcept
    {
        banksOnly.store(show, std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState state;

  private:
    void process(juce::AudioBuffer<float>&, juce::MidiBuffer&, bool hardBypass);
    laggify::TimeVolumeEngine engine;
    std::atomic<float>* enabled = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* bpm = nullptr;
    std::atomic<float>* cycle = nullptr;
    std::atomic<float>* timeSmooth = nullptr;
    std::atomic<float>* volumeSmooth = nullptr;
    std::atomic<float>* timeMix = nullptr;
    std::atomic<float>* volumeMix = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* release = nullptr;
    std::atomic<float>* tension = nullptr;
    std::atomic<float>* hold = nullptr;
    std::atomic<float>* scratch = nullptr;
    std::array<std::atomic<float>*, laggify::steps> times{}, volumes{};
    std::array<juce::RangedAudioParameter*, laggify::steps> timeParams{}, volumeParams{};
    std::atomic<int> timePreset{15}, volumePreset{0}, uiScale{100};
    std::atomic<bool> editingTime{true}, banksOnly{false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LaggifyProcessor)
};
