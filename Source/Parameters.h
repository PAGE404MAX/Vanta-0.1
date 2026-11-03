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
    // Append-only IDs: preserve all 0.7 indices/IDs for automation and old projects.
    oscAEnabled,
    oscATable, oscBTable, oscCTable, oscDTable, oscETable, oscFTable, oscGTable, oscHTable,
    oscATablePosition, oscBTablePosition, oscCTablePosition, oscDTablePosition,
    oscETablePosition, oscFTablePosition, oscGTablePosition, oscHTablePosition,
    ottEnabled, ottDepth, ottMix,
    oscASample, oscBSample, oscCSample, oscDSample, oscESample, oscFSample, oscGSample, oscHSample,
    // Beta 0.12: append only. Legacy mixes keep their original behaviour.
    reverbEnabled, delayEnabled, distortionEnabled,
    chorusEnabled, chorusRate, chorusDepth, chorusMix,
    eqEnabled, eqLow, eqMid, eqHigh, eqMix,
    fxFilterEnabled, fxFilterCutoff, fxFilterResonance, fxFilterMix,
    flangerEnabled, flangerRate, flangerDepth, flangerFeedback, flangerMix,
    phaserEnabled, phaserRate, phaserDepth, phaserFeedback, phaserMix,
    // Beta 0.13: separate amp-envelope stages; not the existing delay effect.
    ampDelay, ampHold,
    // Beta 0.14: new full-range controls are appended. The original ADSR IDs
    // and their normalized mappings stay intact for old host automation.
    ampAttackWide, ampDecayWide, ampReleaseWide,
    ampAttackWideActive, ampDecayWideActive, ampReleaseWideActive,
    lfoSmooth, lfoDelay, lfoStereo,
    oscAUnison, oscBUnison, oscCUnison, oscDUnison,
    oscEUnison, oscFUnison, oscGUnison, oscHUnison,
    oscAPhase, oscBPhase, oscCPhase, oscDPhase,
    oscEPhase, oscFPhase, oscGPhase, oscHPhase,
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
    { "oscHPan",     "Osc H Pan",       -1.0f,    1.0f,    0.0f,   0.001f, 1.0f },
    { "oscAEnabled", "Osc A Enabled", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "oscATable", "Osc A Wavetable", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscBTable", "Osc B Wavetable", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscCTable", "Osc C Wavetable", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscDTable", "Osc D Wavetable", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscETable", "Osc E Wavetable", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscFTable", "Osc F Wavetable", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscGTable", "Osc G Wavetable", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscHTable", "Osc H Wavetable", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscATablePosition", "Osc A Table Position", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscBTablePosition", "Osc B Table Position", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscCTablePosition", "Osc C Table Position", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscDTablePosition", "Osc D Table Position", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscETablePosition", "Osc E Table Position", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscFTablePosition", "Osc F Table Position", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscGTablePosition", "Osc G Table Position", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscHTablePosition", "Osc H Table Position", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "ottEnabled", "Master Multiband Enabled", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "ottDepth", "Master Multiband Depth", 0.0f, 1.0f, 0.5f, 0.001f, 1.0f },
    { "ottMix", "Master Multiband Mix", 0.0f, 1.0f, 1.0f, 0.001f, 1.0f },
    { "oscASample", "Osc A Sample", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscBSample", "Osc B Sample", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscCSample", "Osc C Sample", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscDSample", "Osc D Sample", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscESample", "Osc E Sample", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscFSample", "Osc F Sample", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscGSample", "Osc G Sample", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "oscHSample", "Osc H Sample", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "reverbEnabled", "Reverb Enabled", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "delayEnabled", "Delay Enabled", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "distortionEnabled", "Distortion Enabled", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    { "chorusEnabled", "Chorus Enabled", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "chorusRate", "Chorus Rate", 0.05f, 5.0f, 0.7f, 0.01f, 0.5f },
    { "chorusDepth", "Chorus Depth", 0.0f, 1.0f, 0.6f, 0.001f, 1.0f },
    { "chorusMix", "Chorus Mix", 0.0f, 1.0f, 0.45f, 0.001f, 1.0f },
    { "eqEnabled", "Equalizer Enabled", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "eqLow", "Equalizer Low", -12.0f, 12.0f, 0.0f, 0.1f, 1.0f },
    { "eqMid", "Equalizer Mid", -12.0f, 12.0f, 0.0f, 0.1f, 1.0f },
    { "eqHigh", "Equalizer High", -12.0f, 12.0f, 0.0f, 0.1f, 1.0f },
    { "eqMix", "Equalizer Mix", 0.0f, 1.0f, 1.0f, 0.001f, 1.0f },
    { "fxFilterEnabled", "FX Filter Enabled", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "fxFilterCutoff", "FX Filter Cutoff", 30.0f, 18000.0f, 5000.0f, 1.0f, 0.28f },
    { "fxFilterResonance", "FX Filter Resonance", 0.0f, 0.95f, 0.2f, 0.001f, 1.0f },
    { "fxFilterMix", "FX Filter Mix", 0.0f, 1.0f, 1.0f, 0.001f, 1.0f },
    { "flangerEnabled", "Flanger Enabled", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "flangerRate", "Flanger Rate", 0.05f, 5.0f, 0.35f, 0.01f, 0.5f },
    { "flangerDepth", "Flanger Depth", 0.0f, 1.0f, 0.7f, 0.001f, 1.0f },
    { "flangerFeedback", "Flanger Feedback", -0.8f, 0.8f, 0.3f, 0.001f, 1.0f },
    { "flangerMix", "Flanger Mix", 0.0f, 1.0f, 0.5f, 0.001f, 1.0f },
    { "phaserEnabled", "Phaser Enabled", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "phaserRate", "Phaser Rate", 0.05f, 5.0f, 0.4f, 0.01f, 0.5f },
    { "phaserDepth", "Phaser Depth", 0.0f, 1.0f, 0.7f, 0.001f, 1.0f },
    { "phaserFeedback", "Phaser Feedback", -0.8f, 0.8f, 0.2f, 0.001f, 1.0f },
    { "phaserMix", "Phaser Mix", 0.0f, 1.0f, 0.5f, 0.001f, 1.0f },
    { "ampDelay", "Amp Envelope Delay", 0.0f, 4.0f, 0.0f, 0.001f, 0.4f },
    { "ampHold", "Amp Envelope Hold", 0.0f, 4.0f, 0.0f, 0.001f, 0.4f },
    { "ampAttackWide", "Amp Envelope Attack", 0.0f, 32.0f, 0.02f, 0.001f, 0.4f },
    { "ampDecayWide", "Amp Envelope Decay", 0.0f, 32.0f, 0.10f, 0.001f, 0.4f },
    { "ampReleaseWide", "Amp Envelope Release", 0.0f, 32.0f, 0.35f, 0.001f, 0.4f },
    // These are persisted compatibility flags, not user-facing automation.
    { "ampAttackWideActive", "Attack Full Range Active", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "ampDecayWideActive", "Decay Full Range Active", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "ampReleaseWideActive", "Release Full Range Active", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
    { "lfoSmooth", "Pitch LFO Smooth", 0.0f, 16.0f, 0.0f, 0.001f, 0.4f },
    { "lfoDelay", "Pitch LFO Delay", 0.0f, 4.0f, 0.0f, 0.001f, 0.4f },
    { "lfoStereo", "Pitch LFO Stereo", -0.5f, 0.5f, 0.0f, 0.001f, 1.0f },
    { "oscAUnison", "Osc A Unison", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscBUnison", "Osc B Unison", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscCUnison", "Osc C Unison", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscDUnison", "Osc D Unison", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscEUnison", "Osc E Unison", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscFUnison", "Osc F Unison", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscGUnison", "Osc G Unison", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscHUnison", "Osc H Unison", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscAPhase", "Osc A Phase", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscBPhase", "Osc B Phase", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscCPhase", "Osc C Phase", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscDPhase", "Osc D Phase", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscEPhase", "Osc E Phase", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscFPhase", "Osc F Phase", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscGPhase", "Osc G Phase", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
    { "oscHPhase", "Osc H Phase", 0.0f, 1.0f, 0.0f, 0.001f, 1.0f },
}};

using ParameterValues = std::array<float, parameterCount>;
// Old projects continue using their original time parameters (including host
// automation); full-range controls take over only after they are changed.
inline constexpr std::array<Parameter, 3> legacyEnvelopeTimes { attack, decay, release };
inline constexpr std::array<Parameter, 3> wideEnvelopeTimes { ampAttackWide, ampDecayWide, ampReleaseWide };
inline constexpr std::array<Parameter, 3> wideEnvelopeFlags { ampAttackWideActive, ampDecayWideActive, ampReleaseWideActive };
inline float envelopeTime(const ParameterValues& values, int stage)
{
    const auto i = static_cast<std::size_t>(stage);
    return values[wideEnvelopeFlags[i]] >= 0.5f ? values[wideEnvelopeTimes[i]] : values[legacyEnvelopeTimes[i]];
}
inline constexpr int oscillatorCount = 8;
inline constexpr int oscillatorStride = 6;
// A retains the original two-choice wave parameter.
inline constexpr Parameter oscillatorEnabled(int index) { return index == 0 ? oscAEnabled : static_cast<Parameter>(oscBEnabled + (index - 1) * oscillatorStride); }
inline constexpr Parameter oscillatorWave(int index) { return static_cast<Parameter>(oscBWave + (index - 1) * oscillatorStride); }
inline constexpr Parameter oscillatorLevel(int index) { return index == 0 ? oscALevel : static_cast<Parameter>(oscBLevel + (index - 1) * oscillatorStride); }
inline constexpr Parameter oscillatorTune(int index) { return index == 0 ? oscATune : static_cast<Parameter>(oscBTune + (index - 1) * oscillatorStride); }
inline constexpr Parameter oscillatorFine(int index) { return index == 0 ? oscAFine : static_cast<Parameter>(oscBFine + (index - 1) * oscillatorStride); }
inline constexpr Parameter oscillatorPan(int index) { return index == 0 ? oscAPan : static_cast<Parameter>(oscBPan + (index - 1) * oscillatorStride); }
inline constexpr Parameter oscillatorTable(int index) { return static_cast<Parameter>(oscATable + index); }
inline constexpr Parameter oscillatorSample(int index) { return static_cast<Parameter>(oscASample + index); }
inline constexpr Parameter oscillatorTablePosition(int index) { return static_cast<Parameter>(oscATablePosition + index); }
inline constexpr Parameter oscillatorUnison(int index) { return static_cast<Parameter>(oscAUnison + index); }
inline constexpr Parameter oscillatorPhase(int index) { return static_cast<Parameter>(oscAPhase + index); }
inline bool oscillatorIsEnabled(const ParameterValues& values, int index) { return values[oscillatorEnabled(index)] >= 0.5f; }
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
