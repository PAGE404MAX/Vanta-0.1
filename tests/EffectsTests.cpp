#include "Effects.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
    std::cout << "PASS: " << message << '\n';
}
constexpr int rate = 44100;
juce::AudioBuffer<float> signal(int count, int offset = 0)
{
    juce::AudioBuffer<float> result(2, count);
    for (int i = 0; i < count; ++i)
    {
        const auto t = static_cast<double>(i + offset) / rate;
        const auto a = static_cast<float>(0.28 * std::sin(2.0 * juce::MathConstants<double>::pi * 233.0 * t)
                                          + 0.11 * std::sin(2.0 * juce::MathConstants<double>::pi * 3179.0 * t));
        result.setSample(0, i, a); result.setSample(1, i, a * 0.73f);
    }
    return result;
}
double difference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double sum = 0.0;
    for (int c = 0; c < a.getNumChannels(); ++c)
        for (int i = 0; i < a.getNumSamples(); ++i) sum += std::abs(a.getSample(c, i) - b.getSample(c, i));
    return sum / (a.getNumChannels() * a.getNumSamples());
}
bool finiteAndBounded(const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (!std::isfinite(b.getSample(c, i)) || std::abs(b.getSample(c, i)) > 1.0f) return false;
    return true;
}
void testEffects()
{
    const auto dry = signal(rate);
    auto defaults = vanta::defaultValues();
    vanta::PostEffects bypass; bypass.prepare(rate);
    auto unchanged = dry; bypass.process(unchanged, defaults);
    check(difference(dry, unchanged) == 0.0, "All new effects bypass bit-identically by default");

    const vanta::Parameter switches[] = { vanta::chorusEnabled, vanta::eqEnabled, vanta::fxFilterEnabled,
                                           vanta::flangerEnabled, vanta::phaserEnabled };
    const char* names[] = { "Chorus processes audio", "Equalizer processes audio", "FX filter processes audio",
                            "Flanger processes audio", "Phaser processes audio" };
    for (int effect = 0; effect < 5; ++effect)
    {
        auto p = defaults; p[switches[effect]] = 1.0f;
        p[vanta::eqLow] = 9.0f; p[vanta::eqHigh] = -9.0f;
        vanta::PostEffects fx; fx.prepare(rate);
        auto wet = dry; fx.process(wet, p);
        check(difference(dry, wet) > 0.001 && finiteAndBounded(wet), names[effect]);
    }
    auto p = defaults;
    for (const auto enabled : switches) p[enabled] = 1.0f;
    p[vanta::eqLow] = 12.0f; p[vanta::eqMid] = -12.0f; p[vanta::eqHigh] = 12.0f;
    p[vanta::fxFilterCutoff] = 30.0f; p[vanta::fxFilterResonance] = 0.95f;
    p[vanta::flangerFeedback] = 0.8f; p[vanta::phaserFeedback] = -0.8f;
    for (const auto sr : { 8000.0, 44100.0, 96000.0, 192000.0 })
    {
        vanta::PostEffects fx; fx.prepare(sr);
        auto wet = signal(rate / 2); fx.process(wet, p);
        check(finiteAndBounded(wet), "All effects remain finite and bounded at sample-rate and feedback extremes");
    }
    p[vanta::chorusRate] = std::numeric_limits<float>::quiet_NaN();
    vanta::PostEffects malformed; malformed.prepare(rate);
    auto wet = signal(1024); malformed.process(wet, p);
    check(finiteAndBounded(wet), "Invalid parameter snapshots cannot produce invalid output");
}
}
int main()
{
    testEffects();
    std::cout << "All VANTA effects tests passed.\n";
    return 0;
}
