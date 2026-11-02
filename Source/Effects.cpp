#include "Effects.h"
#include <algorithm>
#include <cmath>

namespace vanta
{
namespace
{
constexpr double pi = juce::MathConstants<double>::pi;
constexpr double twoPi = juce::MathConstants<double>::twoPi;

float value(const ParameterValues& p, Parameter id) noexcept
{
    const auto& spec = parameterSpecs[id];
    return std::isfinite(p[id]) ? juce::jlimit(spec.minValue, spec.maxValue, p[id]) : spec.defaultValue;
}
float approach(float current, float target, float coefficient) noexcept
{
    current += (target - current) * coefficient;
    return target == 0.0f && current < 1.0e-6f ? 0.0f : current;
}
float tap(const juce::AudioBuffer<float>& buffer, int channel, int write, double samplesBack) noexcept
{
    const int size = buffer.getNumSamples();
    if (size < 3) return 0.0f;
    const double bounded = juce::jlimit(0.0, static_cast<double>(size - 2), samplesBack);
    double read = static_cast<double>(write) - bounded;
    if (read < 0.0) read += size;
    const int a = static_cast<int>(read), b = a + 1 == size ? 0 : a + 1;
    const auto fraction = static_cast<float>(read - a);
    const auto x = buffer.getSample(channel, a), y = buffer.getSample(channel, b);
    return x + (y - x) * fraction;
}
void advance(double& phase, double increment) noexcept
{
    phase += increment;
    if (phase >= 1.0) phase -= std::floor(phase);
}
}

void PostEffects::prepare(double sampleRate)
{
    rate = std::isfinite(sampleRate) && sampleRate >= 1000.0 ? sampleRate : 44100.0;
    chorusHistory.setSize(2, static_cast<int>(std::ceil(std::min(rate * 0.06, 23040.0))) + 3);
    flangerHistory.setSize(2, static_cast<int>(std::ceil(std::min(rate * 0.02, 7680.0))) + 3);
    reset();
}

void PostEffects::reset()
{
    chorusHistory.clear(); flangerHistory.clear();
    chorusWrite = flangerWrite = 0;
    chorusPhase = flangerPhase = phaserPhase = 0.0;
    std::fill(std::begin(flangerLast), std::end(flangerLast), 0.0f);
    std::fill(std::begin(eqLowState), std::end(eqLowState), 0.0f);
    std::fill(std::begin(eqHighState), std::end(eqHighState), 0.0f);
    std::fill(std::begin(filterIc1), std::end(filterIc1), 0.0f);
    std::fill(std::begin(filterIc2), std::end(filterIc2), 0.0f);
    for (auto& channel : phaserState) std::fill(std::begin(channel), std::end(channel), 0.0f);
    std::fill(std::begin(phaserLast), std::end(phaserLast), 0.0f);
    chorusAmount = eqAmount = filterAmount = flangerAmount = phaserAmount = 0.0f;
}

void PostEffects::process(juce::AudioBuffer<float>& buffer, const ParameterValues& p)
{
    if (chorusHistory.getNumSamples() < 3 || flangerHistory.getNumSamples() < 3) return;
    const int channels = juce::jmin(2, buffer.getNumChannels());
    const auto smooth = static_cast<float>(1.0 - std::exp(-1.0 / (rate * 0.004)));
    const float chorusTarget = value(p, chorusEnabled) >= 0.5f ? value(p, chorusMix) : 0.0f;
    const float eqTarget = value(p, eqEnabled) >= 0.5f ? value(p, eqMix) : 0.0f;
    const float filterTarget = value(p, fxFilterEnabled) >= 0.5f ? value(p, fxFilterMix) : 0.0f;
    const float flangerTarget = value(p, flangerEnabled) >= 0.5f ? value(p, flangerMix) : 0.0f;
    const float phaserTarget = value(p, phaserEnabled) >= 0.5f ? value(p, phaserMix) : 0.0f;
    const auto chorusDepthValue = value(p, chorusDepth), flangerDepthValue = value(p, flangerDepth);
    const auto phaserDepthValue = value(p, phaserDepth);
    const auto flangerFeedbackValue = value(p, flangerFeedback), phaserFeedbackValue = value(p, phaserFeedback);
    const auto lowGain = std::pow(10.0f, value(p, eqLow) / 20.0f);
    const auto midGain = std::pow(10.0f, value(p, eqMid) / 20.0f);
    const auto highGain = std::pow(10.0f, value(p, eqHigh) / 20.0f);
    const auto lowAlpha = static_cast<float>(1.0 - std::exp(-twoPi * juce::jmin(250.0, rate * 0.45) / rate));
    const auto highAlpha = static_cast<float>(1.0 - std::exp(-twoPi * juce::jmin(2500.0, rate * 0.45) / rate));
    const double cutoff = juce::jlimit(20.0, rate * 0.45, static_cast<double>(value(p, fxFilterCutoff)));
    const double g = std::tan(pi * cutoff / rate);
    const double k = 1.0 / (0.707 + 5.0 * value(p, fxFilterResonance));
    const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const double chorusIncrement = value(p, chorusRate) / rate;
    const double flangerIncrement = value(p, flangerRate) / rate;
    const double phaserIncrement = value(p, phaserRate) / rate;

    // Keep delay histories warm even when bypassed. Each new effect has a
    // short mix ramp so toggles and automation do not produce hard clicks.
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        chorusAmount = approach(chorusAmount, chorusTarget, smooth);
        eqAmount = approach(eqAmount, eqTarget, smooth);
        filterAmount = approach(filterAmount, filterTarget, smooth);
        flangerAmount = approach(flangerAmount, flangerTarget, smooth);
        phaserAmount = approach(phaserAmount, phaserTarget, smooth);
        for (int c = 0; c < channels; ++c)
        {
            float s = buffer.getSample(c, i);
            chorusHistory.setSample(c, chorusWrite, s);
            if (chorusAmount > 0.0f)
            {
                const double lfo = std::sin(twoPi * (chorusPhase + (c == 1 ? 0.25 : 0.0)));
                const double delay = rate * (0.014 + 0.008 * chorusDepthValue * lfo);
                const float wet = 0.5f * (s + tap(chorusHistory, c, chorusWrite, delay));
                s += chorusAmount * (wet - s);
            }

            // Complementary crossovers reconstruct the input at 0 dB.
            eqLowState[c] += lowAlpha * (s - eqLowState[c]);
            eqHighState[c] += highAlpha * (s - eqHighState[c]);
            if (eqAmount > 0.0f)
            {
                const auto wet = eqLowState[c] * lowGain + (eqHighState[c] - eqLowState[c]) * midGain
                                 + (s - eqHighState[c]) * highGain;
                s += eqAmount * (wet - s);
            }

            const double v3 = s - filterIc2[c], v1 = a1 * filterIc1[c] + a2 * v3;
            const double v2 = filterIc2[c] + a2 * filterIc1[c] + a3 * v3;
            filterIc1[c] = static_cast<float>(2.0 * v1 - filterIc1[c]);
            filterIc2[c] = static_cast<float>(2.0 * v2 - filterIc2[c]);
            if (filterAmount > 0.0f) s += filterAmount * (static_cast<float>(v2) - s);

            const auto feedback = flangerAmount > 0.0f ? flangerFeedbackValue : 0.0f;
            flangerHistory.setSample(c, flangerWrite, juce::jlimit(-4.0f, 4.0f, s + feedback * flangerLast[c]));
            if (flangerAmount > 0.0f)
            {
                const double lfo = std::sin(twoPi * (flangerPhase + (c == 1 ? 0.2 : 0.0)));
                const double delay = rate * (0.0045 + 0.0035 * flangerDepthValue * lfo);
                flangerLast[c] = tap(flangerHistory, c, flangerWrite, delay);
                const float wet = 0.5f * (s + flangerLast[c]);
                s += flangerAmount * (wet - s);
            }
            else flangerLast[c] = 0.0f;

            if (phaserAmount > 0.0f)
            {
                const double lfo = std::sin(twoPi * (phaserPhase + (c == 1 ? 0.25 : 0.0)));
                const double sweep = 0.5 + 0.5 * phaserDepthValue * lfo;
                const double frequency = juce::jmin(rate * 0.44, 250.0 * std::pow(16.0, sweep));
                const double tangent = std::tan(pi * frequency / rate);
                const auto coefficient = static_cast<float>((1.0 - tangent) / (1.0 + tangent));
                float shifted = juce::jlimit(-4.0f, 4.0f, s + phaserFeedbackValue * phaserLast[c]);
                for (float& state : phaserState[c])
                {
                    const float output = -coefficient * shifted + state;
                    state = shifted + coefficient * output;
                    shifted = output;
                }
                phaserLast[c] = shifted;
                const float wet = 0.5f * (s + shifted);
                s += phaserAmount * (wet - s);
            }
            if (!std::isfinite(s)) s = 0.0f;
            buffer.setSample(c, i, juce::jlimit(-1.0f, 1.0f, s));
        }
        if (++chorusWrite == chorusHistory.getNumSamples()) chorusWrite = 0;
        if (++flangerWrite == flangerHistory.getNumSamples()) flangerWrite = 0;
        advance(chorusPhase, chorusIncrement); advance(flangerPhase, flangerIncrement); advance(phaserPhase, phaserIncrement);
    }
}

}
