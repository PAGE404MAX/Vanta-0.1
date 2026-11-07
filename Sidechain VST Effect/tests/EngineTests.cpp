#include "SidechainEngine.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <vector>

static std::atomic<int> allocations{0};
void* operator new(std::size_t n)
{
    allocations.fetch_add(1, std::memory_order_relaxed);
    if (auto* p = std::malloc(n))
        return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept
{
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}
void* operator new[](std::size_t n)
{
    return ::operator new(n);
}
void operator delete[](void* p) noexcept
{
    ::operator delete(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
    ::operator delete(p);
}
void check(bool ok, const char* text)
{
    if (!ok)
    {
        std::cerr << "FAIL: " << text << '\n';
        std::exit(1);
    }
    std::cout << "PASS: " << text << '\n';
}
bool near(float a, float b, float epsilon = 1e-5f)
{
    return std::abs(a - b) < epsilon;
}
void flat(sidechain::ModuleSettings& m, float v)
{
    for (auto& b : m.band)
    {
        b.curve.fill(v);
        b.stepped = true;
        b.amount = 1;
        b.detail = .5f;
        b.mix = 1;
    }
    m.smooth = 0;
}
int main()
{
    using namespace sidechain;
    constexpr int n = 256;
    std::array<float, n> l{}, r{}, key{};
    Engine engine;
    engine.prepare(48000);
    Settings s;
    l.fill(.8f);
    r.fill(-.4f);
    engine.process(l.data(), r.data(), nullptr, nullptr, n, s, {});
    check(std::all_of(l.begin(), l.end(), [](float x) { return near(x, .8f); }) &&
              std::all_of(r.begin(), r.end(), [](float x) { return near(x, -.4f); }),
          "No external key leaves the default volume shaper transparent");
    flat(s.module[Volume], 0);
    s.module[Volume].attackMs = 0;
    s.module[Volume].releaseMs = 0;
    l.fill(.8f);
    r.fill(-.4f);
    key.fill(0);
    key[17] = 1;
    engine.process(l.data(), r.data(), key.data(), nullptr, n, s, {});
    check(near(l[16], 0.0f) && near(l[17], 0.0f) && near(r[17], 0.0f),
          "External key triggers the volume curve on both channels");
    // A fresh engine starts at the curve's final value until a trigger.
    engine.reset();
    s.module[Volume].band[0].curve.fill(0);
    s.module[Volume].band[0].curve[15] = 1;
    l.fill(.8f);
    r.fill(-.4f);
    key.fill(0);
    key[17] = 1;
    engine.process(l.data(), r.data(), key.data(), nullptr, n, s, {});
    check(near(l[16], .8f) && near(l[17], 0) && near(r[17], 0),
          "Only external-key audio, not the main input, starts ducking at the correct sample");
    check(engine.keyLevel() > .9f, "A short sidechain transient remains visible on the level meter");
    engine.process(l.data(), r.data(), nullptr, nullptr, n, s, {}, nullptr, 0, true);
    check(engine.keyLevel() == 0, "Host bypass clears the sidechain meter");
    engine.reset();
    s.module[Volume].source = Midi;
    l.fill(.8f);
    r.fill(-.4f);
    const int midiOn = 63;
    engine.process(l.data(), r.data(), nullptr, nullptr, n, s, {}, &midiOn, 1);
    check(near(l[62], .8f) && near(l[63], 0), "MIDI notes retrigger at the supplied sample offset");
    engine.reset();
    s.module[Volume].source = Sync;
    for (auto& b : s.module[Volume].band)
    {
        for (int i = 0; i < stepCount; ++i)
            b.curve[i] = i < 8 ? 0.0f : 1.0f;
    }
    l.fill(1);
    r.fill(1);
    engine.process(l.data(), r.data(), nullptr, nullptr, n, s, {120, 0, true});
    check(near(l[0], 0), "Host tempo and PPQ start the sync curve at its first step");
    engine.reset();
    l.fill(1);
    r.fill(1);
    engine.process(l.data(), r.data(), nullptr, nullptr, n, s, {120, .75, true});
    check(near(l[0], 1), "Sync follows host PPQ instead of the external key");
    engine.reset();
    s.module[Volume].source = External;
    s.module[Volume].oneShot = false;
    key.fill(0);
    l.fill(1);
    r.fill(1);
    engine.process(l.data(), r.data(), key.data(), nullptr, n, s, {});
    check(near(l[0], 1) && near(l[200], 1), "Looping external mode waits for its first key trigger");
    Settings multi;
    flat(multi.module[Volume], 1);
    multi.module[Volume].source = Sync;
    multi.module[Volume].multiband = true;
    multi.module[Volume].band[1].curve.fill(0);
    constexpr int samples = 24000;
    std::vector<float> low(samples), lowR(samples), high(samples), highR(samples);
    for (int i = 0; i < samples; ++i)
    {
        low[i] = lowR[i] = .5f * std::sin(2 * 3.141592653589793 * 50 * i / 48000);
        high[i] = highR[i] = .5f * std::sin(2 * 3.141592653589793 * 6000 * i / 48000);
    }
    engine.reset();
    engine.process(low.data(), lowR.data(), nullptr, nullptr, samples, multi, {120, 0, true});
    engine.reset();
    engine.process(high.data(), highR.data(), nullptr, nullptr, samples, multi, {120, 0, true});
    double lowEnergy = 0, highEnergy = 0;
    for (int i = 12000; i < samples; ++i)
    {
        lowEnergy += low[i] * low[i];
        highEnergy += high[i] * high[i];
    }
    check(lowEnergy < highEnergy * .2, "Independent low-band ducking leaves high frequencies mostly intact");
    Settings bypass;
    flat(bypass.module[Volume], 0);
    bypass.module[Volume].source = Sync;
    l.fill(1.5f);
    r.fill(-2.0f);
    engine.process(l.data(), r.data(), nullptr, nullptr, n, bypass, {}, nullptr, 0, true);
    check(l[0] == 1.5f && r[0] == -2.0f, "Host bypass is exactly dry");
    bypass.enabled = false;
    engine.process(l.data(), r.data(), nullptr, nullptr, n, bypass, {});
    check(l[0] == 1.5f && r[0] == -2.0f, "Master bypass is exactly dry");
    bypass.enabled = true;
    bypass.masterMix = 0;
    engine.process(l.data(), r.data(), nullptr, nullptr, n, bypass, {});
    check(l[0] == 1.5f && r[0] == -2.0f, "Zero master mix is exactly dry");
    Settings mono;
    for (auto& m : mono.module)
        m.enabled = false;
    mono.module[Pan].enabled = true;
    flat(mono.module[Pan], 1);
    mono.module[Pan].source = Sync;
    l.fill(.42f);
    engine.process(l.data(), nullptr, nullptr, nullptr, n, mono, {120, 0, true});
    check(near(l[0], .42f), "Panning a mono bus does not alter its gain");
    constexpr int count = 15000;
    std::vector<float> inputL(count), inputR(count), a(count), b(count);
    for (int i = 0; i < count; ++i)
    {
        inputL[i] = .6f * std::sin(2 * 3.141592653589793 * 239 * i / 48000) + .1f;
        inputR[i] = .35f * std::sin(2 * 3.141592653589793 * 521 * i / 48000);
    }
    for (int module = 0; module < moduleCount; ++module)
    {
        Settings one;
        for (auto& m : one.module)
            m.enabled = false;
        auto& m = one.module[module];
        m.enabled = true;
        m.source = Sync;
        flat(m, module == Filter ? .15f : module == Volume ? .2f : .85f);
        m.band[0].detail = .7f;
        if (module == Compressor)
            m.band[0].amount = .3f;
        engine.reset();
        a = inputL;
        b = inputR;
        engine.process(a.data(), b.data(), nullptr, nullptr, count, one, {120, 0, true});
        double diff = 0;
        bool finite = true;
        for (int i = 4000; i < count; ++i)
        {
            diff += std::abs(a[i] - inputL[i]) + std::abs(b[i] - inputR[i]);
            finite = finite && std::isfinite(a[i]) && std::isfinite(b[i]);
        }
        std::cout << "module " << module << " difference " << diff << '\n';
        check(finite && diff > 1, "Each enabled shaper has an audible finite effect");
    }
    // Every visible secondary control must change the actual audio, not only
    // the drawing. Use matched inputs and reset delay/noise state each time.
    std::vector<float> otherL(count), otherR(count);
    for (int module = 0; module < moduleCount; ++module)
    {
        Settings one;
        for (auto& m : one.module)
            m.enabled = false;
        auto& m = one.module[module];
        m.enabled = true;
        m.source = Sync;
        flat(m, module == Filter ? .15f : module == Time ? .4f : .85f);
        if (module == Compressor)
            m.band[0].amount = .3f;
        m.band[0].detail = 0.0f;
        engine.reset();
        a = inputL;
        b = inputR;
        engine.process(a.data(), b.data(), nullptr, nullptr, count, one, {120, 0, true});
        m.band[0].detail = 1.0f;
        engine.reset();
        otherL = inputL;
        otherR = inputR;
        engine.process(otherL.data(), otherR.data(), nullptr, nullptr, count, one, {120, 0, true});
        double diff = 0;
        for (int i = 0; i < count; ++i)
            diff += std::abs(a[i] - otherL[i]) + std::abs(b[i] - otherR[i]);
        std::cout << "secondary control " << module << " difference " << diff << '\n';
        check(diff > 1.0, "Each shaper's secondary control changes audio");
    }
    Settings fuzz;
    for (int m = 0; m < moduleCount; ++m)
    {
        fuzz.module[m].enabled = true;
        fuzz.module[m].source = Sync;
        fuzz.module[m].multiband = true;
        fuzz.module[m].thresholdDb = std::numeric_limits<float>::quiet_NaN();
        for (auto& bnd : fuzz.module[m].band)
        {
            bnd.amount = std::numeric_limits<float>::infinity();
            bnd.curve[0] = std::numeric_limits<float>::quiet_NaN();
        }
    }
    for (double rate : {8000.0, 44100.0, 192000.0, 384000.0})
    {
        engine.prepare(rate);
        l.fill(.8f);
        r.fill(-.6f);
        l[5] = std::numeric_limits<float>::quiet_NaN();
        const int before = allocations.load();
        engine.process(l.data(), r.data(), nullptr, nullptr, n, fuzz,
                       {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), true});
        const int after = allocations.load();
        check(before == after, "Processing allocates no heap memory");
        check(std::all_of(l.begin(), l.end(), [](float x) { return std::isfinite(x) && std::abs(x) <= 64; }) &&
                  std::all_of(r.begin(), r.end(), [](float x) { return std::isfinite(x) && std::abs(x) <= 64; }),
              "Invalid values and extreme sample rates remain finite and bounded");
    }
    std::cout << "All sidechain engine tests passed.\n";
}
