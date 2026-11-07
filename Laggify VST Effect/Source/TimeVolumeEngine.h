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
    // Neutral defaults preserve the sound of sessions made with Laggify 0.1.
    float timeMix = 1.0f, volumeMix = 1.0f;
    float attackMs = 4.0f, releaseMs = 4.0f, tension = 0.0f;
    float scratch = 0.0f; // additional time-back, as a fraction of a cycle
    bool hold = false;
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

// A bounded, allocation-free curve bend shared by audio processing and the editor.
// Zero is a straight line; negative and positive tension ease in opposite directions.
inline float bend(float progress, float tension) noexcept
{
    const float x = std::clamp(progress, 0.0f, 1.0f);
    const float t = std::clamp(tension, -1.0f, 1.0f);
    return t >= 0.0f ? x + t * (x * x - x) : x + (-t) * (x - x * x);
}

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
    float tapHeld(int channel, double samplesBack) const noexcept;
    double rate = 44100.0, freePhase = 0.0, lastDelay = 0.0, fadeFromDelay = 0.0;
    std::array<std::vector<float>, 2> history;
    int write = 0, fadeRemaining = 0, recorded = 0;
    float mixAmount = 0.0f, volumeAmount = 1.0f;
    float timeMixAmount = 1.0f, volumeMixAmount = 1.0f, tensionAmount = 0.0f;
    float volumeStart = 1.0f, volumeDestination = 1.0f, volumeProgress = 1.0f;
    bool tensionStepActive = false;
    // HOLD freezes the existing ring rather than allocating or copying audio in process().
    bool holdActive = false;
    int holdEnd = 0, holdLength = 1, holdCursor = 0;
    float holdBlend = 0.0f;
    std::atomic<float> visiblePhase{0.0f};
};
} // namespace laggify
