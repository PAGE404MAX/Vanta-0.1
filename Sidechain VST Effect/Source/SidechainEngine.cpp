#include "SidechainEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace sidechain
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
float clean(float v, float fallback, float lo, float hi) noexcept
{
    return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
}
float safeSample(float v) noexcept
{
    return std::isfinite(v) ? std::clamp(v, -64.0f, 64.0f) : 0.0f;
}
float approachCoefficient(double sampleRate, float milliseconds) noexcept
{
    if (milliseconds <= 0.0f)
        return 0.0f;
    return static_cast<float>(std::exp(-1.0 / (sampleRate * milliseconds * .001)));
}
float neutral(int module) noexcept
{
    switch (module)
    {
    case Volume:
    case Filter:
        return 1.0f;
    case Pan:
    case Width:
    case Pitch:
        return .5f;
    default:
        return 0.0f;
    }
}
float lerp(float a, float b, float p) noexcept
{
    return a + (b - a) * p;
}
} // namespace

float defaultCurveValue(int module, int step) noexcept
{
    const float end = neutral(module), start = module == Volume || module == Filter ? .03f : 1.0f;
    const float progress = module == Volume ? std::clamp((static_cast<float>(step) / 16.0f - .05f) / .42f, 0.0f, 1.0f)
                                            : std::clamp(static_cast<float>(step) / 7.0f, 0.0f, 1.0f);
    const float eased = progress * progress * (3.0f - 2.0f * progress);
    return lerp(start, end, eased);
}
Settings::Settings() noexcept
{
    for (int m = 0; m < moduleCount; ++m)
    {
        auto& s = module[static_cast<std::size_t>(m)];
        s.enabled = m == Volume;
        s.order = m;
        for (auto& band : s.band)
        {
            band.amount = m == Volume ? .72f : .5f;
            band.detail = .5f;
            band.mix = m == Volume ? 1.0f : .5f;
            for (int p = 0; p < stepCount; ++p)
                band.curve[static_cast<std::size_t>(p)] = defaultCurveValue(m, p);
        }
    }
}
float evaluateCurve(const BandSettings& band, double phase) noexcept
{
    const float p = std::isfinite(phase) ? static_cast<float>(std::clamp(phase, 0.0, 1.0)) : 1.0f;
    // Sixteen equally spaced editable values; the last is held to the end.
    const float position = p * static_cast<float>(stepCount);
    const int i = std::min(stepCount - 1, static_cast<int>(position));
    const float a = clean(band.curve[static_cast<std::size_t>(i)], 0.0f, 0.0f, 1.0f);
    if (band.stepped || i == stepCount - 1)
        return a;
    const float b = clean(band.curve[static_cast<std::size_t>(i + 1)], a, 0.0f, 1.0f);
    return lerp(a, b, position - static_cast<float>(i));
}
void Engine::Delay::prepare(int samples)
{
    samples = std::max(4, samples);
    for (auto& channel : data)
        channel.assign(static_cast<std::size_t>(samples), 0.0f);
    write = 0;
}
void Engine::Delay::clear() noexcept
{
    for (auto& channel : data)
        std::fill(channel.begin(), channel.end(), 0.0f);
    write = 0;
}
float Engine::Delay::tap(int channel, float samplesBack) const noexcept
{
    const auto& buffer = data[static_cast<std::size_t>(channel)];
    if (buffer.size() < 4)
        return 0.0f;
    const int size = static_cast<int>(buffer.size());
    const float d = clean(samplesBack, 1.0f, 1.0f, static_cast<float>(size - 2));
    const int whole = static_cast<int>(d);
    const float fraction = d - static_cast<float>(whole);
    int i = write - whole;
    if (i < 0)
        i += size;
    int j = i - 1;
    if (j < 0)
        j += size;
    return lerp(buffer[static_cast<std::size_t>(i)], buffer[static_cast<std::size_t>(j)], fraction);
}
void Engine::Delay::push(Pair v) noexcept
{
    if (data[0].empty())
        return;
    data[0][static_cast<std::size_t>(write)] = safeSample(v.l);
    data[1][static_cast<std::size_t>(write)] = safeSample(v.r);
    if (++write >= static_cast<int>(data[0].size()))
        write = 0;
}
void Engine::prepare(double sampleRate)
{
    rate = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 384000.0) : 44100.0;
    for (int b = 0; b < bandCount; ++b)
    {
        timeDelay[static_cast<std::size_t>(b)].prepare(static_cast<int>(rate * .31) + 8);
        pitchDelay[static_cast<std::size_t>(b)].prepare(static_cast<int>(rate * .105) + 8);
        reverbDelay[static_cast<std::size_t>(b)].prepare(static_cast<int>(rate * .19) + 8);
    }
    crossoverLow = 1.0f - static_cast<float>(std::exp(-2.0 * pi * 180.0 / rate));
    crossoverHigh = 1.0f - static_cast<float>(std::exp(-2.0 * pi * 2500.0 / rate));
    reset();
}
void Engine::reset() noexcept
{
    moduleState = {};
    for (auto& delay : timeDelay)
        delay.clear();
    for (auto& delay : pitchDelay)
        delay.clear();
    for (auto& delay : reverbDelay)
        delay.clear();
    for (auto& p : visiblePhase)
        p.store(1.0f, std::memory_order_relaxed);
    visibleKey.store(0.0f, std::memory_order_relaxed);
    keyMeterEnvelope = 0.0f;
    randomState = 0x34ba621du;
}
float Engine::phase(int module) const noexcept
{
    return module >= 0 && module < moduleCount
               ? visiblePhase[static_cast<std::size_t>(module)].load(std::memory_order_relaxed)
               : 0.0f;
}

Engine::Pair Engine::applyEffect(int module, int bandIndex, Pair in, const BandSettings& s, float value) noexcept
{
    auto& st = moduleState[static_cast<std::size_t>(module)].lane[static_cast<std::size_t>(bandIndex)];
    const float amount = clean(s.amount, .5f, 0.0f, 1.0f), detail = clean(s.detail, .5f, 0.0f, 1.0f);
    const float mix = clean(s.mix, 1.0f, 0.0f, 1.0f), v = clean(value, neutral(module), 0.0f, 1.0f);
    Pair wet = in;
    switch (module)
    {
    case Volume:
    {
        const float gain = (1.0f - amount + amount * v) * std::pow(10.0f, (detail - .5f) * 2.4f);
        wet = {in.l * gain, in.r * gain};
        break;
    }
    case Filter:
    {
        const float motion = 1.0f - amount + amount * v;
        const float cutoff = std::min(static_cast<float>(rate * .44), 35.0f * std::pow(500.0f, motion));
        const float g = std::tan(pi * cutoff / static_cast<float>(rate)), k = 1.0f / (.55f + detail * 5.0f);
        const float a = 1.0f / (1.0f + g * (g + k));
        const float input[2] = {in.l, in.r};
        float output[2]{};
        for (int c = 0; c < 2; ++c)
        {
            const float v3 = input[c] - st.filter2[c], v1 = a * (st.filter1[c] + g * v3), v2 = st.filter2[c] + g * v1;
            st.filter1[c] = safeSample(2.0f * v1 - st.filter1[c]);
            st.filter2[c] = safeSample(2.0f * v2 - st.filter2[c]);
            output[c] = v2;
        }
        const float depth = amount * (1.0f - v);
        wet = {lerp(in.l, output[0], depth), lerp(in.r, output[1], depth)};
        break;
    }
    case Drive:
    {
        const float gain = 1.0f + 20.0f * amount * v, norm = std::tanh(gain);
        const float a = .015f + detail * .48f;
        const float l = std::tanh(in.l * gain) / norm, r = std::tanh(in.r * gain) / norm;
        st.tone[0] += a * (l - st.tone[0]);
        st.tone[1] += a * (r - st.tone[1]);
        const float depth = amount * v;
        wet = {lerp(in.l, lerp(st.tone[0], l, detail), depth), lerp(in.r, lerp(st.tone[1], r, detail), depth)};
        break;
    }
    case Crush:
    {
        const float strength = amount * v;
        const int bits = std::clamp(16 - static_cast<int>(std::round(13.0f * strength)), 3, 16);
        const float levels = static_cast<float>(1u << (bits - 1));
        st.crushPhase += 1.0f / (1.0f + detail * strength * 48.0f);
        if (st.crushPhase >= 1.0f)
        {
            st.crushPhase -= 1.0f;
            st.crushHeld[0] = std::round(in.l * levels) / levels;
            st.crushHeld[1] = std::round(in.r * levels) / levels;
        }
        wet = {lerp(in.l, st.crushHeld[0], strength), lerp(in.r, st.crushHeld[1], strength)};
        break;
    }
    case Time:
    {
        auto& delay = timeDelay[static_cast<std::size_t>(bandIndex)];
        const float samples = static_cast<float>(rate) * (.001f + .25f * amount * v);
        const Pair delayed{delay.tap(0, samples), delay.tap(1, samples)};
        delay.push(in);
        const float blend = std::min(1.0f, amount * v * 8.0f);
        wet = {lerp(in.l, delayed.l, blend), lerp(in.r, delayed.r, blend)};
        break;
    }
    case Pan:
    {
        const float pan = std::clamp((v - .5f) * 2.0f * amount, -1.0f, 1.0f);
        const float angle = (pan + 1.0f) * pi * .25f;
        wet = {in.l * std::cos(angle) * 1.41421356237f, in.r * std::sin(angle) * 1.41421356237f};
        break;
    }
    case Width:
    {
        const float mid = (in.l + in.r) * .5f, side = (in.l - in.r) * .5f;
        const float width = std::clamp(1.0f + (v - .5f) * 2.0f * amount, 0.0f, 2.0f);
        wet = {mid + side * width, mid - side * width};
        break;
    }
    case Noise:
    {
        const float peak = std::max(std::abs(in.l), std::abs(in.r));
        st.detector += .003f * (peak - st.detector);
        auto random = [this]() noexcept
        {
            randomState ^= randomState << 13;
            randomState ^= randomState >> 17;
            randomState ^= randomState << 5;
            return static_cast<float>((randomState >> 8) * (1.0 / 8388608.0) - 1.0);
        };
        const float scale = amount * v * .25f * std::min(1.0f, st.detector * 3.0f);
        const float n1 = random(), n2 = random();
        st.tone[0] += (.02f + detail * .7f) * (n1 - st.tone[0]);
        st.tone[1] += (.02f + detail * .7f) * (n2 - st.tone[1]);
        wet = {in.l + st.tone[0] * scale, in.r + st.tone[1] * scale};
        break;
    }
    case Liquid:
    {
        const float coefficient = std::clamp(-.88f + 1.76f * v, -.88f, .88f);
        const float l = -coefficient * in.l + st.liquid[0], r = -coefficient * in.r + st.liquid[1];
        st.liquid[0] = safeSample(in.l + coefficient * l);
        st.liquid[1] = safeSample(in.r + coefficient * r);
        const float depth = amount * v * (.25f + .75f * detail);
        wet = {lerp(in.l, (in.l + l) * .5f, depth), lerp(in.r, (in.r + r) * .5f, depth)};
        break;
    }
    case Reverb:
    {
        auto& delay = reverbDelay[static_cast<std::size_t>(bandIndex)];
        const float leftDelay = static_cast<float>(rate * .071), rightDelay = static_cast<float>(rate * .103);
        const Pair echo{delay.tap(0, leftDelay), delay.tap(1, rightDelay)};
        const float feedback = .25f + detail * .48f;
        delay.push({in.l + echo.r * feedback, in.r + echo.l * feedback});
        wet = {in.l + echo.l * amount * v, in.r + echo.r * amount * v};
        break;
    }
    case Pitch:
    {
        auto& delay = pitchDelay[static_cast<std::size_t>(bandIndex)];
        const float semitones = (v - .5f) * 48.0f * amount;
        const double window = rate * .075, ratio = std::pow(2.0, semitones / 12.0);
        st.pitchPhase += (1.0 - ratio) / window;
        st.pitchPhase -= std::floor(st.pitchPhase);
        const double p1 = st.pitchPhase, p2 = std::fmod(p1 + .5, 1.0);
        const float w1 = static_cast<float>(std::sin(pi * p1) * std::sin(pi * p1)), w2 = 1.0f - w1;
        const float d1 = static_cast<float>(1.0 + p1 * window), d2 = static_cast<float>(1.0 + p2 * window);
        const Pair shifted{delay.tap(0, d1) * w1 + delay.tap(0, d2) * w2,
                           delay.tap(1, d1) * w1 + delay.tap(1, d2) * w2};
        delay.push(in);
        const float blend = std::min(1.0f, std::abs(semitones) * .5f);
        wet = {lerp(in.l, shifted.l, blend), lerp(in.r, shifted.r, blend)};
        break;
    }
    case Compressor:
    {
        const float peak = std::max(std::abs(in.l), std::abs(in.r));
        const float coeff = peak > st.detector ? .02f : .0005f;
        st.detector += coeff * (peak - st.detector);
        const float threshold = std::pow(10.0f, (-48.0f + amount * 48.0f) / 20.0f);
        const float ratio = 1.0f + detail * 19.0f;
        const float gain = st.detector > threshold ? std::pow(threshold / st.detector, 1.0f - 1.0f / ratio) : 1.0f;
        const float shaped = lerp(1.0f, gain, v);
        wet = {in.l * shaped, in.r * shaped};
        break;
    }
    default:
        break;
    }
    return {safeSample(lerp(in.l, wet.l, mix)), safeSample(lerp(in.r, wet.r, mix))};
}

void Engine::process(float* left, float* right, const float* keyLeft, const float* keyRight, int count,
                     const Settings& settings, const Transport& transport, const int* midiNotes, int numMidiNotes,
                     bool hostBypass) noexcept
{
    if (left == nullptr || count <= 0)
        return;
    if (hostBypass || !settings.enabled)
    {
        keyMeterEnvelope = 0.0f;
        visibleKey.store(0.0f, std::memory_order_relaxed);
        return;
    }
    const float master = clean(settings.masterMix, 1.0f, 0.0f, 1.0f);
    if (master <= 0.0f)
    {
        keyMeterEnvelope = 0.0f;
        visibleKey.store(0.0f, std::memory_order_relaxed);
        return;
    }
    const float meterRelease = approachCoefficient(rate, 120.0f);
    const double bpm =
        std::isfinite(transport.bpm) && transport.bpm >= 20.0 && transport.bpm <= 400.0 ? transport.bpm : 120.0;
    const double ppq = std::isfinite(transport.ppq) ? transport.ppq : 0.0;
    const double increment = bpm / (60.0 * rate);
    constexpr double lengths[] = {1.0, .5, .25, 2.0, 4.0, 8.0, 16.0};
    struct Derived
    {
        float attack, release, threshold, curve;
        double beats;
        int source;
    } derived[moduleCount];
    int order[moduleCount];
    for (int m = 0; m < moduleCount; ++m)
    {
        const auto& s = settings.module[static_cast<std::size_t>(m)];
        derived[m] = {approachCoefficient(rate, clean(s.attackMs, 2, 0, 500)),
                      approachCoefficient(rate, clean(s.releaseMs, 140, 0, 2000)),
                      std::pow(10.0f, clean(s.thresholdDb, -24, -60, 0) / 20.0f),
                      approachCoefficient(rate, clean(s.smooth, .3f, 0, 1) * 25.0f),
                      lengths[std::clamp(s.length, 0, 6)],
                      std::clamp(s.source, 0, 2)};
        order[m] = m;
    }
    // Stable order even while the host automates order parameters into duplicates.
    for (int i = 1; i < moduleCount; ++i)
    {
        const int v = order[i];
        int j = i;
        while (j > 0 && settings.module[static_cast<std::size_t>(order[j - 1])].order >
                            settings.module[static_cast<std::size_t>(v)].order)
        {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = v;
    }
    int nextMidi = 0;
    for (int i = 0; i < count; ++i)
    {
        bool noteOn = false;
        while (midiNotes != nullptr && nextMidi < numMidiNotes && midiNotes[nextMidi] <= i)
        {
            noteOn = true;
            ++nextMidi;
        }
        const float key = std::max(std::abs(keyLeft ? safeSample(keyLeft[i]) : 0.0f),
                                   std::abs(keyRight ? safeSample(keyRight[i]) : 0.0f));
        keyMeterEnvelope = std::max(std::min(key, 1.0f), keyMeterEnvelope * meterRelease);
        const Pair dry{safeSample(left[i]), safeSample(right ? right[i] : left[i])};
        Pair signal = dry;
        for (int slot = 0; slot < moduleCount; ++slot)
        {
            const int m = order[slot];
            const auto& s = settings.module[static_cast<std::size_t>(m)];
            // Panning a one-channel bus cannot move it in the stereo field.
            if (!s.enabled || (right == nullptr && m == Pan))
                continue;
            auto& st = moduleState[static_cast<std::size_t>(m)];
            const auto& d = derived[m];
            const float alpha = key > st.keyEnvelope ? d.attack : d.release;
            st.keyEnvelope = alpha * st.keyEnvelope + (1.0f - alpha) * key;
            bool triggered = false;
            if (!st.keyHigh && st.keyEnvelope >= d.threshold)
            {
                st.keyHigh = true;
                triggered = true;
            }
            else if (st.keyHigh && st.keyEnvelope < d.threshold * .65f)
                st.keyHigh = false;
            double phaseValue;
            if (d.source == Sync)
            {
                if (transport.playingPpq)
                {
                    phaseValue = std::fmod((ppq + i * increment) / d.beats, 1.0);
                    if (phaseValue < 0)
                        phaseValue += 1.0;
                }
                else
                {
                    phaseValue = st.freePhase;
                    st.freePhase += increment / d.beats;
                    if (st.freePhase >= 1.0)
                        st.freePhase -= std::floor(st.freePhase);
                }
            }
            else
            {
                if ((d.source == External && triggered) || (d.source == Midi && noteOn))
                {
                    st.oneShotPhase = 0.0;
                    st.running = true;
                }
                phaseValue = st.oneShotPhase;
                if (st.running)
                {
                    st.oneShotPhase += increment / d.beats;
                    if (st.oneShotPhase >= 1.0)
                    {
                        if (s.oneShot)
                        {
                            st.oneShotPhase = 1.0;
                            st.running = false;
                        }
                        else
                            st.oneShotPhase = std::fmod(st.oneShotPhase, 1.0);
                    }
                }
            }
            visiblePhase[static_cast<std::size_t>(m)].store(static_cast<float>(phaseValue), std::memory_order_relaxed);
            auto processBand = [&](int b, Pair input) noexcept
            {
                auto& lane = st.lane[static_cast<std::size_t>(b)];
                const auto& band = s.band[static_cast<std::size_t>(b)];
                const float target = evaluateCurve(band, phaseValue);
                if (!lane.hasSmooth)
                {
                    lane.smoothValue = target;
                    lane.hasSmooth = true;
                }
                lane.smoothValue = d.curve * lane.smoothValue + (1.0f - d.curve) * target;
                return applyEffect(m, b, input, band, lane.smoothValue);
            };
            if (!s.multiband)
                signal = processBand(0, signal);
            else
            {
                st.low[0] += crossoverLow * (signal.l - st.low[0]);
                st.low[1] += crossoverLow * (signal.r - st.low[1]);
                st.upper[0] += crossoverHigh * (signal.l - st.upper[0]);
                st.upper[1] += crossoverHigh * (signal.r - st.upper[1]);
                const Pair low{st.low[0], st.low[1]}, high{signal.l - st.upper[0], signal.r - st.upper[1]},
                    mid{signal.l - low.l - high.l, signal.r - low.r - high.r};
                const Pair a = processBand(1, low), b = processBand(2, mid), c = processBand(3, high);
                signal = {safeSample(a.l + b.l + c.l), safeSample(a.r + b.r + c.r)};
            }
        }
        left[i] = safeSample(lerp(dry.l, signal.l, master));
        if (right)
            right[i] = safeSample(lerp(dry.r, signal.r, master));
    }
    visibleKey.store(keyMeterEnvelope, std::memory_order_relaxed);
}
} // namespace sidechain
