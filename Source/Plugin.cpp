#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

VantaProcessor::VantaProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "VANTA", makeLayout())
{
    for (std::size_t i = 0; i < vanta::parameterCount; ++i)
        rawParameters[i] = state.getRawParameterValue(vanta::parameterSpecs[i].id);
    state.state.setProperty("presetName", "VANTA INIT", nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout VantaProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (std::size_t i = 0; i < vanta::parameterCount; ++i)
    {
        const auto& p = vanta::parameterSpecs[i];
        if (i == vanta::wave)
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { p.id, 1 }, p.name,
                                                                   juce::StringArray { "Sine", "Saw" }, 0));
        else
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { p.id, 1 }, p.name,
                juce::NormalisableRange<float>(p.minValue, p.maxValue, p.step, p.skew), p.defaultValue));
    }
    return layout;
}

void VantaProcessor::prepareToPlay(double rate, int maximumBlockSize) { engine.prepare(rate, maximumBlockSize); }
void VantaProcessor::releaseResources() { engine.reset(); }
bool VantaProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto& output = layout.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}
void VantaProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (wrapperType == wrapperType_Standalone)
        keyboardState.processNextMidiBuffer(midi, 0, buffer.getNumSamples(), true);
    engine.process(buffer, midi, currentValues());
}
vanta::ParameterValues VantaProcessor::currentValues() const
{
    auto values = vanta::defaultValues();
    for (std::size_t i = 0; i < values.size(); ++i)
        if (rawParameters[i] != nullptr)
        {
            const auto value = rawParameters[i]->load(std::memory_order_relaxed);
            const auto& spec = vanta::parameterSpecs[i];
            values[i] = std::isfinite(value) ? juce::jlimit(spec.minValue, spec.maxValue, value) : spec.defaultValue;
        }
    return values;
}
void VantaProcessor::applyValues(const vanta::ParameterValues& values, const juce::String& name)
{
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        const auto& spec = vanta::parameterSpecs[i];
        if (auto* parameter = state.getParameter(spec.id))
        {
            const auto value = std::isfinite(values[i]) ? juce::jlimit(spec.minValue, spec.maxValue, values[i]) : spec.defaultValue;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    }
    state.state.setProperty("presetName", name.trim().substring(0, 80), nullptr);
}
const char* VantaProcessor::factoryPresetName(int index)
{
    static constexpr const char* names[] = { "VANTA INIT", "CRIMSON BELL", "RED HORIZON", "NIGHT CIRCUIT", "GLASS SIGNAL" };
    return names[juce::jlimit(0, factoryPresetCount - 1, index)];
}
const char* VantaProcessor::factoryPresetCategory(int index)
{
    static constexpr const char* categories[] = { "INIT", "SYNTH", "PADS", "LEADS", "SYNTH" };
    return categories[juce::jlimit(0, factoryPresetCount - 1, index)];
}
void VantaProcessor::loadFactoryPreset(int index)
{
    auto v = vanta::defaultValues();
    switch (index)
    {
        case 1: v[vanta::attack] = 0.003f; v[vanta::decay] = 0.55f; v[vanta::sustain] = 0.18f; v[vanta::release] = 0.85f; v[vanta::reverbMix] = 0.22f; break;
        case 2: v[vanta::wave] = 1.0f; v[vanta::attack] = 0.60f; v[vanta::release] = 1.6f; v[vanta::cutoff] = 1800.0f; v[vanta::reverbSize] = 0.8f; v[vanta::reverbMix] = 0.28f; break;
        case 3: v[vanta::wave] = 1.0f; v[vanta::attack] = 0.008f; v[vanta::release] = 0.18f; v[vanta::cutoff] = 1100.0f; v[vanta::resonance] = 0.35f; v[vanta::delayMix] = 0.18f; break;
        case 4: v[vanta::attack] = 0.01f; v[vanta::decay] = 0.8f; v[vanta::sustain] = 0.3f; v[vanta::release] = 1.0f; v[vanta::vibrato] = 0.08f; v[vanta::reverbMix] = 0.32f; break;
        default: break;
    }
    applyValues(v, factoryPresetName(index));
}
vanta::PresetResult VantaProcessor::loadPresetFile(const juce::File& file)
{
    auto result = vanta::PresetCodec::loadFile(file);
    if (result.ok) applyValues(result.values, result.name);
    return result;
}
juce::Result VantaProcessor::savePresetFile(const juce::File& file)
{
    auto result = vanta::PresetCodec::saveFile(file, file.getFileNameWithoutExtension(), currentValues());
    if (result.wasOk()) state.state.setProperty("presetName", file.getFileNameWithoutExtension().substring(0, 80), nullptr);
    return result;
}
juce::String VantaProcessor::presetName() const { return state.state.getProperty("presetName", "VANTA INIT").toString(); }
void VantaProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, destination);
}
void VantaProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* VantaProcessor::createEditor() { return new VantaEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VantaProcessor(); }
