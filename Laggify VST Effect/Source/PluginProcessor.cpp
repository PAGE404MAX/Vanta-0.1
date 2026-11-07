#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>

namespace
{
float read(std::atomic<float>* p, float fallback) noexcept
{
    const auto value = p != nullptr ? p->load(std::memory_order_relaxed) : fallback;
    return std::isfinite(value) ? value : fallback;
}
// Reject DTDs, excessive nesting/size and malformed tags before JUCE's XML parser.
bool safeXml(const std::uint8_t* data, std::size_t size) noexcept
{
    int depth = 0, elements = 0, attributes = 0;
    for (std::size_t i = 0; i < size;)
    {
        if (data[i] >= 128 || data[i] == 0)
            return false; // Our state is numeric/ASCII only.
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
        else if (++depth > 8 || ++elements > 128)
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
            if (c == '=' && ++attributes > 1024)
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
    const auto utf8 = text.toRawUTF8();
    char* end = nullptr;
    const float n = std::strtof(utf8, &end);
    if (end == utf8 || *end != '\0' || !std::isfinite(n))
        return false;
    out = n;
    return true;
}
int boundedProperty(const juce::ValueTree& tree, const char* name, int fallback, int min, int max)
{
    if (!tree.hasProperty(name))
        return fallback;
    float value = 0;
    return number(tree.getProperty(name), value) && value >= min && value <= max && std::floor(value) == value
               ? static_cast<int>(value)
               : fallback;
}
} // namespace

LaggifyProcessor::LaggifyProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "LAGGIFY", makeLayout())
{
    enabled = state.getRawParameterValue("enabled");
    mix = state.getRawParameterValue("mix");
    bpm = state.getRawParameterValue("bpm");
    cycle = state.getRawParameterValue("cycle");
    timeSmooth = state.getRawParameterValue("timeSmooth");
    volumeSmooth = state.getRawParameterValue("volumeSmooth");
    timeMix = state.getRawParameterValue("timeMix");
    volumeMix = state.getRawParameterValue("volumeMix");
    attack = state.getRawParameterValue("attack");
    release = state.getRawParameterValue("release");
    tension = state.getRawParameterValue("tension");
    hold = state.getRawParameterValue("hold");
    scratch = state.getRawParameterValue("scratch");
    for (int i = 0; i < laggify::steps; ++i)
    {
        const auto t = pointId(true, i), v = pointId(false, i);
        times[i] = state.getRawParameterValue(t);
        volumes[i] = state.getRawParameterValue(v);
        timeParams[i] = state.getParameter(t);
        volumeParams[i] = state.getParameter(v);
    }
}
juce::String LaggifyProcessor::pointId(bool time, int step)
{
    return juce::String(time ? "time" : "volume") + juce::String(step + 1).paddedLeft('0', 2);
}
juce::AudioProcessorValueTreeState::ParameterLayout LaggifyProcessor::makeLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{"enabled", 1}, "Enabled", false));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"mix", 1}, "Mix",
                                                     NormalisableRange<float>{0.0f, 1.0f, .001f}, 1.0f));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"bpm", 1}, "Fallback BPM",
                                                     NormalisableRange<float>{30.0f, 400.0f, 1.0f}, 120.0f));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{"cycle", 1}, "Cycle",
                                                      StringArray{"Half bar", "One bar", "Two bars"}, 1));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{"timeSmooth", 1}, "Time interpolation", false));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{"volumeSmooth", 1}, "Volume interpolation", false));
    const auto initial = laggify::makePattern(true, 15);
    for (bool time : {true, false})
        for (int i = 0; i < laggify::steps; ++i)
            layout.add(std::make_unique<AudioParameterFloat>(
                ParameterID{pointId(time, i), 1}, String(time ? "Time " : "Volume ") + String(i + 1),
                NormalisableRange<float>{0.0f, 1.0f, .001f},
                time ? initial.values[static_cast<std::size_t>(i)] : 1.0f));
    // Append new IDs after the original 38 parameters, keeping the existing
    // IDs and order intact for sessions and automation created in 0.1.
    const auto percent = AudioParameterFloatAttributes()
                             .withLabel("%")
                             .withStringFromValueFunction([](float value, int) { return String(value * 100.0f, 1); })
                             .withValueFromStringFunction([](const String& text) { return text.getFloatValue() / 100.0f; });
    const auto milliseconds = AudioParameterFloatAttributes().withLabel("ms");
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"timeMix", 1}, "Time Mix",
                                                     NormalisableRange<float>{0.0f, 1.0f, .001f}, 1.0f, percent));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"volumeMix", 1}, "Volume Mix",
                                                     NormalisableRange<float>{0.0f, 1.0f, .001f}, 1.0f, percent));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"attack", 1}, "Attack",
                                                     NormalisableRange<float>{0.0f, 500.0f, .1f}, 4.0f, milliseconds));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"release", 1}, "Release",
                                                     NormalisableRange<float>{0.0f, 1000.0f, .1f}, 4.0f, milliseconds));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"tension", 1}, "Tension",
                                                     NormalisableRange<float>{-1.0f, 1.0f, .001f}, 0.0f, percent));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{"hold", 1}, "Hold", false));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"scratch", 1}, "Scratch",
                                                     NormalisableRange<float>{0.0f, 1.0f, .001f}, 0.0f, percent));
    return layout;
}
void LaggifyProcessor::prepareToPlay(double rate, int)
{
    engine.prepare(rate);
}
void LaggifyProcessor::releaseResources()
{
    engine.reset();
}
bool LaggifyProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto input = layout.getMainInputChannelSet(), output = layout.getMainOutputChannelSet();
    return input == output && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}
laggify::Settings LaggifyProcessor::currentSettings() const noexcept
{
    laggify::Settings s;
    s.enabled = read(enabled, 0) >= .5f;
    s.mix = read(mix, 1);
    s.fallbackBpm = read(bpm, 120);
    s.cycle = static_cast<int>(std::round(std::clamp(read(cycle, 1), 0.0f, 2.0f)));
    s.timeSmooth = read(timeSmooth, 0) >= .5f;
    s.volumeSmooth = read(volumeSmooth, 0) >= .5f;
    s.timeMix = read(timeMix, 1);
    s.volumeMix = read(volumeMix, 1);
    s.attackMs = read(attack, 4);
    s.releaseMs = read(release, 4);
    s.tension = read(tension, 0);
    s.hold = read(hold, 0) >= .5f;
    s.scratch = read(scratch, 0);
    for (int i = 0; i < laggify::steps; ++i)
    {
        s.time[i] = read(times[i], 0);
        s.volume[i] = read(volumes[i], 1);
    }
    return s;
}
void LaggifyProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& m)
{
    process(b, m, false);
}
void LaggifyProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer& m)
{
    process(b, m, true);
}
void LaggifyProcessor::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool hardBypass)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    laggify::Transport t;
    if (auto* head = getPlayHead())
        if (const auto p = head->getPosition())
        {
            if (const auto tempo = p->getBpm())
                t.bpm = *tempo;
            if (p->getIsPlaying())
                if (const auto ppq = p->getPpqPosition())
                {
                    t.playingPpq = true;
                    t.ppq = *ppq;
                }
        }
    const auto channels = juce::jmin(2, juce::jmin(getTotalNumInputChannels(), buffer.getNumChannels()));
    for (int c = channels; c < buffer.getNumChannels(); ++c)
        buffer.clear(c, 0, buffer.getNumSamples());
    engine.process(buffer.getArrayOfWritePointers(), channels, buffer.getNumSamples(), currentSettings(), t,
                   hardBypass);
}
juce::RangedAudioParameter* LaggifyProcessor::pointParameter(bool time, int step) const noexcept
{
    if (step < 0 || step >= laggify::steps)
        return nullptr;
    return (time ? timeParams : volumeParams)[static_cast<std::size_t>(step)];
}
void LaggifyProcessor::setParameterValue(const juce::String& id, float value)
{
    if (auto* p = state.getParameter(id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(value));
        p->endChangeGesture();
    }
}
void LaggifyProcessor::applyPattern(bool time, int index)
{
    if (index < 0 || index >= laggify::bankSize)
        return;
    const auto pattern = laggify::makePattern(time, index);
    for (int i = 0; i < laggify::steps; ++i)
        if (auto* p = pointParameter(time, i))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(pattern.values[i]));
            p->endChangeGesture();
        }
    setParameterValue(time ? "timeSmooth" : "volumeSmooth", pattern.smooth ? 1.0f : 0.0f);
    (time ? timePreset : volumePreset).store(index, std::memory_order_relaxed);
    setEditTime(time);
}
void LaggifyProcessor::markCustom(bool time) noexcept
{
    (time ? timePreset : volumePreset).store(-1, std::memory_order_relaxed);
}
int LaggifyProcessor::currentPattern(bool time) const noexcept
{
    const int i = (time ? timePreset : volumePreset).load(std::memory_order_relaxed);
    if (i < 0 || i >= laggify::bankSize)
        return -1;
    const auto p = laggify::makePattern(time, i);
    const auto current = currentSettings();
    if ((time ? current.timeSmooth : current.volumeSmooth) != p.smooth)
        return -1;
    const auto& values = time ? current.time : current.volume;
    for (int s = 0; s < laggify::steps; ++s)
        if (std::abs(values[s] - p.values[s]) > .0011f)
            return -1;
    return i;
}
void LaggifyProcessor::getStateInformation(juce::MemoryBlock& data)
{
    try
    {
        auto tree = state.copyState();
        tree.setProperty("scale", editorScalePercent(), nullptr);
        tree.setProperty("timePreset", timePreset.load(std::memory_order_relaxed), nullptr);
        tree.setProperty("volumePreset", volumePreset.load(std::memory_order_relaxed), nullptr);
        tree.setProperty("editTime", editTime() ? 1 : 0, nullptr);
        tree.setProperty("banksOnly", showBanksOnly() ? 1 : 0, nullptr);
        if (auto xml = tree.createXml())
            copyXmlToBinary(*xml, data);
    }
    catch (...)
    {
    }
}
void LaggifyProcessor::setStateInformation(const void* data, int size)
{
    try
    {
        if (data == nullptr || size <= 8 || size > 131072)
            return;
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        const auto len = juce::ByteOrder::littleEndianInt(bytes + 4);
        if (len == 0 || len > static_cast<std::uint32_t>(size - 8) || !safeXml(bytes + 8, len))
            return;
        auto xml = getXmlFromBinary(data, size);
        if (xml == nullptr || !xml->hasTagName("LAGGIFY"))
            return;
        auto saved = juce::ValueTree::fromXml(*xml);
        if (!saved.isValid())
            return;
        auto clean = state.copyState();
        // Start from defaults, then copy only known, finite parameter values.
        for (auto child : clean)
            if (child.hasType("PARAM"))
                if (auto* p = state.getParameter(child.getProperty("id").toString()))
                    child.setProperty("value", p->convertFrom0to1(p->getDefaultValue()), nullptr);
        std::set<juce::String> seen;
        for (auto child : saved)
        {
            if (!child.hasType("PARAM") || child.getNumChildren() != 0)
                return;
            const auto id = child.getProperty("id").toString();
            if (id.length() > 32 || !seen.insert(id).second)
                return;
            auto* p = state.getParameter(id);
            if (p == nullptr)
                continue;
            float value = 0;
            if (!number(child.getProperty("value"), value))
                return;
            const auto range = p->getNormalisableRange();
            value = juce::jlimit(range.start, range.end, value);
            for (auto dest : clean)
                if (dest.hasType("PARAM") && dest.getProperty("id").toString() == id)
                {
                    dest.setProperty("value", value, nullptr);
                    break;
                }
        }
        const int scale = boundedProperty(saved, "scale", 100, 60, 150);
        const int tp = boundedProperty(saved, "timePreset", -1, -1, laggify::bankSize - 1);
        const int vp = boundedProperty(saved, "volumePreset", -1, -1, laggify::bankSize - 1);
        const bool edit = boundedProperty(saved, "editTime", 1, 0, 1) != 0;
        const bool banks = boundedProperty(saved, "banksOnly", 0, 0, 1) != 0;
        state.replaceState(clean);
        setEditorScalePercent(scale);
        timePreset.store(tp, std::memory_order_relaxed);
        volumePreset.store(vp, std::memory_order_relaxed);
        setEditTime(edit);
        setShowBanksOnly(banks);
    }
    catch (...)
    {
    }
}
juce::AudioProcessorEditor* LaggifyProcessor::createEditor()
{
    return new LaggifyEditor(*this);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LaggifyProcessor();
}
