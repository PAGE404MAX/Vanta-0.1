#include "SynthEngine.h"
#include "AmpEnvelope.h"
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

double oscillatorSample(int shape, double phase, double increment)
{
    switch (shape)
    {
        case 1: return 2.0 * phase - 1.0 - polyBlep(phase, increment);
        case 2:
        {
            const double secondEdge = phase < 0.5 ? phase + 0.5 : phase - 0.5;
            return (phase < 0.5 ? 1.0 : -1.0) + polyBlep(phase, increment) - polyBlep(secondEdge, increment);
        }
        case 3: return 1.0 - 4.0 * std::abs(phase - 0.5);
        default: return std::sin(phase * twoPi);
    }
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
        phases.fill(0.0); samplePositions.fill(0.0); importedLfoPhases.fill(0.0); lfoPhase = 0.0;
        ic1.fill(0.0f); ic2.fill(0.0f);
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
        struct Layer
        {
            bool active = false; int shape = 0; double rate = 0.0;
            float left = 0.0f, right = 0.0f, level = 0.0f, pan = 0.0f;
            const WaveTable* table = nullptr; const PcmSample* pcm = nullptr; double sampleRate = 0.0; int frameA = 0, frameB = 0; float frameBlend = 0.0f;
        };
        std::array<Layer, oscillatorCount> layers {};
        float totalLevel = 0.0f;
        for (int layer = 0; layer < oscillatorCount; ++layer)
        {
            auto& entry = layers[static_cast<std::size_t>(layer)];
            entry.active = oscillatorIsEnabled(p, layer);
            if (!entry.active) continue;
            entry.shape = juce::jlimit(0, 3, oscillatorShape(p, layer));
            if (parameters.tables != nullptr && p[oscillatorSample(layer)] >= 0.5f)
            {
                const auto& sample = parameters.tables->sample;
                if (sample.slot == layer && sample.frames > 0 && sample.frames <= maxSampleFrames)
                {
                    entry.pcm = &sample;
                    const double track = sample.keytrack ? baseFrequency / 261.625565 : 1.0;
                    const double tune = (p[oscillatorTune(layer)] + p[oscillatorFine(layer)] * 0.01) / 12.0;
                    entry.sampleRate = sample.sampleRate / getSampleRate() * track * std::pow(2.0, tune);
                }
            }
            else if (parameters.tables != nullptr && p[oscillatorTable(layer)] >= 0.5f)
            {
                const auto& table = parameters.tables->tables[static_cast<std::size_t>(layer)];
                if (table.frameCount > 0 && table.frameCount <= maxWavetableFrames)
                {
                    entry.table = &table;
                    const auto position = juce::jlimit(0.0f, 1.0f, p[oscillatorTablePosition(layer)]);
                    while (entry.frameB + 1 < table.frameCount && table.positions[static_cast<std::size_t>(entry.frameB)] < position)
                        ++entry.frameB;
                    entry.frameA = juce::jmax(0, entry.frameB - 1);
                    const auto a = table.positions[static_cast<std::size_t>(entry.frameA)];
                    const auto b = table.positions[static_cast<std::size_t>(entry.frameB)];
                    entry.frameBlend = b > a ? juce::jlimit(0.0f, 1.0f, (position - a) / (b - a)) : 0.0f;
                }
            }
            const auto level = juce::jlimit(0.0f, 1.0f, p[oscillatorLevel(layer)]);
            const auto pan = juce::jlimit(-1.0f, 1.0f, p[oscillatorPan(layer)]);
            const auto semitones = static_cast<double>(p[oscillatorTune(layer)] + p[oscillatorFine(layer)] * 0.01f);
            entry.rate = baseFrequency * std::pow(2.0, semitones / 12.0) / getSampleRate();
            entry.level = level; entry.pan = pan;
            entry.left = level * (pan > 0.0f ? 1.0f - pan : 1.0f);
            entry.right = level * (pan < 0.0f ? 1.0f + pan : 1.0f);
            totalLevel += level;
        }
        // Preserve the original single-oscillator level; prevent a stack of
        // full-volume layers from multiplying its peak by eight.
        const auto mixScale = 1.0 / static_cast<double>(juce::jmax(1.0f, totalLevel));
        const int channels = juce::jmin(2, output.getNumChannels());
        for (int i = 0; i < count; ++i)
        {
            const double vibratoSemitones = std::sin(lfoPhase) * static_cast<double>(p[vibrato]);
            const double pitchFactor = p[vibrato] == 0.0f ? 1.0 : std::pow(2.0, vibratoSemitones / 12.0);
            std::array<float, oscillatorCount> levelMod {}, panMod {}, pitchMod {}, tableMod {};
            const auto* modulation = parameters.tables != nullptr ? &parameters.tables->modulation : nullptr;
            if (modulation != nullptr && modulation->routeCount > 0)
            {
                std::array<float, importedLfoCount> lfoValues {};
                for (int l = 0; l < importedLfoCount; ++l)
                {
                    const auto& lfo = modulation->lfos[static_cast<std::size_t>(l)];
                    auto& phase = importedLfoPhases[static_cast<std::size_t>(l)];
                    const double position = phase * importedLfoPoints;
                    const int first = static_cast<int>(position) & (importedLfoPoints - 1);
                    const int next = (first + 1) & (importedLfoPoints - 1);
                    const auto a = lfo.shape[static_cast<std::size_t>(first)], b = lfo.shape[static_cast<std::size_t>(next)];
                    lfoValues[static_cast<std::size_t>(l)] = a + (b - a) * static_cast<float>(position - static_cast<int>(position));
                    phase += static_cast<double>(lfo.rateHz) / getSampleRate();
                    if (phase >= 1.0) phase -= std::floor(phase);
                }
                for (int r = 0; r < modulation->routeCount && r < maxImportedRoutes; ++r)
                {
                    const auto& route = modulation->routes[static_cast<std::size_t>(r)];
                    if (route.source < 0 || route.source >= importedLfoCount || route.oscillator < 0 || route.oscillator >= oscillatorCount) continue;
                    const auto source = lfoValues[static_cast<std::size_t>(route.source)];
                    const auto amount = (route.bipolar ? source * 2.0f - 1.0f : source) * route.amount;
                    auto index = static_cast<std::size_t>(route.oscillator);
                    switch (route.target)
                    {
                        case ModTarget::level: levelMod[index] += amount; break;
                        case ModTarget::pan: panMod[index] += amount; break;
                        case ModTarget::pitch: pitchMod[index] += amount; break;
                        case ModTarget::tablePosition: tableMod[index] += amount; break;
                    }
                }
            }
            double mixed[2] { 0.0, 0.0 };
            for (int layer = 0; layer < oscillatorCount; ++layer)
            {
                const auto& entry = layers[static_cast<std::size_t>(layer)];
                if (!entry.active) continue;
                const auto layerIndex = static_cast<std::size_t>(layer);
                auto& phase = phases[layerIndex];
                const auto pitch = pitchMod[layerIndex] == 0.0f ? 1.0 : std::pow(2.0, static_cast<double>(juce::jlimit(-48.0f, 48.0f, pitchMod[layerIndex])) / 12.0);
                const auto increment = juce::jlimit(0.0, 0.49, entry.rate * pitchFactor * pitch);
                const auto currentLevel = juce::jlimit(0.0f, 1.0f, entry.level + levelMod[layerIndex]);
                const auto currentPan = juce::jlimit(-1.0f, 1.0f, entry.pan + panMod[layerIndex]);
                const auto leftGain = levelMod[layerIndex] == 0.0f && panMod[layerIndex] == 0.0f ? entry.left : currentLevel * (currentPan > 0.0f ? 1.0f - currentPan : 1.0f);
                const auto rightGain = levelMod[layerIndex] == 0.0f && panMod[layerIndex] == 0.0f ? entry.right : currentLevel * (currentPan < 0.0f ? 1.0f + currentPan : 1.0f);
                double sample;
                if (p[oscillatorSample(layer)] >= 0.5f)
                {
                    // A missing or finished one-shot is silent, not a fabricated waveform.
                    const auto* pcm = entry.pcm;
                    auto& position = samplePositions[static_cast<std::size_t>(layer)];
                    if (pcm != nullptr && position < pcm->frames)
                    {
                        const int first = static_cast<int>(position);
                        const int next = first + 1 < pcm->frames ? first + 1 : (pcm->loop ? 0 : first);
                        const auto fraction = static_cast<float>(position - first);
                        const auto l = pcm->left[static_cast<std::size_t>(first)] + (pcm->left[static_cast<std::size_t>(next)] - pcm->left[static_cast<std::size_t>(first)]) * fraction;
                        const auto r = pcm->channels == 2 ? pcm->right[static_cast<std::size_t>(first)] + (pcm->right[static_cast<std::size_t>(next)] - pcm->right[static_cast<std::size_t>(first)]) * fraction : l;
                        mixed[0] += l * leftGain * mixScale; mixed[1] += r * rightGain * mixScale;
                        position += entry.sampleRate * pitch * (pcm->keytrack ? pitchFactor : 1.0);
                        if (pcm->loop && position >= pcm->frames) position = std::fmod(position, static_cast<double>(pcm->frames));
                    }
                    continue;
                }
                if (entry.table != nullptr)
                {
                    const double offset = phase * wavetableFrameSize;
                    const int first = static_cast<int>(offset) & (wavetableFrameSize - 1);
                    const int next = (first + 1) & (wavetableFrameSize - 1);
                    const auto fraction = static_cast<float>(offset - static_cast<int>(offset));
                    auto frameSample = [&](int frame)
                    {
                        const auto base = static_cast<std::size_t>(frame * wavetableFrameSize);
                        const auto x = entry.table->samples[base + static_cast<std::size_t>(first)];
                        const auto y = entry.table->samples[base + static_cast<std::size_t>(next)];
                        return x + (y - x) * fraction;
                    };
                    int frameA = entry.frameA, frameB = entry.frameB;
                    float blend = entry.frameBlend;
                    if (tableMod[layerIndex] != 0.0f)
                    {
                        const auto framePosition = juce::jlimit(0.0f, 1.0f, p[oscillatorTablePosition(layer)] + tableMod[layerIndex]);
                        frameB = 0;
                        while (frameB + 1 < entry.table->frameCount && entry.table->positions[static_cast<std::size_t>(frameB)] < framePosition) ++frameB;
                        frameA = juce::jmax(0, frameB - 1);
                        const auto a = entry.table->positions[static_cast<std::size_t>(frameA)], b = entry.table->positions[static_cast<std::size_t>(frameB)];
                        blend = b > a ? juce::jlimit(0.0f, 1.0f, (framePosition - a) / (b - a)) : 0.0f;
                    }
                    const auto a = frameSample(frameA), b = frameSample(frameB);
                    sample = (a + (b - a) * blend) * mixScale;
                }
                else sample = oscillatorSample(entry.shape, phase, increment) * mixScale;
                mixed[0] += sample * leftGain; mixed[1] += sample * rightGain;
                phase += increment;
                if (phase >= 1.0) phase -= 1.0;
            }
            lfoPhase += lfoIncrement;
            if (lfoPhase >= twoPi) lfoPhase -= twoPi;
            if (channels == 1) mixed[0] = 0.5 * (mixed[0] + mixed[1]);
            const auto gain = amplitude * envelope.getNextSample();
            // The same global low-pass follows the layer mixer, with separate
            // left/right states so per-layer panning is preserved.
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto index = static_cast<std::size_t>(channel);
                const double v3 = mixed[index] - ic2[index];
                const double v1 = a1 * ic1[index] + a2 * v3;
                const double v2 = ic2[index] + a2 * ic1[index] + a3 * v3;
                ic1[index] = static_cast<float>(2.0 * v1 - ic1[index]);
                ic2[index] = static_cast<float>(2.0 * v2 - ic2[index]);
                output.addSample(channel, start + i, static_cast<float>(v2) * gain);
            }
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
        envelope.setDelayAndHold(p[ampDelay], p[ampHold]);
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
    AmpEnvelope envelope;
    juce::ADSR::Parameters previousEnvelope;
    bool hasEnvelopeParameters = false;
    std::array<double, oscillatorCount> phases {}, samplePositions {};
    std::array<double, importedLfoCount> importedLfoPhases {};
    std::array<float, 2> ic1 {}, ic2 {};
    double lfoPhase = 0.0, baseFrequency = 440.0;
    float amplitude = 0.0f;
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
    postEffects.prepare(sampleRate);
    delayBuffer.setSize(2, static_cast<int>(std::ceil(sampleRate * 1.5)) + juce::jmax(1, maximumBlockSize) + 2);
    reset();
}
void SynthEngine::reset()
{
    synth.allNotesOff(0, false);
    reverb.reset();
    postEffects.reset();
    delayBuffer.clear();
    delayWritePosition = 0;
    distortionState[0] = distortionState[1] = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        bandLow[c] = bandHighLowpass[c] = 0.0f;
        for (float& envelope : bandEnvelope[c]) envelope = 0.0f;
    }
}
void SynthEngine::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, const ParameterValues& values)
{
    struct ReadGuard
    {
        WaveTableExchange& exchange;
        ~ReadGuard() { exchange.endRead(); }
    } guard { waveTables };
    parameters.tables = &waveTables.beginRead();
    parameters.values = values;
    buffer.clear();
    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.12f + 0.86f * values[reverbSize];
    rp.damping = 0.8f * (1.0f - values[reverbDecay]);
    // JUCE's reverb scales wet by 3 and dry by 2 internally.
    const auto reverbWet = values[reverbEnabled] >= 0.5f ? values[reverbMix] : 0.0f;
    rp.wetLevel = reverbWet / 3.0f;
    rp.dryLevel = 0.5f * (1.0f - reverbWet);
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
    const float delayMixValue = values[delayEnabled] >= 0.5f ? values[delayMix] : 0.0f;
    const float feedback = values[delayFeedback];
    const float distortionMixValue = values[distortionEnabled] >= 0.5f ? values[distortionMix] : 0.0f;
    const float drive = values[distortionDrive];
    const float normalisation = 1.0f / std::tanh(drive);
    const double toneFrequency = 400.0 + 14000.0 * values[distortionTone];
    const float toneAlpha = static_cast<float>(1.0 - std::exp(-twoPi * toneFrequency / sampleRate));
    const bool useMultiband = values[ottEnabled] >= 0.5f && values[ottMix] > 0.0f;
    const float lowAlpha = static_cast<float>(1.0 - std::exp(-twoPi * 250.0 / sampleRate));
    const float highAlpha = static_cast<float>(1.0 - std::exp(-twoPi * 2500.0 / sampleRate));
    const float attackCoeff = static_cast<float>(1.0 - std::exp(-1.0 / (sampleRate * 0.010)));
    const float releaseCoeff = static_cast<float>(1.0 - std::exp(-1.0 / (sampleRate * 0.120)));
    const float depth = juce::jlimit(0.0f, 1.0f, values[ottDepth]);
    const float mix = juce::jlimit(0.0f, 1.0f, values[ottMix]);

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
            const float dry = (delayedMix * (1.0f - distortionMixValue) + distortionState[channel] * distortionMixValue) * values[level];
            float out = dry;
            if (useMultiband)
            {
                // Complementary first-order crossovers reconstruct the dry signal
                // before dynamics. This is VANTA's own OTT-style approximation,
                // not a bit-identical recreation of Xfer/Vital compressors.
                bandLow[channel] += lowAlpha * (dry - bandLow[channel]);
                bandHighLowpass[channel] += highAlpha * (dry - bandHighLowpass[channel]);
                const float bands[] { bandLow[channel], bandHighLowpass[channel] - bandLow[channel], dry - bandHighLowpass[channel] };
                float wet = 0.0f;
                for (int band = 0; band < 3; ++band)
                {
                    auto& env = bandEnvelope[channel][band];
                    const auto magnitude = std::abs(bands[band]);
                    env += (magnitude > env ? attackCoeff : releaseCoeff) * (magnitude - env);
                    // Limited upward/downward compression avoids unbounded noise gain.
                    const auto gain = juce::jlimit(0.55f, 2.0f, std::sqrt(0.08f / juce::jmax(env, 1.0e-6f)));
                    const auto bandGain = 1.0f + depth * (gain - 1.0f);
                    wet += bands[band] * bandGain * (band == 2 ? 1.41253754f : 1.0f); // +3 dB highs
                }
                out = dry + mix * (wet - dry);
            }
            buffer.setSample(channel, i, juce::jlimit(-1.0f, 1.0f, out));
        }
        if (++delayWritePosition == delayLength) delayWritePosition = 0;
    }
    postEffects.process(buffer, values);
}
}
