#include "PresetImporter.h"
#include <zlib.h>
#include <zstd.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace vanta
{
namespace
{
constexpr std::size_t maxDecodedBytes = 64u * 1024u * 1024u;
constexpr std::size_t maxCborItems = 800000;
constexpr int maxNesting = 48;

bool numeric(const juce::var& v) { return v.isInt() || v.isInt64() || v.isDouble() || v.isBool(); }
double number(const juce::var& v, double fallback)
{
    if (!numeric(v)) return fallback;
    const double n = static_cast<double>(v);
    return std::isfinite(n) ? n : fallback;
}
const juce::var& property(const juce::var& v, const juce::String& key)
{
    static const juce::var missing;
    if (auto* o = v.getDynamicObject()) return o->getProperty(key);
    return missing;
}
bool has(const juce::var& v, const juce::String& key)
{
    auto* o = v.getDynamicObject();
    return o != nullptr && o->hasProperty(key);
}
double get(const juce::var& v, const juce::String& key, double fallback)
{
    return number(property(v, key), fallback);
}
juce::String nameOr(const juce::var& v, const juce::String& fallback)
{
    if (!v.isString()) return fallback;
    const auto name = v.toString().trim().substring(0, 80);
    return name.isNotEmpty() ? name : fallback;
}
void warn(juce::StringArray& warnings, const juce::String& warning)
{
    if (!warnings.contains(warning)) warnings.add(warning);
}
void set(PresetResult& result, Parameter id, double value, juce::StringArray& warnings)
{
    if (!std::isfinite(value)) return;
    const auto& spec = parameterSpecs[static_cast<std::size_t>(id)];
    if (value < spec.minValue || value > spec.maxValue)
        warn(warnings, juce::String(spec.name) + " was limited to VANTA's range.");
    result.values[id] = static_cast<float>(juce::jlimit(static_cast<double>(spec.minValue), static_cast<double>(spec.maxValue), value));
}
void finish(PresetResult& result, const juce::String& format, juce::StringArray& warnings)
{
    bool anyEnabled = false, anyLevel = false;
    for (int osc = 0; osc < oscillatorCount; ++osc)
        if (oscillatorIsEnabled(result.values, osc))
        {
            anyEnabled = true;
            anyLevel |= result.values[oscillatorLevel(osc)] > 0.0f;
        }
    if (!anyEnabled) warn(warnings, "No oscillator source is enabled; the imported patch will be silent.");
    else if (!anyLevel) warn(warnings, "All enabled oscillator base levels are zero; the patch may be silent if its volume modulation was unsupported. Raise a layer level to audition it.");
    if (result.values[level] <= 0.0f) warn(warnings, "The imported master level is zero; the patch will be silent.");
    result.ok = true;
    result.message = "Approximate " + format + " import. The sound may differ from the original.";
    if (!warnings.isEmpty()) result.message += "\n\n" + warnings.joinIntoString("\n");
}
PresetResult failure(const juce::String& message)
{
    PresetResult result; result.message = message + " Your current sound was not changed."; return result;
}

std::uint32_t le32(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
std::uint32_t be32(const std::uint8_t* p)
{
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16)
         | (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}
std::uint64_t le64(const std::uint8_t* p) { return le32(p) | (static_cast<std::uint64_t>(le32(p + 4)) << 32); }
float readFloat(const std::uint8_t* p)
{
    const auto bits = le32(p); float result; std::memcpy(&result, &bits, sizeof(result)); return result;
}
void appendFloat(juce::MemoryOutputStream& out, float value)
{
    std::uint32_t bits; std::memcpy(&bits, &value, sizeof(bits));
    const std::uint8_t b[] { static_cast<std::uint8_t>(bits), static_cast<std::uint8_t>(bits >> 8),
                             static_cast<std::uint8_t>(bits >> 16), static_cast<std::uint8_t>(bits >> 24) };
    out.write(b, sizeof(b));
}

bool strictBase64(const juce::String& text, juce::MemoryBlock& bytes, std::size_t maxBytes)
{
    // Base64 is ASCII. JUCE String::operator[] walks UTF-8 from the start, so
    // indexing it in a loop is quadratic for large embedded samples.
    const auto* raw = text.toRawUTF8();
    const auto length = std::strlen(raw);
    if (length == 0 || length % 4 != 0 || length > ((maxBytes + 2) / 3) * 4) return false;
    for (std::size_t i = 0; i < length; ++i)
    {
        const auto c = raw[i];
        if (c == '=') { if (i < length - 2) return false; }
        else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/')) return false;
        if (i > 0 && raw[i - 1] == '=' && c != '=') return false;
    }
    juce::MemoryOutputStream stream;
    if (!juce::Base64::convertFromBase64(stream, text)) return false;
    bytes = stream.getMemoryBlock();
    return bytes.getSize() <= maxBytes;
}

// Cyclic linear resampling; sample counts and all values are checked before
// touching the fixed-size destination. Output is scaled as a whole per table
// (never independently per frame) to preserve relative frame levels.
bool putFrame(WaveTable& table, int frame, float position, const void* raw, std::size_t byteCount, juce::String& error)
{
    if (byteCount % 4 != 0 || byteCount < 8 || byteCount > 65536 || frame < 0 || frame >= maxWavetableFrames
        || !std::isfinite(position) || position < 0.0f || position > 1.0f)
    { error = "Invalid wavetable frame size or position."; return false; }
    const auto count = byteCount / 4;
    const auto* data = static_cast<const std::uint8_t*>(raw);
    for (std::size_t i = 0; i < count; ++i)
    {
        const float sample = readFloat(data + i * 4);
        if (!std::isfinite(sample) || std::abs(sample) > 1.0e6f)
        { error = "A wavetable contains non-finite or unsafe sample values."; return false; }
    }
    table.positions[static_cast<std::size_t>(frame)] = position;
    for (int i = 0; i < wavetableFrameSize; ++i)
    {
        const double source = static_cast<double>(i) * static_cast<double>(count) / wavetableFrameSize;
        const auto left = static_cast<std::size_t>(source), right = (left + 1) % count;
        const auto fraction = static_cast<float>(source - static_cast<double>(left));
        const float a = readFloat(data + left * 4), b = readFloat(data + right * 4);
        table.samples[static_cast<std::size_t>(frame * wavetableFrameSize + i)] = a + (b - a) * fraction;
    }
    table.frameCount = frame + 1;
    return true;
}
bool normalizeTable(WaveTable& table)
{
    float peak = 0.0f;
    for (int i = 0; i < table.frameCount * wavetableFrameSize; ++i)
        peak = juce::jmax(peak, std::abs(table.samples[static_cast<std::size_t>(i)]));
    if (peak <= 1.0f) return false;
    for (int i = 0; i < table.frameCount * wavetableFrameSize; ++i)
        table.samples[static_cast<std::size_t>(i)] /= peak;
    return true;
}

bool inflateChecked(const std::uint8_t* source, std::size_t size, std::size_t limit,
                    std::vector<std::uint8_t>& output, std::size_t& consumed)
{
    if (source == nullptr || size == 0 || size > std::numeric_limits<uInt>::max()) return false;
    z_stream stream {};
    if (inflateInit(&stream) != Z_OK) return false;
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(source));
    stream.avail_in = static_cast<uInt>(size);
    int status = Z_OK;
    std::array<std::uint8_t, 16384> chunk {};
    while (status == Z_OK)
    {
        stream.next_out = chunk.data(); stream.avail_out = static_cast<uInt>(chunk.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const auto produced = chunk.size() - stream.avail_out;
        if (produced > limit - output.size()) { inflateEnd(&stream); return false; }
        output.insert(output.end(), chunk.data(), chunk.data() + produced);
        if (status == Z_OK && produced == 0 && stream.avail_in == 0) break;
    }
    consumed = static_cast<std::size_t>(stream.total_in);
    inflateEnd(&stream);
    return status == Z_STREAM_END;
}

struct CurvePoint { double x, y, power; };
void triangle(ImportedLfo& lfo)
{
    for (int i = 0; i < importedLfoPoints; ++i)
    {
        const auto x = static_cast<float>(i) / importedLfoPoints;
        lfo.shape[static_cast<std::size_t>(i)] = x < 0.5f ? x * 2.0f : (1.0f - x) * 2.0f;
    }
}
bool reconstruct(ImportedLfo& lfo, const std::vector<CurvePoint>& points, bool serumTension)
{
    if (points.size() < 2 || points.size() > 480 || points.front().x > 0.0001 || points.back().x < 0.9999) return false;
    double previous = -1.0;
    for (const auto& p : points)
    {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.power) || p.x < 0 || p.x > 1 || p.y < 0 || p.y > 1 || p.x < previous) return false;
        previous = p.x;
    }
    std::size_t segment = 0;
    for (int i = 0; i < importedLfoPoints; ++i)
    {
        const double x = static_cast<double>(i) / importedLfoPoints;
        while (segment + 1 < points.size() - 1 && points[segment + 1].x <= x) ++segment;
        const auto& a = points[segment]; const auto& b = points[segment + 1];
        const double t = b.x > a.x ? juce::jlimit(0.0, 1.0, (x - a.x) / (b.x - a.x)) : 1.0;
        const double curve = serumTension ? (0.5 - juce::jlimit(0.0, 1.0, a.power)) * 4.0 : juce::jlimit(-4.0, 4.0, a.power);
        const double shaped = std::pow(t, std::pow(2.0, curve));
        // Both file formats use screen y=0 at the top, i.e. maximum output.
        lfo.shape[static_cast<std::size_t>(i)] = static_cast<float>(juce::jlimit(0.0, 1.0, 1.0 - (a.y + (b.y - a.y) * shaped)));
    }
    return true;
}
bool addRoute(ImportedModulation& mod, int source, int oscillator, ModTarget target, double amount, bool bipolar)
{
    if (source < 0 || source >= importedLfoCount || oscillator < 0 || oscillator >= oscillatorCount
        || !std::isfinite(amount) || std::abs(amount) > 48.0 || mod.routeCount >= maxImportedRoutes) return false;
    mod.routes[static_cast<std::size_t>(mod.routeCount++)] = { source, oscillator, target, static_cast<float>(amount), bipolar };
    return true;
}
float safeLfoRate(double hz) { return static_cast<float>(juce::jlimit(0.01, 40.0, std::isfinite(hz) ? hz : 2.0)); }
double leDouble(const std::uint8_t* p)
{
    const auto bits = le64(p); double value; std::memcpy(&value, &bits, sizeof(value)); return value;
}

// JUCE's UTF-8 string constructor assumes valid input. Reject malformed,
// overlong, surrogate, out-of-range and embedded-NUL sequences before making
// strings; otherwise iteration over hostile bytes can read past the terminator.
bool validUtf8(const void* raw, std::size_t size) noexcept
{
    const auto* data = static_cast<const std::uint8_t*>(raw);
    if (data == nullptr) return size == 0;
    for (std::size_t i = 0; i < size;)
    {
        const auto a = data[i++];
        if (a == 0) return false;
        if (a < 0x80) continue;
        int following = 0;
        if (a >= 0xc2 && a <= 0xdf) following = 1;
        else if (a >= 0xe0 && a <= 0xef) following = 2;
        else if (a >= 0xf0 && a <= 0xf4) following = 3;
        else return false;
        if (static_cast<std::size_t>(following) > size - i) return false;
        const auto b = data[i];
        if ((a == 0xe0 && b < 0xa0) || (a == 0xed && b >= 0xa0)
            || (a == 0xf0 && b < 0x90) || (a == 0xf4 && b > 0x8f)) return false;
        for (int n = 0; n < following; ++n) if ((data[i++] & 0xc0) != 0x80) return false;
    }
    return true;
}

// A bounded CBOR reader for Serum 2's maps/arrays/scalars. Indefinite-length
// containers are accepted, but depth, item counts, byte strings and output
// size are limited. Every read checks its remaining input before advancing.
class CborReader
{
public:
    CborReader(const void* data, std::size_t bytes) : input(static_cast<const std::uint8_t*>(data)), size(bytes) {}
    bool read(juce::var& result) { return value(result, 0) && offset == size; }
private:
    bool take(std::size_t count, const std::uint8_t*& p)
    {
        if (count > size - offset) return false;
        p = input + offset; offset += count; return true;
    }
    bool argument(unsigned info, std::uint64_t& n)
    {
        if (info < 24) { n = info; return true; }
        int bytes = info == 24 ? 1 : info == 25 ? 2 : info == 26 ? 4 : info == 27 ? 8 : 0;
        const std::uint8_t* p;
        if (bytes == 0 || !take(static_cast<std::size_t>(bytes), p)) return false;
        n = 0; for (int i = 0; i < bytes; ++i) n = (n << 8) | p[i];
        return true;
    }
    bool value(juce::var& result, int depth)
    {
        if (depth > maxNesting || ++items > maxCborItems) return false;
        const std::uint8_t* p;
        if (!take(1, p)) return false;
        const unsigned major = *p >> 5, info = *p & 31;
        const bool indefinite = info == 31;
        std::uint64_t n = 0;
        if (!indefinite && !argument(info, n)) return false;
        if (major == 0 || major == 1)
        {
            if (indefinite || n > static_cast<std::uint64_t>(std::numeric_limits<juce::int64>::max())) return false;
            const auto integer = static_cast<juce::int64>(n);
            result = major == 0 ? integer : (-1 - integer); return true;
        }
        if (major == 2 || major == 3)
        {
            if (indefinite || n > 16u * 1024u * 1024u || !take(static_cast<std::size_t>(n), p)) return false;
            if (major == 3)
            {
                if (!validUtf8(p, static_cast<std::size_t>(n))) return false;
                result = juce::String::fromUTF8(reinterpret_cast<const char*>(p), static_cast<int>(n));
            }
            else result = juce::var(juce::MemoryBlock(p, static_cast<std::size_t>(n)));
            return true;
        }
        if (major == 4 || major == 5)
        {
            if (!indefinite && (n > maxCborItems - items || (major == 5 && n > 4096))) return false;
            if (major == 4) result = juce::var(juce::Array<juce::var>());
            else result = juce::var(new juce::DynamicObject());
            std::uint64_t index = 0;
            while (indefinite || index < n)
            {
                if (indefinite && offset < size && input[offset] == 0xff) { ++offset; return true; }
                if (index++ >= maxCborItems || (major == 5 && index > 4096)) return false;
                juce::var a;
                if (!value(a, depth + 1)) return false;
                if (major == 4) result.getArray()->add(a);
                else
                {
                    if (!a.isString() || a.toString().isEmpty() || a.toString().length() > 255) return false;
                    const auto key = a.toString();
                    auto* object = result.getDynamicObject();
                    if (object->hasProperty(key)) return false;
                    juce::var b;
                    if (!value(b, depth + 1)) return false;
                    object->setProperty(key, b);
                }
            }
            return !indefinite;
        }
        if (major == 6 && !indefinite) return value(result, depth + 1); // semantic tag
        if (major != 7 || indefinite) return false;
        if (info == 20 || info == 21) { result = (info == 21); return true; }
        if (info == 22 || info == 23) { result = juce::var(); return true; }
        double d = 0.0;
        if (info == 25)
        {
            const auto sign = (n & 0x8000u) ? -1.0 : 1.0;
            const int exp = static_cast<int>((n >> 10) & 31u);
            const auto mantissa = static_cast<unsigned>(n & 1023u);
            if (exp == 31) return false;
            d = sign * (exp == 0 ? std::ldexp(static_cast<double>(mantissa), -24)
                                : std::ldexp(1.0 + mantissa / 1024.0, exp - 15));
        }
        else if (info == 26)
        {
            const auto bits = static_cast<std::uint32_t>(n); float f; std::memcpy(&f, &bits, sizeof(f)); d = f;
        }
        else if (info == 27) std::memcpy(&d, &n, sizeof(d));
        else return false;
        if (!std::isfinite(d)) return false;
        result = d; return true;
    }
    const std::uint8_t* input;
    std::size_t size, offset = 0, items = 0;
};
}

bool PresetImporter::isValidUtf8(const void* data, std::size_t size) noexcept { return validUtf8(data, size); }

bool PresetImporter::parseJson(const juce::String& text, juce::var& result, juce::String& error)
{
    // JUCE's JSON parser is recursive and accepts trailing text. Bound nesting
    // and enforce one complete root before invoking it on untrusted files.
    const auto* utf8 = text.toRawUTF8();
    const auto length = std::strlen(utf8);
    if (length > static_cast<std::size_t>(PresetCodec::maxFileBytes) || !validUtf8(utf8, length))
    { error = "JSON is oversized or contains invalid UTF-8."; return false; }
    std::array<juce::juce_wchar, maxNesting> stack {};
    std::array<std::size_t, maxNesting> members {};
    std::size_t items = 0;
    int depth = 0; bool string = false, escaped = false, started = false, complete = false;
    for (auto c : text)
    {
        if (string)
        {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') string = false;
            else if (c < 0x20) { error = "Invalid control character in JSON string."; return false; }
            continue;
        }
        if (complete)
        {
            if (!juce::CharacterFunctions::isWhitespace(c)) { error = "Unexpected data after JSON document."; return false; }
            continue;
        }
        if (c == '"') { string = true; continue; }
        if (c == '{' || c == '[')
        {
            if (depth >= maxNesting) { error = "JSON nesting limit exceeded."; return false; }
            if (++items > maxCborItems) { error = "JSON item limit exceeded."; return false; }
            started = true; stack[static_cast<std::size_t>(depth)] = c; members[static_cast<std::size_t>(depth++)] = 1;
        }
        else if (c == '}' || c == ']')
        {
            if (depth == 0 || stack[static_cast<std::size_t>(--depth)] != (c == '}' ? '{' : '['))
            { error = "Mismatched JSON brackets."; return false; }
            if (depth == 0) complete = true;
        }
        else if (c == ',' && depth > 0)
        {
            if (++items > maxCborItems || (++members[static_cast<std::size_t>(depth - 1)] > 4096 && stack[static_cast<std::size_t>(depth - 1)] == '{'))
            { error = "JSON item limit exceeded."; return false; }
        }
        else if (!started && !juce::CharacterFunctions::isWhitespace(c))
        { error = "Expected a JSON object or array."; return false; }
    }
    if (!complete || depth != 0 || string) { error = "Incomplete JSON document."; return false; }
    const auto parsed = juce::JSON::parse(text, result);
    if (parsed.failed()) { error = parsed.getErrorMessage(); return false; }
    return true;
}

juce::var PresetImporter::encodeWaveTables(const WaveTableBank& bank)
{
    juce::Array<juce::var> tables;
    for (const auto& table : bank.tables)
    {
        juce::Array<juce::var> frames;
        for (int frame = 0; frame < juce::jlimit(0, maxWavetableFrames, table.frameCount); ++frame)
        {
            juce::MemoryOutputStream bytes;
            for (int i = 0; i < wavetableFrameSize; ++i)
                appendFloat(bytes, table.samples[static_cast<std::size_t>(frame * wavetableFrameSize + i)]);
            auto* entry = new juce::DynamicObject();
            entry->setProperty("position", table.positions[static_cast<std::size_t>(frame)]);
            entry->setProperty("data", juce::Base64::toBase64(bytes.getData(), bytes.getDataSize()));
            frames.add(juce::var(entry));
        }
        tables.add(juce::var(frames));
    }
    return juce::var(tables);
}

bool PresetImporter::decodeWaveTables(const juce::var& value, std::shared_ptr<WaveTableBank>& bank, juce::String& error)
{
    auto* tables = value.getArray();
    if (tables == nullptr || tables->size() > oscillatorCount) { error = "Invalid VANTA wavetable list."; return false; }
    auto decoded = std::make_shared<WaveTableBank>();
    for (int i = 0; i < tables->size(); ++i)
    {
        auto* frames = tables->getReference(i).getArray();
        if (frames == nullptr || frames->size() > maxWavetableFrames) { error = "Invalid VANTA wavetable frame list."; return false; }
        auto& table = decoded->tables[static_cast<std::size_t>(i)];
        for (int f = 0; f < frames->size(); ++f)
        {
            const auto& entry = frames->getReference(f);
            const auto& pos = property(entry, "position"), &data = property(entry, "data");
            juce::MemoryBlock bytes;
            if (!numeric(pos) || !data.isString() || !strictBase64(data.toString(), bytes, wavetableFrameSize * 4)
                || bytes.getSize() != wavetableFrameSize * 4)
            { error = "Invalid VANTA wavetable data."; return false; }
            const auto p = static_cast<float>(number(pos, -1));
            if (f > 0 && p <= table.positions[static_cast<std::size_t>(f - 1)]) { error = "Wavetable positions must increase."; return false; }
            if (!putFrame(table, f, p, bytes.getData(), bytes.getSize(), error)) return false;
        }
        // Native state is expected to have already been normalized.
        if (normalizeTable(table)) { error = "VANTA wavetable sample exceeds the normalized range."; return false; }
    }
    bank = std::move(decoded);
    return true;
}

juce::var PresetImporter::encodeSample(const PcmSample& sample)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("slot", sample.slot); object->setProperty("frames", sample.frames);
    object->setProperty("channels", sample.channels); object->setProperty("sampleRate", sample.sampleRate);
    object->setProperty("loop", sample.loop); object->setProperty("keytrack", sample.keytrack);
    juce::MemoryOutputStream left, right;
    for (int i = 0; i < juce::jlimit(0, maxSampleFrames, sample.frames); ++i)
    {
        appendFloat(left, sample.left[static_cast<std::size_t>(i)]);
        if (sample.channels == 2) appendFloat(right, sample.right[static_cast<std::size_t>(i)]);
    }
    object->setProperty("left", juce::Base64::toBase64(left.getData(), left.getDataSize()));
    if (sample.channels == 2) object->setProperty("right", juce::Base64::toBase64(right.getData(), right.getDataSize()));
    return juce::var(object);
}
bool PresetImporter::decodeSample(const juce::var& value, std::shared_ptr<WaveTableBank>& bank, juce::String& error)
{
    const auto slot = get(value, "slot", -1), frames = get(value, "frames", -1), channels = get(value, "channels", -1);
    const auto rate = get(value, "sampleRate", -1);
    if (!value.isObject() || slot < 0 || slot >= oscillatorCount || std::floor(slot) != slot || frames < 1 || frames > maxSampleFrames
        || std::floor(frames) != frames || (channels != 1 && channels != 2) || rate < 8000 || rate > 192000)
    { error = "Invalid VANTA sample metadata."; return false; }
    juce::MemoryBlock left, right;
    const auto expected = static_cast<std::size_t>(frames) * 4u;
    const auto& l = property(value, "left"), &r = property(value, "right");
    if (!l.isString() || !strictBase64(l.toString(), left, maxSampleFrames * 4u) || left.getSize() != expected
        || (channels == 2 && (!r.isString() || !strictBase64(r.toString(), right, maxSampleFrames * 4u) || right.getSize() != expected)))
    { error = "Invalid VANTA sample data."; return false; }
    if (bank == nullptr) bank = std::make_shared<WaveTableBank>();
    auto& sample = bank->sample;
    sample.slot = static_cast<int>(slot); sample.frames = static_cast<int>(frames); sample.channels = static_cast<int>(channels);
    sample.sampleRate = static_cast<float>(rate); sample.loop = get(value, "loop", 0) >= 0.5; sample.keytrack = get(value, "keytrack", 0) >= 0.5;
    const auto* ldata = static_cast<const std::uint8_t*>(left.getData());
    const auto* rdata = static_cast<const std::uint8_t*>(right.getData());
    for (int i = 0; i < sample.frames; ++i)
    {
        const auto x = readFloat(ldata + i * 4), y = sample.channels == 2 ? readFloat(rdata + i * 4) : x;
        if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 1.0f || std::abs(y) > 1.0f)
        { error = "Invalid or unsafe VANTA sample values."; return false; }
        sample.left[static_cast<std::size_t>(i)] = x; sample.right[static_cast<std::size_t>(i)] = y;
    }
    return true;
}

juce::var PresetImporter::encodeModulation(const ImportedModulation& modulation)
{
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> lfos, routes;
    for (const auto& lfo : modulation.lfos)
    {
        auto* object = new juce::DynamicObject(); juce::MemoryOutputStream bytes;
        for (float point : lfo.shape) appendFloat(bytes, point);
        object->setProperty("rate", lfo.rateHz);
        object->setProperty("curve", juce::Base64::toBase64(bytes.getData(), bytes.getDataSize()));
        lfos.add(juce::var(object));
    }
    for (int i = 0; i < juce::jlimit(0, maxImportedRoutes, modulation.routeCount); ++i)
    {
        const auto& route = modulation.routes[static_cast<std::size_t>(i)];
        auto* object = new juce::DynamicObject();
        object->setProperty("source", route.source); object->setProperty("oscillator", route.oscillator);
        object->setProperty("target", static_cast<int>(route.target)); object->setProperty("amount", route.amount);
        object->setProperty("bipolar", route.bipolar); routes.add(juce::var(object));
    }
    root->setProperty("lfos", juce::var(lfos)); root->setProperty("routes", juce::var(routes));
    return juce::var(root);
}
bool PresetImporter::decodeModulation(const juce::var& value, std::shared_ptr<WaveTableBank>& bank, juce::String& error)
{
    const auto* lfos = property(value, "lfos").getArray(); const auto* routes = property(value, "routes").getArray();
    if (!value.isObject() || lfos == nullptr || lfos->size() != importedLfoCount || routes == nullptr || routes->size() > maxImportedRoutes)
    { error = "Invalid VANTA modulation structure."; return false; }
    if (bank == nullptr) bank = std::make_shared<WaveTableBank>();
    auto& modulation = bank->modulation;
    for (int i = 0; i < importedLfoCount; ++i)
    {
        const auto& encoded = lfos->getReference(i); const auto rate = get(encoded, "rate", -1);
        const auto& curve = property(encoded, "curve"); juce::MemoryBlock bytes;
        if (rate < static_cast<double>(0.01f) || rate > 40 || !curve.isString() || !strictBase64(curve.toString(), bytes, importedLfoPoints * 4u)
            || bytes.getSize() != importedLfoPoints * 4u)
        { error = "Invalid VANTA LFO curve."; return false; }
        auto& lfo = modulation.lfos[static_cast<std::size_t>(i)]; lfo.rateHz = static_cast<float>(rate);
        const auto* data = static_cast<const std::uint8_t*>(bytes.getData());
        for (int n = 0; n < importedLfoPoints; ++n)
        {
            const auto f = readFloat(data + n * 4);
            if (!std::isfinite(f) || f < 0 || f > 1) { error = "Invalid VANTA LFO point."; return false; }
            lfo.shape[static_cast<std::size_t>(n)] = f;
        }
    }
    modulation.routeCount = 0;
    for (const auto& encoded : *routes)
    {
        const auto source = get(encoded, "source", -1), osc = get(encoded, "oscillator", -1), target = get(encoded, "target", -1);
        const auto amount = get(encoded, "amount", 1000.0);
        if (source < 0 || source >= importedLfoCount || osc < 0 || osc >= oscillatorCount || target < 0 || target > 3
            || std::floor(source) != source || std::floor(osc) != osc || std::floor(target) != target
            || !addRoute(modulation, static_cast<int>(source), static_cast<int>(osc), static_cast<ModTarget>(static_cast<int>(target)), amount, get(encoded, "bipolar", 0) >= 0.5))
        { error = "Invalid VANTA modulation route."; return false; }
    }
    return true;
}

PresetResult PresetImporter::vital(const juce::var& root, const juce::String& filename)
{
    if (!root.isObject() || !property(root, "synth_version").isString()) return failure("This is not a recognised Vital preset.");
    const auto& settings = property(root, "settings");
    if (!settings.isObject()) return failure("This Vital preset has no settings object.");
    if (!has(settings, "env_1_attack") && !has(settings, "osc_1_on") && !has(settings, "wavetables"))
        return failure("This Vital preset has no recognised oscillator or envelope settings.");

    PresetResult result;
    result.name = nameOr(property(root, "preset_name"), filename.isNotEmpty() ? filename.substring(0, 80) : "Imported Vital");
    result.waveTables = std::make_shared<WaveTableBank>();
    juce::StringArray warnings;
    result.values[wave] = 1.0f; // audible saw fallback where a source table is unavailable
    result.values[cutoff] = parameterSpecs[cutoff].maxValue;
    result.values[resonance] = 0.0f;
    // Vital stores master dB as sqrt(value) - 80; convert to linear gain.
    const auto volume = get(settings, "volume", 5473.0404);
    if (volume < 0.0 || volume > 7399.4404) warn(warnings, "The source master volume was limited to its valid range.");
    const auto boundedVolume = juce::jlimit(0.0, 7399.4404, volume);
    set(result, level, boundedVolume == 0.0 ? 0.0 : std::pow(10.0, (std::sqrt(boundedVolume) - 80.0) / 20.0), warnings);
    for (const auto item : { std::pair<const char*, Parameter>{"env_1_delay", ampDelay}, {"env_1_attack", attack},
                             {"env_1_hold", ampHold}, {"env_1_decay", decay}, {"env_1_release", release} })
    {
        const auto n = get(settings, item.first, -1.0);
        if (n >= 0.0 && n <= 3.0) set(result, item.second, std::pow(n, 4.0), warnings);
    }
    set(result, sustain, get(settings, "env_1_sustain", 1.0), warnings);
    warn(warnings, "Envelope curve shapes use VANTA's linear ramps and may sound different.");
    if (get(settings, "filter_1_on", 0.0) >= 0.5)
    {
        const auto pitch = get(settings, "filter_1_cutoff", 120.0);
        if (pitch >= -128.0 && pitch <= 256.0) set(result, cutoff, 8.1757989156 * std::pow(2.0, pitch / 12.0), warnings);
        set(result, resonance, get(settings, "filter_1_resonance", 0.0) * 0.95, warnings);
        warn(warnings, "The source filter is approximated by VANTA's shared low-pass filter.");
    }
    if (get(settings, "filter_2_on", 0.0) >= 0.5) warn(warnings, "The second source filter is not imported.");

    const auto* tables = property(settings, "wavetables").getArray();
    for (int osc = 0; osc < 3; ++osc)
    {
        const auto prefix = "osc_" + juce::String(osc + 1) + "_";
        const bool enabled = get(settings, prefix + "on", osc == 0 ? 1.0 : 0.0) >= 0.5;
        result.values[oscillatorEnabled(osc)] = enabled ? 1.0f : 0.0f;
        set(result, oscillatorLevel(osc), std::pow(juce::jlimit(0.0, 1.0, get(settings, prefix + "level", 0.70710678119)), 2.0), warnings);
        set(result, oscillatorTune(osc), get(settings, prefix + "transpose", 0.0), warnings);
        set(result, oscillatorFine(osc), get(settings, prefix + "tune", 0.0) * 100.0, warnings);
        set(result, oscillatorPan(osc), get(settings, prefix + "pan", 0.0), warnings);
        // Vital uses 257 oscillator frames, indexed 0..256, including the endpoint.
        set(result, oscillatorTablePosition(osc), get(settings, prefix + "wave_frame", 0.0) / 256.0, warnings);
        if (get(settings, prefix + "distortion_type", 0.0) != 0.0 || get(settings, prefix + "spectral_morph_type", 0.0) != 0.0)
            warn(warnings, "Unsupported oscillator warp/spectral morph was disabled (clean wavetable)." );
        if (get(settings, prefix + "unison_voices", 1.0) > 1.0)
            warn(warnings, "Source oscillator unison is not imported.");
        if (!enabled) continue;

        const juce::Array<juce::var>* frames = nullptr;
        if (tables != nullptr && osc < tables->size())
        {
            const auto* groups = property(tables->getReference(osc), "groups").getArray();
            if (groups != nullptr)
                for (const auto& group : *groups)
                    if (const auto* components = property(group, "components").getArray())
                        for (const auto& component : *components)
                        {
                            const auto* candidate = property(component, "keyframes").getArray();
                            if (frames == nullptr && property(component, "type").toString() == "Wave Source" && candidate != nullptr && !candidate->isEmpty())
                                frames = candidate;
                            else if (candidate != nullptr && !candidate->isEmpty())
                                warn(warnings, "Additional wavetable generators/effects are not imported.");
                        }
        }
        if (frames == nullptr)
        {
            warn(warnings, "An oscillator's embedded wavetable was unavailable; a basic saw was substituted.");
            if (osc > 0) result.values[oscillatorWave(osc)] = 1.0f;
            continue;
        }
        if (frames->size() > 4096) return failure("The Vital wavetable contains too many frames.");
        auto& table = result.waveTables->tables[static_cast<std::size_t>(osc)];
        const int count = juce::jmin(frames->size(), maxWavetableFrames);
        if (frames->size() > count) warn(warnings, "Wavetables over 16 frames were reduced to 16 evenly spaced keyframes.");
        for (int frame = 0; frame < count; ++frame)
        {
            const int index = count == 1 ? 0 : static_cast<int>(std::lround(static_cast<double>(frame) * (frames->size() - 1) / (count - 1)));
            const auto& source = frames->getReference(index);
            const auto& encoded = property(source, "wave_data");
            juce::MemoryBlock bytes; juce::String error;
            const auto position = static_cast<float>(get(source, "position", index * 256.0 / juce::jmax(1, frames->size() - 1)) / 256.0);
            if (!encoded.isString() || !strictBase64(encoded.toString(), bytes, 65536)
                || (frame > 0 && position <= table.positions[static_cast<std::size_t>(frame - 1)])
                || !putFrame(table, frame, position, bytes.getData(), bytes.getSize(), error))
                return failure("Invalid Vital wavetable frame. " + error);
        }
        if (normalizeTable(table)) warn(warnings, "A wavetable was peak-normalized to avoid unsafe levels.");
        result.values[oscillatorTable(osc)] = 1.0f;
    }
    if (tables != nullptr && tables->size() > oscillatorCount)
        warn(warnings, "Oscillators beyond VANTA's eight slots were ignored.");
    if (get(settings, "sample_on", 0.0) >= 0.5)
    {
        const auto& source = property(settings, "sample");
        const auto length = get(source, "length", -1), rate = get(source, "sample_rate", -1);
        juce::MemoryBlock left, right;
        const auto& l = property(source, "samples"), &r = property(source, "samples_stereo");
        const bool stereo = r.isString() && r.toString().isNotEmpty();
        if (length < 1 || length > 8.0 * 1024.0 * 1024.0 || std::floor(length) != length || rate < 8000 || rate > 192000
            || !l.isString() || !strictBase64(l.toString(), left, 16u * 1024u * 1024u) || left.getSize() != static_cast<std::size_t>(length) * 2u
            || (stereo && (!strictBase64(r.toString(), right, 16u * 1024u * 1024u) || right.getSize() != left.getSize())))
            return failure("Invalid Vital sample data.");
        auto& pcm = result.waveTables->sample;
        pcm.slot = 3; pcm.frames = static_cast<int>(juce::jmin<double>(length, maxSampleFrames));
        pcm.channels = stereo ? 2 : 1; pcm.sampleRate = static_cast<float>(rate);
        pcm.loop = get(settings, "sample_loop", 1.0) >= 0.5; pcm.keytrack = get(settings, "sample_keytrack", 0.0) >= 0.5;
        const auto* a = static_cast<const std::uint8_t*>(left.getData());
        const auto* b = static_cast<const std::uint8_t*>(right.getData());
        for (int i = 0; i < pcm.frames; ++i)
        {
            const auto decode = [](const std::uint8_t* p)
            {
                const auto u = static_cast<unsigned>(p[0]) | (static_cast<unsigned>(p[1]) << 8);
                return static_cast<float>(u >= 32768 ? static_cast<int>(u) - 65536 : static_cast<int>(u)) / 32768.0f;
            };
            pcm.left[static_cast<std::size_t>(i)] = decode(a + i * 2);
            pcm.right[static_cast<std::size_t>(i)] = stereo ? decode(b + i * 2) : pcm.left[static_cast<std::size_t>(i)];
        }
        result.values[oscDEnabled] = 1.0f; result.values[oscDSample] = 1.0f;
        set(result, oscDLevel, std::pow(juce::jlimit(0.0, 1.0, get(settings, "sample_level", 0.70710678119)), 2.0), warnings);
        set(result, oscDTune, get(settings, "sample_transpose", 0.0), warnings);
        set(result, oscDFine, get(settings, "sample_tune", 0.0) * 100.0, warnings);
        set(result, oscDPan, get(settings, "sample_pan", 0.0), warnings);
        if (length > maxSampleFrames) warn(warnings, "The imported sample was truncated to 262,144 frames.");
        if (get(settings, "sample_bounce", 0.0) >= 0.5 || get(settings, "sample_random_phase", 0.0) >= 0.5)
            warn(warnings, "Sample bounce and random start are not imported.");
        warn(warnings, "The sample is mapped to OSC D; source routing and loop points may differ.");
    }
    if (const auto* mods = property(settings, "modulations").getArray())
    {
        auto& modulation = result.waveTables->modulation;
        for (auto& lfo : modulation.lfos) triangle(lfo);
        const auto* sourceLfos = property(settings, "lfos").getArray();
        bool initialized[importedLfoCount] {}; int active = 0, dropped = 0; bool overLimit = false;
        for (int index = 0; index < mods->size(); ++index)
        {
            const auto& mod = mods->getReference(index);
            const auto source = property(mod, "source").toString();
            const auto destination = property(mod, "destination").toString();
            if (source.isEmpty() || destination.isEmpty()) continue;
            ++active;
            int lfoIndex = -1;
            for (int i = 0; i < importedLfoCount; ++i) if (source == "lfo_" + juce::String(i + 1)) lfoIndex = i;
            if (source.startsWithIgnoreCase("lfo_"))
            {
                // Saturating digit parse avoids integer overflow on hostile names.
                const auto suffix = source.substring(4);
                int number = 0; bool digits = suffix.isNotEmpty();
                for (const auto ch : suffix)
                {
                    if (ch < '0' || ch > '9') { digits = false; break; }
                    number = juce::jmin(5, number * 10 + static_cast<int>(ch - '0'));
                }
                if (digits && number > 4) overLimit = true;
            }
            int osc = -1; ModTarget target = ModTarget::level; double scale = 1.0;
            for (int i = 0; i < 3; ++i)
            {
                const auto prefix = "osc_" + juce::String(i + 1) + "_";
                if (destination == prefix + "level") { osc = i; target = ModTarget::level; scale = 1.0; }
                else if (destination == prefix + "pan") { osc = i; target = ModTarget::pan; scale = 2.0; }
                else if (destination == prefix + "transpose") { osc = i; target = ModTarget::pitch; scale = 24.0; }
                else if (destination == prefix + "tune") { osc = i; target = ModTarget::pitch; scale = 2.0; }
                else if (destination == prefix + "wave_frame") { osc = i; target = ModTarget::tablePosition; scale = 1.0; }
            }
            if (lfoIndex < 0 || osc < 0) { ++dropped; continue; }
            const auto key = "modulation_" + juce::String(index + 1) + "_";
            const auto amount = juce::jlimit(-1.0, 1.0, get(settings, key + "amount", get(mod, "amount", 0.0))) * scale;
            if (!addRoute(modulation, lfoIndex, osc, target, amount, get(settings, key + "bipolar", 0.0) >= 0.5)) { ++dropped; continue; }
            if (!initialized[lfoIndex])
            {
                initialized[lfoIndex] = true;
                auto& lfo = modulation.lfos[static_cast<std::size_t>(lfoIndex)];
                const auto prefix = "lfo_" + juce::String(lfoIndex + 1) + "_";
                lfo.rateHz = safeLfoRate(std::pow(2.0, juce::jlimit(-7.0, 9.0, get(settings, prefix + "frequency", 1.0))));
                if (sourceLfos != nullptr && lfoIndex < sourceLfos->size())
                {
                    const auto& sourceLfo = sourceLfos->getReference(lfoIndex);
                    const auto* points = property(sourceLfo, "points").getArray();
                    const auto* powers = property(sourceLfo, "powers").getArray();
                    if (points != nullptr && !points->isEmpty())
                    {
                        if (points->size() % 2 != 0 || points->size() > 960) return failure("Invalid Vital LFO point list.");
                        std::vector<CurvePoint> curve;
                        for (int p = 0; p < points->size() / 2; ++p)
                            curve.push_back({ number(points->getReference(p * 2), -1), number(points->getReference(p * 2 + 1), -1),
                                              powers != nullptr && p < powers->size() ? number(powers->getReference(p), 0) : 0 });
                        if (!reconstruct(lfo, curve, false)) return failure("Invalid Vital LFO curve.");
                    }
                }
            }
        }
        if (modulation.routeCount > 0)
            warn(warnings, juce::String(modulation.routeCount) + " LFO-to-oscillator route(s) imported approximately; curve tension, tempo sync and trigger behavior may differ.");
        if (dropped > 0) warn(warnings, juce::String(dropped) + " unsupported modulation route(s) were not imported.");
        if (overLimit) warn(warnings, "Source LFOs beyond four are not imported.");
        juce::ignoreUnused(active);
    }
    if (get(settings, "compressor_on", 0.0) >= 0.5)
    {
        result.values[ottEnabled] = 1.0f;
        set(result, ottMix, get(settings, "compressor_mix", 1.0), warnings);
        set(result, ottDepth, 0.5, warnings);
        warn(warnings, "The source compressor is approximated by VANTA's three-band dynamics (+3 dB high band); exact thresholds and ratios differ.");
    }
    if (get(settings, "reverb_on", 0.0) >= 0.5 || get(settings, "delay_on", 0.0) >= 0.5 || get(settings, "distortion_on", 0.0) >= 0.5)
        warn(warnings, "Other source effects are not imported.");
    warn(warnings, "Imported wavetable playback is not band-limited; high notes may alias.");
    finish(result, "Vital", warnings);
    return result;
}

PresetResult PresetImporter::serum1(const void* raw, std::size_t size, const juce::String& filename)
{
    const auto* data = static_cast<const std::uint8_t*>(raw);
    if (data == nullptr || size < 68 || std::memcmp(data, "CcnK", 4) != 0 || std::memcmp(data + 8, "FPCh", 4) != 0
        || std::memcmp(data + 16, "XfsX", 4) != 0 || (be32(data + 4) != size && be32(data + 4) != size - 8)
        || be32(data + 56) != size - 60)
        return failure("Not a valid Serum 1 VST2 preset container.");
    const auto* chunk = data + 60;
    const auto chunkSize = size - 60;
    const auto firstLength = static_cast<std::size_t>(le32(data + size - 4));
    if (firstLength == 0 || firstLength > chunkSize - 4) return failure("Invalid Serum 1 compressed-stream trailer.");
    std::vector<std::uint8_t> state, extra;
    std::size_t consumed = 0;
    if (!inflateChecked(chunk, firstLength, 2u * 1024u * 1024u, state, consumed) || consumed != firstLength)
        return failure("Serum 1 state decompression failed or exceeded its limit.");
    const auto extraSize = chunkSize - firstLength - 4;
    if (extraSize > 0)
    {
        if (!inflateChecked(chunk + firstLength, extraSize, maxDecodedBytes, extra, consumed)
            || (extraSize - consumed != 0 && extraSize - consumed != 4))
            return failure("Serum 1 embedded-data stream is incomplete or invalid.");
    }
    if (state.size() != 20704 && state.size() != 28232 && state.size() != 33872 && state.size() != 172736)
        return failure("This Serum 1 state layout is not recognised safely.");
    auto parameter = [&](int index, double fallback) -> double
    {
        const std::size_t offset = index < 228 ? 0x3460u + static_cast<std::size_t>(index) * 4u
                                               : 0x4ae0u + static_cast<std::size_t>(index - 228) * 4u;
        const int maxTrusted = state.size() < 21000 ? 227 : state.size() < 33000 ? 272 : 314;
        if (index > maxTrusted || offset + 4 > state.size()) return fallback;
        const auto v = readFloat(state.data() + offset);
        return std::isfinite(v) && v >= 0.0f && v <= 1.0f ? v : fallback;
    };
    // Reject malformed values for the controls actually translated, rather than
    // accidentally treating corrupt bytes as plausible default parameters.
    const int required[] {0, 1, 2, 3, 4, 5, 9, 11, 14, 15, 16, 17, 18, 22, 24, 35, 36, 37, 38, 39, 140, 159, 212, 213, 214, 215, 216};
    for (const int id : required) if (parameter(id, -1.0) < 0.0) return failure("Invalid Serum 1 parameter data.");

    PresetResult result;
    result.name = filename.isNotEmpty() ? filename.substring(0, 80) : "Imported Serum";
    result.waveTables = std::make_shared<WaveTableBank>();
    juce::StringArray warnings;
    result.values[wave] = 1.0f;
    result.values[cutoff] = parameterSpecs[cutoff].maxValue;
    set(result, level, 0.8541468079 * std::pow(parameter(0, 0.7), 3.0), warnings);
    set(result, attack, 32.0 * std::pow(parameter(35, 0.11), 5.0), warnings);
    set(result, ampHold, 32.0 * std::pow(parameter(36, 0.0), 5.0), warnings);
    set(result, decay, 32.0 * std::pow(parameter(37, 0.5), 5.0), warnings);
    set(result, sustain, parameter(38, 1.0), warnings);
    set(result, release, 32.0 * std::pow(parameter(39, 0.215), 5.0), warnings);
    warn(warnings, "Envelope curve shapes use VANTA's linear ramps and may sound different.");
    if (parameter(216, 0.0) >= 0.5)
    {
        // This cutoff law is only a perceptual approximation, not a confirmed
        // reproduction of Serum's undocumented filter scale or topology.
        set(result, cutoff, 9.0 * std::pow(2100.0, parameter(45, 0.5)), warnings);
        set(result, resonance, 0.95 * parameter(46, 0.1), warnings);
        warn(warnings, "The source filter and cutoff scale are approximated by VANTA's shared low-pass filter.");
    }
    std::size_t dataOffset = 0;
    for (int osc = 0; osc < 2; ++osc)
    {
        const int first = osc == 0 ? 1 : 14;
        const bool enabled = parameter(212 + osc, osc == 0 ? 1.0 : 0.0) >= 0.5;
        result.values[oscillatorEnabled(osc)] = enabled ? 1.0f : 0.0f;
        const double vol = parameter(first, 0.75);
        set(result, oscillatorLevel(osc), vol * vol, warnings);
        set(result, oscillatorPan(osc), parameter(first + 1, 0.5) * 2.0 - 1.0, warnings);
        set(result, oscillatorTune(osc), std::round(parameter(first + 2, 0.5) * 8.0 - 4.0) * 12.0 + std::round(parameter(first + 3, 0.5) * 24.0 - 12.0), warnings);
        set(result, oscillatorFine(osc), parameter(first + 4, 0.5) * 200.0 - 100.0, warnings);
        set(result, oscillatorTablePosition(osc), parameter(first + 10, 0.0), warnings);
        if (osc > 0) result.values[oscillatorWave(osc)] = 1.0f;
        if (parameter(168 + osc, 0.0) > 0.001)
            warn(warnings, "Unsupported oscillator warp was disabled (clean wavetable).");
        if (parameter(first + 5, 0.0) > 0.07) warn(warnings, "Source oscillator unison is not imported.");
        const auto bytes = static_cast<std::size_t>(le32(state.data() + 0x4968 + static_cast<std::size_t>(osc) * 4));
        if (bytes > extra.size() - juce::jmin(dataOffset, extra.size()) || bytes % (wavetableFrameSize * 4u) != 0)
            return failure("Serum 1 embedded wavetable lengths are invalid.");
        const auto frameCount = bytes / (wavetableFrameSize * 4u);
        if (enabled && frameCount > 0)
        {
            auto& table = result.waveTables->tables[static_cast<std::size_t>(osc)];
            const int count = static_cast<int>(juce::jmin<std::size_t>(frameCount, maxWavetableFrames));
            if (frameCount > maxWavetableFrames) warn(warnings, "Wavetables over 16 frames were reduced to 16 evenly spaced frames.");
            for (int frame = 0; frame < count; ++frame)
            {
                const auto index = count == 1 ? 0u : static_cast<std::size_t>(std::llround(static_cast<double>(frame) * (frameCount - 1) / (count - 1)));
                juce::String error;
                if (!putFrame(table, frame, frameCount == 1 ? 0.0f : static_cast<float>(index) / static_cast<float>(frameCount - 1),
                              extra.data() + dataOffset + index * wavetableFrameSize * 4u, wavetableFrameSize * 4u, error))
                    return failure("Invalid Serum 1 embedded wavetable. " + error);
            }
            if (normalizeTable(table)) warn(warnings, "A wavetable was peak-normalized to avoid unsafe levels.");
            result.values[oscillatorTable(osc)] = 1.0f;
        }
        else if (enabled) warn(warnings, "A Serum wavetable was not embedded; a basic saw was substituted.");
        dataOffset += bytes;
    }
    if (parameter(214, 0.0) >= 0.5)
    {
        std::size_t noiseOffset = dataOffset;
        if (state.size() >= 28232)
        {
            const auto tuningBytes = static_cast<std::size_t>(le32(state.data() + 0x53e0));
            if (tuningBytes > extra.size() - juce::jmin(noiseOffset, extra.size())) return failure("Invalid Serum 1 tuning length.");
            noiseOffset += tuningBytes;
            const auto channels = le32(state.data() + 0x5540);
            const auto noiseBytes = static_cast<std::size_t>(le32(state.data() + 0x5544));
            if (noiseBytes > 0)
            {
                if ((channels != 1 && channels != 2) || noiseBytes % (channels * 4u) != 0
                    || noiseOffset > extra.size() || noiseBytes > extra.size() - noiseOffset)
                    return failure("Invalid Serum 1 embedded noise sample.");
                const auto frameCount = noiseBytes / (channels * 4u);
                if (frameCount == 0) return failure("Invalid Serum 1 noise sample length.");
                auto& pcm = result.waveTables->sample;
                pcm.slot = 3; pcm.channels = static_cast<int>(channels); pcm.frames = static_cast<int>(juce::jmin<std::size_t>(frameCount, maxSampleFrames));
                pcm.sampleRate = 44100.0f; pcm.loop = true; pcm.keytrack = false;
                float peak = 1.0f;
                for (int i = 0; i < pcm.frames; ++i)
                {
                    const auto left = readFloat(extra.data() + noiseOffset + static_cast<std::size_t>(i) * 4u);
                    const auto right = channels == 2 ? readFloat(extra.data() + noiseOffset + frameCount * 4u + static_cast<std::size_t>(i) * 4u) : left;
                    if (!std::isfinite(left) || !std::isfinite(right) || std::abs(left) > 1.0e6f || std::abs(right) > 1.0e6f)
                        return failure("Invalid Serum 1 noise sample values.");
                    pcm.left[static_cast<std::size_t>(i)] = left; pcm.right[static_cast<std::size_t>(i)] = right;
                    peak = juce::jmax(peak, juce::jmax(std::abs(left), std::abs(right)));
                }
                if (peak > 1.0f)
                {
                    for (int i = 0; i < pcm.frames; ++i) { pcm.left[static_cast<std::size_t>(i)] /= peak; pcm.right[static_cast<std::size_t>(i)] /= peak; }
                    warn(warnings, "The noise sample was peak-normalized to avoid unsafe levels.");
                }
                result.values[oscDEnabled] = 1.0f; result.values[oscDSample] = 1.0f;
                set(result, oscDLevel, std::pow(parameter(27, 0.25), 2.0), warnings);
                set(result, oscDPan, parameter(30, 0.5) * 2.0 - 1.0, warnings);
                if (frameCount > maxSampleFrames) warn(warnings, "The noise sample was truncated to 262,144 frames.");
                warn(warnings, "Embedded noise is mapped to OSC D at an assumed 44.1 kHz; loop and pitch settings may differ.");
            }
            else warn(warnings, "The Serum noise sample is external or unavailable and was not imported.");
        }
        else warn(warnings, "The Serum noise sample is external or unavailable and was not imported.");
    }
    if (parameter(215, 0.0) >= 0.5)
    {
        result.values[oscCEnabled] = 1.0f;
        set(result, oscCLevel, std::pow(parameter(33, 0.75), 2.0), warnings);
        set(result, oscCPan, parameter(34, 0.5) * 2.0 - 1.0, warnings);
        const int shape = static_cast<int>(std::lround(parameter(170, 0.0) * 5.0));
        result.values[oscCWave] = static_cast<float>(shape == 2 ? 3 : shape == 3 ? 1 : shape == 4 || shape == 5 ? 2 : 0);
        set(result, oscCTune, std::round(parameter(171, 0.5) * 8.0 - 4.0) * 12.0, warnings);
        if (shape == 1 || shape == 5) warn(warnings, "The sub oscillator waveform is approximated by a basic shape.");
    }
    auto& modulation = result.waveTables->modulation;
    for (auto& lfo : modulation.lfos) triangle(lfo);
    bool initialized[importedLfoCount] {};
    int dropped = 0; bool lfosBeyondFour = false;
    const int slots = state.size() >= 28232 ? 32 : 16;
    for (int slot = 0; slot < slots; ++slot)
    {
        const std::size_t base = slot < 16 ? static_cast<std::size_t>(slot * 40) : 0x50e0u + static_cast<std::size_t>((slot - 16) * 40);
        if (base + 40 > state.size()) break;
        const int source = state[base + 20] | (state[base + 21] << 8);
        const int aux = state[base + 22] | (state[base + 23] << 8);
        if (source == 0 && aux == 0) continue;
        if (state.size() >= 28232 && ((source >= 9 && source <= 12) || (aux >= 9 && aux <= 12))) lfosBeyondFour = true;
        const int dest = state[base + 26] | (state[base + 27] << 8);
        int osc = -1; ModTarget target = ModTarget::level; double scale = 1.0;
        for (int i = 0; i < 2; ++i)
        {
            const int first = i == 0 ? 1 : 14;
            if (dest == first) { osc = i; target = ModTarget::level; scale = 1.0; }
            else if (dest == first + 1) { osc = i; target = ModTarget::pan; scale = 2.0; }
            else if (dest == first + 2) { osc = i; target = ModTarget::pitch; scale = 48.0; }
            else if (dest == first + 3) { osc = i; target = ModTarget::pitch; scale = 24.0; }
            else if (dest == first + 4) { osc = i; target = ModTarget::pitch; scale = 2.0; }
            else if (dest == first + 10) { osc = i; target = ModTarget::tablePosition; scale = 1.0; }
        }
        const int lfoIndex = source - 5;
        const auto amount = readFloat(state.data() + base + 4), range = readFloat(state.data() + base + 8);
        const int mode = state[base + 28] | (state[base + 29] << 8);
        if (lfoIndex < 0 || lfoIndex >= importedLfoCount || osc < 0 || aux != 0 || mode == 2
            || !std::isfinite(amount) || !std::isfinite(range) || std::abs(amount) > 1.0f || range < 0.0f || range > 1.0f)
        { ++dropped; continue; }
        if (!addRoute(modulation, lfoIndex, osc, target, amount * range * scale, state[base + 12] != 0)) { ++dropped; continue; }
        if (!initialized[lfoIndex])
        {
            initialized[lfoIndex] = true;
            auto& lfo = modulation.lfos[static_cast<std::size_t>(lfoIndex)];
            lfo.rateHz = safeLfoRate(100.0 * std::pow(parameter(61 + lfoIndex, 0.5), 4.0));
            std::vector<CurvePoint> points;
            const bool modern = state.size() == 172736;
            const auto block = modern ? 0x84d8u + static_cast<std::size_t>(lfoIndex) * 0x2d28u : 0x280u;
            const auto tension = modern ? block + 8u : block + static_cast<std::size_t>(lfoIndex) * 520u;
            const auto xs = modern ? block + 0x0f08u : block + static_cast<std::size_t>(4 + lfoIndex) * 520u;
            const auto ys = modern ? block + 0x1e08u : block + static_cast<std::size_t>(8 + lfoIndex) * 520u;
            const int maxPoints = modern ? 480 : 65;
            for (int i = 0; i < maxPoints; ++i)
            {
                const auto offset = static_cast<std::size_t>(i) * 8u;
                if (ys + offset + 8 > state.size()) break;
                const auto x = leDouble(state.data() + xs + offset), y = leDouble(state.data() + ys + offset), p = leDouble(state.data() + tension + offset);
                points.push_back({ x, y, p });
                if (i > 0 && x >= 1.0) break;
            }
            if (!reconstruct(lfo, points, true)) warn(warnings, "An unreadable Serum LFO curve was approximated by a triangle.");
        }
    }
    if (modulation.routeCount > 0) warn(warnings, juce::String(modulation.routeCount) + " LFO-to-oscillator route(s) imported approximately; curve tension, tempo sync and trigger behavior may differ.");
    if (dropped > 0) warn(warnings, juce::String(dropped) + " unsupported modulation route(s) were not imported.");
    if (lfosBeyondFour) warn(warnings, "Source LFOs beyond four are not imported.");
    const auto sourceVersion = readFloat(state.data() + 0x4994);
    const bool compressorBypassed = std::isfinite(sourceVersion) && sourceVersion >= 0.05f && state[0x3976 + 68 * 5] != 0;
    if (!compressorBypassed && parameter(159, 0.0) >= 0.5 && parameter(140, 0.0) >= 0.5)
    {
        result.values[ottEnabled] = 1.0f;
        set(result, ottMix, parameter(269, 1.0), warnings);
        set(result, ottDepth, parameter(136, 0.5), warnings);
        warn(warnings, "The source multiband compressor is approximated by VANTA's three-band dynamics (+3 dB high band); exact settings differ.");
    }
    else if (!compressorBypassed && parameter(159, 0.0) >= 0.5) warn(warnings, "The source single-band compressor is not imported.");
    if (parameter(154, 0.0) >= 0.5 || parameter(158, 0.0) >= 0.5 || parameter(160, 0.0) >= 0.5)
        warn(warnings, "Other source effects are not imported.");
    warn(warnings, "Imported wavetable playback is not band-limited; high notes may alias.");
    finish(result, "Serum 1", warnings);
    return result;
}

PresetResult PresetImporter::serum2(const void* raw, std::size_t size, const juce::String& filename)
{
    const auto* data = static_cast<const std::uint8_t*>(raw);
    static constexpr char magic[] = { 'X','f','e','r','J','s','o','n','\0' };
    if (data == nullptr || size < 25 || std::memcmp(data, magic, sizeof(magic)) != 0)
        return failure("Not a Serum 2 XferJson container.");
    const auto metadataLength = le64(data + 9);
    if (metadataLength > 1024u * 1024u || metadataLength > size - 25)
        return failure("Invalid Serum 2 metadata length.");
    const auto payloadOffset = static_cast<std::size_t>(17 + metadataLength);
    juce::var metadata; juce::String error;
    if (!validUtf8(data + 17, static_cast<std::size_t>(metadataLength))) return failure("Serum 2 metadata contains invalid UTF-8.");
    if (!parseJson(juce::String::fromUTF8(reinterpret_cast<const char*>(data + 17), static_cast<int>(metadataLength)), metadata, error)
        || property(metadata, "fileType").toString() != "SerumPreset")
        return failure("Invalid Serum 2 metadata. " + error);
    const auto decodedSize = static_cast<std::size_t>(le32(data + payloadOffset));
    const auto encoding = le32(data + payloadOffset + 4);
    const auto* compressed = data + payloadOffset + 8;
    const auto compressedSize = size - payloadOffset - 8;
    if (encoding != 2 || decodedSize == 0 || decodedSize > maxDecodedBytes || compressedSize == 0)
        return failure("Unsupported or oversized Serum 2 payload.");
    const auto oneFrame = ZSTD_findFrameCompressedSize(compressed, compressedSize);
    if (ZSTD_isError(oneFrame) || oneFrame != compressedSize) return failure("Invalid or trailing Serum 2 zstd frame data.");
    std::vector<std::uint8_t> bytes(decodedSize);
    auto* context = ZSTD_createDCtx();
    if (context == nullptr) return failure("Unable to allocate a Serum 2 decoder.");
    const auto setting = ZSTD_DCtx_setParameter(context, ZSTD_d_windowLogMax, 26);
    const auto decoded = ZSTD_isError(setting) ? setting : ZSTD_decompressDCtx(context, bytes.data(), bytes.size(), compressed, compressedSize);
    ZSTD_freeDCtx(context);
    if (ZSTD_isError(decoded) || decoded != decodedSize) return failure("Serum 2 decompression failed or its declared size was wrong.");
    juce::var root;
    CborReader reader(bytes.data(), bytes.size());
    if (!reader.read(root) || !root.isObject() || !property(root, "Oscillator0").isObject() || !property(root, "Env0").isObject())
        return failure("Malformed or unsupported Serum 2 CBOR state.");

    PresetResult result;
    result.name = nameOr(property(metadata, "presetName"), filename.isNotEmpty() ? filename.substring(0, 80) : "Imported Serum 2");
    result.waveTables = std::make_shared<WaveTableBank>();
    juce::StringArray warnings;
    result.values[wave] = 1.0f;
    result.values[cutoff] = parameterSpecs[cutoff].maxValue;
    const auto& global = property(property(root, "Global0"), "plainParams");
    set(result, level, get(global, "kParamMasterVolume", 0.7), warnings);
    const auto& env = property(property(root, "Env0"), "plainParams");
    set(result, attack, get(env, "kParamAttack", 0.0005), warnings);
    set(result, ampHold, get(env, "kParamHold", 0.0), warnings);
    set(result, decay, get(env, "kParamDecay", 1.0), warnings);
    set(result, sustain, get(env, "kParamSustain", 1.0), warnings);
    set(result, release, get(env, "kParamRelease", 0.015), warnings);
    warn(warnings, "Envelope curve shapes use VANTA's linear ramps and may sound different.");
    const auto& filter = property(property(root, "VoiceFilter0"), "plainParams");
    if (get(filter, "kParamEnable", 0.0) >= 0.5)
    {
        set(result, cutoff, 9.0 * std::pow(2100.0, juce::jlimit(0.0, 1.0, get(filter, "kParamFreq", 0.5))), warnings);
        set(result, resonance, get(filter, "kParamReso", 10.0) * 0.0095, warnings);
        warn(warnings, "The source filter and cutoff scale are approximated by VANTA's shared low-pass filter.");
    }
    if (get(property(property(root, "VoiceFilter1"), "plainParams"), "kParamEnable", 0.0) >= 0.5)
        warn(warnings, "The second source filter is not imported.");
    for (int osc = 0; osc < oscillatorCount; ++osc)
    {
        const auto& container = property(root, "Oscillator" + juce::String(osc));
        const auto& params = property(container, "plainParams");
        const bool exists = container.isObject();
        const bool enabled = exists && get(params, "kParamEnable", osc == 0 ? 1.0 : 0.0) >= 0.5;
        result.values[oscillatorEnabled(osc)] = enabled ? 1.0f : 0.0f;
        if (!enabled) continue;
        set(result, oscillatorLevel(osc), get(params, "kParamVolume", 0.75), warnings);
        set(result, oscillatorTune(osc), get(params, "kParamOctave", 0.0) * 12.0
            + get(params, "kParamCoarsePit", 0.0) + get(params, "kParamPitch", 0.0), warnings);
        set(result, oscillatorFine(osc), get(params, "kParamFine", 0.0), warnings);
        set(result, oscillatorPan(osc), get(params, "kParamPan", 0.0) / 50.0, warnings);
        if (get(params, "kParamUnison", 1.0) > 1.0) warn(warnings, "Source oscillator unison is not imported.");
        if (osc == 3)
        {
            const auto& noise = property(container, "NoiseOsc3");
            const auto* channels = property(noise, "embeddedNoiseData").getArray();
            if (channels != nullptr && (channels->size() == 1 || channels->size() == 2))
            {
                const auto* left = channels->getReference(0).getArray();
                const auto* right = channels->size() == 2 ? channels->getReference(1).getArray() : nullptr;
                if (left == nullptr || left->isEmpty() || (channels->size() == 2 && (right == nullptr || right->size() != left->size())))
                    return failure("Invalid Serum 2 embedded noise sample.");
                auto& pcm = result.waveTables->sample;
                pcm.slot = osc; pcm.frames = juce::jmin(left->size(), maxSampleFrames); pcm.channels = channels->size();
                const auto rate = get(noise, "sampleRate", 44100.0);
                if (rate < 8000 || rate > 192000) return failure("Invalid Serum 2 sample rate.");
                pcm.sampleRate = static_cast<float>(rate);
                pcm.loop = get(property(noise, "plainParams"), "kParamOneShot", 0.0) < 0.5;
                pcm.keytrack = get(params, "kParamPitchTrack", 0.0) >= 0.5;
                float peak = 1.0f;
                for (int i = 0; i < pcm.frames; ++i)
                {
                    const auto x = number(left->getReference(i), 1.0e9);
                    const auto y = right != nullptr ? number(right->getReference(i), 1.0e9) : x;
                    if (std::abs(x) > 1.0e6 || std::abs(y) > 1.0e6) return failure("Invalid Serum 2 noise sample value.");
                    pcm.left[static_cast<std::size_t>(i)] = static_cast<float>(x); pcm.right[static_cast<std::size_t>(i)] = static_cast<float>(y);
                    peak = juce::jmax(peak, juce::jmax(std::abs(static_cast<float>(x)), std::abs(static_cast<float>(y))));
                }
                if (peak > 1.0f)
                {
                    for (int i = 0; i < pcm.frames; ++i) { pcm.left[static_cast<std::size_t>(i)] /= peak; pcm.right[static_cast<std::size_t>(i)] /= peak; }
                    warn(warnings, "The noise sample was peak-normalized to avoid unsafe levels.");
                }
                result.values[oscillatorSample(osc)] = 1.0f;
                if (left->size() > maxSampleFrames) warn(warnings, "The noise sample was truncated to 262,144 frames.");
                warn(warnings, "Embedded Serum 2 noise is mapped to OSC D; missing sample-rate metadata is assumed to be 44.1 kHz and loop/pitch may differ.");
            }
            else
            {
                result.values[oscillatorEnabled(osc)] = 0.0f;
                warn(warnings, "The Serum noise/sample oscillator is external or unavailable and was not imported.");
            }
            continue;
        }
        if (osc == 4)
        {
            const auto& sub = property(property(container, "SubOsc4"), "plainParams");
            const auto shape = property(sub, "kParamShape").toString();
            const int basicShape = shape == "kSquare" ? 2 : shape == "kTriangle" ? 3 : 1;
            result.values[oscillatorWave(osc)] = static_cast<float>(basicShape);
            if (shape == "kPulse" || shape == "kRoundRect") warn(warnings, "The sub oscillator waveform is approximated by a basic shape.");
            continue;
        }
        const auto type = property(params, "kParamType").toString();
        if (type.isNotEmpty() && type != "kOsc_WT")
        {
            result.values[oscillatorEnabled(osc)] = 0.0f;
            warn(warnings, "Sample, granular, spectral and multisample oscillator engines are not imported.");
            continue;
        }
        const auto& wt = property(container, "WTOsc" + juce::String(osc));
        const auto& wp = property(wt, "plainParams");
        set(result, oscillatorTablePosition(osc), (get(wp, "kParamTablePos", 1.0) - 1.0) / 255.0, warnings);
        if (get(wp, "kParamWarp", 0.0) != 0.0 || get(wp, "kParamWarp2", 0.0) != 0.0)
            warn(warnings, "Unsupported oscillator warp was disabled (clean wavetable).");
        const auto* embedded = property(wt, "embeddedWTData").getArray();
        if (embedded != nullptr && !embedded->isEmpty())
        {
            if (embedded->size() % wavetableFrameSize != 0) return failure("Invalid Serum 2 embedded wavetable length.");
            const int frameCount = embedded->size() / wavetableFrameSize;
            const int count = juce::jmin(frameCount, maxWavetableFrames);
            auto& table = result.waveTables->tables[static_cast<std::size_t>(osc)];
            if (frameCount > count) warn(warnings, "Wavetables over 16 frames were reduced to 16 evenly spaced frames.");
            for (int frame = 0; frame < count; ++frame)
            {
                const int index = count == 1 ? 0 : static_cast<int>(std::lround(static_cast<double>(frame) * (frameCount - 1) / (count - 1)));
                juce::MemoryOutputStream frameBytes;
                for (int i = 0; i < wavetableFrameSize; ++i)
                {
                    const auto value = number(embedded->getReference(index * wavetableFrameSize + i), 1.0e9);
                    if (std::abs(value) > 1.0e6) return failure("Invalid Serum 2 wavetable sample value.");
                    appendFloat(frameBytes, static_cast<float>(value));
                }
                juce::String frameError;
                if (!putFrame(table, frame, frameCount == 1 ? 0.0f : static_cast<float>(index) / static_cast<float>(frameCount - 1),
                              frameBytes.getData(), frameBytes.getDataSize(), frameError))
                    return failure("Invalid Serum 2 embedded wavetable. " + frameError);
            }
            if (normalizeTable(table)) warn(warnings, "A wavetable was peak-normalized to avoid unsafe levels.");
            result.values[oscillatorTable(osc)] = 1.0f;
            warn(warnings, "Imported wavetable playback is not band-limited; high notes may alias.");
        }
        else
        {
            const auto path = property(wt, "relativePathToWT").toString();
            if (path.isNotEmpty())
                warn(warnings, "A Serum 2 wavetable is an external reference and was not loaded (basic saw substituted): " + path.substring(0, 120));
            else warn(warnings, "A Serum 2 wavetable was unavailable; a basic saw was substituted.");
            if (osc > 0) result.values[oscillatorWave(osc)] = 1.0f;
        }
    }
    for (int osc = oscillatorCount; osc < 64; ++osc)
        if (property(root, "Oscillator" + juce::String(osc)).isObject())
        { warn(warnings, "Oscillators beyond VANTA's eight slots were ignored."); break; }

    auto& modulation = result.waveTables->modulation;
    for (auto& lfo : modulation.lfos) triangle(lfo);
    bool initialized[importedLfoCount] {}; int dropped = 0; bool lfosBeyondFour = false;
    for (int slot = 0; slot < 128; ++slot)
    {
        const auto& mod = property(root, "ModSlot" + juce::String(slot));
        const auto* source = property(mod, "source").getArray();
        if (source == nullptr || source->isEmpty()) continue;
        const double a = number(source->getReference(0), 0);
        const double b = source->size() > 1 ? number(source->getReference(1), 0) : 0;
        const auto type = property(mod, "destModuleTypeString").toString();
        if ((a == 0 && b == 0) || type.isEmpty()) continue;
        if ((a >= 10 && a <= 15) || (b >= 10 && b <= 15)) lfosBeyondFour = true;
        const double sourceIndex = a - 6;
        const double oscValue = get(mod, "destModuleID", -1);
        const auto dest = property(mod, "destModuleParamName").toString();
        ModTarget target = ModTarget::level; double scale = 1.0; bool supported = true;
        if (type == "Oscillator" && dest == "kParamVolume") { target = ModTarget::level; scale = 1.0; }
        else if (type == "Oscillator" && dest == "kParamPan") { target = ModTarget::pan; scale = 2.0; }
        else if (type == "Oscillator" && dest == "kParamOctave") { target = ModTarget::pitch; scale = 48.0; }
        else if (type == "Oscillator" && dest == "kParamPitch") { target = ModTarget::pitch; scale = 24.0; }
        else if (type == "Oscillator" && dest == "kParamFine") { target = ModTarget::pitch; scale = 2.0; }
        else if (type == "WTOsc" && dest == "kParamTablePos") { target = ModTarget::tablePosition; scale = 1.0; }
        else supported = false;
        const auto& routeParams = property(mod, "plainParams");
        if (!supported || sourceIndex < 0 || sourceIndex >= importedLfoCount || std::floor(sourceIndex) != sourceIndex
            || oscValue < 0 || oscValue >= oscillatorCount || std::floor(oscValue) != oscValue || b != 0 || get(routeParams, "kParamBypass", 0) >= 0.5)
        { ++dropped; continue; }
        const int lfoIndex = static_cast<int>(sourceIndex), osc = static_cast<int>(oscValue);
        const auto& sourceLfo = property(root, "LFO" + juce::String(lfoIndex));
        const auto& lp = property(sourceLfo, "plainParams");
        if (property(lp, "kParamType").toString().isNotEmpty()) { ++dropped; continue; }
        if (!initialized[lfoIndex])
        {
            initialized[lfoIndex] = true;
            auto& lfo = modulation.lfos[static_cast<std::size_t>(lfoIndex)];
            lfo.rateHz = safeLfoRate(get(lp, "kParamBeatSync", 1.0) >= 0.5 ? 2.0 : get(lp, "kParamRate", 6.25));
            const auto& curve = property(sourceLfo, "curveData");
            const auto* xs = property(curve, "xVals").getArray();
            const auto* ys = property(curve, "yVals").getArray();
            const auto* ps = property(curve, "curveVals").getArray();
            if (xs != nullptr || ys != nullptr || ps != nullptr)
            {
                if (xs == nullptr || ys == nullptr || ps == nullptr || xs->size() != ys->size() || xs->size() != ps->size() || xs->size() > 480)
                    return failure("Invalid Serum 2 LFO point list.");
                std::vector<CurvePoint> points;
                for (int i = 0; i < xs->size(); ++i)
                    points.push_back({ number(xs->getReference(i), -1), number(ys->getReference(i), -1), number(ps->getReference(i), 0.5) });
                if (!reconstruct(lfo, points, true)) return failure("Invalid Serum 2 LFO curve.");
            }
        }
        const double amount = juce::jlimit(-100.0, 100.0, get(routeParams, "kParamAmount", 0.0)) / 100.0 * scale;
        if (!addRoute(modulation, lfoIndex, osc, target, amount, get(routeParams, "kParamBipolar", 0.0) >= 0.5)) ++dropped;
    }
    if (modulation.routeCount > 0) warn(warnings, juce::String(modulation.routeCount) + " LFO-to-oscillator route(s) imported approximately; curve tension, tempo sync and trigger behavior may differ.");
    if (dropped > 0) warn(warnings, juce::String(dropped) + " unsupported modulation route(s) were not imported.");
    if (lfosBeyondFour) warn(warnings, "Source LFOs beyond four are not imported.");

    bool otherEffects = false, multiband = false; double mix = 1.0, depth = 0.5;
    for (int rack = 0; rack < 3; ++rack)
    {
        const auto* fx = property(property(root, "FXRack" + juce::String(rack)), "FX").getArray();
        if (fx == nullptr) continue;
        auto isActive = [](const juce::var& entry)
        {
            return get(entry, "bypassed", 0.0) < 0.5 && get(entry, "kParamBypass", 0.0) < 0.5;
        };
        for (int index = 0; index < fx->size(); ++index)
        {
            const auto& entry = fx->getReference(index);
            if (!isActive(entry)) continue;
            const auto type = get(entry, "type", -1);
            if (type == 5)
            {
                const auto& cp = property(property(entry, "FXComp"), "plainParams");
                if (get(cp, "kParamEnable", 1.0) < 0.5 || get(cp, "kParamBypass", 0.0) >= 0.5) continue;
                if (get(cp, "kParamMultiband", 0.0) >= 0.5 || get(cp, "kParamMultiBand", 0.0) >= 0.5)
                {
                    multiband = true;
                    mix = juce::jlimit(0.0, 1.0, get(cp, "kParamWet", 100.0) / 100.0);
                    depth = juce::jlimit(0.0, 1.0, (get(cp, "kParamRatio", 4.0) - 1.0) / 9.0);
                }
                else otherEffects = true;
            }
            else if (type == 14)
            {
                // A three-way split is only an OTT candidate if each declared
                // branch actually contains an active compressor. Three serial
                // compressors elsewhere in a rack must not trigger it.
                const auto& split = property(property(entry, "FXSplit3"), "plainParams");
                int offset = index + 1; bool allBands = true;
                double wetTotal = 0.0, depthTotal = 0.0;
                for (int band = 1; band <= 3; ++band)
                {
                    const auto count = get(split, "kParamModuleCount" + juce::String(band), 0);
                    if (count < 1 || count > 16 || std::floor(count) != count || offset + count > fx->size()) { allBands = false; break; }
                    bool found = false;
                    for (int j = 0; j < static_cast<int>(count); ++j)
                    {
                        const auto& child = fx->getReference(offset + j);
                        const auto& cp = property(property(child, "FXComp"), "plainParams");
                        if (get(child, "type", -1) == 5 && isActive(child) && get(cp, "kParamEnable", 1.0) >= 0.5 && get(cp, "kParamBypass", 0.0) < 0.5)
                        {
                            found = true; wetTotal += juce::jlimit(0.0, 1.0, get(cp, "kParamWet", 100.0) / 100.0);
                            depthTotal += juce::jlimit(0.0, 1.0, (get(cp, "kParamRatio", 4.0) - 1.0) / 9.0); break;
                        }
                    }
                    if (!found) { allBands = false; break; }
                    offset += static_cast<int>(count);
                }
                if (allBands) { multiband = true; mix = wetTotal / 3.0; depth = depthTotal / 3.0; }
                else otherEffects = true;
            }
            else otherEffects = true;
        }
    }
    if (multiband)
    {
        result.values[ottEnabled] = 1.0f; set(result, ottMix, mix, warnings); set(result, ottDepth, depth, warnings);
        warn(warnings, "The source multiband compressor is approximated by VANTA's three-band dynamics (+3 dB high band); exact settings/routing differ.");
    }
    if (otherEffects) warn(warnings, "Other source effects are not imported.");
    finish(result, "Serum 2", warnings);
    return result;
}
}
