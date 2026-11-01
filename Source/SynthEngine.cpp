#include "SynthEngine.h"
#include <algorithm>
#include <cmath>

namespace vanta
{
namespace
{
constexpr double pi = juce::MathConstants<double>::pi;
constexpr double twoPi = juce::MathConstants<double>::twoPi;

// Smooth the discontinuity in a sawtooth to reduce high-frequency aliasing.
double polyBlep(double t, double increment)
{
    if (increment <= 0.0) return 0.0;
    if (t < increment) { t /= increment; return t + t - t * t - 1.0; }
    if (t > 1.0 - increment) { t = (t - 1.0) / increment; return t * t + t + t + 1.0; }
    return 0.0;
}
}

class SynthEngine::Sound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class SynthEngine::Voice final : public juce::SynthesiserVoice
{
public:
    explicit Voice(SharedParameters& p) : parameters(p) {}
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<Sound*>(s) != nullptr; }

    void startNote(int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override
    {
        phase = lfoPhase = 0.0;
        ic1 = ic2 = 0.0f;
        amplitude = juce::jlimit(0.0f, 1.0f, velocity) * 0.12f;
        note = midiNote;
        wheel = pitchWheel;
        updatePitch();
        envelope.setSampleRate(getSampleRate());
        hasEnvelopeParameters = false;
        updateEnvelope();
        envelope.noteOn();
    }
    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff) { updateEnvelope(); envelope.noteOff(); }
        else { envelope.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved(int value) override { wheel = value; updatePitch(); }
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& output, int start, int count) override
    {
        if (!isVoiceActive() || getSampleRate() <= 0.0) return;
        updateEnvelope();
        const auto& p = parameters.values;
        const auto frequency = juce::jlimit(20.0, getSampleRate() * 0.45, static_cast<double>(p[cutoff]));
        const auto g = std::tan(pi * frequency / getSampleRate());
        const auto q = 0.707 + 5.0 * static_cast<double>(p[resonance]);
        const auto k = 1.0 / q;
        const auto a1 = 1.0 / (1.0 + g * (g + k));
        const auto a2 = g * a1;
        const auto a3 = g * a2;
        const auto lfoIncrement = twoPi * p[vibratoRate] / getSampleRate();

        for (int i = 0; i < count; ++i)
        {
            const double vibratoSemitones = std::sin(lfoPhase) * static_cast<double>(p[vibrato]);
            const auto increment = juce::jlimit(0.0, 0.49, baseFrequency * std::pow(2.0, vibratoSemitones / 12.0) / getSampleRate());
            double sample = (p[wave] < 0.5f) ? std::sin(phase * twoPi)
                                             : (2.0 * phase - 1.0 - polyBlep(phase, increment));
            phase += increment;
            if (phase >= 1.0) phase -= 1.0;
            lfoPhase += lfoIncrement;
            if (lfoPhase >= twoPi) lfoPhase -= twoPi;

            // Stable topology-preserving state-variable low-pass, one state per voice.
            const double v3 = sample - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = static_cast<float>(2.0 * v1 - ic1);
            ic2 = static_cast<float>(2.0 * v2 - ic2);
            const auto value = static_cast<float>(v2) * amplitude * envelope.getNextSample();
            for (int channel = 0; channel < output.getNumChannels(); ++channel)
                output.addSample(channel, start + i, value);
        }
        if (!envelope.isActive()) clearCurrentNote();
    }

private:
    void updateEnvelope()
    {
        const auto& p = parameters.values;
        juce::ADSR::Parameters adsr;
        adsr.attack = p[attack]; adsr.decay = p[decay];
        adsr.sustain = p[sustain]; adsr.release = p[release];
        if (!hasEnvelopeParameters || adsr.attack != previousEnvelope.attack || adsr.decay != previousEnvelope.decay
            || adsr.sustain != previousEnvelope.sustain || adsr.release != previousEnvelope.release)
        {
            envelope.setParameters(adsr);
            previousEnvelope = adsr;
            hasEnvelopeParameters = true;
        }
    }
    void updatePitch()
    {
        const double semitones = (static_cast<double>(wheel) - 8192.0) / 8192.0 * 2.0;
        baseFrequency = juce::MidiMessage::getMidiNoteInHertz(note) * std::pow(2.0, semitones / 12.0);
    }
    SharedParameters& parameters;
    juce::ADSR envelope;
    juce::ADSR::Parameters previousEnvelope;
    bool hasEnvelopeParameters = false;
    double phase = 0.0, lfoPhase = 0.0, baseFrequency = 440.0;
    float ic1 = 0.0f, ic2 = 0.0f, amplitude = 0.0f;
    int note = 69, wheel = 8192;
};

SynthEngine::SynthEngine()
{
    synth.addSound(new Sound());
    for (int i = 0; i < 16; ++i) synth.addVoice(new Voice(parameters));
}
void SynthEngine::prepare(double rate, int maximumBlockSize)
{
    sampleRate = rate > 0.0 ? rate : 44100.0;
    synth.setCurrentPlaybackSampleRate(sampleRate);
    reverb.setSampleRate(sampleRate);
    delayBuffer.setSize(2, static_cast<int>(std::ceil(sampleRate * 1.5)) + juce::jmax(1, maximumBlockSize) + 2);
    reset();
}
void SynthEngine::reset()
{
    synth.allNotesOff(0, false);
    reverb.reset();
    delayBuffer.clear();
    delayWritePosition = 0;
    distortionState[0] = distortionState[1] = 0.0f;
}
void SynthEngine::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, const ParameterValues& values)
{
    parameters.values = values;
    buffer.clear();
    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.12f + 0.86f * values[reverbSize];
    rp.damping = 0.8f * (1.0f - values[reverbDecay]);
    // JUCE's reverb scales wet by 3 and dry by 2 internally.
    rp.wetLevel = values[reverbMix] / 3.0f;
    rp.dryLevel = 0.5f * (1.0f - values[reverbMix]);
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters(rp);
    if (buffer.getNumChannels() >= 2)
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
    else if (buffer.getNumChannels() == 1)
        reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());

    if (delayBuffer.getNumSamples() == 0) { buffer.applyGain(values[level]); return; }
    const int delayLength = delayBuffer.getNumSamples();
    const int delaySamples = juce::jlimit(1, delayLength - 1, static_cast<int>(values[delayTime] * sampleRate));
    const float delayMixValue = values[delayMix];
    const float feedback = values[delayFeedback];
    const float distortionMixValue = values[distortionMix];
    const float drive = values[distortionDrive];
    const float normalisation = 1.0f / std::tanh(drive);
    const double toneFrequency = 400.0 + 14000.0 * values[distortionTone];
    const float toneAlpha = static_cast<float>(1.0 - std::exp(-twoPi * toneFrequency / sampleRate));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const int readPosition = (delayWritePosition + delayLength - delaySamples) % delayLength;
        for (int channel = 0; channel < juce::jmin(2, buffer.getNumChannels()); ++channel)
        {
            const float input = buffer.getSample(channel, i);
            const float delayed = delayBuffer.getSample(channel, readPosition);
            delayBuffer.setSample(channel, delayWritePosition, input + delayed * feedback);
            const float delayedMix = input * (1.0f - delayMixValue) + delayed * delayMixValue;
            const float distorted = std::tanh(delayedMix * drive) * normalisation;
            distortionState[channel] += toneAlpha * (distorted - distortionState[channel]);
            const float out = (delayedMix * (1.0f - distortionMixValue) + distortionState[channel] * distortionMixValue) * values[level];
            buffer.setSample(channel, i, juce::jlimit(-1.0f, 1.0f, out));
        }
        if (++delayWritePosition == delayLength) delayWritePosition = 0;
    }
}
}
