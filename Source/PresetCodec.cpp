#include "PresetCodec.h"
#include <algorithm>
#include <cmath>

namespace vanta
{
namespace
{
bool isNumber(const juce::var& value)
{
    return value.isInt() || value.isInt64() || value.isDouble();
}

bool readNumber(const juce::DynamicObject& object, const char* key, double& out)
{
    auto value = object.getProperty(key);
    if (!isNumber(value)) return false;
    out = static_cast<double>(value);
    return std::isfinite(out);
}

float bounded(std::size_t i, double value)
{
    const auto& spec = parameterSpecs[i];
    return static_cast<float>(juce::jlimit(static_cast<double>(spec.minValue), static_cast<double>(spec.maxValue), value));
}

juce::String safeName(const juce::var& value, const juce::String& fallback)
{
    if (!value.isString()) return fallback;
    auto name = value.toString().trim().substring(0, 80);
    return name.isEmpty() ? fallback : name;
}

PresetResult decodeNative(const juce::var& root)
{
    PresetResult result;
    auto* object = root.getDynamicObject();
    if (object == nullptr || object->getProperty("format").toString() != "vanta-preset"
        || !isNumber(object->getProperty("version")) || static_cast<double>(object->getProperty("version")) != 1.0)
    {
        result.message = "Not a supported VANTA preset (expected format version 1).";
        return result;
    }
    auto* parameters = object->getProperty("parameters").getDynamicObject();
    if (parameters == nullptr) { result.message = "The preset has no parameter object."; return result; }
    int mapped = 0;
    for (std::size_t i = 0; i < parameterSpecs.size(); ++i)
    {
        const auto& spec = parameterSpecs[i];
        if (!parameters->hasProperty(spec.id)) continue;
        double value = 0.0;
        if (!readNumber(*parameters, spec.id, value))
        {
            result.message = "Invalid numeric value for " + juce::String(spec.name) + ".";
            return result;
        }
        result.values[i] = bounded(i, value);
        ++mapped;
    }
    if (mapped == 0) { result.message = "The VANTA preset contains no recognised parameters."; return result; }
    result.name = safeName(object->getProperty("name"), "Imported VANTA");
    result.ok = true;
    result.message = "VANTA preset loaded.";
    return result;
}

PresetResult decodeVital(const juce::var& root)
{
    PresetResult result;
    auto* object = root.getDynamicObject();
    if (object == nullptr || !object->getProperty("synth_version").isString())
    {
        result.message = "This is not a recognised Vital preset.";
        return result;
    }
    auto* settings = object->getProperty("settings").getDynamicObject();
    if (settings == nullptr) { result.message = "This Vital preset has no settings object."; return result; }

    // Vital stores envelope times in fourth-root seconds. Its filters,
    // oscillators and modulation differ from VANTA: this is deliberately only
    // a small, clearly labelled approximation rather than full compatibility.
    result.values[wave] = 1.0f; // VANTA saw; Vital wavetable is not decoded.
    result.values[cutoff] = parameterSpecs[cutoff].maxValue;
    result.values[resonance] = 0.0f;
    int mapped = 0;
    auto mapTime = [&](const char* key, Parameter parameter)
    {
        double value;
        if (readNumber(*settings, key, value) && value >= 0.0 && value <= 10.0)
        { result.values[parameter] = bounded(parameter, std::pow(value, 4.0)); ++mapped; }
    };
    mapTime("env_1_attack", attack);
    mapTime("env_1_decay", decay);
    mapTime("env_1_release", release);
    double value;
    if (readNumber(*settings, "env_1_sustain", value))
    { result.values[sustain] = bounded(sustain, value); ++mapped; }
    double filterEnabled = 0.0;
    if (readNumber(*settings, "filter_1_on", filterEnabled) && filterEnabled >= 0.5
        && readNumber(*settings, "filter_1_cutoff", value) && value >= -128.0 && value <= 256.0)
    {
        result.values[cutoff] = bounded(cutoff, 8.1757989156 * std::pow(2.0, value / 12.0));
        ++mapped;
    }
    if (mapped == 0)
    {
        result.message = "No supported envelope or filter values were found in this Vital preset.";
        return result;
    }
    result.name = safeName(object->getProperty("preset_name"), "Imported Vital");
    result.ok = true;
    result.message = "Approximate Vital import: mapped available envelope values and filter cutoff. "
                     "VANTA uses a saw oscillator; Vital wavetables, samples, modulation, filter models, "
                     "envelope curves and effects are not imported. The sound will differ.";
    return result;
}
}

juce::String PresetCodec::encode(const juce::String& name, const ParameterValues& values)
{
    auto root = std::make_unique<juce::DynamicObject>();
    auto parameters = std::make_unique<juce::DynamicObject>();
    for (std::size_t i = 0; i < parameterSpecs.size(); ++i)
        parameters->setProperty(parameterSpecs[i].id, bounded(i, std::isfinite(values[i]) ? values[i] : parameterSpecs[i].defaultValue));
    root->setProperty("format", "vanta-preset");
    root->setProperty("version", 1);
    root->setProperty("name", name.trim().substring(0, 80));
    root->setProperty("parameters", juce::var(parameters.release()));
    return juce::JSON::toString(juce::var(root.release()), false);
}

PresetResult PresetCodec::decode(const juce::String& text, const juce::String& extension)
{
    const auto ext = extension.toLowerCase();
    if (ext != ".vanta" && ext != ".vital")
        return { false, defaultValues(), {}, "Serum, Serum 2 and ZENOLOGY presets are not supported in Beta 0.4. Your current sound was not changed." };
    juce::var document;
    auto parsed = juce::JSON::parse(text, document);
    if (parsed.failed()) return { false, defaultValues(), {}, "Could not parse preset JSON: " + parsed.getErrorMessage() };
    return ext == ".vanta" ? decodeNative(document) : decodeVital(document);
}

PresetResult PresetCodec::loadFile(const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    if (ext != ".vanta" && ext != ".vital") return decode({}, ext);
    if (!file.existsAsFile()) return { false, defaultValues(), {}, "The preset file was not found." };
    if (file.getSize() <= 0 || file.getSize() > maxFileBytes)
        return { false, defaultValues(), {}, "The preset is empty or exceeds the 32 MB limit." };
    return decode(file.loadFileAsString(), ext);
}

juce::Result PresetCodec::saveFile(const juce::File& file, const juce::String& name, const ParameterValues& values)
{
    if (!file.hasFileExtension(".vanta")) return juce::Result::fail("VANTA presets must use the .vanta extension.");
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(encode(name, values)) || !temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not write the preset file. Check the destination permissions.");
    return juce::Result::ok();
}
}
