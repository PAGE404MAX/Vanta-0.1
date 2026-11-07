#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace vanta
{
// A per-voice DAHDSR. Keep the existing JUCE ADSR as the underlying A/D/S/R
// implementation so that projects with zero delay and hold are sample-identical
// to earlier VANTA versions. Delay and hold durations are captured at note-on;
// changing them while a note is held affects subsequent notes, not this note.
class AmpEnvelope
{
public:
    void setSampleRate(double newSampleRate) noexcept
    {
        sampleRate = (std::isfinite(newSampleRate) && newSampleRate > 0.0) ? newSampleRate : 44100.0;
        adsr.setSampleRate(sampleRate);
    }

    void setParameters(const juce::ADSR::Parameters& parameters)
    {
        realParameters = parameters;
        auto applied = parameters;
        // A host may automate attack to zero while a delayed/held note is
        // still approaching its peak. Preserve the hold in that case too.
        instantAttackForHold = waitingForPeak && applied.attack <= 0.0f;
        if (instantAttackForHold)
            applied.attack = static_cast<float>(0.5 / sampleRate);
        adsr.setParameters(applied);
    }
    void setDelayAndHold(float delay, float hold) noexcept
    {
        delaySeconds = boundedTime(delay);
        holdSeconds = boundedTime(hold);
    }

    void noteOn() noexcept
    {
        delayRemaining = toSamples(delaySeconds);
        holdLength = toSamples(holdSeconds);
        holdRemaining = 0;
        waitingForPeak = holdLength > 0;
        // JUCE skips straight past the peak for zero attack. Give it a
        // one-sample attack only when needed to anchor a real hold at 1.0.
        // The peak sample is counted as the first hold sample below.
        instantAttackForHold = waitingForPeak && realParameters.attack <= 0.0f;
        if (instantAttackForHold)
        {
            auto applied = realParameters;
            applied.attack = static_cast<float>(0.5 / sampleRate);
            adsr.setParameters(applied);
        }
        adsr.noteOn();
    }

    void noteOff() noexcept
    {
        // A release at any stage starts from the envelope's actual level. In
        // particular, releasing during delay cannot later trigger the attack.
        delayRemaining = 0;
        holdRemaining = 0;
        waitingForPeak = false;
        adsr.noteOff();
        if (instantAttackForHold) adsr.setParameters(realParameters);
        instantAttackForHold = false;
    }

    void reset() noexcept
    {
        adsr.reset();
        delayRemaining = holdLength = holdRemaining = 0;
        waitingForPeak = false;
        if (instantAttackForHold) adsr.setParameters(realParameters);
        instantAttackForHold = false;
    }

    bool isActive() const noexcept { return adsr.isActive(); }

    float getNextSample() noexcept
    {
        if (delayRemaining > 0)
        {
            --delayRemaining;
            return 0.0f;
        }
        if (holdRemaining > 0)
        {
            --holdRemaining;
            return 1.0f;
        }

        const auto value = adsr.getNextSample();
        // Pause JUCE at the exact peak before it starts decay.
        if (waitingForPeak && value >= 1.0f)
        {
            waitingForPeak = false;
            holdRemaining = holdLength - (instantAttackForHold ? 1 : 0);
            if (instantAttackForHold) adsr.setParameters(realParameters);
            instantAttackForHold = false;
        }
        return value;
    }

private:
    static float boundedTime(float seconds) noexcept
    {
        return std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 4.0f) : 0.0f;
    }
    int toSamples(float seconds) const noexcept
    {
        const double count = static_cast<double>(seconds) * sampleRate;
        if (!std::isfinite(count) || count <= 0.0) return 0;
        return static_cast<int>(std::min(count + 0.5, static_cast<double>(std::numeric_limits<int>::max())));
    }

    juce::ADSR adsr;
    juce::ADSR::Parameters realParameters;
    double sampleRate = 44100.0;
    float delaySeconds = 0.0f, holdSeconds = 0.0f;
    int delayRemaining = 0, holdLength = 0, holdRemaining = 0;
    bool waitingForPeak = false, instantAttackForHold = false;
};
}
