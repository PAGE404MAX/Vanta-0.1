#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>

namespace
{
const char* names[] = {"Volume", "Filter", "Drive",  "Crush",  "Time",  "Pan",
                       "Width",  "Noise",  "Liquid", "Reverb", "Pitch", "Compressor"};
float read(const std::atomic<float>* p, float fallback) noexcept
{
    const float value = p ? p->load(std::memory_order_relaxed) : fallback;
    return std::isfinite(value) ? value : fallback;
}
bool safeXml(const std::uint8_t* data, std::size_t size) noexcept
{
    int depth = 0, elements = 0, attributes = 0;
    for (std::size_t i = 0; i < size;)
    {
        if (data[i] >= 128 || data[i] == 0)
            return false;
        if (data[i] != '<')
        {
            ++i;
            continue;
        }
        auto starts = [&](const char* t)
        {
            const auto n = std::strlen(t);
            return n <= size - i && std::memcmp(data + i, t, n) == 0;
        };
        auto skip = [&](const char* end, std::size_t start)
        {
            const auto n = std::strlen(end);
            for (std::size_t p = start; p + n <= size; ++p)
            {
                if (data[p] >= 128 || data[p] == 0)
                    return false;
                if (std::memcmp(data + p, end, n) == 0)
                {
                    i = p + n;
                    return true;
                }
            }
            return false;
        };
        if (starts("<!--"))
        {
            if (!skip("-->", i + 4))
                return false;
            continue;
        }
        if (starts("<?"))
        {
            if (!skip("?>", i + 2))
                return false;
            continue;
        }
        if (starts("<!"))
            return false;
        const bool closing = starts("</");
        if (closing)
        {
            if (--depth < 0)
                return false;
        }
        else if (++depth > 8 || ++elements > 2500)
            return false;
        std::uint8_t quote = 0, last = 0;
        ++i;
        for (; i < size; ++i)
        {
            const auto c = data[i];
            if (c >= 128 || c == 0)
                return false;
            if (quote)
            {
                if (c == quote)
                    quote = 0;
                continue;
            }
            if (c == '\'' || c == '"')
            {
                quote = c;
                continue;
            }
            if (c == '>')
                break;
            if (c == '=' && ++attributes > 8000)
                return false;
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                last = c;
        }
        if (i == size || quote)
            return false;
        if (!closing && last == '/')
            --depth;
        ++i;
    }
    return depth == 0;
}
bool number(const juce::var& v, float& out)
{
    const auto text = v.toString().trim();
    if (text.isEmpty() || text.length() > 32)
        return false;
    const char* start = text.toRawUTF8();
    char* end = nullptr;
    const float n = std::strtof(start, &end);
    if (end == start || *end != '\0' || !std::isfinite(n))
        return false;
    out = n;
    return true;
}
int boundedProperty(const juce::ValueTree& tree, const char* name, int fallback, int lo, int hi)
{
    float n = 0;
    return tree.hasProperty(name) && number(tree.getProperty(name), n) && n >= lo && n <= hi && std::floor(n) == n
               ? static_cast<int>(n)
               : fallback;
}
} // namespace

SidechainProcessor::SidechainProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "VANTA_SIDECHAIN", makeLayout())
{
    masterEnabled = state.getRawParameterValue("enabled");
    masterMix = state.getRawParameterValue("masterMix");
    for (int m = 0; m < sidechain::moduleCount; ++m)
    {
        auto& r = refs[static_cast<std::size_t>(m)];
        auto get = [&](const char* name) { return state.getRawParameterValue(moduleId(m, name)); };
        r.enabled = get("on");
        r.multi = get("multi");
        r.source = get("source");
        r.length = get("length");
        r.order = get("order");
        r.oneShot = get("oneshot");
        r.threshold = get("threshold");
        r.attack = get("attack");
        r.release = get("release");
        r.smooth = get("smooth");
        for (int b = 0; b < sidechain::bandCount; ++b)
        {
            auto& br = r.bands[static_cast<std::size_t>(b)];
            br.amount = state.getRawParameterValue(bandId(m, b, "amount"));
            br.detail = state.getRawParameterValue(bandId(m, b, "detail"));
            br.mix = state.getRawParameterValue(bandId(m, b, "mix"));
            br.stepped = state.getRawParameterValue(bandId(m, b, "stepped"));
            for (int i = 0; i < sidechain::stepCount; ++i)
            {
                const auto id = pointId(m, b, i);
                br.curve[i] = state.getRawParameterValue(id);
                br.point[i] = state.getParameter(id);
            }
        }
    }
}
juce::String SidechainProcessor::moduleId(int m, const char* suffix)
{
    return "m" + juce::String(m).paddedLeft('0', 2) + "_" + suffix;
}
juce::String SidechainProcessor::bandId(int m, int b, const char* suffix)
{
    return "m" + juce::String(m).paddedLeft('0', 2) + "_b" + juce::String(b) + "_" + suffix;
}
juce::String SidechainProcessor::pointId(int m, int b, int p)
{
    return bandId(m, b, ("p" + juce::String(p).paddedLeft('0', 2)).toRawUTF8());
}
juce::AudioProcessorValueTreeState::ParameterLayout SidechainProcessor::makeLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto boolean = [&](const String& id, const String& name, bool value)
    { layout.add(std::make_unique<AudioParameterBool>(ParameterID{id, 1}, name, value)); };
    auto percent = [&](const String& id, const String& name, float value)
    {
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, name,
                                                         NormalisableRange<float>{0, 1, .001f}, value));
    };
    auto scalar = [&](const String& id, const String& name, float min, float max, float step, float value)
    {
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, name,
                                                         NormalisableRange<float>{min, max, step}, value));
    };
    boolean("enabled", "Active", true);
    percent("masterMix", "Master Mix", 1);
    for (int m = 0; m < sidechain::moduleCount; ++m)
    {
        const String name = names[m];
        boolean(moduleId(m, "on"), name + " On", m == sidechain::Volume);
        boolean(moduleId(m, "multi"), name + " Multiband", false);
        layout.add(std::make_unique<AudioParameterChoice>(ParameterID{moduleId(m, "source"), 1}, name + " Trigger",
                                                          StringArray{"Sidechain", "Sync", "MIDI"}, 0));
        layout.add(std::make_unique<AudioParameterChoice>(
            ParameterID{moduleId(m, "length"), 1}, name + " Length",
            StringArray{"1/4 note", "1/8 note", "1/16 note", "1/2 note", "1 bar", "2 bars", "4 bars"}, 0));
        layout.add(std::make_unique<AudioParameterInt>(ParameterID{moduleId(m, "order"), 1}, name + " Order", 0,
                                                       sidechain::moduleCount - 1, m));
        boolean(moduleId(m, "oneshot"), name + " One Shot", true);
        scalar(moduleId(m, "threshold"), name + " Trigger Threshold", -60, 0, .1f, -24);
        scalar(moduleId(m, "attack"), name + " Trigger Attack", 0, 500, .1f, 2);
        scalar(moduleId(m, "release"), name + " Trigger Release", 0, 2000, .1f, 140);
        percent(moduleId(m, "smooth"), name + " Smoothing", .3f);
        for (int b = 0; b < sidechain::bandCount; ++b)
        {
            const String lane = name + " " + StringArray{"Full", "Low", "Mid", "High"}[b];
            percent(bandId(m, b, "amount"), lane + " Amount", m == sidechain::Volume ? .72f : .5f);
            percent(bandId(m, b, "detail"), lane + " Detail", .5f);
            percent(bandId(m, b, "mix"), lane + " Mix", m == sidechain::Volume ? 1.0f : .5f);
            boolean(bandId(m, b, "stepped"), lane + " Held Steps", false);
            for (int i = 0; i < sidechain::stepCount; ++i)
                percent(pointId(m, b, i), lane + " Point " + String(i + 1), sidechain::defaultCurveValue(m, i));
        }
    }
    return layout;
}
void SidechainProcessor::prepareToPlay(double rate, int)
{
    engine.prepare(rate);
}
void SidechainProcessor::releaseResources()
{
    engine.reset();
}
bool SidechainProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    if (l.inputBuses.size() < 1 || l.outputBuses.size() != 1)
        return false;
    const auto main = l.getMainInputChannelSet(), output = l.getMainOutputChannelSet();
    if (main != output || (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo()))
        return false;
    if (l.inputBuses.size() > 1)
    {
        const auto key = l.inputBuses[1];
        if (!key.isDisabled() && key != juce::AudioChannelSet::mono() && key != juce::AudioChannelSet::stereo())
            return false;
    }
    return l.inputBuses.size() <= 2;
}
sidechain::Settings SidechainProcessor::currentSettings() const noexcept
{
    sidechain::Settings s;
    s.enabled = read(masterEnabled, 1) >= .5f;
    s.masterMix = read(masterMix, 1);
    for (int m = 0; m < sidechain::moduleCount; ++m)
    {
        const auto& r = refs[m];
        auto& v = s.module[m];
        v.enabled = read(r.enabled, m == 0) >= .5f;
        v.multiband = read(r.multi, 0) >= .5f;
        v.oneShot = read(r.oneShot, 1) >= .5f;
        v.source = static_cast<int>(std::round(std::clamp(read(r.source, 0), 0.0f, 2.0f)));
        v.length = static_cast<int>(std::round(std::clamp(read(r.length, 0), 0.0f, 6.0f)));
        v.order = static_cast<int>(std::round(std::clamp(read(r.order, static_cast<float>(m)), 0.0f, 11.0f)));
        v.thresholdDb = read(r.threshold, -24);
        v.attackMs = read(r.attack, 2);
        v.releaseMs = read(r.release, 140);
        v.smooth = read(r.smooth, .3f);
        for (int b = 0; b < sidechain::bandCount; ++b)
        {
            const auto& br = r.bands[b];
            auto& dst = v.band[b];
            dst.amount = read(br.amount, .5f);
            dst.detail = read(br.detail, .5f);
            dst.mix = read(br.mix, 1);
            dst.stepped = read(br.stepped, 0) >= .5f;
            for (int i = 0; i < sidechain::stepCount; ++i)
                dst.curve[i] = read(br.curve[i], sidechain::defaultCurveValue(m, i));
        }
    }
    return s;
}
void SidechainProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& m)
{
    process(b, m, false);
}
void SidechainProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer& m)
{
    process(b, m, true);
}
void SidechainProcessor::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool hostBypass)
{
    juce::ScopedNoDenormals guard;
    if (buffer.getNumChannels() < getMainBusNumOutputChannels() || getMainBusNumOutputChannels() < 1)
        return;
    auto output = getBusBuffer(buffer, false, 0);
    const float *keyLeft = nullptr, *keyRight = nullptr;
    if (auto* bus = getBus(true, 1); bus != nullptr && bus->isEnabled())
    {
        const int offset = getChannelIndexInProcessBlockBuffer(true, 1, 0), channels = getChannelCountOfBus(true, 1);
        if (channels >= 1 && offset >= 0 && offset + channels <= buffer.getNumChannels())
        {
            keyLeft = buffer.getReadPointer(offset);
            if (channels >= 2)
                keyRight = buffer.getReadPointer(offset + 1);
        }
    }
    sidechain::Transport transport;
    if (auto* head = getPlayHead())
        if (const auto position = head->getPosition())
        {
            if (const auto bpm = position->getBpm())
                transport.bpm = *bpm;
            if (position->getIsPlaying())
                if (const auto ppq = position->getPpqPosition())
                {
                    transport.playingPpq = true;
                    transport.ppq = *ppq;
                }
        }
    std::array<int, 2048> notes{};
    int count = 0;
    // Inspect the raw short MIDI message: constructing a MidiMessage from a
    // SysEx packet here could allocate on the audio thread.
    for (const auto metadata : midi)
        if (metadata.numBytes >= 3 && (metadata.data[0] & 0xf0) == 0x90 && metadata.data[2] != 0 &&
            count < static_cast<int>(notes.size()))
            notes[count++] = metadata.samplePosition;
    auto settings = currentSettings();
    engine.process(output.getWritePointer(0), output.getNumChannels() > 1 ? output.getWritePointer(1) : nullptr,
                   keyLeft, keyRight, buffer.getNumSamples(), settings, transport, notes.data(), count, hostBypass);
}
juce::RangedAudioParameter* SidechainProcessor::pointParameter(int m, int b, int p) const noexcept
{
    if (m < 0 || m >= sidechain::moduleCount || b < 0 || b >= sidechain::bandCount || p < 0 ||
        p >= sidechain::stepCount)
        return nullptr;
    return refs[m].bands[b].point[p];
}
void SidechainProcessor::setParameterValue(const juce::String& id, float value)
{
    if (auto* p = state.getParameter(id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(value));
        p->endChangeGesture();
    }
}
void SidechainProcessor::applyCurve(int m, int b, const std::array<float, sidechain::stepCount>& values, bool stepped)
{
    if (m < 0 || m >= sidechain::moduleCount || b < 0 || b >= sidechain::bandCount)
        return;
    for (int i = 0; i < sidechain::stepCount; ++i)
        if (auto* p = pointParameter(m, b, i))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(std::clamp(values[i], 0.0f, 1.0f));
            p->endChangeGesture();
        }
    setParameterValue(bandId(m, b, "stepped"), stepped ? 1.0f : 0.0f);
}
void SidechainProcessor::getStateInformation(juce::MemoryBlock& data)
{
    try
    {
        auto tree = state.copyState();
        tree.setProperty("scale", editorScalePercent(), nullptr);
        tree.setProperty("module", selectedModule(), nullptr);
        tree.setProperty("band", selectedBand(), nullptr);
        tree.setProperty("stepDraw", stepDraw() ? 1 : 0, nullptr);
        if (auto xml = tree.createXml())
            copyXmlToBinary(*xml, data);
    }
    catch (...)
    {
    }
}
void SidechainProcessor::setStateInformation(const void* data, int size)
{
    try
    {
        if (data == nullptr || size <= 8 || size > 524288)
            return;
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        const auto len = juce::ByteOrder::littleEndianInt(bytes + 4);
        if (len == 0 || len > static_cast<std::uint32_t>(size - 8) || !safeXml(bytes + 8, len))
            return;
        auto xml = getXmlFromBinary(data, size);
        if (xml == nullptr || !xml->hasTagName("VANTA_SIDECHAIN"))
            return;
        auto saved = juce::ValueTree::fromXml(*xml);
        if (!saved.isValid() || saved.getNumChildren() > 2500)
            return;
        std::map<juce::String, float> values;
        for (auto child : saved)
        {
            if (!child.hasType("PARAM") || child.getNumChildren() != 0)
                return;
            const auto id = child.getProperty("id").toString();
            if (id.isEmpty() || id.length() > 48 || values.count(id))
                return;
            auto* p = state.getParameter(id);
            if (p == nullptr)
                continue;
            float value = 0;
            if (!number(child.getProperty("value"), value))
                return;
            const auto range = p->getNormalisableRange();
            values.emplace(id, juce::jlimit(range.start, range.end, value));
        }
        auto clean = state.copyState();
        for (auto child : clean)
            if (child.hasType("PARAM"))
                if (auto* p = state.getParameter(child.getProperty("id").toString()))
                {
                    const auto found = values.find(child.getProperty("id").toString());
                    child.setProperty("value",
                                      found == values.end() ? p->convertFrom0to1(p->getDefaultValue()) : found->second,
                                      nullptr);
                }
        const int scale = boundedProperty(saved, "scale", 100, 60, 150),
                  module = boundedProperty(saved, "module", 0, 0, sidechain::moduleCount - 1),
                  band = boundedProperty(saved, "band", 0, 0, sidechain::bandCount - 1),
                  step = boundedProperty(saved, "stepDraw", 0, 0, 1);
        state.replaceState(clean);
        setEditorScalePercent(scale);
        setSelectedModule(module);
        setSelectedBand(band);
        setStepDraw(step != 0);
    }
    catch (...)
    {
    }
}
juce::AudioProcessorEditor* SidechainProcessor::createEditor()
{
    return new SidechainEditor(*this);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SidechainProcessor();
}
