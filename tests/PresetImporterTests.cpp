#include "PresetCodec.h"
#include "PresetImporter.h"
#include "SynthEngine.h"
#include <zlib.h>
#include <zstd.h>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

namespace
{
void check(bool ok, const char* label)
{
    if (!ok) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
    std::cout << "PASS: " << label << '\n';
}
void put32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value, bool big = false)
{
    for (int i = 0; i < 4; ++i) data[offset + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value >> (big ? (3 - i) * 8 : i * 8));
}
void putFloat(std::vector<std::uint8_t>& data, std::size_t offset, float value)
{
    std::uint32_t bits; std::memcpy(&bits, &value, 4); put32(data, offset, bits);
}
void putDouble(std::vector<std::uint8_t>& data, std::size_t offset, double value)
{
    std::uint64_t bits; std::memcpy(&bits, &value, 8);
    for (int i = 0; i < 8; ++i) data[offset + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(bits >> (8 * i));
}
void addLe32(std::vector<std::uint8_t>& data, std::uint32_t value)
{
    const auto old = data.size(); data.resize(old + 4); put32(data, old, value);
}
void addLe64(std::vector<std::uint8_t>& data, std::uint64_t value)
{
    const auto old = data.size(); data.resize(old + 8);
    for (int i = 0; i < 8; ++i) data[old + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value >> (8 * i));
}
std::vector<std::uint8_t> zlibBytes(const std::vector<std::uint8_t>& data)
{
    uLongf size = compressBound(static_cast<uLong>(data.size())); std::vector<std::uint8_t> output(size);
    check(compress2(output.data(), &size, data.data(), static_cast<uLong>(data.size()), 6) == Z_OK, "Synthetic zlib stream built");
    output.resize(size); return output;
}
std::vector<std::uint8_t> serum1Container(const std::vector<std::uint8_t>& state, const std::vector<std::uint8_t>& extra)
{
    const auto one = zlibBytes(state), two = zlibBytes(extra);
    std::vector<std::uint8_t> result(60, 0);
    std::memcpy(result.data(), "CcnK", 4); std::memcpy(result.data() + 8, "FPCh", 4); std::memcpy(result.data() + 16, "XfsX", 4);
    result.insert(result.end(), one.begin(), one.end()); result.insert(result.end(), two.begin(), two.end());
    addLe32(result, 0); addLe32(result, static_cast<std::uint32_t>(one.size()));
    put32(result, 4, static_cast<std::uint32_t>(result.size() - 8), true);
    put32(result, 56, static_cast<std::uint32_t>(result.size() - 60), true);
    return result;
}
void setSerum1Parameter(std::vector<std::uint8_t>& data, int index, float value)
{
    const auto offset = index < 228 ? 0x3460u + static_cast<std::size_t>(index) * 4u : 0x4ae0u + static_cast<std::size_t>(index - 228) * 4u;
    if (offset + 4 <= data.size()) putFloat(data, offset, value);
}
std::vector<std::uint8_t> serum1State(std::size_t size)
{
    std::vector<std::uint8_t> state(size, 0);
    auto set = [&](int i, float v) { setSerum1Parameter(state, i, v); };
    set(0, 0.8f); set(1, 0.8f); set(2, 0.5f); set(3, 0.5f); set(4, 0.5f); set(5, 0.5f);
    set(9, 0.4f); set(11, 0.25f); set(14, 0.6f); set(15, 0.5f); set(16, 0.5f); set(17, 0.5f); set(18, 0.5f);
    set(22, 0); set(24, 0); set(27, 0.6f); set(30, 0.5f);
    set(35, 0.25f); set(37, 0.6f); set(38, 0.7f); set(39, 0.4f); set(45, 0.5f); set(46, 0.2f);
    set(61, 0.5f); set(136, 0.7f); set(140, 1.0f); set(159, 1.0f); set(168, 0.1f);
    set(212, 1.0f); set(213, 0.0f); set(214, 0.0f); set(215, 0.0f); set(216, 1.0f); set(269, 0.4f);
    // One LFO1 -> A table-position route, with a three-point classic curve.
    putFloat(state, 4, 0.5f); putFloat(state, 8, 1.0f); state[12] = 0;
    state[20] = 5; state[26] = 11;
    for (int i = 0; i < 3; ++i)
    {
        putDouble(state, 0x280 + static_cast<std::size_t>(i) * 8, 0.5);
        putDouble(state, 0x280 + 4 * 520 + static_cast<std::size_t>(i) * 8, i * 0.5);
        putDouble(state, 0x280 + 8 * 520 + static_cast<std::size_t>(i) * 8, i == 1 ? 0.0 : 1.0);
    }
    return state;
}

class CborWriter
{
public:
    std::vector<std::uint8_t> bytes;
    void header(unsigned major, std::uint64_t n)
    {
        if (n < 24) { bytes.push_back(static_cast<std::uint8_t>((major << 5) | n)); return; }
        const int count = n <= 0xff ? 1 : n <= 0xffff ? 2 : n <= 0xffffffffu ? 4 : 8;
        bytes.push_back(static_cast<std::uint8_t>((major << 5) | (count == 1 ? 24 : count == 2 ? 25 : count == 4 ? 26 : 27)));
        for (int i = count - 1; i >= 0; --i) bytes.push_back(static_cast<std::uint8_t>(n >> (i * 8)));
    }
    void write(const juce::var& value)
    {
        if (value.isBool()) { bytes.push_back(static_cast<bool>(value) ? 0xf5 : 0xf4); return; }
        if (value.isInt() || value.isInt64())
        {
            const auto n = static_cast<juce::int64>(value);
            if (n >= 0) header(0, static_cast<std::uint64_t>(n)); else header(1, static_cast<std::uint64_t>(-1 - n));
            return;
        }
        if (value.isDouble())
        {
            bytes.push_back(0xfb); double x = static_cast<double>(value); std::uint64_t bits; std::memcpy(&bits, &x, 8);
            for (int i = 7; i >= 0; --i) bytes.push_back(static_cast<std::uint8_t>(bits >> (8 * i)));
            return;
        }
        if (value.isString())
        {
            const auto text = value.toString().toUTF8(); const auto length = std::strlen(text.getAddress());
            header(3, length); bytes.insert(bytes.end(), text.getAddress(), text.getAddress() + length); return;
        }
        if (const auto* array = value.getArray())
        {
            header(4, static_cast<std::uint64_t>(array->size())); for (const auto& entry : *array) write(entry); return;
        }
        if (auto* object = value.getDynamicObject())
        {
            const auto& props = object->getProperties(); header(5, static_cast<std::uint64_t>(props.size()));
            for (int i = 0; i < props.size(); ++i) { write(juce::var(props.getName(i).toString())); write(props.getValueAt(i)); }
            return;
        }
        bytes.push_back(0xf6);
    }
};
std::vector<std::uint8_t> serum2Container(const std::vector<std::uint8_t>& cbor)
{
    std::vector<std::uint8_t> compressed(ZSTD_compressBound(cbor.size()));
    const auto size = ZSTD_compress(compressed.data(), compressed.size(), cbor.data(), cbor.size(), 1);
    check(!ZSTD_isError(size), "Synthetic zstd frame built"); compressed.resize(size);
    const char magic[] { 'X','f','e','r','J','s','o','n','\0' };
    const std::string metadata = R"({"fileType":"SerumPreset","presetName":"Synthetic Serum 2"})";
    std::vector<std::uint8_t> output(magic, magic + sizeof(magic)); addLe64(output, metadata.size());
    output.insert(output.end(), metadata.begin(), metadata.end()); addLe32(output, static_cast<std::uint32_t>(cbor.size())); addLe32(output, 2);
    output.insert(output.end(), compressed.begin(), compressed.end()); return output;
}
std::vector<std::uint8_t> serum2FromJson(const juce::String& json)
{
    juce::var doc; check(juce::JSON::parse(json, doc).wasOk(), "Synthetic Serum 2 schema built");
    CborWriter writer; writer.write(doc); return serum2Container(writer.bytes);
}
juce::String waveformBase64(int count, bool square)
{
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(count) * 4u);
    for (int i = 0; i < count; ++i)
    {
        const auto phase = static_cast<double>(i) / count;
        putFloat(bytes, static_cast<std::size_t>(i) * 4u, square ? (phase < 0.5 ? 0.8f : -0.8f) : static_cast<float>(0.8 * std::sin(phase * 6.283185307179586)));
    }
    return juce::Base64::toBase64(bytes.data(), bytes.size());
}
juce::String sampleBase64(int count, bool opposite)
{
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(count) * 2u);
    for (int i = 0; i < count; ++i)
    {
        const auto x = static_cast<int>(12000 * std::sin(i * 0.09));
        const auto v = static_cast<std::uint16_t>(opposite ? -x : x);
        bytes[static_cast<std::size_t>(i) * 2] = static_cast<std::uint8_t>(v);
        bytes[static_cast<std::size_t>(i) * 2 + 1] = static_cast<std::uint8_t>(v >> 8);
    }
    return juce::Base64::toBase64(bytes.data(), bytes.size());
}
juce::String vitalFixture()
{
    auto frameA = waveformBase64(64, false), frameB = waveformBase64(64, true);
    auto left = sampleBase64(4096, false), right = sampleBase64(4096, true);
    return R"({"synth_version":"1.5.5","preset_name":"Synthetic Vital","settings":{)"
        "\"volume\":6400,\"env_1_attack\":0.5,\"env_1_decay\":1.0,\"env_1_sustain\":0.75,\"env_1_release\":0.5,"
        "\"osc_1_on\":1,\"osc_1_level\":0.8,\"osc_1_pan\":0.3,\"osc_1_wave_frame\":128,\"osc_1_distortion_type\":2,"
        "\"sample_on\":1,\"sample_loop\":0,\"sample_keytrack\":0,\"sample_level\":0.8,"
        "\"sample\":{\"length\":4096,\"sample_rate\":44100,\"samples\":\"" + left + "\",\"samples_stereo\":\"" + right + "\"},"
        "\"wavetables\":[{\"groups\":[{\"components\":[{\"type\":\"Wave Source\",\"keyframes\":["
        "{\"position\":0,\"wave_data\":\"" + frameA + "\"},{\"position\":256,\"wave_data\":\"" + frameB + "\"}]}]}]}],"
        "\"lfos\":[{\"points\":[0,1,0.5,0,1,1],\"powers\":[0,0,0]}],\"lfo_1_frequency\":1,"
        "\"modulations\":[{\"source\":\"lfo_1\",\"destination\":\"osc_1_level\"},{\"source\":\"lfo_5\",\"destination\":\"osc_1_pan\"}],"
        "\"modulation_1_amount\":-0.8,\"compressor_on\":1,\"compressor_mix\":0.4}}";
}
juce::AudioBuffer<float> render(const vanta::ParameterValues& p, const vanta::WaveTableBank* bank, int note = 69)
{
    vanta::SynthEngine engine; engine.prepare(44100.0, 1024);
    if (bank != nullptr) engine.publishWaveTables(*bank);
    juce::AudioBuffer<float> audio(2, 16384); juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)), 0);
    engine.process(audio, midi, p); return audio;
}
double difference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double result = 0; for (int i = 500; i < a.getNumSamples(); ++i) result += std::abs(a.getSample(0, i) - b.getSample(0, i));
    return result / (a.getNumSamples() - 500);
}
void testVital()
{
    const auto source = vitalFixture();
    const auto imported = vanta::PresetCodec::decode(source, ".vital");
    const auto envSource = source.replace("\"env_1_attack\":0.5", "\"env_1_delay\":0.5,\"env_1_hold\":1.0,\"env_1_attack\":0.5");
    const auto withStages = vanta::PresetCodec::decode(envSource, ".vital");
    check(withStages.ok && std::abs(withStages.values[vanta::ampDelay] - 0.0625f) < 0.0001f
          && std::abs(withStages.values[vanta::ampHold] - 1.0f) < 0.0001f
          && withStages.message.containsIgnoreCase("curve shapes"),
          "Vital delay and hold use fourth-power seconds and curves remain approximate");
    const auto clipped = vanta::PresetCodec::decode(envSource.replace("\"env_1_hold\":1.0", "\"env_1_hold\":2.0"), ".vital");
    check(clipped.ok && clipped.values[vanta::ampHold] == 4.0f && clipped.message.containsIgnoreCase("limited"),
          "Out-of-range Vital hold is bounded and reported");
    check(imported.ok && imported.name == "Synthetic Vital" && imported.waveTables != nullptr, "Vital JSON is recognised");
    const auto noName = source.replace("\"preset_name\":\"Synthetic Vital\",", "");
    const auto bytes = noName.toUTF8();
    const auto namedFromFile = vanta::PresetCodec::decodeBytes(bytes.getAddress(), std::strlen(bytes.getAddress()), ".vital", "Filename patch");
    check(namedFromFile.ok && namedFromFile.name == "Filename patch", "Vital import falls back to the filename when no preset name is stored");
    check(std::abs(imported.values[vanta::attack] - 0.0625f) < 0.0001f && std::abs(imported.values[vanta::oscAPan] - 0.3f) < 0.001f && imported.values[vanta::level] == 1.0f,
          "Vital master gain, fourth-root envelope and oscillator parameters mapped");
    check(imported.waveTables->tables[0].frameCount == 2 && imported.values[vanta::oscATable] == 1
          && imported.waveTables->tables[0].positions[1] == 1.0f && imported.values[vanta::oscATablePosition] == 0.5f,
          "Vital 0..256 keyframes resampled to 2048 samples and mapped to position");
    check(imported.values[vanta::oscDSample] == 1 && imported.waveTables->sample.frames == 4096 && imported.waveTables->sample.channels == 2,
          "Vital stereo PCM decoded to sample oscillator");
    check(imported.waveTables->modulation.routeCount == 1 && imported.message.containsIgnoreCase("beyond four"), "Vital LFO route mapped and excess LFO warned");
    check(imported.values[vanta::ottEnabled] == 1 && std::abs(imported.values[vanta::ottMix] - 0.4f) < 0.001f,
          "Vital active compressor enables VANTA multiband");
    check(imported.message.containsIgnoreCase("warp") && imported.message.containsIgnoreCase("not imported"), "Vital unsupported features produce diagnostics");

    auto a = imported.values; a[vanta::oscDEnabled] = 0; a[vanta::ottEnabled] = 0; a[vanta::attack] = 0.001f; a[vanta::cutoff] = 18000;
    auto noMod = std::make_unique<vanta::WaveTableBank>(*imported.waveTables); noMod->modulation.routeCount = 0;
    a[vanta::oscATablePosition] = 0; const auto first = render(a, noMod.get());
    a[vanta::oscATablePosition] = 1; const auto second = render(a, noMod.get());
    check(difference(first, second) > 0.005, "Imported wavetable frames audibly differ");
    const auto modulated = render(a, imported.waveTables.get());
    check(difference(second, modulated) > 0.002, "Reconstructed LFO route modulates audio");
    a[vanta::oscAEnabled] = 0; a[vanta::oscDEnabled] = 1;
    const auto sample = render(a, imported.waveTables.get());
    check(sample.getRMSLevel(0, 500, 2500) > 0.005f && sample.getRMSLevel(1, 500, 2500) > 0.005f,
          "Imported stereo sample produces sound");
    check(sample.getRMSLevel(0, 10000, 1000) < 1.0e-5f, "Imported non-looping sample finishes");
    double side = 0; for (int i = 500; i < 2500; ++i) side += std::abs(sample.getSample(0, i) - sample.getSample(1, i));
    check(side > 5, "Imported stereo PCM channels remain distinct");

    const auto native = vanta::PresetCodec::encode("Converted", imported.values, imported.waveTables.get());
    const auto restored = vanta::PresetCodec::decode(native, ".vanta");
    check(restored.ok && restored.waveTables != nullptr && restored.waveTables->tables[0].frameCount == 2
          && restored.waveTables->sample.frames == 4096 && restored.waveTables->modulation.routeCount == 1,
          "VANTA preset round-trips tables, sample and modulation");
    check(std::abs(restored.waveTables->sample.left[130] - imported.waveTables->sample.left[130]) < 1.0e-7f
          && std::abs(restored.waveTables->modulation.lfos[0].shape[128] - 0.5f) < 0.01f,
          "VANTA asset values and reconstructed curve survive round-trip");
    check(!vanta::PresetCodec::decode(source.replace(waveformBase64(64, false).substring(0, 12), "!!!!!!!!!!!!"), ".vital").ok,
          "Corrupt Vital wavetable base64 is rejected");
    check(!vanta::PresetCodec::decode(source.replace("\"length\":4096", "\"length\":9999999"), ".vital").ok,
          "Oversized Vital sample metadata is rejected");
}
void testSerum1()
{
    auto state = serum1State(20704); std::vector<std::uint8_t> frames(2 * 2048 * 4);
    for (int i = 0; i < 2048; ++i)
    {
        putFloat(frames, static_cast<std::size_t>(i) * 4, static_cast<float>(std::sin(6.283185307 * i / 2048.0)));
        putFloat(frames, static_cast<std::size_t>(2048 + i) * 4, i < 1024 ? 0.8f : -0.8f);
    }
    put32(state, 0x4968, static_cast<std::uint32_t>(frames.size()));
    const auto fxp = serum1Container(state, frames);
    const auto imported = vanta::PresetCodec::decodeBytes(fxp.data(), fxp.size(), ".fxp", "Synthetic Serum 1");
    auto heldState = state; setSerum1Parameter(heldState, 36, 0.5f);
    const auto heldFxp = serum1Container(heldState, frames);
    const auto heldImport = vanta::PresetCodec::decodeBytes(heldFxp.data(), heldFxp.size(), ".fxp");
    check(heldImport.ok && std::abs(heldImport.values[vanta::ampHold] - 1.0f) < 0.0001f
          && heldImport.values[vanta::ampDelay] == 0.0f && heldImport.message.containsIgnoreCase("curve shapes"),
          "Serum 1 hold uses the approximate fifth-power seconds mapping");
    setSerum1Parameter(heldState, 36, 1.0f);
    const auto longFxp = serum1Container(heldState, frames);
    const auto longHold = vanta::PresetCodec::decodeBytes(longFxp.data(), longFxp.size(), ".fxp");
    check(longHold.ok && longHold.values[vanta::ampHold] == 4.0f && longHold.message.containsIgnoreCase("limited"),
          "Long Serum 1 hold is bounded and reported");
    setSerum1Parameter(heldState, 36, std::numeric_limits<float>::quiet_NaN());
    const auto malformedHold = serum1Container(heldState, frames);
    check(!vanta::PresetCodec::decodeBytes(malformedHold.data(), malformedHold.size(), ".fxp").ok,
          "Non-finite Serum 1 hold is rejected");
    check(imported.ok && imported.name == "Synthetic Serum 1", "Serum 1 VST2 chunk and zlib streams decoded");
    check(imported.waveTables->tables[0].frameCount == 2 && imported.values[vanta::oscATable] == 1,
          "Serum 1 embedded float wavetable decoded");
    check(std::abs(imported.values[vanta::attack] - 32.0f * std::pow(0.25f, 5)) < 0.0001f
          && imported.values[vanta::oscBEnabled] == 0, "Serum 1 envelope scale and oscillator switches mapped");
    check(imported.waveTables->modulation.routeCount == 1 && imported.values[vanta::ottEnabled] == 1,
          "Serum 1 LFO route and multiband switch decoded");
    check(imported.message.containsIgnoreCase("warp"), "Serum 1 unsupported warp warns");
    auto bad = fxp; bad[16] = 'N'; check(!vanta::PresetCodec::decodeBytes(bad.data(), bad.size(), ".fxp").ok, "Other plugins' FXP rejected");
    bad = fxp; bad.pop_back(); check(!vanta::PresetCodec::decodeBytes(bad.data(), bad.size(), ".fxp").ok, "Truncated Serum 1 container rejected");
    bad = fxp; bad[60] = 0; check(!vanta::PresetCodec::decodeBytes(bad.data(), bad.size(), ".fxp").ok, "Corrupt Serum 1 zlib data rejected");
    auto badFrames = frames; putFloat(badFrames, 0, std::numeric_limits<float>::quiet_NaN());
    bad = serum1Container(state, badFrames); check(!vanta::PresetCodec::decodeBytes(bad.data(), bad.size(), ".fxp").ok, "Non-finite Serum 1 wavetable sample rejected");

    auto noiseState = serum1State(28232); setSerum1Parameter(noiseState, 212, 0); setSerum1Parameter(noiseState, 214, 1);
    setSerum1Parameter(noiseState, 159, 0); put32(noiseState, 0x5540, 2); put32(noiseState, 0x5544, 2 * 4096 * 4);
    std::vector<std::uint8_t> noise(2 * 4096 * 4);
    for (int i = 0; i < 4096; ++i)
    {
        const auto s = static_cast<float>(0.5 * std::sin(0.09 * i));
        putFloat(noise, static_cast<std::size_t>(i) * 4, s); putFloat(noise, static_cast<std::size_t>(i + 4096) * 4, -s);
    }
    const auto noiseFxp = serum1Container(noiseState, noise);
    const auto noiseImport = vanta::PresetCodec::decodeBytes(noiseFxp.data(), noiseFxp.size(), ".fxp");
    check(noiseImport.ok && noiseImport.values[vanta::oscDSample] == 1 && noiseImport.waveTables->sample.channels == 2,
          "Serum 1 planar embedded stereo noise decoded");
    check(noiseImport.values[vanta::ottEnabled] == 0, "Inactive Serum 1 compression leaves multiband bypassed");
}
void testSerum2()
{
    const juce::String json = R"({
      "Oscillator0":{"plainParams":{"kParamEnable":true,"kParamVolume":0.8,"kParamPan":25,"kParamPitch":7,"kParamCoarsePit":5},
                     "WTOsc0":{"plainParams":{"kParamWarp":0.25,"kParamTablePos":128},"relativePathToWT":"Tables/External.wav"}},
      "Oscillator1":{"plainParams":{"kParamEnable":false}},
      "Oscillator8":{"plainParams":{"kParamEnable":true}},
      "Env0":{"plainParams":{"kParamAttack":0.25,"kParamDecay":1.2,"kParamSustain":0.6,"kParamRelease":2.0}},
      "VoiceFilter0":{"plainParams":{"kParamEnable":true,"kParamFreq":0.5,"kParamReso":10}},
      "LFO0":{"plainParams":{"kParamBeatSync":false,"kParamRate":3.0},
              "curveData":{"xVals":[0,0.5,1],"yVals":[1,0,1],"curveVals":[0.5,0.5,0.5]}},
      "ModSlot0":{"source":[6,0],"destModuleTypeString":"Oscillator","destModuleID":0,"destModuleParamName":"kParamVolume",
                  "plainParams":{"kParamAmount":-50,"kParamBipolar":true}},
      "ModSlot1":{"source":[10,0],"destModuleTypeString":"Oscillator","destModuleID":0,"destModuleParamName":"kParamPan"},
      "FXRack0":{"FX":[{"type":14,"FXSplit3":{"plainParams":{"kParamModuleCount1":1,"kParamModuleCount2":1,"kParamModuleCount3":1}}},{"type":5,"FXComp":{"plainParams":{"kParamWet":30,"kParamRatio":4}}},
                        {"type":5,"FXComp":{"plainParams":{"kParamWet":30,"kParamRatio":4}}},
                        {"type":5,"FXComp":{"plainParams":{"kParamWet":30,"kParamRatio":4}}}]}
    })";
    const auto bytes = serum2FromJson(json);
    const auto imported = vanta::PresetCodec::decodeBytes(bytes.data(), bytes.size(), ".SerumPreset");
    const auto holdBytes = serum2FromJson(json.replace("\"kParamAttack\":0.25", "\"kParamHold\":0.75,\"kParamAttack\":0.25"));
    const auto heldImport = vanta::PresetCodec::decodeBytes(holdBytes.data(), holdBytes.size(), ".SerumPreset");
    check(heldImport.ok && std::abs(heldImport.values[vanta::ampHold] - 0.75f) < 0.001f
          && heldImport.values[vanta::ampDelay] == 0.0f && heldImport.message.containsIgnoreCase("curve shapes"),
          "Serum 2 hold is imported in seconds with an honest curve warning");
    const auto longBytes = serum2FromJson(json.replace("\"kParamAttack\":0.25", "\"kParamHold\":9.0,\"kParamAttack\":0.25"));
    const auto longHold = vanta::PresetCodec::decodeBytes(longBytes.data(), longBytes.size(), ".SerumPreset");
    check(longHold.ok && longHold.values[vanta::ampHold] == 4.0f && longHold.message.containsIgnoreCase("limited"),
          "Long Serum 2 hold is bounded and reported");
    check(imported.ok && imported.name == "Synthetic Serum 2", "Serum 2 XferJson zstd CBOR decoded");
    check(std::abs(imported.values[vanta::attack] - 0.25f) < 0.001f && std::abs(imported.values[vanta::oscAPan] - 0.5f) < 0.001f
          && imported.values[vanta::oscATune] == 12 && std::abs(imported.values[vanta::oscALevel] - 0.8f) < 0.001f, "Serum 2 plain-unit envelope and oscillator values mapped");
    check(imported.waveTables->modulation.routeCount == 1 && std::abs(imported.waveTables->modulation.lfos[0].rateHz - 3) < 0.001f,
          "Serum 2 vector curve and free-running LFO route reconstructed");
    check(imported.values[vanta::ottEnabled] == 1 && std::abs(imported.values[vanta::ottMix] - 0.3f) < 0.001f,
          "Serum 2 three-band split/compressor enables multiband");
    check(imported.message.containsIgnoreCase("external") && imported.message.containsIgnoreCase("warp")
          && imported.message.containsIgnoreCase("beyond four") && imported.message.containsIgnoreCase("eight slots"),
          "Serum 2 missing assets, warp and excess slots produce warnings");
    // Synthetic embedded audio uses the same documented flat/planar CBOR arrays.
    juce::var audioDoc; check(juce::JSON::parse(R"({"Oscillator0":{"plainParams":"default","WTOsc0":{"plainParams":"default"}},"Oscillator3":{"plainParams":{"kParamEnable":true,"kParamPitchTrack":true},"NoiseOsc3":{"plainParams":{"kParamOneShot":true}}},"Env0":{"plainParams":"default"}})", audioDoc).wasOk(), "Synthetic embedded Serum 2 structure built");
    juce::Array<juce::var> samples, left, right;
    for (int i = 0; i < 4096; ++i) samples.add(static_cast<double>(0.5 * std::sin(6.283185307 * i / 2048.0)));
    for (int i = 0; i < 64; ++i) { left.add(0.25); right.add(-0.25); }
    auto* rootObject = audioDoc.getDynamicObject();
    auto* wt = rootObject->getProperty("Oscillator0").getDynamicObject()->getProperty("WTOsc0").getDynamicObject();
    wt->setProperty("embeddedWTData", juce::var(samples));
    juce::Array<juce::var> channels; channels.add(juce::var(left)); channels.add(juce::var(right));
    rootObject->getProperty("Oscillator3").getDynamicObject()->getProperty("NoiseOsc3").getDynamicObject()->setProperty("embeddedNoiseData", juce::var(channels));
    CborWriter audioWriter; audioWriter.write(audioDoc); const auto audioBytes = serum2Container(audioWriter.bytes);
    const auto audioImport = vanta::PresetCodec::decodeBytes(audioBytes.data(), audioBytes.size(), ".SerumPreset");
    check(audioImport.ok && audioImport.waveTables->tables[0].frameCount == 2 && audioImport.waveTables->sample.frames == 64
          && audioImport.waveTables->sample.channels == 2 && audioImport.waveTables->sample.keytrack && !audioImport.waveTables->sample.loop,
          "Serum 2 embedded wavetable and planar stereo sample decoded with playback flags");

    auto bad = bytes; bad.pop_back(); check(!vanta::PresetCodec::decodeBytes(bad.data(), bad.size(), ".SerumPreset").ok, "Truncated Serum 2 frame rejected");
    bad = bytes; for (int i = 9; i < 17; ++i) bad[static_cast<std::size_t>(i)] = 0xff;
    check(!vanta::PresetCodec::decodeBytes(bad.data(), bad.size(), ".SerumPreset").ok, "Oversized Serum 2 metadata length rejected");
    bad = bytes; bad.push_back(0); check(!vanta::PresetCodec::decodeBytes(bad.data(), bad.size(), ".SerumPreset").ok, "Trailing zstd frame data rejected");
    const auto invalidCbor = serum2Container({ 0xbf, 0x69, 'E','n','v','0','x','x','x','x','x', 0x9f });
    check(!vanta::PresetCodec::decodeBytes(invalidCbor.data(), invalidCbor.size(), ".SerumPreset").ok, "Truncated indefinite CBOR rejected");
    auto singleBand = serum2FromJson(R"({"Oscillator0":{"plainParams":"default"},"Env0":{"plainParams":"default"},"FXRack0":{"FX":[{"type":5,"FXComp":{"plainParams":{}}}]}})");
    const auto single = vanta::PresetCodec::decodeBytes(singleBand.data(), singleBand.size(), ".SerumPreset");
    check(single.ok && single.values[vanta::ottEnabled] == 0, "A single-band Serum 2 compressor does not falsely enable OTT");

    auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("vanta-synthetic-" + juce::Uuid().toString() + ".SerumPreset");
    check(temp.replaceWithData(bytes.data(), bytes.size()) && vanta::PresetCodec::loadFile(temp).ok, "Binary Serum 2 preset loads from disk");
    temp.deleteFile();
}
void testLimitsAndSafety()
{
    const auto deep = juce::String::repeatedString("[", 100) + "0" + juce::String::repeatedString("]", 100);
    check(!vanta::PresetCodec::decode(deep, ".vital").ok, "Excessive JSON nesting rejected before recursive parsing");
    check(!vanta::PresetCodec::decode("{} trailing", ".vanta").ok, "Trailing JSON garbage rejected");
    check(!vanta::PresetCodec::decodeBytes(nullptr, 0, ".fxp").ok, "Empty input rejected");
    const auto silent = vanta::PresetCodec::decode(R"({"synth_version":"1","settings":{"osc_1_on":1,"osc_1_level":0}})", ".vital");
    check(silent.ok && silent.message.containsIgnoreCase("may be silent"), "Zero source levels produce a clear silence warning");
    const auto largeSample = sampleBase64(vanta::maxSampleFrames + 16, false);
    const auto largeJson = juce::String(R"({"synth_version":"1","settings":{"osc_1_on":0,"sample_on":1,"sample":{"length":)")
        + juce::String(vanta::maxSampleFrames + 16) + R"(,"sample_rate":44100,"samples":")" + largeSample + R"("}}})";
    const auto largeImport = vanta::PresetCodec::decode(largeJson, ".vital");
    check(largeImport.ok && largeImport.waveTables->sample.frames == vanta::maxSampleFrames
          && largeImport.message.containsIgnoreCase("truncated"), "Large embedded PCM is bounded and truncated with a warning");
    const auto largeNative = vanta::PresetCodec::encode("Large sample", largeImport.values, largeImport.waveTables.get());
    const auto largeRoundTrip = vanta::PresetCodec::decode(largeNative, ".vanta");
    check(largeRoundTrip.ok && largeRoundTrip.waveTables->sample.frames == vanta::maxSampleFrames,
          "Maximum-size imported PCM survives native preset serialization");
    const std::vector<std::vector<std::uint8_t>> invalidUtf8 {
        {0xc2}, {0xc0, 0xaf}, {0xe0, 0x80, 0x80}, {0xed, 0xa0, 0x80},
        {0xf0, 0x80, 0x80, 0x80}, {0xf4, 0x90, 0x80, 0x80}, {0x80}, {0}
    };
    bool badUtf8Rejected = true;
    for (const auto& bad : invalidUtf8)
        badUtf8Rejected &= !vanta::PresetImporter::isValidUtf8(bad.data(), bad.size())
                        && !vanta::PresetCodec::decodeBytes(bad.data(), bad.size(), ".vital").ok;
    const std::vector<std::uint8_t> goodUtf8 {0x41, 0xe2, 0x82, 0xac, 0xf0, 0x9f, 0x8e, 0xb5};
    check(badUtf8Rejected && vanta::PresetImporter::isValidUtf8(goodUtf8.data(), goodUtf8.size()),
          "Invalid, truncated, overlong and surrogate UTF-8 is rejected safely");
    auto invalidMeta = serum2FromJson(R"({"Oscillator0":{"plainParams":"default"},"Env0":{"plainParams":"default"}})");
    invalidMeta[17] = 0xff;
    check(!vanta::PresetCodec::decodeBytes(invalidMeta.data(), invalidMeta.size(), ".SerumPreset").ok,
          "Invalid UTF-8 in Serum 2 metadata is rejected");
    const auto invalidText = serum2Container({ 0xa1, 0x61, 0xff, 0xf6 });
    check(!vanta::PresetCodec::decodeBytes(invalidText.data(), invalidText.size(), ".SerumPreset").ok,
          "Invalid UTF-8 in Serum 2 CBOR text is rejected");
    const auto valid = serum2FromJson(R"({"Oscillator0":{"plainParams":"default"},"Env0":{"plainParams":"default"}})");
    std::mt19937 rng(12345);
    bool noThrow = true;
    for (int i = 0; i < 200; ++i)
    {
        auto fuzzed = valid;
        const int edits = 1 + static_cast<int>(rng() % 8);
        for (int j = 0; j < edits; ++j) fuzzed[rng() % fuzzed.size()] ^= static_cast<std::uint8_t>(1u << (rng() % 8));
        try { (void) vanta::PresetCodec::decodeBytes(fuzzed.data(), fuzzed.size(), ".SerumPreset"); }
        catch (...) { noThrow = false; break; }
    }
    check(noThrow, "Mutated compressed presets do not throw from the public decoder");
    const auto vitalText = vitalFixture(); const auto vitalUtf8 = vitalText.toUTF8();
    const std::vector<std::uint8_t> vitalBytes(vitalUtf8.getAddress(), vitalUtf8.getAddress() + std::strlen(vitalUtf8.getAddress()));
    const auto serumBytes = serum1Container(serum1State(28232), {});
    for (int format = 0; format < 2; ++format)
        for (int i = 0; i < 150; ++i)
        {
            auto fuzzed = format == 0 ? vitalBytes : serumBytes;
            const int edits = 1 + static_cast<int>(rng() % 6);
            for (int j = 0; j < edits; ++j) fuzzed[rng() % fuzzed.size()] ^= static_cast<std::uint8_t>(1u << (rng() % 8));
            try { (void) vanta::PresetCodec::decodeBytes(fuzzed.data(), fuzzed.size(), format == 0 ? ".vital" : ".fxp"); }
            catch (...) { noThrow = false; }
        }
    check(noThrow, "Mutated Vital JSON and Serum 1 FXP do not throw from the public decoder");

    vanta::WaveTableExchange exchange;
    auto one = std::make_unique<vanta::WaveTableBank>(), two = std::make_unique<vanta::WaveTableBank>();
    one->tables[0].frameCount = two->tables[0].frameCount = 1;
    one->tables[0].samples.fill(0.25f); two->tables[0].samples.fill(-0.25f);
    exchange.publish(*one);
    std::atomic<int> done { 0 }; std::atomic<bool> consistent { true };
    auto writer = [&](bool reverse)
    {
        for (int i = 0; i < 100; ++i) exchange.publish(((i & 1) != 0) == reverse ? *one : *two);
        ++done;
    };
    std::thread producer(writer, false), secondProducer(writer, true);
    std::thread snapshotter([&]
    {
        for (int n = 0; n < 10; ++n)
        {
            const auto snapshot = exchange.copyPublished();
            const auto first = snapshot->tables[0].samples[0];
            for (int i = 0; i < vanta::wavetableFrameSize; ++i)
                if (snapshot->tables[0].samples[static_cast<std::size_t>(i)] != first) consistent.store(false);
        }
    });
    while (done.load() < 2)
    {
        const auto& current = exchange.beginRead();
        const auto& samples = current.tables[0].samples;
        const auto first = samples[0];
        for (int i = 0; i < vanta::wavetableFrameSize; ++i)
            if (samples[static_cast<std::size_t>(i)] != first) consistent.store(false);
        exchange.endRead();
    }
    producer.join(); secondProducer.join(); snapshotter.join();
    check(consistent.load(), "Audio reader and state snapshots observe whole immutable banks during concurrent swaps");

    auto p = vanta::defaultValues(); p[vanta::attack] = 0.001f; p[vanta::wave] = 1.0f;
    const auto dry = render(p, nullptr); auto bypass = p; bypass[vanta::ottEnabled] = 0; bypass[vanta::ottDepth] = 1; bypass[vanta::ottMix] = 1;
    const auto bypassed = render(bypass, nullptr);
    check(difference(dry, bypassed) == 0.0, "Bypassed multiband is bit-identical to dry rendering");
    auto wet = p; wet[vanta::ottEnabled] = 1; wet[vanta::ottDepth] = 1; wet[vanta::ottMix] = 1;
    const auto compressed = render(wet, nullptr);
    check(difference(dry, compressed) > 0.0005, "Enabled three-band dynamics affects audio");
    bool finite = true;
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < compressed.getNumSamples(); ++i)
            finite &= std::isfinite(compressed.getSample(channel, i)) && std::abs(compressed.getSample(channel, i)) <= 1.0f;
    check(finite, "Three-band output stays finite and bounded");
    p[vanta::oscAEnabled] = 0; p[vanta::oscBEnabled] = 1; p[vanta::oscBLevel] = 1; p[vanta::oscBWave] = 1;
    check(render(p, nullptr).getRMSLevel(0, 1000, 4000) > 0.005f, "An imported patch can disable oscillator A while keeping B active");
}
}
int main()
{
    testVital(); testSerum1(); testSerum2(); testLimitsAndSafety();
    std::cout << "All importer tests passed.\n";
}
