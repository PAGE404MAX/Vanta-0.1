#pragma once

#include <array>
#include <cstddef>

namespace vanta
{
enum Parameter : std::size_t
{
    level, attack, release, wave, decay, sustain, cutoff, resonance,
    vibrato, vibratoRate, reverbSize, reverbDecay, reverbMix,
    delayTime, delayFeedback, delayMix, distortionDrive, distortionTone,
    distortionMix, parameterCount
};

struct ParameterSpec
{
    const char* id;
    const char* name;
    float minValue, maxValue, defaultValue, step, skew;
};

// The first four IDs and their ranges are retained from Beta 0.3 so host
// projects can restore those parameters. New parameters have stable IDs too.
inline constexpr std::array<ParameterSpec, parameterCount> parameterSpecs {{
    { "level",           "Level",            0.0f,    1.0f,    0.70f, 0.0f,   1.0f },
    { "attack",          "Attack",           0.001f,  2.0f,    0.02f, 0.001f, 0.4f },
    { "release",         "Release",          0.01f,   4.0f,    0.35f, 0.001f, 0.4f },
    { "wave",            "Waveform",         0.0f,    1.0f,    0.0f,  1.0f,   1.0f },
    { "decay",           "Decay",            0.001f,  4.0f,    0.10f, 0.001f, 0.4f },
    { "sustain",         "Sustain",          0.0f,    1.0f,    0.80f, 0.001f, 1.0f },
    { "cutoff",          "Filter Cutoff",   30.0f, 18000.0f, 14000.0f, 1.0f, 0.28f },
    { "resonance",       "Filter Resonance", 0.0f,    0.95f,   0.0f,  0.001f, 1.0f },
    { "vibrato",         "Vibrato Depth",    0.0f,    1.0f,    0.0f,  0.001f, 1.0f },
    { "vibratoRate",     "Vibrato Rate",     0.1f,   12.0f,    5.0f,  0.01f,  0.5f },
    { "reverbSize",      "Reverb Size",      0.0f,    1.0f,    0.5f,  0.001f, 1.0f },
    { "reverbDecay",     "Reverb Decay",     0.0f,    1.0f,    0.5f,  0.001f, 1.0f },
    { "reverbMix",       "Reverb Mix",       0.0f,    1.0f,    0.0f,  0.001f, 1.0f },
    { "delayTime",       "Delay Time",       0.04f,   1.5f,    0.3f,  0.001f, 0.45f },
    { "delayFeedback",   "Delay Feedback",   0.0f,    0.85f,   0.3f,  0.001f, 1.0f },
    { "delayMix",        "Delay Mix",        0.0f,    1.0f,    0.0f,  0.001f, 1.0f },
    { "distortionDrive", "Distortion Drive", 1.0f,   12.0f,    2.0f,  0.01f,  0.5f },
    { "distortionTone",  "Distortion Tone",  0.0f,    1.0f,    0.65f, 0.001f, 1.0f },
    { "distortionMix",   "Distortion Mix",   0.0f,    1.0f,    0.0f,  0.001f, 1.0f }
}};

using ParameterValues = std::array<float, parameterCount>;
inline ParameterValues defaultValues()
{
    ParameterValues result {};
    for (std::size_t i = 0; i < parameterSpecs.size(); ++i)
        result[i] = parameterSpecs[i].defaultValue;
    return result;
}
}
