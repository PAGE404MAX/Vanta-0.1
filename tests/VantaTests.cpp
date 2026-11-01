#include "PresetCodec.h"
#include "SynthEngine.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void check(bool condition, const char* name)
{
    if (!condition) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
    std::cout << "PASS: " << name << '\n';
}
float rms(const juce::AudioBuffer<float>& b, int start, int count) { return b.getRMSLevel(0, start, count); }
juce::AudioBuffer<float> render(const vanta::ParameterValues& values, int totalSamples, int noteOff = -1, bool noteOn = true)
{
    vanta::SynthEngine engine;
    engine.prepare(44100.0, 512);
    juce::AudioBuffer<float> buffer(2, totalSamples);
    juce::MidiBuffer midi;
    if (noteOn) midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0);
    if (noteOff >= 0) midi.addEvent(juce::MidiMessage::noteOff(1, 69), noteOff);
    engine.process(buffer, midi, values);
    return buffer;
}
void testAudio()
{
    constexpr int rate = 44100;
    auto p = vanta::defaultValues(); p[vanta::attack] = 0.001f;
    auto silent = render(p, rate / 10, -1, false);
    check(rms(silent, 0, silent.getNumSamples()) == 0.0f, "No MIDI produces silence");
    auto sine = render(p, rate / 4);
    check(rms(sine, rate / 20, rate / 10) > 0.01f, "Sine produces audio");
    p[vanta::wave] = 1.0f;
    auto saw = render(p, rate / 4);
    double difference = 0.0;
    for (int i = rate / 20; i < rate / 10; ++i) difference += std::abs(sine.getSample(0, i) - saw.getSample(0, i));
    check(difference / (rate / 20) > 0.01, "Sine and saw differ");
    bool stereo = true;
    for (int i = 0; i < sine.getNumSamples(); ++i) stereo &= std::abs(sine.getSample(0, i) - sine.getSample(1, i)) < 1.0e-6f;
    check(stereo, "Dry voice renders equally in stereo");
    auto muted = p; muted[vanta::level] = 0.0f;
    auto zero = render(muted, rate / 5);
    check(rms(zero, 0, zero.getNumSamples()) == 0.0f, "Level zero is silent");
    auto quick = p; quick[vanta::attack] = 0.001f;
    auto slow = p; slow[vanta::attack] = 1.0f;
    auto fastAttack = render(quick, rate / 20);
    auto slowAttack = render(slow, rate / 20);
    check(rms(fastAttack, 0, rate / 20) > 8.0f * rms(slowAttack, 0, rate / 20), "Attack changes the envelope");
    quick[vanta::release] = 0.01f; slow[vanta::attack] = 0.001f; slow[vanta::release] = 1.0f;
    auto shortTail = render(quick, rate / 2, rate / 10);
    auto longTail = render(slow, rate / 2, rate / 10);
    check(rms(shortTail, rate / 5, rate / 10) < 1.0e-6f && rms(longTail, rate / 5, rate / 10) > 0.01f, "Release controls note tail");
    auto filtered = p; filtered[vanta::cutoff] = 200.0f;
    auto low = render(filtered, rate / 4);
    check(rms(low, rate / 10, rate / 10) < rms(saw, rate / 10, rate / 10) * 0.5f, "Low-pass cutoff changes the sound");
    auto delayed = quick; delayed[vanta::delayTime] = 0.15f; delayed[vanta::delayMix] = 0.7f; delayed[vanta::delayFeedback] = 0.3f;
    auto echo = render(delayed, rate / 2, rate / 20);
    check(rms(echo, rate / 5, rate / 20) > 0.001f, "Delay creates an audible tail");
    auto wet = quick; wet[vanta::reverbMix] = 0.7f;
    auto reverbed = render(wet, rate / 2, rate / 20);
    check(rms(reverbed, rate / 5, rate / 20) > 0.00001f, "Reverb creates an audible tail");
    auto distorted = p; distorted[vanta::distortionMix] = 1.0f; distorted[vanta::distortionDrive] = 8.0f;
    auto distortedAudio = render(distorted, rate / 4);
    difference = 0.0;
    bool finite = true;
    for (int i = 0; i < distortedAudio.getNumSamples(); ++i)
    {
        const auto sample = distortedAudio.getSample(0, i);
        finite &= std::isfinite(sample) && std::abs(sample) <= 1.0f;
        difference += std::abs(sample - saw.getSample(0, i));
    }
    check(finite && difference / distortedAudio.getNumSamples() > 0.002, "Distortion changes audio without non-finite or out-of-range samples");

    // Exercise MIDI handling across blocks, including a release before the
    // attack has reached its sustain level (which must not reset every block).
    vanta::SynthEngine held;
    held.prepare(rate, 512);
    auto early = vanta::defaultValues(); early[vanta::attack] = 0.5f; early[vanta::sustain] = 0.0f; early[vanta::release] = 1.0f;
    juce::AudioBuffer<float> block(2, rate / 10);
    juce::MidiBuffer events;
    events.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0);
    held.process(block, events, early);
    events.clear(); events.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
    held.process(block, events, early);
    events.clear();
    for (int i = 0; i < 4; ++i) held.process(block, events, early);
    check(rms(block, 0, block.getNumSamples()) > 0.0001f, "Early note-off keeps its release tail across blocks");
    for (int i = 0; i < 7; ++i) held.process(block, events, early);
    check(rms(block, 0, block.getNumSamples()) < 1.0e-6f, "Release eventually becomes silent across blocks");

    vanta::SynthEngine pedal;
    pedal.prepare(rate, 512);
    auto heldParams = vanta::defaultValues(); heldParams[vanta::attack] = 0.001f; heldParams[vanta::release] = 0.01f;
    events.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0);
    events.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 1);
    events.addEvent(juce::MidiMessage::noteOff(1, 69), 2);
    pedal.process(block, events, heldParams);
    check(rms(block, rate / 20, rate / 40) > 0.005f, "MIDI sustain pedal holds a released key");
    events.clear(); events.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
    pedal.process(block, events, heldParams);
    check(rms(block, rate / 20, rate / 40) < 1.0e-6f, "Releasing the sustain pedal releases the note");
}
void testPresets()
{
    auto values = vanta::defaultValues();
    values[vanta::attack] = 0.234f; values[vanta::wave] = 1.0f; values[vanta::cutoff] = 1234.0f;
    const auto encoded = vanta::PresetCodec::encode("My patch", values);
    auto decoded = vanta::PresetCodec::decode(encoded, ".vanta");
    check(decoded.ok && decoded.name == "My patch", "Native preset round-trip loads name");
    bool matches = true;
    for (std::size_t i = 0; i < values.size(); ++i) matches &= std::abs(values[i] - decoded.values[i]) < 1.0e-3f;
    check(matches, "Native preset round-trip loads every parameter");
    check(!vanta::PresetCodec::decode("not json", ".vanta").ok, "Malformed JSON is rejected");
    check(!vanta::PresetCodec::decode("{}", ".vanta").ok, "Unrecognised VANTA schema is rejected");
    check(!vanta::PresetCodec::decode(encoded, ".fxp").ok && !vanta::PresetCodec::decode(encoded, ".SerumPreset").ok
          && !vanta::PresetCodec::decode(encoded, ".svz").ok, "Unsupported formats are rejected");
    const juce::String vital = R"({"synth_version":"1.0.7","preset_name":"Example","settings":{"env_1_attack":0.5,"env_1_decay":1.0,"env_1_sustain":0.25,"env_1_release":0.75,"filter_1_on":1,"filter_1_cutoff":69}})";
    auto converted = vanta::PresetCodec::decode(vital, ".vital");
    check(converted.ok && converted.name == "Example" && std::abs(converted.values[vanta::attack] - 0.0625f) < 0.0001f
          && std::abs(converted.values[vanta::release] - 0.31640625f) < 0.0001f
          && std::abs(converted.values[vanta::cutoff] - 440.0f) < 0.1f, "Vital envelope and cutoff mapping");
    check(converted.message.containsIgnoreCase("approximate") && converted.message.containsIgnoreCase("not imported"), "Vital conversion warns about unsupported sound features");
    auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("vanta-test-" + juce::Uuid().toString() + ".vanta");
    auto save = vanta::PresetCodec::saveFile(temp, "Disk test", values);
    auto disk = vanta::PresetCodec::loadFile(temp);
    check(save.wasOk() && disk.ok && disk.name == "Disk test", "Native preset saves and loads from disk");
    temp.deleteFile();
}
}
int main()
{
    testAudio();
    testPresets();
    std::cout << "All VANTA tests passed.\n";
    return 0;
}
