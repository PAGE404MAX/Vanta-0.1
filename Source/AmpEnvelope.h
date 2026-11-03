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

    void setParameters(const juce::ADSR::Parameters& parameters) { adsr.setParameters(parameters); }
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
    }

    void reset() noexcept
    {
        adsr.reset();
        delayRemaining = holdLength = holdRemaining = 0;
        waitingForPeak = false;
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
        // The existing attack has a positive minimum, so JUCE emits an exact
        // peak of one before advancing to decay. Pause it there for the hold.
        if (waitingForPeak && value >= 1.0f)
        {
            waitingForPeak = false;
            holdRemaining = holdLength;
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
    double sampleRate = 44100.0;
    float delaySeconds = 0.0f, holdSeconds = 0.0f;
    int delayRemaining = 0, holdLength = 0, holdRemaining = 0;
    bool waitingForPeak = false;
};
}
