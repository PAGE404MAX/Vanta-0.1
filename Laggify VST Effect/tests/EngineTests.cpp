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

void testExtraControls()
{
    constexpr int lowRate = 1000;
    auto mixed = [](float timeMix, float volumeMix, float overall)
    {
        laggify::TimeVolumeEngine engine;
        engine.prepare(lowRate);
        laggify::Settings s;
        s.time.fill(.25f);
        s.volume.fill(.5f);
        s.timeMix = timeMix;
        s.volumeMix = volumeMix;
        s.mix = overall;
        std::vector<float> history(2000, .8f);
        process(engine, history, s, {120, 0, false});
        s.enabled = true;
        std::vector<float> input(200, .2f);
        process(engine, input, s, {120, 0, false});
        return input[150];
    };
    check(std::abs(mixed(0, 0, 1) - .2f) < .002f, "Independent time and volume mixes both reach dry");
    check(std::abs(mixed(1, 0, 1) - .8f) < .002f, "Time mix can apply history without the volume envelope");
    check(std::abs(mixed(0, 1, 1) - .1f) < .002f, "Volume mix can apply gain without the time effect");
    check(std::abs(mixed(1, 1, 1) - .4f) < .002f, "Full time and volume mixes combine independently");
    check(std::abs(mixed(1, 1, .5f) - .3f) < .002f, "Overall mix still blends the combined effect");

    laggify::TimeVolumeEngine scratch;
    scratch.prepare(lowRate);
    laggify::Settings x;
    std::vector<float> prior(2000, .7f);
    process(scratch, prior, x, {120, 0, false});
    x.enabled = true;
    x.scratch = .25f;
    std::vector<float> moving(250, .2f);
    process(scratch, moving, x, {120, 0, false});
    check(std::abs(moving[200] - .7f) < .002f, "Scratch clock reads a quarter-cycle back from live audio");
    x.scratch = 0;
    std::vector<float> reset(100, .2f);
    process(scratch, reset, x, {120, 0, false});
    check(std::abs(reset[80] - .2f) < .002f, "Resetting scratch returns to live input");

    auto transition = [](bool rising, float milliseconds, float tns)
    {
        laggify::TimeVolumeEngine engine;
        engine.prepare(lowRate);
        laggify::Settings s;
        s.enabled = true;
        s.attackMs = rising ? milliseconds : 0;
        s.releaseMs = rising ? 0 : milliseconds;
        s.tension = tns;
        s.volume.fill(rising ? 0.0f : 1.0f);
        std::vector<float> warm(100, 1.0f);
        process(engine, warm, s, {120, 0, false});
        s.volume.fill(rising ? 1.0f : 0.0f);
        std::vector<float> result(450, 1.0f);
        process(engine, result, s, {120, 0, false});
        return result;
    };
    const auto attack = transition(true, 200, 0), release = transition(false, 400, 0);
    check(attack[99] > .37f && attack[99] < .42f && attack[199] > .61f && attack[199] < .65f,
          "ATT controls the volume rise time");
    check(release[199] > .59f && release[199] < .62f && release[399] > .35f && release[399] < .39f,
          "REL independently controls the volume fall time");
    check(transition(true, 0, 0)[10] > .999f && transition(false, 0, 0)[10] < .001f,
          "Zero attack and release respond immediately");
    const auto curvedSlow = transition(true, 200, 1), curvedFast = transition(true, 200, -1);
    check(curvedSlow[99] < attack[99] - .1f && curvedFast[99] > attack[99] + .1f,
          "TNS bends stepped attack and release transitions in both directions");
    auto line = [](float tns)
    {
        laggify::TimeVolumeEngine engine;
        engine.prepare(lowRate);
        laggify::Settings s;
        s.enabled = true;
        s.attackMs = s.releaseMs = 0;
        s.volumeSmooth = true;
        s.tension = tns;
        s.volume[0] = 0;
        s.volume[1] = 1;
        std::vector<float> input(120, 1.0f);
        process(engine, input, s, {120, 0, true});
        return input[60];
    };
    check(line(1) < line(0) - .12f && line(-1) > line(0) + .12f,
          "TNS bends line-mode volume segments without moving their endpoints");
    check(laggify::bend(0, -1) == 0 && laggify::bend(1, 1) == 1 && laggify::bend(.5f, 0) == .5f,
          "Tension preserves segment endpoints and neutral linear interpolation");

    laggify::TimeVolumeEngine held;
    held.prepare(lowRate);
    laggify::Settings hold;
    hold.cycle = 0; // 2 beats at 120 BPM = 1000 samples
    std::vector<float> left(2000), right(2000);
    for (int i = 0; i < 2000; ++i)
    {
        left[i] = .1f + .0004f * (i % 1000);
        right[i] = -.05f - .0002f * (i % 1000);
    }
    const auto originalLeft = left, originalRight = right;
    float* capture[] = {left.data(), right.data()};
    held.process(capture, 2, 2000, hold, {120, 0, false});
    hold.enabled = hold.hold = true;
    std::vector<float> loopL(22000, .01f), loopR(22000, .02f);
    float* loop[] = {loopL.data(), loopR.data()};
    held.process(loop, 2, static_cast<int>(loopL.size()), hold, {120, 0, false});
    bool repeats = true;
    for (int i = 0; i < 1000; ++i)
        repeats &= std::abs(loopL[1000 + i] - originalLeft[1000 + i]) < .00001f &&
                   std::abs(loopR[1000 + i] - originalRight[1000 + i]) < .00001f &&
                   std::abs(loopL[21000 + i] - loopL[1000 + i]) < .00001f &&
                   std::abs(loopR[21000 + i] - loopR[1000 + i]) < .00001f;
    check(repeats, "HOLD repeats the last captured stereo cycle beyond the history-buffer length");
    // A saved HOLD can be on before any input exists. It must wait for a full
    // cycle instead of freezing an empty buffer, even inside a large block.
    laggify::TimeVolumeEngine pending;
    pending.prepare(lowRate);
    laggify::Settings pendingSettings;
    pendingSettings.cycle = 0;
    pendingSettings.enabled = pendingSettings.hold = true;
    std::vector<float> startup(1200, .4f);
    std::fill(startup.begin() + 1000, startup.end(), .1f);
    process(pending, startup, pendingSettings, {120, 0, false});
    check(std::abs(startup[500] - .4f) < .001f && std::abs(startup[1020] - .4f) < .001f,
          "HOLD waits for one fresh cycle then captures sample-accurately within a block");
    pendingSettings.hold = false;
    std::vector<float> resumePending(20, .3f);
    process(pending, resumePending, pendingSettings, {120, 0, false});
    pendingSettings.hold = true;
    std::vector<float> beforeFreshCycle(900, .2f);
    process(pending, beforeFreshCycle, pendingSettings, {120, 0, false});
    check(std::abs(beforeFreshCycle[500] - .2f) < .001f,
          "A new HOLD waits for fresh contiguous audio after releasing a frozen loop");
    hold.hold = false;
    std::vector<float> resumed(100, .3f);
    process(held, resumed, hold, {120, 0, false});
    check(std::abs(resumed[50] - .3f) < .0001f, "Releasing HOLD returns to the incoming track");
    hold.hold = true;
    hold.scratch = 1;
    std::vector<float> changedTempo(500, .4f);
    process(held, changedTempo, hold, {400, 0, false});
    check(std::all_of(changedTempo.begin(), changedTempo.end(), [](float v) { return std::isfinite(v); }),
          "Held audio remains bounded when tempo and scratch offset change");
    std::vector<float> bypass(100, .9f);
    float* bypassChannels[] = {bypass.data()};
    held.process(bypassChannels, 1, static_cast<int>(bypass.size()), hold, {120, 0, false}, true);
    check(std::all_of(bypass.begin(), bypass.end(), [](float v) { return v == .9f; }),
          "Host hard bypass releases HOLD and stays bit-identical");

    laggify::TimeVolumeEngine invalid;
    invalid.prepare(lowRate);
    laggify::Settings bad;
    bad.enabled = bad.hold = true;
    bad.scratch = bad.timeMix = bad.volumeMix = bad.tension = bad.attackMs = bad.releaseMs =
        std::numeric_limits<float>::quiet_NaN();
    std::vector<float> safe(500, .3f);
    process(invalid, safe, bad, {120, 0, false});
    check(std::all_of(safe.begin(), safe.end(), [](float v) { return std::isfinite(v); }),
          "Malformed new control values cannot poison held audio");
}
} // namespace
int main()
{
    testPatterns();
    testAudio();
    testExtraControls();
    std::cout << "Laggify core engine tests passed.\n";
}
