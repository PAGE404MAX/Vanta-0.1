#pragma once

#include "Parameters.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace vanta
{
// VANTA's own fixed-order post effects.
// Buffers are allocated in prepare(), never in process().
class PostEffects
{
public:
    void prepare(double sampleRate);
    void reset();
    void process(juce::AudioBuffer<float>&, const ParameterValues&);

private:
    double rate = 44100.0;
    juce::AudioBuffer<float> chorusHistory, flangerHistory;
    int chorusWrite = 0, flangerWrite = 0;
    double chorusPhase = 0.0, flangerPhase = 0.0, phaserPhase = 0.0;
    float flangerLast[2] {}, eqLowState[2] {}, eqHighState[2] {};
    float filterIc1[2] {}, filterIc2[2] {};
    float phaserState[2][4] {}, phaserLast[2] {};
    float chorusAmount = 0.0f, eqAmount = 0.0f, filterAmount = 0.0f;
    float flangerAmount = 0.0f, phaserAmount = 0.0f;
};
}
