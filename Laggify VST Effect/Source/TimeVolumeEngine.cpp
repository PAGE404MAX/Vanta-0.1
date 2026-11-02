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
    return target == 0.0f && current < 1.0e-6f ? 0.0f : current;
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
    write = fadeRemaining = 0;
    freePhase = lastDelay = fadeFromDelay = 0.0;
    mixAmount = 0.0f;
    volumeAmount = 1.0f;
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
    const auto coefficient = static_cast<float>(1.0 - std::exp(-1.0 / (rate * .004)));
    const int fadeLength = std::max(1, static_cast<int>(rate * .005));
    const double threshold = std::max(8.0, rate * .008);
    Curve times{}, volumes{};
    for (int i = 0; i < steps; ++i)
    {
        times[i] = safe(input.time[i], 0.0f, 0.0f, 1.0f);
        volumes[i] = safe(input.volume[i], 1.0f, 0.0f, 1.0f);
    }
    for (int i = 0; i < count; ++i)
    {
        const double position = phase * steps;
        const int step = std::clamp(static_cast<int>(position), 0, steps - 1), next = std::min(steps - 1, step + 1);
        const float a = times[step], b = times[next];
        const bool line = input.timeSmooth && !(a - b > .5f);
        const double time = line ? a + (b - a) * (position - step) : a;
        const double delay = std::clamp(time * cycleSamples, 0.0, cycleSamples);
        if (std::abs(delay - lastDelay) > threshold)
        {
            fadeFromDelay = lastDelay;
            fadeRemaining = fadeLength;
        }
        const float va = volumes[step], vb = volumes[next];
        const bool volumeLine = input.volumeSmooth && !(va - vb > .5f);
        const float gain = volumeLine ? static_cast<float>(va + (vb - va) * (position - step)) : va;
        volumeAmount = approach(volumeAmount, gain, coefficient);
        mixAmount = approach(mixAmount, target, coefficient);
        const float fade = fadeRemaining > 0 ? static_cast<float>(fadeRemaining) / fadeLength : 0.0f;
        for (int c = 0; c < channelCount; ++c)
        {
            if (channels[c] == nullptr)
                continue;
            const float original = channels[c][i];
            const float sample = std::isfinite(original) ? original : 0.0f;
            history[static_cast<std::size_t>(c)][static_cast<std::size_t>(write)] = sample;
            if (mixAmount > 0.0f && !hardBypass)
            {
                float played = tap(c, delay);
                if (fade > 0.0f)
                    played = played * (1.0f - fade) + tap(c, fadeFromDelay) * fade;
                const float result = sample + mixAmount * (played * volumeAmount - sample);
                channels[c][i] = std::isfinite(result) ? result : 0.0f;
            }
            else if (!std::isfinite(original))
                channels[c][i] = 0.0f;
        }
        if (fadeRemaining > 0)
            --fadeRemaining;
        lastDelay = delay;
        if (++write == static_cast<int>(history[0].size()))
            write = 0;
        phase += increment;
        if (phase >= 1.0)
            phase -= std::floor(phase);
    }
    freePhase = phase;
    visiblePhase.store(static_cast<float>(phase), std::memory_order_relaxed);
}
} // namespace laggify
