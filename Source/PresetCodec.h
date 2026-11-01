#pragma once

#include "Parameters.h"
#include <juce_core/juce_core.h>

namespace vanta
{
struct PresetResult
{
    bool ok = false;
    ParameterValues values = defaultValues();
    juce::String name;
    juce::String message;
};

// Parsing is deliberately separate from applying a preset: invalid or unsupported
// input cannot modify the currently playing patch.
class PresetCodec
{
public:
    static constexpr juce::int64 maxFileBytes = 32 * 1024 * 1024;
    static juce::String encode(const juce::String& name, const ParameterValues&);
    static PresetResult decode(const juce::String& text, const juce::String& extension);
    static PresetResult loadFile(const juce::File&);
    static juce::Result saveFile(const juce::File&, const juce::String& name, const ParameterValues&);
};
}
