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

    // Standalone on-screen/computer keyboard -> MIDI buffer -> synth engine.
    vanta::SynthEngine keyboardEngine;
    keyboardEngine.prepare(rate, 512);
    juce::MidiKeyboardState keyboard;
    events.clear();
    keyboard.noteOn(1, 60, 0.8f);
    keyboard.processNextMidiBuffer(events, 0, block.getNumSamples(), true);
    keyboardEngine.process(block, events, heldParams);
    check(rms(block, rate / 20, rate / 40) > 0.005f, "Virtual keyboard MIDI plays the synth");
    events.clear();
    keyboard.noteOff(1, 60, 0.0f);
    keyboard.processNextMidiBuffer(events, 0, block.getNumSamples(), true);
    keyboardEngine.process(block, events, heldParams);
    check(rms(block, rate / 20, rate / 40) < 1.0e-6f, "Virtual keyboard note-off releases the note");
}
void testOscillators()
{
    constexpr int samples = 11025;
    auto base = vanta::defaultValues(); base[vanta::attack] = 0.001f;
    const auto sine = render(base, samples);
    auto squareParams = base; squareParams[vanta::oscAExtraWave] = 1.0f;
    auto triangleParams = base; triangleParams[vanta::oscAExtraWave] = 2.0f;
    const auto square = render(squareParams, samples);
    const auto triangle = render(triangleParams, samples);
    auto difference = [](const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        double total = 0.0;
        for (int i = 1000; i < a.getNumSamples(); ++i) total += std::abs(a.getSample(0, i) - b.getSample(0, i));
        return total / static_cast<double>(a.getNumSamples() - 1000);
    };
    check(rms(square, 1000, samples - 1000) > 0.01f && rms(triangle, 1000, samples - 1000) > 0.01f
          && difference(sine, square) > 0.01 && difference(square, triangle) > 0.01,
          "OSC A sine, square and triangle are distinct audible shapes");
    bool independent = true;
    for (int layer = 1; layer < vanta::oscillatorCount; ++layer)
    {
        auto isolated = base;
        isolated[vanta::oscALevel] = 0.0f;
        isolated[vanta::oscillatorEnabled(layer)] = 1.0f;
        isolated[vanta::oscillatorLevel(layer)] = 1.0f;
        isolated[vanta::oscillatorWave(layer)] = static_cast<float>(layer % 4);
        const auto sound = render(isolated, samples);
        independent &= rms(sound, 1000, samples - 1000) > 0.01f;
        isolated[vanta::oscillatorEnabled(layer)] = 0.0f;
        const auto silent = render(isolated, samples);
        independent &= rms(silent, 1000, samples - 1000) == 0.0f;
    }
    check(independent, "Each additional oscillator can independently produce and mute audio");
    auto layered = base;
    for (int layer = 1; layer < 4; ++layer)
    {
        layered[vanta::oscillatorEnabled(layer)] = 1.0f;
        layered[vanta::oscillatorWave(layer)] = static_cast<float>(layer);
        layered[vanta::oscillatorTune(layer)] = static_cast<float>(layer * 3);
        layered[vanta::oscillatorFine(layer)] = static_cast<float>(layer * 7);
    }
    const auto fourLayers = render(layered, samples);
    check(rms(fourLayers, 1000, samples - 1000) > 0.01f && difference(sine, fourLayers) > 0.01,
          "Four independently shaped and tuned layers produce a different sound");
    auto detuned = base; detuned[vanta::oscAFine] = 25.0f;
    const auto detunedAudio = render(detuned, samples);
    check(difference(sine, detunedAudio) > 0.01, "Fine tune changes oscillator pitch");
    auto panned = base; panned[vanta::oscALevel] = 0.0f; panned[vanta::oscBEnabled] = 1.0f;
    panned[vanta::oscBLevel] = 1.0f; panned[vanta::oscBPan] = -1.0f;
    const auto left = render(panned, samples);
    check(left.getRMSLevel(0, 1000, samples - 1000) > 0.01f && left.getRMSLevel(1, 1000, samples - 1000) < 1.0e-6f,
          "Per-layer pan stays separated through the global filter");
    for (int layer = 1; layer < vanta::oscillatorCount; ++layer)
    {
        layered[vanta::oscillatorEnabled(layer)] = 1.0f;
        layered[vanta::oscillatorLevel(layer)] = 1.0f;
        layered[vanta::oscillatorWave(layer)] = static_cast<float>(layer % 4);
    }
    const auto allLayers = render(layered, samples);
    bool finite = true;
    for (int channel = 0; channel < allLayers.getNumChannels(); ++channel)
        for (int i = 0; i < allLayers.getNumSamples(); ++i)
            finite &= std::isfinite(allLayers.getSample(channel, i)) && std::abs(allLayers.getSample(channel, i)) <= 1.0f;
    check(finite, "Eight enabled layers render finite bounded audio");

    // Pitch wheel should affect an isolated additional layer as well as OSC A.
    auto bendParameters = base;
    bendParameters[vanta::oscALevel] = 0.0f;
    bendParameters[vanta::oscBEnabled] = 1.0f;
    bendParameters[vanta::oscBLevel] = 1.0f;
    auto renderBend = [&](int wheel)
    {
        vanta::SynthEngine engine; engine.prepare(44100.0, 512);
        juce::AudioBuffer<float> buffer(2, 11025); juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::pitchWheel(1, wheel), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 1);
        engine.process(buffer, midi, bendParameters);
        return buffer;
    };
    auto crossings = [](const juce::AudioBuffer<float>& buffer)
    {
        int count = 0;
        for (int i = 1501; i < buffer.getNumSamples(); ++i)
            if (buffer.getSample(0, i - 1) <= 0.0f && buffer.getSample(0, i) > 0.0f) ++count;
        return count;
    };
    const auto normalPitch = renderBend(8192);
    const auto highPitch = renderBend(16383);
    const auto ratio = static_cast<double>(crossings(highPitch)) / crossings(normalPitch);
    check(ratio > 1.10 && ratio < 1.14, "Pitch bend moves an additional oscillator by approximately two semitones");

    vanta::SynthEngine fullPolyphony; fullPolyphony.prepare(48000.0, 512);
    juce::AudioBuffer<float> poly(2, 8192); juce::MidiBuffer notes;
    for (int note = 48; note < 64; ++note)
        notes.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(127)), 0);
    fullPolyphony.process(poly, notes, layered);
    finite = true;
    for (int channel = 0; channel < poly.getNumChannels(); ++channel)
        for (int i = 0; i < poly.getNumSamples(); ++i)
            finite &= std::isfinite(poly.getSample(channel, i)) && std::abs(poly.getSample(channel, i)) <= 1.0f;
    check(finite && rms(poly, 1000, 7000) > 0.001f, "Sixteen voices with eight layers remain audible and bounded");

    vanta::SynthEngine monoEngine; monoEngine.prepare(44100.0, 512);
    juce::AudioBuffer<float> mono(1, samples); juce::MidiBuffer oneNote;
    oneNote.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0);
    monoEngine.process(mono, oneNote, panned);
    check(rms(mono, 1000, samples - 1000) > 0.005f, "Hard-panned layers remain audible through a mono host output");
}
void testPresets()
{
    auto values = vanta::defaultValues();
    values[vanta::attack] = 0.234f; values[vanta::wave] = 1.0f; values[vanta::cutoff] = 1234.0f;
    values[vanta::oscAExtraWave] = 2.0f; values[vanta::oscBEnabled] = 1.0f; values[vanta::oscBWave] = 2.0f;
    values[vanta::oscBTune] = -12.0f; values[vanta::oscBFine] = 17.0f; values[vanta::oscBPan] = -0.75f;
    values[vanta::oscDEnabled] = 1.0f; values[vanta::oscHEnabled] = 1.0f;
    const auto encoded = vanta::PresetCodec::encode("My patch", values);
    auto decoded = vanta::PresetCodec::decode(encoded, ".vanta");
    check(decoded.ok && decoded.name == "My patch", "Native preset round-trip loads name");
    bool matches = true;
    for (std::size_t i = 0; i < values.size(); ++i) matches &= std::abs(values[i] - decoded.values[i]) < 1.0e-3f;
    check(matches, "Native preset round-trip loads every parameter");
    const auto legacy = vanta::PresetCodec::decode(R"({"format":"vanta-preset","version":1,"name":"Old","parameters":{"wave":1,"level":0.6}})", ".vanta");
    check(legacy.ok && legacy.values[vanta::wave] == 1.0f && legacy.values[vanta::oscAExtraWave] == 0.0f
          && legacy.values[vanta::oscALevel] == 1.0f && legacy.values[vanta::oscBEnabled] == 0.0f,
          "Older VANTA presets retain the original saw and default new layers to off");
    check(!vanta::PresetCodec::decode("not json", ".vanta").ok, "Malformed JSON is rejected");
    check(!vanta::PresetCodec::decode("{}", ".vanta").ok, "Unrecognised VANTA schema is rejected");
    check(!vanta::PresetCodec::decode(encoded, ".fxp").ok && !vanta::PresetCodec::decode(encoded, ".SerumPreset").ok
          && !vanta::PresetCodec::decode(encoded, ".svz").ok, "Mislabelled and unsupported formats are rejected");
    const juce::String vital = R"({"synth_version":"1.0.7","preset_name":"Example","settings":{"env_1_attack":0.5,"env_1_decay":1.0,"env_1_sustain":0.25,"env_1_release":0.75,"filter_1_on":1,"filter_1_cutoff":69}})";
    auto converted = vanta::PresetCodec::decode(vital, ".vital");
    check(converted.ok && converted.name == "Example" && std::abs(converted.values[vanta::attack] - 0.0625f) < 0.0001f
          && std::abs(converted.values[vanta::release] - 0.31640625f) < 0.0001f
          && std::abs(converted.values[vanta::cutoff] - 440.0f) < 0.1f, "Vital envelope and cutoff mapping");
    check(converted.message.containsIgnoreCase("approximate") && converted.message.containsIgnoreCase("substituted"), "Vital conversion warns about unsupported sound features");
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
    testOscillators();
    testPresets();
    std::cout << "All VANTA tests passed.\n";
    return 0;
}
