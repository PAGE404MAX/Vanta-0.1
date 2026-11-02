#pragma once
#include "Patterns.h"
#include <array>
#include <atomic>
#include <vector>

namespace laggify
{
struct Settings
{
    bool enabled = false;
    float mix = 1.0f, fallbackBpm = 120.0f;
    int cycle = 1; // 0 = half bar, 1 = one bar, 2 = two bars (4/4)
    bool timeSmooth = false, volumeSmooth = false;
    Curve time{}, volume{};
    Settings()
    {
        volume.fill(1.0f);
    }
};
struct Transport
{
    double bpm = 0.0, ppq = 0.0;
    bool playingPpq = false;
};

// Single audio-thread owner. All storage is allocated outside process().
class TimeVolumeEngine
{
  public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float* const* channels, int channelCount, int count, const Settings&, const Transport&,
                 bool hardBypass = false) noexcept;
    float currentPhase() const noexcept
    {
        return visiblePhase.load(std::memory_order_relaxed);
    }

  private:
    float tap(int channel, double samplesBack) const noexcept;
    double rate = 44100.0, freePhase = 0.0, lastDelay = 0.0, fadeFromDelay = 0.0;
    std::array<std::vector<float>, 2> history;
    int write = 0, fadeRemaining = 0;
    float mixAmount = 0.0f, volumeAmount = 1.0f;
    std::atomic<float> visiblePhase{0.0f};
};
} // namespace laggify
