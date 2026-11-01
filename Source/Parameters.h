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
    distortionMix,
    // Append only: these IDs must not change the legacy wave parameter's 0/1 range.
    oscAExtraWave, oscALevel, oscATune, oscAFine, oscAPan,
    oscBEnabled, oscBWave, oscBLevel, oscBTune, oscBFine, oscBPan,
    oscCEnabled, oscCWave, oscCLevel, oscCTune, oscCFine, oscCPan,
    oscDEnabled, oscDWave, oscDLevel, oscDTune, oscDFine, oscDPan,
    oscEEnabled, oscEWave, oscELevel, oscETune, oscEFine, oscEPan,
    oscFEnabled, oscFWave, oscFLevel, oscFTune, oscFFine, oscFPan,
    oscGEnabled, oscGWave, oscGLevel, oscGTune, oscGFine, oscGPan,
    oscHEnabled, oscHWave, oscHLevel, oscHTune, oscHFine, oscHPan,
    parameterCount
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
    { "distortionMix",   "Distortion Mix",   0.0f,    1.0f,    0.0f,  0.001f, 1.0f },
    { "oscAExtraWave",   "Osc A Extra Wave", 0.0f,    2.0f,    0.0f,   1.0f,   1.0f },
    { "oscALevel",       "Osc A Level",      0.0f,    1.0f,    1.0f,   0.001f, 1.0f },
    { "oscATune",        "Osc A Semitones",-24.0f,   24.0f,    0.0f,   1.0f,   1.0f },
    { "oscAFine",        "Osc A Fine",    -100.0f,  100.0f,    0.0f,   1.0f,   1.0f },
    { "oscAPan",         "Osc A Pan",       -1.0f,    1.0f,    0.0f,   0.001f, 1.0f },
    { "oscBEnabled", "Osc B Enabled",    0.0f,    1.0f,    0.0f,   1.0f,   1.0f },
    { "oscBWave",    "Osc B Waveform",   0.0f,    3.0f,    0.0f,   1.0f,   1.0f },
    { "oscBLevel",   "Osc B Level",      0.0f,    1.0f,    0.5f,   0.001f, 1.0f },
    { "oscBTune",    "Osc B Semitones",-24.0f,   24.0f,    0.0f,   1.0f,   1.0f },
    { "oscBFine",    "Osc B Fine",    -100.0f,  100.0f,    0.0f,   1.0f,   1.0f },
    { "oscBPan",     "Osc B Pan",       -1.0f,    1.0f,    0.0f,   0.001f, 1.0f },
    { "oscCEnabled", "Osc C Enabled",    0.0f,    1.0f,    0.0f,   1.0f,   1.0f },
    { "oscCWave",    "Osc C Waveform",   0.0f,    3.0f,    0.0f,   1.0f,   1.0f },
    { "oscCLevel",   "Osc C Level",      0.0f,    1.0f,    0.5f,   0.001f, 1.0f },
    { "oscCTune",    "Osc C Semitones",-24.0f,   24.0f,    0.0f,   1.0f,   1.0f },
    { "oscCFine",    "Osc C Fine",    -100.0f,  100.0f,    0.0f,   1.0f,   1.0f },
    { "oscCPan",     "Osc C Pan",       -1.0f,    1.0f,    0.0f,   0.001f, 1.0f },
    { "oscDEnabled", "Osc D Enabled",    0.0f,    1.0f,    0.0f,   1.0f,   1.0f },
    { "oscDWave",    "Osc D Waveform",   0.0f,    3.0f,    0.0f,   1.0f,   1.0f },
    { "oscDLevel",   "Osc D Level",      0.0f,    1.0f,    0.5f,   0.001f, 1.0f },
    { "oscDTune",    "Osc D Semitones",-24.0f,   24.0f,    0.0f,   1.0f,   1.0f },
    { "oscDFine",    "Osc D Fine",    -100.0f,  100.0f,    0.0f,   1.0f,   1.0f },
    { "oscDPan",     "Osc D Pan",       -1.0f,    1.0f,    0.0f,   0.001f, 1.0f },
    { "oscEEnabled", "Osc E Enabled",    0.0f,    1.0f,    0.0f,   1.0f,   1.0f },
    { "oscEWave",    "Osc E Waveform",   0.0f,    3.0f,    0.0f,   1.0f,   1.0f },
    { "oscELevel",   "Osc E Level",      0.0f,    1.0f,    0.5f,   0.001f, 1.0f },
    { "oscETune",    "Osc E Semitones",-24.0f,   24.0f,    0.0f,   1.0f,   1.0f },
    { "oscEFine",    "Osc E Fine",    -100.0f,  100.0f,    0.0f,   1.0f,   1.0f },
    { "oscEPan",     "Osc E Pan",       -1.0f,    1.0f,    0.0f,   0.001f, 1.0f },
    { "oscFEnabled", "Osc F Enabled",    0.0f,    1.0f,    0.0f,   1.0f,   1.0f },
    { "oscFWave",    "Osc F Waveform",   0.0f,    3.0f,    0.0f,   1.0f,   1.0f },
    { "oscFLevel",   "Osc F Level",      0.0f,    1.0f,    0.5f,   0.001f, 1.0f },
    { "oscFTune",    "Osc F Semitones",-24.0f,   24.0f,    0.0f,   1.0f,   1.0f },
    { "oscFFine",    "Osc F Fine",    -100.0f,  100.0f,    0.0f,   1.0f,   1.0f },
    { "oscFPan",     "Osc F Pan",       -1.0f,    1.0f,    0.0f,   0.001f, 1.0f },
    { "oscGEnabled", "Osc G Enabled",    0.0f,    1.0f,    0.0f,   1.0f,   1.0f },
    { "oscGWave",    "Osc G Waveform",   0.0f,    3.0f,    0.0f,   1.0f,   1.0f },
    { "oscGLevel",   "Osc G Level",      0.0f,    1.0f,    0.5f,   0.001f, 1.0f },
    { "oscGTune",    "Osc G Semitones",-24.0f,   24.0f,    0.0f,   1.0f,   1.0f },
    { "oscGFine",    "Osc G Fine",    -100.0f,  100.0f,    0.0f,   1.0f,   1.0f },
    { "oscGPan",     "Osc G Pan",       -1.0f,    1.0f,    0.0f,   0.001f, 1.0f },
    { "oscHEnabled", "Osc H Enabled",    0.0f,    1.0f,    0.0f,   1.0f,   1.0f },
    { "oscHWave",    "Osc H Waveform",   0.0f,    3.0f,    0.0f,   1.0f,   1.0f },
    { "oscHLevel",   "Osc H Level",      0.0f,    1.0f,    0.5f,   0.001f, 1.0f },
    { "oscHTune",    "Osc H Semitones",-24.0f,   24.0f,    0.0f,   1.0f,   1.0f },
    { "oscHFine",    "Osc H Fine",    -100.0f,  100.0f,    0.0f,   1.0f,   1.0f },
    { "oscHPan",     "Osc H Pan",       -1.0f,    1.0f,    0.0f,   0.001f, 1.0f }
}};

using ParameterValues = std::array<float, parameterCount>;
inline constexpr int oscillatorCount = 8;
inline constexpr int oscillatorStride = 6;
// A is always enabled and retains the original two-choice wave parameter.
inline constexpr Parameter oscillatorEnabled(int index) { return static_cast<Parameter>(oscBEnabled + (index - 1) * oscillatorStride); }
inline constexpr Parameter oscillatorWave(int index) { return static_cast<Parameter>(oscBWave + (index - 1) * oscillatorStride); }
inline constexpr Parameter oscillatorLevel(int index) { return index == 0 ? oscALevel : static_cast<Parameter>(oscBLevel + (index - 1) * oscillatorStride); }
inline constexpr Parameter oscillatorTune(int index) { return index == 0 ? oscATune : static_cast<Parameter>(oscBTune + (index - 1) * oscillatorStride); }
inline constexpr Parameter oscillatorFine(int index) { return index == 0 ? oscAFine : static_cast<Parameter>(oscBFine + (index - 1) * oscillatorStride); }
inline constexpr Parameter oscillatorPan(int index) { return index == 0 ? oscAPan : static_cast<Parameter>(oscBPan + (index - 1) * oscillatorStride); }
inline bool oscillatorIsEnabled(const ParameterValues& values, int index) { return index == 0 || values[oscillatorEnabled(index)] >= 0.5f; }
inline int oscillatorShape(const ParameterValues& values, int index)
{
    if (index != 0) return static_cast<int>(values[oscillatorWave(index)] + 0.5f);
    const int extra = static_cast<int>(values[oscAExtraWave] + 0.5f);
    return extra == 0 ? static_cast<int>(values[wave] + 0.5f) : extra + 1;
}
inline ParameterValues defaultValues()
{
    ParameterValues result {};
    for (std::size_t i = 0; i < parameterSpecs.size(); ++i)
        result[i] = parameterSpecs[i].defaultValue;
    return result;
}
}
