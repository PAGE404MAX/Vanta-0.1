#pragma once

#include "Parameters.h"
#include "WaveTable.h"
#include <memory>
#include <juce_core/juce_core.h>

namespace vanta
{
struct PresetResult
{
    bool ok = false;
    ParameterValues values = defaultValues();
    juce::String name;
    juce::String message;
    std::shared_ptr<WaveTableBank> waveTables;
};

// Parsing is deliberately separate from applying a preset: invalid or unsupported
// input cannot modify the currently playing patch.
class PresetCodec
{
public:
    static constexpr juce::int64 maxFileBytes = 32 * 1024 * 1024;
    static juce::String encode(const juce::String& name, const ParameterValues&, const WaveTableBank* = nullptr);
    static PresetResult decode(const juce::String& text, const juce::String& extension);
    static PresetResult decodeBytes(const void*, std::size_t, const juce::String& extension, const juce::String& filename = {});
    static PresetResult loadFile(const juce::File&);
    static juce::Result saveFile(const juce::File&, const juce::String& name, const ParameterValues&, const WaveTableBank* = nullptr);
};
}
