#pragma once

#include "Parameters.h"
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

private:
    struct SharedParameters { ParameterValues values = defaultValues(); };
    class Sound;
    class Voice;
    SharedParameters parameters;
    juce::Synthesiser synth;
    juce::Reverb reverb;
    juce::AudioBuffer<float> delayBuffer;
    double sampleRate = 44100.0;
    int delayWritePosition = 0;
    float distortionState[2] { 0.0f, 0.0f };
};
}
