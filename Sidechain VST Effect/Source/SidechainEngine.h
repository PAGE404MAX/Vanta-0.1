#pragma once
#include <array>
#include <atomic>
#include <vector>

namespace sidechain
{
constexpr int moduleCount = 12, bandCount = 4, stepCount = 16;
enum Module
{
    Volume,
    Filter,
    Drive,
    Crush,
    Time,
    Pan,
    Width,
    Noise,
    Liquid,
    Reverb,
    Pitch,
    Compressor
};
enum Source
{
    External,
    Sync,
    Midi
};

// Lanes: full range, low, mid and high. In multiband mode the three
// complementary bands are processed separately, then recombined.
struct BandSettings
{
    std::array<float, stepCount> curve{};
    float amount = .5f, detail = .5f, mix = 1.0f;
    bool stepped = false;
};
struct ModuleSettings
{
    bool enabled = false, multiband = false, oneShot = true;
    int source = External, length = 0, order = 0;
    float thresholdDb = -24.0f, attackMs = 2.0f, releaseMs = 140.0f, smooth = .30f;
    std::array<BandSettings, bandCount> band{};
};
struct Settings
{
    bool enabled = true;
    float masterMix = 1.0f;
    std::array<ModuleSettings, moduleCount> module{};
    Settings() noexcept;
};
struct Transport
{
    double bpm = 120.0, ppq = 0.0;
    bool playingPpq = false;
};
float defaultCurveValue(int module, int step) noexcept;
float evaluateCurve(const BandSettings&, double phase) noexcept;

// Audio-thread-owned engine. Its bounded delay storage is allocated in prepare,
// never in process. Sidechain input is independent from the main audio bus.
class Engine
{
  public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float* left, float* right, const float* keyLeft, const float* keyRight, int count, const Settings&,
                 const Transport&, const int* midiNoteOffsets = nullptr, int numMidiNotes = 0,
                 bool hostBypass = false) noexcept;
    float phase(int module) const noexcept;
    float keyLevel() const noexcept
    {
        return visibleKey.load(std::memory_order_relaxed);
    }

  private:
    struct Pair
    {
        float l = 0, r = 0;
    };
    struct Delay
    {
        std::array<std::vector<float>, 2> data;
        int write = 0;
        void prepare(int samples);
        void clear() noexcept;
        float tap(int channel, float samplesBack) const noexcept;
        void push(Pair value) noexcept;
    };
    struct LaneState
    {
        std::array<float, 2> filter1{}, filter2{}, tone{}, liquid{}, crushHeld{};
        float crushPhase = 0, detector = 0, smoothValue = 0;
        bool hasSmooth = false;
        double pitchPhase = .25;
    };
    struct ModuleState
    {
        std::array<LaneState, bandCount> lane{};
        std::array<float, 2> low{}, upper{};
        float keyEnvelope = 0;
        bool keyHigh = false, running = false;
        double oneShotPhase = 1.0, freePhase = 0.0;
    };
    Pair applyEffect(int module, int band, Pair input, const BandSettings&, float curve) noexcept;
    std::array<ModuleState, moduleCount> moduleState{};
    std::array<Delay, bandCount> timeDelay{}, pitchDelay{}, reverbDelay{};
    std::array<std::atomic<float>, moduleCount> visiblePhase{};
    std::atomic<float> visibleKey{0.0f};
    double rate = 44100.0;
    float crossoverLow = 0, crossoverHigh = 0, keyMeterEnvelope = 0;
    unsigned int randomState = 0x34ba621du;
};
} // namespace sidechain
