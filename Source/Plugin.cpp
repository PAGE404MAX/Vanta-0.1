#include <JuceHeader.h>
#include <atomic>
#include <cmath>

class Sound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
class Voice final : public juce::SynthesiserVoice
{
public:
    Voice(std::atomic<float>* a, std::atomic<float>* r, std::atomic<float>* shape)
        : attack(a), release(r), waveform(shape) {}
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<Sound*>(s) != nullptr; }
    void startNote(int note, float velocity, juce::SynthesiserSound*, int) override
    {
        phase = 0.0; amplitude = velocity * 0.12f;
        increment = juce::MathConstants<double>::twoPi * juce::MidiMessage::getMidiNoteInHertz(note) / getSampleRate();
        juce::ADSR::Parameters p;
        p.attack = attack->load(); p.decay = 0.1f; p.sustain = 0.8f; p.release = release->load();
        envelope.setSampleRate(getSampleRate()); envelope.setParameters(p); envelope.noteOn();
    }
    void stopNote(float, bool tail) override { if (tail) envelope.noteOff(); else { envelope.reset(); clearCurrentNote(); } }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int count) override
    {
        if (!isVoiceActive()) return;
        const auto shape = waveform->load();
        for (int i = 0; i < count; ++i)
        {
            const auto sample = static_cast<float>(shape < 0.5f ? std::sin(phase) : (phase / juce::MathConstants<double>::pi - 1.0));
            const auto value = sample * amplitude * envelope.getNextSample();
            for (int ch = 0; ch < out.getNumChannels(); ++ch) out.addSample(ch, start + i, value);
            phase += increment;
            if (phase >= juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
        }
        if (!envelope.isActive()) clearCurrentNote();
    }
private:
    std::atomic<float>* attack; std::atomic<float>* release; std::atomic<float>* waveform;
    juce::ADSR envelope; double phase = 0, increment = 0; float amplitude = 0;
};
class Vanta final : public juce::AudioProcessor
{
public:
    Vanta() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
              state(*this, nullptr, "VANTA", makeLayout())
    {
        synth.addSound(new Sound());
        for (int i = 0; i < 16; ++i)
            synth.addVoice(new Voice(state.getRawParameterValue("attack"), state.getRawParameterValue("release"), state.getRawParameterValue("wave")));
    }
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"level",1}, "Level", juce::NormalisableRange<float>(0.f,1.f), 0.7f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"attack",1}, "Attack", juce::NormalisableRange<float>(0.001f,2.f,0.001f,0.4f), 0.02f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"release",1}, "Release", juce::NormalisableRange<float>(0.01f,4.f,0.001f,0.4f), 0.35f));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"wave",1}, "Waveform", juce::StringArray{"Sine", "Saw"}, 0));
        return p;
    }
    const juce::String getName() const override { return "Vanta Beta 0.1"; }
    void prepareToPlay(double rate, int) override { synth.setCurrentPlaybackSampleRate(rate); }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono(); }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    { buffer.clear(); synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples()); buffer.applyGain(state.getRawParameterValue("level")->load()); }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 4.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& dest) override { if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest); }
    void setStateInformation(const void* data, int size) override { if (auto xml = getXmlFromBinary(data,size)) if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml)); }
private:
    juce::AudioProcessorValueTreeState state; juce::Synthesiser synth;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Vanta(); }
