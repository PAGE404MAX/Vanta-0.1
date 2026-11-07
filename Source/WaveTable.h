#pragma once

#include "Parameters.h"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>

namespace vanta
{
// Imported cycles are normalized and resampled off the audio thread. Storage is
// deliberately fixed and bounded: no vector growth or reference-count release
// can occur while rendering. Positions are ordered in the inclusive range 0..1.
inline constexpr int wavetableFrameSize = 2048;
inline constexpr int maxWavetableFrames = 16;
struct WaveTable
{
    int frameCount = 0;
    std::array<float, maxWavetableFrames> positions {};
    std::array<float, wavetableFrameSize * maxWavetableFrames> samples {};
};
// One imported stereo sample per preset, capped to about 5.9 seconds at 44.1 kHz.
// A single shared buffer keeps memory bounded even with eight oscillators/voices.
inline constexpr int maxSampleFrames = 262144;
struct PcmSample
{
    int slot = -1, frames = 0, channels = 0;
    float sampleRate = 44100.0f;
    bool loop = false, keytrack = false;
    std::array<float, maxSampleFrames> left {}, right {};
};
inline constexpr int importedLfoCount = 4, importedLfoPoints = 512, maxImportedRoutes = 32;
enum class ModTarget : int { level, pan, pitch, tablePosition };
struct ImportedLfo
{
    float rateHz = 1.0f;
    std::array<float, importedLfoPoints> shape {}; // normalized, already reconstructed off-thread
};
struct ImportedRoute
{
    int source = 0, oscillator = 0;
    ModTarget target = ModTarget::level;
    float amount = 0.0f; // destination units
    bool bipolar = false;
};
struct ImportedModulation
{
    std::array<ImportedLfo, importedLfoCount> lfos {};
    std::array<ImportedRoute, maxImportedRoutes> routes {};
    int routeCount = 0;
};
struct WaveTableBank
{
    std::array<WaveTable, oscillatorCount> tables {};
    PcmSample sample;
    ImportedModulation modulation;
};

// One audio reader, any number of non-audio writers (serialized by the mutex).
// The reader announces a hazard and verifies publication before dereferencing.
// A writer never touches either the published bank or the announced reader's
// bank, so a complete replacement needs neither locks nor allocations in DSP.
class WaveTableExchange
{
public:
    WaveTableExchange()
    {
        for (auto& bank : banks) bank = std::make_unique<WaveTableBank>();
    }
    void publish(const WaveTableBank& value)
    {
        std::lock_guard<std::mutex> lock(writerMutex);
        const auto current = published.load(std::memory_order_seq_cst);
        const auto reading = reader.load(std::memory_order_seq_cst);
        int destination = 0;
        while (destination == current || destination == reading) ++destination;
        *banks[static_cast<std::size_t>(destination)] = value;
        published.store(destination, std::memory_order_seq_cst);
    }
    std::unique_ptr<WaveTableBank> copyPublished() const
    {
        std::lock_guard<std::mutex> lock(writerMutex);
        return std::make_unique<WaveTableBank>(*banks[static_cast<std::size_t>(published.load(std::memory_order_seq_cst))]);
    }
    const WaveTableBank& beginRead() noexcept
    {
        int candidate;
        do
        {
            candidate = published.load(std::memory_order_seq_cst);
            reader.store(candidate, std::memory_order_seq_cst);
        }
        while (candidate != published.load(std::memory_order_seq_cst));
        return *banks[static_cast<std::size_t>(candidate)];
    }
    void endRead() noexcept { reader.store(-1, std::memory_order_seq_cst); }
private:
    std::array<std::unique_ptr<WaveTableBank>, 3> banks;
    mutable std::mutex writerMutex;
    std::atomic<int> published { 0 }, reader { -1 };
};
}
