#include "PresetCodec.h"
#include "PresetImporter.h"
#include <cmath>
#include <cstring>

namespace vanta
{
namespace
{
bool isNumber(const juce::var& value) { return value.isInt() || value.isInt64() || value.isDouble(); }
float bounded(std::size_t i, double value)
{
    const auto& spec = parameterSpecs[i];
    return static_cast<float>(juce::jlimit(static_cast<double>(spec.minValue), static_cast<double>(spec.maxValue), value));
}
PresetResult fail(const juce::String& message)
{
    PresetResult result; result.message = message + " Your current sound was not changed."; return result;
}
PresetResult decodeNative(const juce::var& root)
{
    PresetResult result;
    auto* object = root.getDynamicObject();
    if (object == nullptr || object->getProperty("format").toString() != "vanta-preset"
        || !isNumber(object->getProperty("version")) || static_cast<double>(object->getProperty("version")) != 1.0)
        return fail("Not a supported VANTA preset (expected format version 1).");
    auto* parameters = object->getProperty("parameters").getDynamicObject();
    if (parameters == nullptr) return fail("The preset has no parameter object.");
    int mapped = 0;
    for (std::size_t i = 0; i < parameterSpecs.size(); ++i)
    {
        const auto& spec = parameterSpecs[i];
        if (!parameters->hasProperty(spec.id)) continue;
        const auto& value = parameters->getProperty(spec.id);
        if (!isNumber(value) || !std::isfinite(static_cast<double>(value)))
            return fail("Invalid numeric value for " + juce::String(spec.name) + ".");
        result.values[i] = bounded(i, static_cast<double>(value)); ++mapped;
    }
    if (mapped == 0) return fail("The VANTA preset contains no recognised parameters.");
    if (object->hasProperty("wavetables"))
    {
        juce::String error;
        if (!PresetImporter::decodeWaveTables(object->getProperty("wavetables"), result.waveTables, error)) return fail(error);
    }
    if (object->hasProperty("sample"))
    {
        juce::String error;
        if (!PresetImporter::decodeSample(object->getProperty("sample"), result.waveTables, error)) return fail(error);
    }
    if (object->hasProperty("modulation"))
    {
        juce::String error;
        if (!PresetImporter::decodeModulation(object->getProperty("modulation"), result.waveTables, error)) return fail(error);
    }
    auto name = object->getProperty("name").toString().trim().substring(0, 80);
    result.name = name.isNotEmpty() ? name : "Imported VANTA";
    result.ok = true; result.message = "VANTA preset loaded.";
    return result;
}
}

juce::String PresetCodec::encode(const juce::String& name, const ParameterValues& values, const WaveTableBank* tables)
{
    auto root = std::make_unique<juce::DynamicObject>();
    auto parameters = std::make_unique<juce::DynamicObject>();
    for (std::size_t i = 0; i < parameterSpecs.size(); ++i)
        parameters->setProperty(parameterSpecs[i].id, bounded(i, std::isfinite(values[i]) ? values[i] : parameterSpecs[i].defaultValue));
    root->setProperty("format", "vanta-preset"); root->setProperty("version", 1);
    root->setProperty("name", name.trim().substring(0, 80)); root->setProperty("parameters", juce::var(parameters.release()));
    if (tables != nullptr)
    {
        bool present = false;
        for (const auto& table : tables->tables) present |= table.frameCount > 0;
        if (present) root->setProperty("wavetables", PresetImporter::encodeWaveTables(*tables));
        if (tables->sample.frames > 0) root->setProperty("sample", PresetImporter::encodeSample(tables->sample));
        if (tables->modulation.routeCount > 0) root->setProperty("modulation", PresetImporter::encodeModulation(tables->modulation));
    }
    return juce::JSON::toString(juce::var(root.release()), false);
}

PresetResult PresetCodec::decode(const juce::String& text, const juce::String& extension)
{
    const auto utf8 = text.toUTF8();
    return decodeBytes(utf8.getAddress(), std::strlen(utf8.getAddress()), extension);
}

PresetResult PresetCodec::decodeBytes(const void* data, std::size_t size, const juce::String& extension, const juce::String& filename)
{
    try
    {
        const auto ext = extension.toLowerCase();
        if (ext != ".vanta" && ext != ".vital" && ext != ".fxp" && ext != ".serumpreset")
            return fail("This preset format is not supported (ZENOLOGY is not supported).");
        if (data == nullptr || size == 0 || size > static_cast<std::size_t>(maxFileBytes))
            return fail("The preset is empty or exceeds the 32 MB limit.");
        if (ext == ".fxp") return PresetImporter::serum1(data, size, filename);
        if (ext == ".serumpreset") return PresetImporter::serum2(data, size, filename);
        if (!PresetImporter::isValidUtf8(data, size)) return fail("Preset JSON contains invalid UTF-8 or an embedded NUL byte.");
        juce::var document; juce::String error;
        const auto text = juce::String::fromUTF8(static_cast<const char*>(data), static_cast<int>(size));
        if (!PresetImporter::parseJson(text, document, error)) return fail("Could not parse preset JSON: " + error);
        return ext == ".vanta" ? decodeNative(document) : PresetImporter::vital(document, filename);
    }
    catch (const std::exception&)
    {
        return fail("The preset could not be decoded safely.");
    }
    catch (...)
    {
        return fail("The preset could not be decoded safely.");
    }
}

PresetResult PresetCodec::loadFile(const juce::File& file)
{
    try
    {
        if (!file.existsAsFile()) return fail("The preset file was not found.");
        const auto size = file.getSize();
        if (size <= 0 || size > maxFileBytes) return fail("The preset is empty or exceeds the 32 MB limit.");
        auto stream = file.createInputStream();
        if (stream == nullptr || !stream->openedOk()) return fail("Could not open the preset file.");
        juce::MemoryBlock bytes(static_cast<std::size_t>(size), false);
        if (stream->read(bytes.getData(), static_cast<int>(size)) != size || !stream->isExhausted())
            return fail("Could not read a stable, complete preset file.");
        return decodeBytes(bytes.getData(), bytes.getSize(), file.getFileExtension(), file.getFileNameWithoutExtension());
    }
    catch (...) { return fail("The preset file could not be read safely."); }
}

juce::Result PresetCodec::saveFile(const juce::File& file, const juce::String& name, const ParameterValues& values, const WaveTableBank* tables)
{
    try
    {
        if (!file.hasFileExtension(".vanta")) return juce::Result::fail("VANTA presets must use the .vanta extension.");
        juce::TemporaryFile temporary(file);
        if (!temporary.getFile().replaceWithText(encode(name, values, tables)) || !temporary.overwriteTargetFileWithTemporary())
            return juce::Result::fail("Could not write the preset file. Check the destination permissions.");
        return juce::Result::ok();
    }
    catch (...) { return juce::Result::fail("Could not save the VANTA preset safely."); }
}
}
