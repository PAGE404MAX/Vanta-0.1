#pragma once

#include "Parameters.h"
#include "Effects.h"
#include "WaveTable.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace vanta
{
// Entirely controlled by a block snapshot, so the voices do not access the UI.
class SynthEngine
{
public:
    SynthEngine();
    void prepare(double sampleRate, int maximumBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>&, juce::MidiBuffer&, const ParameterValues&);
    // publish/copy are for UI or state threads, never for processBlock.
    void publishWaveTables(const WaveTableBank& bank) { waveTables.publish(bank); }
    std::unique_ptr<WaveTableBank> copyWaveTables() const { return waveTables.copyPublished(); }

private:
    struct SharedParameters { ParameterValues values = defaultValues(); const WaveTableBank* tables = nullptr; };
    class Sound;
    class Voice;
    WaveTableExchange waveTables;
    SharedParameters parameters;
    juce::Synthesiser synth;
    juce::Reverb reverb;
    PostEffects postEffects;
    juce::AudioBuffer<float> delayBuffer;
    double sampleRate = 44100.0;
    int delayWritePosition = 0;
    float distortionState[2] { 0.0f, 0.0f };
    // Three complementary bands; a bypass leaves audio bit-identical to 0.7.
    float bandLow[2] {}, bandHighLowpass[2] {}, bandEnvelope[2][3] {};
};
}
