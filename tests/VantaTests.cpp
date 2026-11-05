#include "PresetCodec.h"
#include "SynthEngine.h"
#include "AmpEnvelope.h"
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

void testAmpEnvelope()
{
    static_assert(vanta::ampDelay == vanta::phaserMix + 1 && vanta::ampHold == vanta::ampDelay + 1,
                  "New parameters must be appended after all Beta 0.12 IDs");
    check(vanta::defaultValues()[vanta::ampDelay] == 0.0f && vanta::defaultValues()[vanta::ampHold] == 0.0f,
          "New amp-envelope stages default to zero");

    // Exercise releases, retriggers, automation, resets and sample-rate changes
    // against the exact ADSR used by Beta 0.12. Every output sample must match.
    bool legacyIdentical = true;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        vanta::AmpEnvelope current; juce::ADSR legacy;
        current.setSampleRate(rate); legacy.setSampleRate(rate);
        juce::ADSR::Parameters a; a.attack = 0.041f; a.decay = 0.07f; a.sustain = 0.34f; a.release = 0.08f;
        current.setParameters(a); legacy.setParameters(a); current.setDelayAndHold(0.0f, 0.0f);
        current.noteOn(); legacy.noteOn();
        for (int i = 0; i < 24000; ++i)
        {
            if (i == 900) { a.attack = 0.13f; current.setParameters(a); legacy.setParameters(a); }
            if (i == 4000) { current.noteOff(); legacy.noteOff(); }
            if (i == 5000) { current.noteOn(); legacy.noteOn(); }
            if (i == 10000) { a.decay = 0.2f; a.sustain = 0.7f; current.setParameters(a); legacy.setParameters(a); }
            if (i == 15000) { current.noteOff(); legacy.noteOff(); }
            if (i == 19000) { current.reset(); legacy.reset(); current.noteOn(); legacy.noteOn(); }
            legacyIdentical &= current.getNextSample() == legacy.getNextSample();
            legacyIdentical &= current.isActive() == legacy.isActive();
        }
    }
    check(legacyIdentical, "Zero delay/hold are sample-identical to the legacy ADSR at multiple rates");

    vanta::AmpEnvelope env; env.setSampleRate(1000.0);
    juce::ADSR::Parameters p; p.attack = 0.01f; p.decay = 0.02f; p.sustain = 0.25f; p.release = 0.03f;
    env.setParameters(p); env.setDelayAndHold(0.02f, 0.03f); env.noteOn();
    bool delayed = true;
    for (int i = 0; i < 10; ++i) delayed &= env.getNextSample() == 0.0f;
    env.setDelayAndHold(0.0f, 0.0f); // changes apply on the next note
    for (int i = 0; i < 10; ++i) delayed &= env.getNextSample() == 0.0f;
    const auto firstAttack = env.getNextSample();
    check(delayed && firstAttack > 0.05f && firstAttack < 0.15f,
          "Delay is silent for its full sample count and is latched for each note");
    float value = firstAttack; int attackSamples = 1;
    while (value < 1.0f && attackSamples < 20) { value = env.getNextSample(); ++attackSamples; }
    bool held = attackSamples >= 9 && attackSamples <= 11 && value == 1.0f;
    for (int i = 0; i < 30; ++i) held &= env.getNextSample() == 1.0f;
    value = env.getNextSample();
    check(held && value < 1.0f && value > p.sustain,
          "Attack reaches the peak, hold lasts its latched sample count, then decay starts");
    for (int i = 0; i < 30; ++i) value = env.getNextSample();
    check(std::abs(value - p.sustain) < 0.00001f, "Decay reaches sustain");
    env.noteOff(); const auto releaseStart = env.getNextSample();
    for (int i = 0; i < 40; ++i) value = env.getNextSample();
    check(releaseStart < p.sustain && releaseStart > 0.0f && value == 0.0f && !env.isActive(),
          "Note-off from sustain follows release to silence");

    env.reset(); env.setDelayAndHold(0.1f, 0.03f); env.noteOn();
    for (int i = 0; i < 25; ++i) env.getNextSample();
    env.noteOff(); bool cancelled = true;
    for (int i = 0; i < 200; ++i) cancelled &= env.getNextSample() == 0.0f;
    check(cancelled && !env.isActive(), "Note-off during delay cancels the future attack");

    env.reset(); env.setDelayAndHold(0.0f, 0.1f); env.noteOn();
    for (int i = 0; i < 4; ++i) value = env.getNextSample();
    env.noteOff(); const auto attackRelease = env.getNextSample();
    for (int i = 0; i < 40; ++i) env.getNextSample();
    check(attackRelease > 0.0f && attackRelease < value && !env.isActive(),
          "Note-off during attack releases from the current level without entering hold");

    env.reset(); env.setDelayAndHold(0.0f, 0.1f); env.noteOn();
    for (int i = 0; i < 20 && env.getNextSample() < 1.0f; ++i) {}
    for (int i = 0; i < 8; ++i) env.getNextSample();
    env.noteOff(); const auto holdRelease = env.getNextSample();
    for (int i = 0; i < 40; ++i) env.getNextSample();
    check(holdRelease < 1.0f && holdRelease > 0.0f && !env.isActive(),
          "Note-off during hold starts release immediately from the peak");

    env.reset(); env.setDelayAndHold(0.0f, 0.0f); env.noteOn();
    check(env.getNextSample() > 0.0f, "A new note uses newly automated delay/hold values");
    env.reset(); check(!env.isActive() && env.getNextSample() == 0.0f, "Voice reset clears every envelope stage");

    for (double rate : { 44100.0, 48000.0 })
    {
        env.reset(); env.setSampleRate(rate); env.setParameters(p); env.setDelayAndHold(0.125f, 0.0f); env.noteOn();
        const int samples = static_cast<int>(std::lround(rate * 0.125));
        bool silent = true; for (int i = 0; i < samples; ++i) silent &= env.getNextSample() == 0.0f;
        check(silent && env.getNextSample() > 0.0f, "Delay length follows the sample rate");
    }

    constexpr int rate = 44100;
    auto settings = vanta::defaultValues(); settings[vanta::attack] = 0.001f; settings[vanta::ampDelay] = 0.1f;
    auto delayedAudio = render(settings, rate / 3);
    check(rms(delayedAudio, 0, rate / 10) == 0.0f && rms(delayedAudio, rate / 8, rate / 20) > 0.005f,
          "Amp delay changes real rendered audio independently of the delay effect");
    auto shortNote = render(settings, rate / 3, rate / 20);
    check(rms(shortNote, 0, shortNote.getNumSamples()) == 0.0f, "A note released before delay remains silent");
    settings[vanta::ampDelay] = 0.0f; settings[vanta::decay] = 0.02f; settings[vanta::sustain] = 0.1f;
    auto noHold = render(settings, rate / 3); settings[vanta::ampHold] = 0.2f;
    auto heldAudio = render(settings, rate / 3);
    check(rms(heldAudio, rate / 12, rate / 20) > 5.0f * rms(noHold, rate / 12, rate / 20),
          "Amp hold audibly delays decay in the real synth");

    // Different notes own separate delay counters: releasing the first note
    // during delay must not cancel the later note's scheduled attack.
    settings[vanta::ampDelay] = 0.1f; settings[vanta::ampHold] = 0.05f;
    vanta::SynthEngine poly; poly.prepare(rate, 512); juce::AudioBuffer<float> voices(2, rate / 3); juce::MidiBuffer notes;
    notes.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
    notes.addEvent(juce::MidiMessage::noteOff(1, 60), rate / 25);
    notes.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(100)), rate / 20);
    poly.process(voices, notes, settings);
    check(rms(voices, 0, rate * 15 / 100) == 0.0f && rms(voices, rate * 17 / 100, rate / 20) > 0.005f,
          "Overlapping voices have independent delay and note-off stages");
    poly.reset(); juce::MidiBuffer none; poly.process(voices, none, settings);
    check(rms(voices, 0, voices.getNumSamples()) == 0.0f, "Engine reset clears delayed and held voices");

    vanta::SynthEngine pedal; pedal.prepare(rate, 512); juce::AudioBuffer<float> pedalAudio(2, rate / 2); juce::MidiBuffer pedalNotes;
    settings[vanta::release] = 0.03f;
    pedalNotes.addEvent(juce::MidiMessage::noteOn(1, 67, static_cast<juce::uint8>(100)), 0);
    pedalNotes.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 64);
    pedalNotes.addEvent(juce::MidiMessage::noteOff(1, 67), rate / 20);
    pedalNotes.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), rate / 4);
    pedal.process(pedalAudio, pedalNotes, settings);
    check(rms(pedalAudio, 0, rate / 10) == 0.0f && rms(pedalAudio, rate * 13 / 100, rate / 20) > 0.005f
          && rms(pedalAudio, rate * 35 / 100, rate / 20) < 0.00001f,
          "Sustain pedal defers note-off through delay and release begins when the pedal lifts");

    // Overflow the 16-voice pool while every original voice is in delay.
    // The stolen voice must be fully reset before the later note is scheduled.
    vanta::SynthEngine stolen; stolen.prepare(rate, 512); juce::AudioBuffer<float> stolenAudio(2, rate / 2); juce::MidiBuffer busy;
    settings[vanta::ampDelay] = 0.12f;
    for (int note = 40; note < 56; ++note)
        busy.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)), 0);
    busy.addEvent(juce::MidiMessage::noteOn(1, 72, static_cast<juce::uint8>(100)), rate / 20);
    for (int note = 40; note < 56; ++note) busy.addEvent(juce::MidiMessage::noteOff(1, note), rate / 10);
    busy.addEvent(juce::MidiMessage::noteOff(1, 72), rate / 4);
    stolen.process(stolenAudio, busy, settings);
    check(rms(stolenAudio, 0, rate * 17 / 100) == 0.0f && rms(stolenAudio, rate * 20 / 100, rate / 25) > 0.005f
          && rms(stolenAudio, rate * 35 / 100, rate / 20) < 0.00001f,
          "Voice stealing resets old delay/hold state and schedules the new note independently");

    // Identical MIDI and parameters produce the same envelope regardless of
    // host audio-block boundaries, including note-off in a held peak.
    settings[vanta::ampDelay] = 0.035f; settings[vanta::ampHold] = 0.14f;
    constexpr int chunk = 256, blocks = 80, total = chunk * blocks;
    vanta::SynthEngine wholeEngine, blockEngine; wholeEngine.prepare(rate, chunk); blockEngine.prepare(rate, chunk);
    juce::AudioBuffer<float> whole(2, total), chunked(2, total); juce::MidiBuffer wholeMidi;
    wholeMidi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0);
    wholeMidi.addEvent(juce::MidiMessage::noteOff(1, 69), chunk * 30);
    wholeEngine.process(whole, wholeMidi, settings);
    for (int block = 0; block < blocks; ++block)
    {
        juce::AudioBuffer<float> part(2, chunk); juce::MidiBuffer midi;
        if (block == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0);
        if (block == 30) midi.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
        blockEngine.process(part, midi, settings);
        for (int channel = 0; channel < 2; ++channel) chunked.copyFrom(channel, block * chunk, part, channel, 0, chunk);
    }
    bool blockIdentical = true;
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < total; ++i) blockIdentical &= whole.getSample(channel, i) == chunked.getSample(channel, i);
    check(blockIdentical, "DAHDSR rendering is identical across host block boundaries");
}

void testAudio()
{
    constexpr int rate = 44100;
    auto p = vanta::defaultValues(); p[vanta::attack] = 0.001f; p[vanta::wave] = 0.0f;
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
    auto mutedDistortion = distorted; mutedDistortion[vanta::distortionEnabled] = 0.0f;
    auto noDistortion = distorted; noDistortion[vanta::distortionMix] = 0.0f;
    const auto bypassedDistortion = render(mutedDistortion, rate / 4), originalDry = render(noDistortion, rate / 4);
    double bypassDifference = 0.0;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < rate / 4; ++i)
        bypassDifference += std::abs(bypassedDistortion.getSample(c, i) - originalDry.getSample(c, i));
    check(bypassDifference == 0.0, "Legacy distortion power switch bypasses the effect");
    auto withChorus = p; withChorus[vanta::chorusEnabled] = 1.0f;
    const auto chorusAudio = render(withChorus, rate / 4);
    difference = 0.0;
    for (int i = rate / 20; i < rate / 4; ++i) difference += std::abs(chorusAudio.getSample(0, i) - saw.getSample(0, i));
    check(difference / (rate / 4 - rate / 20) > 0.001, "Effects rack is connected to the synth output");

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
    auto base = vanta::defaultValues(); base[vanta::attack] = 0.001f; base[vanta::wave] = 0.0f;
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
double averageDifference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int start, int end, int channel = 0)
{
    double sum = 0.0;
    for (int i = start; i < end; ++i) sum += std::abs(a.getSample(channel, i) - b.getSample(channel, i));
    return sum / (end - start);
}
void testVitalRanges()
{
    using namespace vanta;
    static_assert(ampAttackWide == ampHold + 1, "0.13 parameter IDs must stay in their original order");
    check(parameterSpecs[ampDelay].minValue == 0.0f && parameterSpecs[ampDelay].maxValue == 4.0f
          && parameterSpecs[ampHold].minValue == 0.0f && parameterSpecs[ampHold].maxValue == 4.0f
          && parameterSpecs[sustain].minValue == 0.0f && parameterSpecs[sustain].maxValue == 1.0f,
          "Envelope delay/hold and sustain cover the requested ranges");
    bool envelopeRanges = true, oscRanges = true;
    for (auto id : wideEnvelopeTimes)
        envelopeRanges &= parameterSpecs[id].minValue == 0.0f && parameterSpecs[id].maxValue == 32.0f;
    for (int layer = 0; layer < oscillatorCount; ++layer)
        oscRanges &= parameterSpecs[oscillatorLevel(layer)].minValue == 0.0f && parameterSpecs[oscillatorLevel(layer)].maxValue == 1.0f
                  && parameterSpecs[oscillatorPan(layer)].minValue == -1.0f && parameterSpecs[oscillatorPan(layer)].maxValue == 1.0f
                  && parameterSpecs[oscillatorUnison(layer)].minValue == 0.0f && parameterSpecs[oscillatorUnison(layer)].maxValue == 1.0f
                  && parameterSpecs[oscillatorPhase(layer)].minValue == 0.0f && parameterSpecs[oscillatorPhase(layer)].maxValue == 1.0f;
    check(envelopeRanges && oscRanges, "Long envelope and all eight oscillator ranges are available");
    check(parameterSpecs[lfoSmooth].minValue == 0.0f && parameterSpecs[lfoSmooth].maxValue == 16.0f
          && parameterSpecs[lfoDelay].minValue == 0.0f && parameterSpecs[lfoDelay].maxValue == 4.0f
          && parameterSpecs[lfoStereo].minValue == -0.5f && parameterSpecs[lfoStereo].maxValue == 0.5f,
          "Pitch LFO smooth, delay and stereo cover the requested ranges");
    auto defaults = defaultValues();
    check(defaults[wave] == 1.0f && oscillatorShape(defaults, 0) == 1,
          "VANTA INIT defaults OSC A to a saw wave");
    check(defaults[ampAttackWideActive] == 0.0f && defaults[ampDecayWideActive] == 0.0f
          && defaults[ampReleaseWideActive] == 0.0f && defaults[lfoSmooth] == 0.0f
          && defaults[lfoDelay] == 0.0f && defaults[lfoStereo] == 0.0f,
          "New controls default to legacy-compatible behavior");

    AmpEnvelope env; env.setSampleRate(1000.0);
    juce::ADSR::Parameters instant; instant.attack = 0.0f; instant.decay = 0.0f; instant.sustain = 0.0f; instant.release = 0.2f;
    env.setParameters(instant); env.setDelayAndHold(4.0f, 4.0f); env.noteOn();
    bool delayed = true, held = true;
    for (int i = 0; i < 4000; ++i) delayed &= env.getNextSample() == 0.0f;
    for (int i = 0; i < 4000; ++i) held &= env.getNextSample() == 1.0f;
    check(delayed && held && env.getNextSample() == 0.0f,
          "Zero attack/decay and maximum delay/hold have correct stage lengths");
    env.reset(); env.setDelayAndHold(0.0f, 1.0f); env.noteOn();
    for (int i = 0; i < 25; ++i) env.getNextSample();
    env.noteOff(); const auto releasedDuringHold = env.getNextSample();
    check(releasedDuringHold > 0.98f && releasedDuringHold < 1.0f,
          "Releasing a zero-attack hold starts release from the peak");
    env.reset(); auto changing = instant; changing.attack = 1.0f;
    env.setParameters(changing); env.setDelayAndHold(0.0f, 0.1f); env.noteOn();
    for (int i = 0; i < 20; ++i) env.getNextSample();
    changing.attack = 0.0f; env.setParameters(changing);
    bool automatedHold = true;
    for (int i = 0; i < 100; ++i) automatedHold &= env.getNextSample() == 1.0f;
    check(automatedHold && env.getNextSample() == 0.0f,
          "Automating attack to zero mid-stage still reaches and holds the peak");
    instant.release = 0.0f; env.reset(); env.setParameters(instant); env.noteOn(); env.getNextSample(); env.noteOff();
    check(!env.isActive() && env.getNextSample() == 0.0f, "A zero release stops immediately");

    juce::ADSR::Parameters longTimes; longTimes.attack = 32.0f; longTimes.decay = 32.0f;
    longTimes.sustain = 0.5f; longTimes.release = 32.0f;
    env.reset(); env.setParameters(longTimes); env.setDelayAndHold(0.0f, 0.0f); env.noteOn();
    float sample = 0.0f;
    for (int i = 0; i < 16000; ++i) sample = env.getNextSample();
    check(sample > 0.49f && sample < 0.51f, "A 32-second attack reaches its midpoint after 16 seconds");
    for (int i = 0; i < 16020; ++i) sample = env.getNextSample();
    for (int i = 0; i < 16000; ++i) sample = env.getNextSample();
    check(sample > 0.74f && sample < 0.76f, "A 32-second decay follows the requested duration");
    env.noteOff();
    for (int i = 0; i < 16000; ++i) sample = env.getNextSample();
    check(sample > 0.36f && sample < 0.39f, "A 32-second release follows the requested duration");

    constexpr int rate = 44100, length = rate / 2;
    auto base = defaultValues(); base[attack] = 0.001f; base[decay] = 0.001f; base[sustain] = 1.0f;
    const auto original = render(base, length);
    auto phase = base; phase[oscAPhase] = 0.25f;
    const auto quarter = render(phase, length);
    phase[oscAPhase] = 0.50f; const auto half = render(phase, length);
    phase[oscAPhase] = 1.0f; const auto wrapped = render(phase, length);
    check(averageDifference(original, quarter, 1000, length) > 0.01
          && averageDifference(quarter, half, 1000, length) > 0.01
          && averageDifference(original, wrapped, 0, length) == 0.0,
          "Oscillator phase changes the start and 100 percent wraps to zero");
    auto unison = base; unison[oscAUnison] = 0.5f; const auto medium = render(unison, length);
    unison[oscAUnison] = 1.0f; const auto maximum = render(unison, length);
    double side = 0.0;
    for (int i = 1000; i < length; ++i) side += std::abs(maximum.getSample(0, i) - maximum.getSample(1, i));
    check(averageDifference(original, medium, 1000, length) > 0.001
          && averageDifference(medium, maximum, 1000, length) > 0.001 && side / (length - 1000) > 0.001,
          "Unison amount produces different, stereo detuned sounds at 50 and 100 percent");
    auto layerB = base; layerB[oscAEnabled] = 0.0f; layerB[oscBEnabled] = 1.0f; layerB[oscBLevel] = 1.0f;
    const auto bClean = render(layerB, length); layerB[oscBUnison] = 1.0f; layerB[oscBPhase] = 0.5f;
    const auto bChanged = render(layerB, length);
    check(averageDifference(bClean, bChanged, 1000, length) > 0.001, "Unison and phase work on independent OSC B");

    auto vibrato = base; vibrato[vibratoRate] = 5.0f; vibrato[vanta::vibrato] = 1.0f;
    const auto direct = render(vibrato, length);
    auto delayedLfo = vibrato; delayedLfo[lfoDelay] = 0.25f; const auto delayedSound = render(delayedLfo, length);
    check(averageDifference(original, delayedSound, 0, rate / 5) == 0.0
          && averageDifference(original, delayedSound, rate * 3 / 10, length) > 0.001,
          "Pitch LFO delay holds modulation off, then starts it per note");
    auto smoothed = vibrato; smoothed[lfoSmooth] = 0.4f; const auto smoothSound = render(smoothed, length);
    check(averageDifference(direct, smoothSound, 1000, length) > 0.001, "Pitch LFO smooth changes the rendered modulation");
    auto stereo = vibrato; stereo[lfoStereo] = 0.5f; const auto positive = render(stereo, length);
    stereo[lfoStereo] = -0.5f; const auto negative = render(stereo, length);
    side = 0.0; double swapped = 0.0;
    for (int i = 1000; i < length; ++i)
    {
        side += std::abs(positive.getSample(0, i) - positive.getSample(1, i));
        swapped += std::abs(positive.getSample(0, i) - negative.getSample(1, i));
    }
    check(side / (length - 1000) > 0.001 && swapped / (length - 1000) < 1.0e-7,
          "Pitch LFO stereo creates opposite phase modulation and reverses channels at negative values");
    stereo[lfoSmooth] = 16.0f; stereo[lfoDelay] = 4.0f; const auto extremes = render(stereo, length);
    bool finite = true;
    for (int c = 0; c < extremes.getNumChannels(); ++c)
        for (int i = 0; i < length; ++i) finite &= std::isfinite(extremes.getSample(c, i)) && std::abs(extremes.getSample(c, i)) <= 1.0f;
    check(finite, "Maximum LFO smooth/delay and negative stereo stay finite and bounded");
    auto rich = vibrato; rich[lfoStereo] = 0.4f; rich[oscAUnison] = 0.7f;
    const auto richStereo = render(rich, length);
    SynthEngine monoEngine; monoEngine.prepare(rate, 512);
    juce::AudioBuffer<float> mono(1, length); juce::MidiBuffer monoMidi;
    monoMidi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0);
    monoEngine.process(mono, monoMidi, rich);
    double monoError = 0.0;
    for (int i = 0; i < length; ++i)
        monoError += std::abs(mono.getSample(0, i) - 0.5f * (richStereo.getSample(0, i) + richStereo.getSample(1, i)));
    check(monoError / length < 1.0e-6, "Stereo unison and LFO downmix safely to mono");
}
void testPresets()
{
    auto values = vanta::defaultValues();
    values[vanta::attack] = 0.234f; values[vanta::ampDelay] = 0.456f; values[vanta::ampHold] = 1.23f;
    values[vanta::wave] = 1.0f; values[vanta::cutoff] = 1234.0f;
    values[vanta::oscAExtraWave] = 2.0f; values[vanta::oscBEnabled] = 1.0f; values[vanta::oscBWave] = 2.0f;
    values[vanta::oscBTune] = -12.0f; values[vanta::oscBFine] = 17.0f; values[vanta::oscBPan] = -0.75f;
    values[vanta::oscDEnabled] = 1.0f; values[vanta::oscHEnabled] = 1.0f;
    values[vanta::chorusEnabled] = 1.0f; values[vanta::chorusRate] = 1.7f;
    values[vanta::ampAttackWide] = 32.0f; values[vanta::ampAttackWideActive] = 1.0f;
    values[vanta::lfoSmooth] = 16.0f; values[vanta::lfoDelay] = 4.0f; values[vanta::lfoStereo] = -0.5f;
    values[vanta::oscHUnison] = 0.5f; values[vanta::oscHPhase] = 1.0f;
    const auto encoded = vanta::PresetCodec::encode("My patch", values);
    auto decoded = vanta::PresetCodec::decode(encoded, ".vanta");
    check(decoded.ok && decoded.name == "My patch", "Native preset round-trip loads name");
    bool matches = true;
    for (std::size_t i = 0; i < values.size(); ++i) matches &= std::abs(values[i] - decoded.values[i]) < 1.0e-3f;
    check(matches, "Native preset round-trip loads every parameter");
    const auto legacy = vanta::PresetCodec::decode(R"({"format":"vanta-preset","version":1,"name":"Old","parameters":{"wave":1,"level":0.6}})", ".vanta");
    check(legacy.ok && legacy.values[vanta::wave] == 1.0f && legacy.values[vanta::oscAExtraWave] == 0.0f
          && legacy.values[vanta::oscALevel] == 1.0f && legacy.values[vanta::oscBEnabled] == 0.0f
          && legacy.values[vanta::ampDelay] == 0.0f && legacy.values[vanta::ampHold] == 0.0f,
          "Older VANTA presets retain the original saw and default new layers to off");
    check(legacy.values[vanta::reverbEnabled] == 1.0f && legacy.values[vanta::delayEnabled] == 1.0f
          && legacy.values[vanta::distortionEnabled] == 1.0f
          && legacy.values[vanta::chorusEnabled] == 0.0f,
          "Old presets retain legacy FX behaviour and default new features off");
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
    testAmpEnvelope();
    testAudio();
    testOscillators();
    testVitalRanges();
    testPresets();
    std::cout << "All VANTA tests passed.\n";
    return 0;
}
