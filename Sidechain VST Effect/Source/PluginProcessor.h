#pragma once
#include <JuceHeader.h>
#include "SidechainEngine.h"
#include <array>
#include <atomic>

class SidechainProcessor final : public juce::AudioProcessor
{
  public:
    SidechainProcessor();
    ~SidechainProcessor() override = default;
    const juce::String getName() const override
    {
        return "VANTA Sidechain";
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
        return 5.0;
    }
    bool acceptsMidi() const override
    {
        return true;
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
    static juce::String moduleId(int module, const char* suffix);
    static juce::String bandId(int module, int band, const char* suffix);
    static juce::String pointId(int module, int band, int point);
    sidechain::Settings currentSettings() const noexcept;
    juce::RangedAudioParameter* pointParameter(int module, int band, int point) const noexcept;
    void setParameterValue(const juce::String&, float);
    void applyCurve(int module, int band, const std::array<float, sidechain::stepCount>&, bool stepped);
    int editorScalePercent() const noexcept
    {
        return uiScale.load(std::memory_order_relaxed);
    }
    void setEditorScalePercent(int v) noexcept
    {
        uiScale.store(juce::jlimit(60, 150, v), std::memory_order_relaxed);
    }
    int selectedModule() const noexcept
    {
        return uiModule.load(std::memory_order_relaxed);
    }
    void setSelectedModule(int v) noexcept
    {
        uiModule.store(juce::jlimit(0, sidechain::moduleCount - 1, v), std::memory_order_relaxed);
    }
    int selectedBand() const noexcept
    {
        return uiBand.load(std::memory_order_relaxed);
    }
    void setSelectedBand(int v) noexcept
    {
        uiBand.store(juce::jlimit(0, sidechain::bandCount - 1, v), std::memory_order_relaxed);
    }
    bool stepDraw() const noexcept
    {
        return uiStep.load(std::memory_order_relaxed);
    }
    void setStepDraw(bool v) noexcept
    {
        uiStep.store(v, std::memory_order_relaxed);
    }
    float playheadPhase(int module) const noexcept
    {
        return engine.phase(module);
    }
    float keyLevel() const noexcept
    {
        return engine.keyLevel();
    }
    juce::AudioProcessorValueTreeState state;

  private:
    struct BandRefs
    {
        std::atomic<float>* amount = nullptr;
        std::atomic<float>* detail = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* stepped = nullptr;
        std::array<std::atomic<float>*, sidechain::stepCount> curve{};
        std::array<juce::RangedAudioParameter*, sidechain::stepCount> point{};
    };
    struct ModuleRefs
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* multi = nullptr;
        std::atomic<float>* source = nullptr;
        std::atomic<float>* length = nullptr;
        std::atomic<float>* order = nullptr;
        std::atomic<float>* oneShot = nullptr;
        std::atomic<float>* threshold = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* smooth = nullptr;
        std::array<BandRefs, sidechain::bandCount> bands{};
    };
    void process(juce::AudioBuffer<float>&, juce::MidiBuffer&, bool hostBypass);
    sidechain::Engine engine;
    std::atomic<float>* masterEnabled = nullptr;
    std::atomic<float>* masterMix = nullptr;
    std::array<ModuleRefs, sidechain::moduleCount> refs{};
    std::atomic<int> uiScale{100}, uiModule{0}, uiBand{0};
    std::atomic<bool> uiStep{false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidechainProcessor)
};
