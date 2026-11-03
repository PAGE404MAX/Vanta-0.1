#include "TimeVolumeEngine.h"
#include <algorithm>
#include <cmath>

namespace laggify
{
namespace
{
float safe(float value, float fallback, float low, float high) noexcept
{
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}
float approach(float current, float target, float coefficient) noexcept
{
    current += (target - current) * coefficient;
    return target == 0.0f && std::abs(current) < 1.0e-6f ? 0.0f : current;
}
float timeCoefficient(double rate, float milliseconds) noexcept
{
    if (milliseconds <= 0.0f)
        return 1.0f;
    return static_cast<float>(1.0 - std::exp(-1.0 / (rate * (milliseconds * .001))));
}
} // namespace
void TimeVolumeEngine::prepare(double sampleRate)
{
    rate = std::isfinite(sampleRate) && sampleRate >= 1000.0 && sampleRate <= 1536000.0 ? sampleRate : 44100.0;
    // Two bars at the minimum supported tempo (30 BPM), capped at 16 seconds
    // at 384 kHz for unusually high sample rates.
    const auto samples = static_cast<std::size_t>(std::ceil(std::min(rate * 16.0, 6144000.0))) + 3u;
    for (auto& lane : history)
        lane.assign(samples, 0.0f);
    reset();
}
void TimeVolumeEngine::reset() noexcept
{
    for (auto& lane : history)
        std::fill(lane.begin(), lane.end(), 0.0f);
    write = fadeRemaining = recorded = 0;
    freePhase = lastDelay = fadeFromDelay = 0.0;
    mixAmount = 0.0f;
    volumeAmount = timeMixAmount = volumeMixAmount = 1.0f;
    tensionAmount = 0.0f;
    volumeStart = volumeDestination = volumeProgress = 1.0f;
    tensionStepActive = holdActive = false;
    holdEnd = holdCursor = 0;
    holdLength = 1;
    holdBlend = 0.0f;
    visiblePhase.store(0.0f, std::memory_order_relaxed);
}
float TimeVolumeEngine::tap(int channel, double samplesBack) const noexcept
{
    const auto& lane = history[static_cast<std::size_t>(channel)];
    const int size = static_cast<int>(lane.size());
    if (size < 3)
        return 0.0f;
    const double bounded = std::clamp(samplesBack, 0.0, static_cast<double>(size - 2));
    double position = static_cast<double>(write) - bounded;
    if (position < 0.0)
        position += size;
    const int a = static_cast<int>(position), b = a + 1 == size ? 0 : a + 1;
    const float fraction = static_cast<float>(position - a);
    return lane[static_cast<std::size_t>(a)] +
           (lane[static_cast<std::size_t>(b)] - lane[static_cast<std::size_t>(a)]) * fraction;
}
float TimeVolumeEngine::tapHeld(int channel, double samplesBack) const noexcept
{
    const auto& lane = history[static_cast<std::size_t>(channel)];
    const int size = static_cast<int>(lane.size());
    // Address the frozen interval modulo its own length, not the entire ring.
    double position = std::fmod(static_cast<double>(holdCursor) - samplesBack, static_cast<double>(holdLength));
    if (position < 0.0)
        position += holdLength;
    const int a = static_cast<int>(position), b = a + 1 == holdLength ? 0 : a + 1;
    int first = holdEnd - holdLength + a, second = holdEnd - holdLength + b;
    if (first < 0)
        first += size;
    if (second < 0)
        second += size;
    const float fraction = static_cast<float>(position - a);
    return lane[static_cast<std::size_t>(first)] +
           (lane[static_cast<std::size_t>(second)] - lane[static_cast<std::size_t>(first)]) * fraction;
}
void TimeVolumeEngine::process(float* const* channels, int channelCount, int count, const Settings& input,
                               const Transport& transport, bool hardBypass) noexcept
{
    if (channels == nullptr || count <= 0 || channelCount <= 0 || history[0].size() < 3)
        return;
    channelCount = std::min(channelCount, 2);
    const double fallback = safe(input.fallbackBpm, 120.0f, 30.0f, 400.0f);
    const double bpm =
        std::clamp(std::isfinite(transport.bpm) && transport.bpm > 0.0 ? transport.bpm : fallback, 30.0, 400.0);
    const int choice = std::clamp(input.cycle, 0, 2);
    const double beats = choice == 0 ? 2.0 : choice == 1 ? 4.0 : 8.0;
    const double cycleSamples = std::min(beats * 60.0 * rate / bpm, static_cast<double>(history[0].size() - 2));
    const double increment = bpm / (60.0 * rate * beats);
    double phase = freePhase;
    if (transport.playingPpq && std::isfinite(transport.ppq))
    {
        const double position = transport.ppq / beats;
        phase = position - std::floor(position);
    }
    const float target = input.enabled && !hardBypass ? safe(input.mix, 1.0f, 0.0f, 1.0f) : 0.0f;
    const float timeMixTarget = safe(input.timeMix, 1.0f, 0.0f, 1.0f);
    const float volumeMixTarget = safe(input.volumeMix, 1.0f, 0.0f, 1.0f);
    const float tensionTarget = safe(input.tension, 0.0f, -1.0f, 1.0f);
    const float scratch = safe(input.scratch, 0.0f, 0.0f, 1.0f);
    const auto coefficient = static_cast<float>(1.0 - std::exp(-1.0 / (rate * .004)));
    const float attack = safe(input.attackMs, 4.0f, 0.0f, 500.0f);
    const float release = safe(input.releaseMs, 4.0f, 0.0f, 1000.0f);
    // Using the original 4 ms coefficient exactly maintains 0.1 session sound.
    const auto attackCoefficient = attack == 4.0f ? coefficient : timeCoefficient(rate, attack);
    const auto releaseCoefficient = release == 4.0f ? coefficient : timeCoefficient(rate, release);
    const int fadeLength = std::max(1, static_cast<int>(rate * .005));
    const double threshold = std::max(8.0, rate * .008);
    const bool holdRequested = input.hold && input.enabled && !hardBypass;
    const int requestedHoldLength = std::clamp(static_cast<int>(std::round(cycleSamples)), 1,
                                               static_cast<int>(history[0].size()) - 2);
    if (hardBypass)
    {
        // Host bypass remains bit-identical and continues recording live input.
        if (holdActive)
            recorded = 0; // frozen input is not a continuous recent history
        holdActive = false;
        holdBlend = 0.0f;
    }
    Curve times{}, volumes{};
    for (int i = 0; i < steps; ++i)
    {
        times[i] = safe(input.time[i], 0.0f, 0.0f, 1.0f);
        volumes[i] = safe(input.volume[i], 1.0f, 0.0f, 1.0f);
    }
    for (int i = 0; i < count; ++i)
    {
        if (holdRequested && !holdActive && recorded >= requestedHoldLength)
        {
            // Wait for one complete cycle of fresh input, including on project
            // restore. Capture is then an O(1) snapshot: freezing the write
            // pointer keeps the loop intact without copying or allocating.
            holdEnd = write;
            holdLength = requestedHoldLength;
            holdCursor = 0;
            holdBlend = 0.0f;
            holdActive = true;
        }
        const double position = phase * steps;
        const int step = std::clamp(static_cast<int>(position), 0, steps - 1), next = std::min(steps - 1, step + 1);
        const float a = times[step], b = times[next];
        const bool line = input.timeSmooth && !(a - b > .5f);
        const double time = line ? a + (b - a) * (position - step) : a;
        const double delay = std::clamp((time + scratch) * cycleSamples, 0.0, cycleSamples);
        if (std::abs(delay - lastDelay) > threshold)
        {
            fadeFromDelay = lastDelay;
            fadeRemaining = fadeLength;
        }
        tensionAmount = approach(tensionAmount, tensionTarget, coefficient);
        const float va = volumes[step], vb = volumes[next];
        const bool volumeLine = input.volumeSmooth && !(va - vb > .5f);
        const double fraction = position - step;
        const float gain = volumeLine
                               ? static_cast<float>(va + (vb - va) *
                                                            (tensionAmount == 0.0f
                                                                 ? fraction
                                                                 : bend(static_cast<float>(fraction), tensionAmount)))
                               : va;
        const auto volumeCoefficient = gain >= volumeAmount ? attackCoefficient : releaseCoefficient;
        if (input.volumeSmooth || tensionAmount == 0.0f)
        {
            volumeAmount = approach(volumeAmount, gain, volumeCoefficient);
            tensionStepActive = false;
        }
        else
        {
            // In step mode, TNS shapes the attack/release transition itself.
            if (!tensionStepActive || gain != volumeDestination)
            {
                volumeStart = volumeAmount;
                volumeDestination = gain;
                volumeProgress = 0.0f;
                tensionStepActive = true;
            }
            volumeProgress = approach(volumeProgress, 1.0f, volumeCoefficient);
            volumeAmount = volumeStart + (volumeDestination - volumeStart) * bend(volumeProgress, tensionAmount);
        }
        mixAmount = approach(mixAmount, target, coefficient);
        timeMixAmount = approach(timeMixAmount, timeMixTarget, coefficient);
        volumeMixAmount = approach(volumeMixAmount, volumeMixTarget, coefficient);
        if (holdActive)
            holdBlend = std::clamp(holdBlend + (holdRequested ? 1.0f : -1.0f) / fadeLength, 0.0f, 1.0f);
        const float fade = fadeRemaining > 0 ? static_cast<float>(fadeRemaining) / fadeLength : 0.0f;
        for (int c = 0; c < channelCount; ++c)
        {
            if (channels[c] == nullptr)
                continue;
            const float original = channels[c][i];
            const float sample = std::isfinite(original) ? original : 0.0f;
            if (!holdActive)
                history[static_cast<std::size_t>(c)][static_cast<std::size_t>(write)] = sample;
            if (mixAmount > 0.0f && !hardBypass)
            {
                const float live = holdActive ? tapHeld(c, 0.0) : sample;
                float played = holdActive ? tapHeld(c, delay) : tap(c, delay);
                if (fade > 0.0f)
                {
                    const float previous = holdActive ? tapHeld(c, fadeFromDelay) : tap(c, fadeFromDelay);
                    played = played * (1.0f - fade) + previous * fade;
                }
                const float timed = timeMixAmount == 1.0f ? played : live + timeMixAmount * (played - live);
                const float appliedGain =
                    volumeMixAmount == 1.0f ? volumeAmount : 1.0f + volumeMixAmount * (volumeAmount - 1.0f);
                float processed = timed * appliedGain;
                if (holdActive)
                    processed = sample + holdBlend * (processed - sample);
                const float result = sample + mixAmount * (processed - sample);
                channels[c][i] = std::isfinite(result) ? result : 0.0f;
            }
            else if (!std::isfinite(original))
                channels[c][i] = 0.0f;
        }
        if (fadeRemaining > 0)
            --fadeRemaining;
        lastDelay = delay;
        if (holdActive)
        {
            if (++holdCursor >= holdLength)
                holdCursor = 0;
            if (!holdRequested && holdBlend <= 0.0f)
            {
                holdActive = false;
                recorded = 0;
            }
        }
        else
        {
            if (++write == static_cast<int>(history[0].size()))
                write = 0;
            recorded = std::min(recorded + 1, static_cast<int>(history[0].size()) - 2);
        }
        phase += increment;
        if (phase >= 1.0)
            phase -= std::floor(phase);
    }
    freePhase = phase;
    visiblePhase.store(static_cast<float>(phase), std::memory_order_relaxed);
}
} // namespace laggify
