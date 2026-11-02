#include "TimeVolumeEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace
{
void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
    std::cout << "PASS: " << message << '\n';
}
constexpr int rate = 44100;
std::vector<float> signal(int count, int offset = 0)
{
    std::vector<float> out(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        out[static_cast<std::size_t>(i)] =
            static_cast<float>(.28 * std::sin(2 * 3.141592653589793 * 233 * (i + offset) / rate) +
                               .11 * std::sin(2 * 3.141592653589793 * 3179 * (i + offset) / rate));
    return out;
}
void process(laggify::TimeVolumeEngine& engine, std::vector<float>& data, const laggify::Settings& s,
             laggify::Transport t = {})
{
    float* channels[] = {data.data()};
    engine.process(channels, 1, static_cast<int>(data.size()), s, t);
}
double difference(const std::vector<float>& a, const std::vector<float>& b)
{
    double sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        sum += std::abs(a[i] - b[i]);
    return sum / a.size();
}
void testPatterns()
{
    for (bool time : {true, false})
    {
        std::set<std::string> ids;
        std::set<std::pair<std::array<int, laggify::steps>, bool>> curves;
        for (int i = 0; i < laggify::bankSize; ++i)
        {
            ids.insert((time ? laggify::timeIds : laggify::volumeIds)[static_cast<std::size_t>(i)]);
            const auto p = laggify::makePattern(time, i);
            std::array<int, laggify::steps> rounded{};
            for (int j = 0; j < laggify::steps; ++j)
            {
                check(std::isfinite(p.values[j]) && p.values[j] >= 0 && p.values[j] <= 1,
                      "Pattern value is finite and normalized");
                rounded[j] = static_cast<int>(p.values[j] * 10000);
            }
            curves.insert({rounded, p.smooth});
        }
        check(ids.size() == 36 && curves.size() == 36,
              time ? "Time bank has 36 distinct original patterns" : "Volume bank has 36 distinct original patterns");
    }
    const auto empty = laggify::makePattern(false, 0), mute = laggify::makePattern(false, 35);
    check(std::all_of(empty.values.begin(), empty.values.end(), [](float x) { return x == 1; }) &&
              std::all_of(mute.values.begin(), mute.values.end(), [](float x) { return x == 0; }),
          "Empty and Mute have meaningful gain curves");
}
void testAudio()
{
    laggify::Settings s;
    auto original = signal(rate);
    auto dry = original;
    laggify::TimeVolumeEngine bypass;
    bypass.prepare(rate);
    process(bypass, dry, s, {120, 0, true});
    check(difference(original, dry) == 0, "Default bypass is bit-identical for mixer-track audio");
    s.enabled = true;
    laggify::TimeVolumeEngine live;
    live.prepare(rate);
    dry = original;
    process(live, dry, s, {120, 0, true});
    check(difference(original, dry) == 0, "Live and open curves are transparent");
    s.volume[1] = 0;
    std::vector<float> constant(16000, .5f);
    laggify::TimeVolumeEngine at120;
    at120.prepare(rate);
    auto a = constant;
    process(at120, a, s, {120, 0, true});
    laggify::TimeVolumeEngine at60;
    at60.prepare(rate);
    auto b = constant;
    process(at60, b, s, {60, 0, true});
    check(a[2000] > .49f && a[8000] < .01f && a[14000] > .49f, "Volume gates and reopens on 120 BPM host timing");
    check(b[8000] > .49f && b[14000] < .01f, "Host BPM changes the gate timing");
    laggify::TimeVolumeEngine pos;
    pos.prepare(rate);
    auto c = constant;
    process(pos, c, s, {120, .25, true});
    check(c[2000] < .01f, "Host PPQ selects the correct initial step");
    s.fallbackBpm = 60;
    laggify::TimeVolumeEngine fallback;
    fallback.prepare(rate);
    auto d = constant;
    process(fallback, d, s);
    check(d[8000] > .49f && d[14000] < .01f, "Fallback tempo drives free-running operation");
    laggify::Settings line;
    line.enabled = true;
    line.volume[0] = 0;
    line.volume[1] = 1;
    line.volumeSmooth = true;
    laggify::TimeVolumeEngine l;
    l.prepare(rate);
    auto e = constant;
    process(l, e, line, {120, 0, true});
    line.volumeSmooth = false;
    laggify::TimeVolumeEngine held;
    held.prepare(rate);
    auto f = constant;
    process(held, f, line, {120, 0, true});
    check(e[3000] > .18f && e[3000] < .32f && e[5000] > .4f && f[3000] < .01f,
          "Volume supports independent line and held-step modes");
    auto render = [](int pattern)
    {
        laggify::Settings x;
        x.enabled = true;
        const auto p = laggify::makePattern(true, pattern);
        x.time = p.values;
        x.timeSmooth = p.smooth;
        std::vector<float> v(rate * 3);
        for (int i = 0; i < rate * 3; ++i)
            v[i] = .05f + static_cast<float>(i) / rate * .1f;
        laggify::TimeVolumeEngine engine;
        engine.prepare(rate);
        process(engine, v, x, {120, 0, true});
        return v;
    };
    const auto half = render(1), reverse = render(6), stutter = render(11);
    const int t21 = static_cast<int>(rate * 2.1), t22 = static_cast<int>(rate * 2.2),
              t26 = static_cast<int>(rate * 2.6);
    check(half[t22] - half[t21] > .0045f && half[t22] - half[t21] < .0055f,
          "Half-speed curve reads track history at half speed");
    check(reverse[t22] < reverse[t21] - .008f, "Reverse curve reads recent track history backwards");
    check(std::abs(stutter[t26] - stutter[t21]) < .0003f, "Stutter curve repeats a previous quarter-cycle");
    laggify::TimeVolumeEngine history;
    history.prepare(rate);
    laggify::Settings h;
    h.time.fill(.25f);
    std::vector<float> before(rate, .4f);
    process(history, before, h, {120, 0, false});
    h.enabled = true;
    std::vector<float> after(4000, .4f);
    process(history, after, h, {120, 0, false});
    check(after[3000] > .39f, "History is recorded while bypassed and works in mono");
    laggify::TimeVolumeEngine hostBypass;
    hostBypass.prepare(rate);
    laggify::Settings hostSettings;
    hostSettings.enabled = true;
    hostSettings.time.fill(.25f);
    std::vector<float> hostInput(rate, .4f);
    float* hostChannels[] = {hostInput.data()};
    hostBypass.process(hostChannels, 1, rate, hostSettings, {120, 0, false}, true);
    check(std::all_of(hostInput.begin(), hostInput.end(), [](float x) { return x == .4f; }),
          "Host hard bypass is bit-identical while the effect is enabled");
    std::vector<float> resumed(4000, 0.0f);
    process(hostBypass, resumed, hostSettings, {120, 0, false});
    check(resumed[3000] > .39f, "Host hard bypass records input history for the resumed effect");
    laggify::Settings chunk;
    chunk.enabled = true;
    chunk.time = laggify::makePattern(true, 1).values;
    chunk.timeSmooth = true;
    chunk.volume[3] = 0;
    chunk.volume[8] = .25f;
    laggify::TimeVolumeEngine whole, split;
    whole.prepare(rate);
    split.prepare(rate);
    auto one = signal(rate * 2);
    process(whole, one, chunk, {120, 0, true});
    std::vector<float> joined(rate * 2);
    for (int start = 0; start < rate * 2; start += 257)
    {
        const int count = std::min(257, rate * 2 - start);
        auto part = signal(count, start);
        process(split, part, chunk, {120, 2.0 * start / rate, true});
        std::copy(part.begin(), part.end(), joined.begin() + start);
    }
    check(difference(one, joined) < .00002, "Host-synced processing is stable across block sizes");
    laggify::TimeVolumeEngine headroom;
    headroom.prepare(rate);
    laggify::Settings unity;
    unity.enabled = true;
    std::vector<float> hot(4096, 1.5f);
    process(headroom, hot, unity, {120, 0, true});
    check(hot[3000] > 1.49f, "Valid mixer headroom above 0 dBFS is not clipped");
    laggify::TimeVolumeEngine malformed;
    malformed.prepare(rate);
    auto bad = signal(4096);
    bad[40] = std::numeric_limits<float>::quiet_NaN();
    unity.time[0] = std::numeric_limits<float>::infinity();
    unity.fallbackBpm = std::numeric_limits<float>::quiet_NaN();
    process(malformed, bad, unity,
            {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), true});
    check(std::all_of(bad.begin(), bad.end(), [](float x) { return std::isfinite(x); }),
          "Invalid input, timing and parameters cannot poison the audio history");
}
} // namespace
int main()
{
    testPatterns();
    testAudio();
    std::cout << "Laggify core engine tests passed.\n";
}
