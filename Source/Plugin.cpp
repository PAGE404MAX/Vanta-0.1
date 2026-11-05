#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetImporter.h"
#include <cmath>
#include <cstring>

namespace
{
struct ApplyingStateGuard
{
    explicit ApplyingStateGuard(std::atomic<bool>& value) : flag(value) { flag.store(true, std::memory_order_release); }
    ~ApplyingStateGuard() { flag.store(false, std::memory_order_release); }
    std::atomic<bool>& flag;
};
// ValueTree host state is XML. Bound nested element depth before JUCE's
// recursive parser, while skipping comments, CDATA, declarations and quoted
// attribute values. The XML parser still validates names and syntax afterward.
bool safeHostXmlDepth(const std::uint8_t* data, std::size_t size) noexcept
{
    int depth = 0, elements = 0, attributes = 0;
    for (std::size_t i = 0; i < size;)
    {
        if (data[i] != '<') { ++i; continue; }
        auto starts = [&](const char* token)
        {
            const auto n = std::strlen(token);
            return n <= size - i && std::memcmp(data + i, token, n) == 0;
        };
        auto skipUntil = [&](const char* end, std::size_t start)
        {
            const auto n = std::strlen(end);
            for (std::size_t p = start; p + n <= size; ++p)
                if (std::memcmp(data + p, end, n) == 0) { i = p + n; return true; }
            return false;
        };
        if (starts("<!--")) { if (!skipUntil("-->", i + 4)) return false; continue; }
        if (starts("<![CDATA[")) { if (!skipUntil("]]>", i + 9)) return false; continue; }
        if (starts("<?")) { if (!skipUntil("?>", i + 2)) return false; continue; }
        if (starts("<!")) return false; // Do not process DTDs or external entities in host state.
        const bool closing = starts("</");
        if (closing) { if (--depth < 0) return false; }
        else if (++depth > 48 || ++elements > 4096) return false;
        std::uint8_t quote = 0, lastNonspace = 0;
        ++i;
        for (; i < size; ++i)
        {
            const auto c = data[i];
            if (quote != 0) { if (c == quote) quote = 0; continue; }
            if (c == '\'' || c == '"') { quote = c; continue; }
            if (c == '>') break;
            if (c == '=' && ++attributes > 8192) return false;
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') lastNonspace = c;
        }
        if (i == size || quote != 0) return false;
        if (!closing && lastNonspace == '/') --depth;
        ++i;
    }
    return depth == 0;
}
}

VantaProcessor::VantaProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "VANTA", makeLayout())
{
    for (std::size_t i = 0; i < vanta::parameterCount; ++i)
        rawParameters[i] = state.getRawParameterValue(vanta::parameterSpecs[i].id);
    state.state.setProperty("presetName", "VANTA INIT", nullptr);
    for (const auto id : vanta::wideEnvelopeTimes)
        state.addParameterListener(vanta::parameterSpecs[id].id, this);
}
VantaProcessor::~VantaProcessor()
{
    for (const auto id : vanta::wideEnvelopeTimes)
        state.removeParameterListener(vanta::parameterSpecs[id].id, this);
}
void VantaProcessor::parameterChanged(const juce::String& id, float)
{
    // This callback may run on the audio thread. Only atomics are touched.
    if (applyingState.load(std::memory_order_acquire)) return;
    for (std::size_t i = 0; i < vanta::wideEnvelopeTimes.size(); ++i)
        if (id == vanta::parameterSpecs[vanta::wideEnvelopeTimes[i]].id)
            wideEnvelopeTouched.fetch_or(1u << i, std::memory_order_relaxed);
}

void VantaProcessor::touchWideEnvelopeFromUI(int stage)
{
    if (stage < 0 || stage >= static_cast<int>(vanta::wideEnvelopeFlags.size())) return;
    const auto index = static_cast<std::size_t>(stage);
    if (rawParameters[vanta::wideEnvelopeFlags[index]]->load(std::memory_order_relaxed) >= 0.5f) return;
    const auto bit = 1u << index;
    if ((wideEnvelopeTouched.fetch_or(bit, std::memory_order_relaxed) & bit) == 0)
        updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged(true));
}

juce::AudioProcessorValueTreeState::ParameterLayout VantaProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (std::size_t i = 0; i < vanta::parameterCount; ++i)
    {
        const auto& p = vanta::parameterSpecs[i];
        if (i == vanta::wave)
        {
            // Do not expand this choice: old host automation maps 0/1 to sine/saw.
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { p.id, 1 }, p.name,
                                                                   juce::StringArray { "Sine", "Saw" }, 1));
        }
        else if (i == vanta::oscAExtraWave)
        {
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { p.id, 1 }, p.name,
                                                                   juce::StringArray { "Legacy waveform", "Square", "Triangle" }, 0));
        }
        else if (i >= vanta::oscBEnabled && i <= vanta::oscHPan && (i - vanta::oscBEnabled) % vanta::oscillatorStride == 0)
        {
            layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { p.id, 1 }, p.name, false));
        }
        else if (i >= vanta::oscBEnabled && i <= vanta::oscHPan && (i - vanta::oscBEnabled) % vanta::oscillatorStride == 1)
        {
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { p.id, 1 }, p.name,
                                                                   juce::StringArray { "Sine", "Saw", "Square", "Triangle" }, 0));
        }
        else if (i >= vanta::ampAttackWideActive && i <= vanta::ampReleaseWideActive)
            layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { p.id, 1 }, p.name, false,
                       juce::AudioParameterBoolAttributes().withAutomatable(false).withMeta(true)));
        else if (i == vanta::oscAEnabled || (i >= vanta::oscATable && i <= vanta::oscHTable) || (i >= vanta::oscASample && i <= vanta::oscHSample) || i == vanta::ottEnabled
                 || i == vanta::reverbEnabled || i == vanta::delayEnabled || i == vanta::distortionEnabled
                 || i == vanta::chorusEnabled || i == vanta::eqEnabled || i == vanta::fxFilterEnabled
                 || i == vanta::flangerEnabled || i == vanta::phaserEnabled)
            layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { p.id, 1 }, p.name, p.defaultValue >= 0.5f));
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
    const auto touched = wideEnvelopeTouched.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < vanta::wideEnvelopeFlags.size(); ++i)
        if ((touched & (1u << i)) != 0) values[vanta::wideEnvelopeFlags[i]] = 1.0f;
    return values;
}
void VantaProcessor::applyValues(const vanta::ParameterValues& values, const juce::String& name)
{
    // Loading a complete patch must not look like the user automating one of
    // the new parameters; its saved flags determine the source for each stage.
    ApplyingStateGuard guard(applyingState);
    wideEnvelopeTouched.store(0, std::memory_order_relaxed);
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
        case 1: v[vanta::wave] = 0.0f; v[vanta::attack] = 0.003f; v[vanta::decay] = 0.55f; v[vanta::sustain] = 0.18f; v[vanta::release] = 0.85f; v[vanta::reverbMix] = 0.22f; break;
        case 2: v[vanta::wave] = 1.0f; v[vanta::attack] = 0.60f; v[vanta::release] = 1.6f; v[vanta::cutoff] = 1800.0f; v[vanta::reverbSize] = 0.8f; v[vanta::reverbMix] = 0.28f; break;
        case 3: v[vanta::wave] = 1.0f; v[vanta::attack] = 0.008f; v[vanta::release] = 0.18f; v[vanta::cutoff] = 1100.0f; v[vanta::resonance] = 0.35f; v[vanta::delayMix] = 0.18f; break;
        case 4: v[vanta::wave] = 0.0f; v[vanta::attack] = 0.01f; v[vanta::decay] = 0.8f; v[vanta::sustain] = 0.3f; v[vanta::release] = 1.0f; v[vanta::vibrato] = 0.08f; v[vanta::reverbMix] = 0.32f; break;
        default: v[vanta::wave] = 1.0f; break; // VANTA INIT is a saw wave.
    }
    auto empty = std::make_unique<vanta::WaveTableBank>();
    engine.publishWaveTables(*empty);
    applyValues(v, factoryPresetName(index));
}
bool VantaProcessor::hasWaveTable(int oscillator) const
{
    if (oscillator < 0 || oscillator >= vanta::oscillatorCount) return false;
    return engine.copyWaveTables()->tables[static_cast<std::size_t>(oscillator)].frameCount > 0;
}
bool VantaProcessor::hasSample(int oscillator) const
{
    if (oscillator < 0 || oscillator >= vanta::oscillatorCount) return false;
    auto tables = engine.copyWaveTables();
    return tables->sample.slot == oscillator && tables->sample.frames > 0;
}
void VantaProcessor::applyPreset(const vanta::PresetResult& result)
{
    if (!result.ok) return;
    if (result.waveTables != nullptr) engine.publishWaveTables(*result.waveTables);
    else
    {
        auto empty = std::make_unique<vanta::WaveTableBank>();
        engine.publishWaveTables(*empty);
    }
    applyValues(result.values, result.name);
}
vanta::PresetResult VantaProcessor::loadPresetFile(const juce::File& file)
{
    auto result = vanta::PresetCodec::loadFile(file);
    applyPreset(result);
    return result;
}
juce::Result VantaProcessor::savePresetFile(const juce::File& file)
{
    auto tables = engine.copyWaveTables();
    auto result = vanta::PresetCodec::saveFile(file, file.getFileNameWithoutExtension(), currentValues(), tables.get());
    if (result.wasOk()) state.state.setProperty("presetName", file.getFileNameWithoutExtension().substring(0, 80), nullptr);
    return result;
}
juce::String VantaProcessor::presetName() const { return state.state.getProperty("presetName", "VANTA INIT").toString(); }
void VantaProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    // Host state calls are distinct from processBlock; tables are copied and
    // serialized here, never by an audio voice.
    try
    {
        auto snapshot = state.copyState();
        // A wide parameter can be activated by host automation on the audio
        // thread. Persist that atomically observed choice without writing the
        // ValueTree or notifying the host from the audio callback.
        const auto touched = wideEnvelopeTouched.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < vanta::wideEnvelopeFlags.size(); ++i)
            if ((touched & (1u << i)) != 0)
                for (auto child : snapshot)
                    if (child.hasType("PARAM") && child.getProperty("id").toString() == vanta::parameterSpecs[vanta::wideEnvelopeFlags[i]].id)
                        child.setProperty("value", 1.0f, nullptr);
        snapshot.setProperty("vantaEditorScale", editorScalePercent(), nullptr);
        auto tables = engine.copyWaveTables();
        bool present = false;
        for (const auto& table : tables->tables) present |= table.frameCount > 0;
        if (present) snapshot.setProperty("vantaWavetables", juce::JSON::toString(vanta::PresetImporter::encodeWaveTables(*tables), true), nullptr);
        else snapshot.removeProperty("vantaWavetables", nullptr);
        if (tables->sample.frames > 0) snapshot.setProperty("vantaSample", juce::JSON::toString(vanta::PresetImporter::encodeSample(tables->sample), true), nullptr);
        else snapshot.removeProperty("vantaSample", nullptr);
        if (tables->modulation.routeCount > 0) snapshot.setProperty("vantaModulation", juce::JSON::toString(vanta::PresetImporter::encodeModulation(tables->modulation), true), nullptr);
        else snapshot.removeProperty("vantaModulation", nullptr);
        if (auto xml = snapshot.createXml()) copyXmlToBinary(*xml, destination);
    }
    catch (...) { /* Leave destination untouched if a host requests state under memory pressure. */ }
}
void VantaProcessor::setStateInformation(const void* data, int size)
{
    try
    {
        if (data == nullptr || size <= 8 || size > vanta::PresetCodec::maxFileBytes) return;
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        const auto xmlLength = juce::ByteOrder::littleEndianInt(bytes + 4);
        if (xmlLength == 0 || xmlLength > static_cast<std::uint32_t>(size - 8)
            || !vanta::PresetImporter::isValidUtf8(bytes + 8, xmlLength)
            || !safeHostXmlDepth(bytes + 8, xmlLength)) return;
        auto xml = getXmlFromBinary(data, size);
        if (xml == nullptr || !xml->hasTagName(state.state.getType())) return;
        auto restored = juce::ValueTree::fromXml(*xml);
        // Host state is untrusted. Limit the text before converting it and use
        // the default for missing/invalid values from old or malformed projects.
        const auto scaleText = restored.getProperty("vantaEditorScale", juce::String(vanta::defaultEditorScale)).toString().trim();
        int restoredScale = vanta::defaultEditorScale;
        if (scaleText.isNotEmpty() && scaleText.length() <= 3 && scaleText.containsOnly("0123456789"))
        {
            const auto n = scaleText.getIntValue();
            if (n >= vanta::minEditorScale && n <= vanta::maxEditorScale) restoredScale = n;
        }
        restored.removeProperty("vantaEditorScale", nullptr);
        std::shared_ptr<vanta::WaveTableBank> tables;
        if (restored.hasProperty("vantaWavetables"))
        {
            juce::var encoded; juce::String error;
            if (!vanta::PresetImporter::parseJson(restored.getProperty("vantaWavetables").toString(), encoded, error)
                || !vanta::PresetImporter::decodeWaveTables(encoded, tables, error)) return;
            restored.removeProperty("vantaWavetables", nullptr);
        }
        if (restored.hasProperty("vantaSample"))
        {
            juce::var encoded; juce::String error;
            if (!vanta::PresetImporter::parseJson(restored.getProperty("vantaSample").toString(), encoded, error)
                || !vanta::PresetImporter::decodeSample(encoded, tables, error)) return;
            restored.removeProperty("vantaSample", nullptr);
        }
        if (restored.hasProperty("vantaModulation"))
        {
            juce::var encoded; juce::String error;
            if (!vanta::PresetImporter::parseJson(restored.getProperty("vantaModulation").toString(), encoded, error)
                || !vanta::PresetImporter::decodeModulation(encoded, tables, error)) return;
            restored.removeProperty("vantaModulation", nullptr);
        }
        // Seed a missing wide value from its corresponding legacy value, so
        // host automation starts from what the old project actually displayed.
        // The old parameter remains authoritative until a wide value is edited.
        auto findParameterChild = [&](const char* id)
        {
            for (const auto& child : restored)
                if (child.hasType("PARAM") && child.getProperty("id").toString() == id) return child;
            return juce::ValueTree();
        };
        for (std::size_t stage = 0; stage < vanta::legacyEnvelopeTimes.size(); ++stage)
        {
            const auto legacy = vanta::legacyEnvelopeTimes[stage], wide = vanta::wideEnvelopeTimes[stage];
            const auto flag = vanta::wideEnvelopeFlags[stage];
            const auto hasWide = findParameterChild(vanta::parameterSpecs[wide].id).isValid();
            if (!hasWide)
            {
                auto initial = vanta::parameterSpecs[wide].defaultValue;
                const auto old = findParameterChild(vanta::parameterSpecs[legacy].id);
                if (old.isValid())
                {
                    const auto text = old.getProperty("value").toString().trim();
                    if (text.isNotEmpty() && text.length() <= 64 && text.containsOnly("-+.0123456789eE"))
                    {
                        const auto parsed = text.getDoubleValue();
                        const auto& spec = vanta::parameterSpecs[legacy];
                        if (std::isfinite(parsed)) initial = static_cast<float>(juce::jlimit(
                            static_cast<double>(spec.minValue), static_cast<double>(spec.maxValue), parsed));
                    }
                }
                juce::ValueTree child("PARAM"); child.setProperty("id", vanta::parameterSpecs[wide].id, nullptr);
                child.setProperty("value", initial, nullptr); restored.addChild(child, -1, nullptr);
            }
            if (!findParameterChild(vanta::parameterSpecs[flag].id).isValid())
            {
                juce::ValueTree child("PARAM"); child.setProperty("id", vanta::parameterSpecs[flag].id, nullptr);
                child.setProperty("value", hasWide ? 1.0f : 0.0f, nullptr); restored.addChild(child, -1, nullptr);
            }
        }
        // Earlier projects lack the newly added parameters. Explicitly insert
        // defaults instead of retaining values from a previously loaded state.
        for (const auto& spec : vanta::parameterSpecs)
        {
            bool found = false;
            for (const auto& child : restored)
                if (child.hasType("PARAM") && child.getProperty("id").toString() == spec.id) { found = true; break; }
            if (!found)
            {
                juce::ValueTree child("PARAM"); child.setProperty("id", spec.id, nullptr);
                child.setProperty("value", spec.defaultValue, nullptr); restored.addChild(child, -1, nullptr);
            }
        }
        if (tables == nullptr) tables = std::make_shared<vanta::WaveTableBank>();
        engine.publishWaveTables(*tables);
        {
            ApplyingStateGuard guard(applyingState);
            wideEnvelopeTouched.store(0, std::memory_order_relaxed);
            state.replaceState(restored);
        }
        setEditorScalePercent(restoredScale);
    }
    catch (...) { /* Never let corrupt host state unwind through the host. */ }
}
juce::AudioProcessorEditor* VantaProcessor::createEditor() { return new VantaEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VantaProcessor(); }
