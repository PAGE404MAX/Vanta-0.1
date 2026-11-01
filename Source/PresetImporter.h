#pragma once

#include "PresetCodec.h"

namespace vanta
{
// Offline, bounded readers for independently documented portions of these
// unofficial formats. The resulting patch is not applied until parsing has
// succeeded; callers must run these functions away from processBlock.
class PresetImporter
{
public:
    static bool isValidUtf8(const void*, std::size_t) noexcept;
    static bool parseJson(const juce::String&, juce::var&, juce::String& error);
    static PresetResult vital(const juce::var& document, const juce::String& filename = {});
    static PresetResult serum1(const void* data, std::size_t size, const juce::String& filename);
    static PresetResult serum2(const void* data, std::size_t size, const juce::String& filename);

    static juce::var encodeWaveTables(const WaveTableBank&);
    static bool decodeWaveTables(const juce::var&, std::shared_ptr<WaveTableBank>&, juce::String& error);
    static juce::var encodeSample(const PcmSample&);
    static bool decodeSample(const juce::var&, std::shared_ptr<WaveTableBank>&, juce::String& error);
    static juce::var encodeModulation(const ImportedModulation&);
    static bool decodeModulation(const juce::var&, std::shared_ptr<WaveTableBank>&, juce::String& error);
};
}
