#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace laggify
{
constexpr int steps = 16;
constexpr int bankSize = 36;
using Curve = std::array<float, steps>;
struct Pattern
{
    Curve values{};
    bool smooth = false;
};

// The names follow the user's visual reference. These are independently made
// curves; they are not Gross Beat factory data and do not import its presets.
inline constexpr std::array<const char*, bankSize> timeNames{
    {"Empty",     "Basic 1",   "Basic 2",   "Basic 3",   "Basic 4",   "Basic 5",    "Basic 6",    "Basic 7",
     "Basic 8",   "Basic 9",   "Basic 10",  "Basic 11",  "Complex 1", "Complex 2",  "Complex 3",  "Complex 4",
     "Complex 5", "Complex 6", "Complex 7", "Complex 8", "Complex 9", "Complex 10", "Complex 11", "Complex 12",
     "Chaos 1",   "Chaos 2",   "Chaos 3",   "Chaos 4",   "Chaos 5",   "Chaos 6",    "Chaos 7",    "Chaos 8",
     "Chaos 9",   "Chaos 10",  "Chaos 11",  "Chaos 12"}};
inline constexpr std::array<const char*, bankSize> timeIds{
    {"live",   "half",   "slow-a",  "slow-b", "slow-c",   "slow-d",   "reverse",  "rev-a",    "rev-b",
     "rev-c",  "rev-d",  "stutter", "jump-a", "rept-a",   "rept-b",   "jump-b",   "rept-c",   "rept-d",
     "ramp-a", "ramp-b", "ramp-c",  "ramp-d", "bounce-a", "bounce-b", "bounce-c", "bounce-d", "jump-c",
     "jump-d", "warp-a", "warp-b",  "warp-c", "warp-d",   "slice-a",  "slice-b",  "slice-c",  "slice-d"}};
inline constexpr std::array<const char*, bankSize> volumeNames{
    {"Empty",       "2 Beat Gate", "1 Beat Gate",  "1/2 Bt Gate",  "1/3 Bt Gate", "1/4 Bt Gate",
     "1/6 Bt Gate", "1/8 Bt Gate", "Trance Gt 1",  "Trance Gt 2",  "Trance Gt 3", "Trance Gt 4",
     "Saw Gate 1",  "Saw Gate 2",  "1st Step",     "1st Stp Fade", "2 Step Gate", "Off Beat",
     "1/4 Dynamic", "1/4 Swing",   "1/4 Gate Out", "1/4 Gate In",  "Spd Up Gate", "Slw Dn Gate",
     "End Fade",    "End Gate",    "Tremelo Slw",  "Tremelo Fst",  "Sidechain",   "Drum Loop",
     "Copter",      "AM",          "Fade In",      "Fade Out",     "Fade Out In", "Mute"}};
inline constexpr std::array<const char*, bankSize> volumeIds{
    {"open",   "gate-d", "gate-b",    "gate-a",  "gate-third", "gate",       "pulse-a",    "pulse-b",     "chop-a",
     "chop-b", "chop-c", "chop-d",    "pump-a",  "pump-b",     "first-step", "first-fade", "pulse-c",     "off-beat",
     "duck-a", "chop",   "gate-out",  "gate-in", "speed-up",   "slow-down",  "end-fade",   "end-gate",    "trem-a",
     "trem-d", "pump",   "drum-loop", "trem-c",  "trem-b",     "fade-a",     "fade-b",     "fade-out-in", "mute"}};
inline const char* patternName(bool time, int index) noexcept
{
    return index >= 0 && index < bankSize ? (time ? timeNames : volumeNames)[static_cast<std::size_t>(index)]
                                          : "CUSTOM";
}
inline Pattern makePattern(bool time, int index) noexcept
{
    index = std::clamp(index, 0, bankSize - 1);
    const std::string_view id = (time ? timeIds : volumeIds)[static_cast<std::size_t>(index)];
    const auto dash = id.find('-');
    const auto family = id.substr(0, dash);
    const int variant =
        dash != std::string_view::npos && dash + 2 == id.size() && id[dash + 1] >= 'a' && id[dash + 1] <= 'd'
            ? id[dash + 1] - 'a'
            : 0;
    const auto clamp = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    constexpr float pi = 3.14159265358979323846f;
    constexpr float jumps[4][steps] = {
        {0, 0, 0, .18f, .18f, .18f, .08f, .08f, .30f, .30f, .30f, .16f, .16f, .26f, .26f, 0},
        {0, 0, 0, 0, .14f, .14f, .14f, .10f, .10f, .10f, .32f, .32f, .25f, .29f, .32f, 0},
        {0, .22f, .22f, .22f, .10f, .10f, .36f, .36f, .36f, .18f, .18f, .28f, .28f, .40f, .40f, .10f},
        {0, 0, .12f, .12f, .28f, .28f, .16f, .16f, .42f, .42f, .25f, .25f, .35f, .35f, .12f, .12f}};
    constexpr int speedUp[steps] = {1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1};
    constexpr int slowDown[steps] = {1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1};
    constexpr int drum[steps] = {1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1};
    Pattern p;
    p.smooth = time ? !(family == "stutter" || family == "rept" || family == "jump" || family == "slice")
                    : (family == "pump" || family == "fade" || family == "swell" || family == "trem" ||
                       id == "first-fade" || id == "end-fade");
    for (int s = 0; s < steps; ++s)
    {
        const float f = static_cast<float>(s) / 15.0f;
        float v = time ? 0.0f : 1.0f;
        if (time)
        {
            if (id == "half")
                v = static_cast<float>(s) / 32.0f;
            else if (id == "reverse")
                v = static_cast<float>(s % 8) / 8.0f;
            else if (id == "stutter")
                v = static_cast<float>(s / 4) / 4.0f;
            else if (family == "slow")
            {
                constexpr float m[] = {.125f, .25f, .375f, .75f};
                v = f * m[variant];
            }
            else if (family == "rev")
            {
                constexpr int m[] = {2, 4, 6, 16};
                v = static_cast<float>(s % m[variant]) / m[variant];
            }
            else if (family == "rept")
            {
                constexpr int m[] = {8, 3, 2, 1};
                v = static_cast<float>((s / m[variant]) * m[variant]) / 16.0f;
            }
            else if (family == "ramp")
            {
                constexpr float m[] = {.5f, 1.5f, 2, 3};
                v = .8f * std::pow(f, m[variant]);
            }
            else if (family == "bounce")
            {
                constexpr float m[] = {.4f, .5f, .7f, .9f};
                v = .5f * (1.0f - std::cos(2.0f * pi * f * (variant + 1))) * m[variant];
            }
            else if (family == "jump")
                v = jumps[variant][s];
            else if (family == "warp")
                v = clamp(.6f * f + .12f * std::sin(2.0f * pi * f * (variant + 1)) + .04f * variant);
            else if (family == "slice")
            {
                constexpr int m[] = {2, 4, 8, 16};
                const int period = m[variant];
                v = clamp(static_cast<float>((s / period) * period) / 16.0f +
                          .35f * static_cast<float>(s % period) / period);
            }
        }
        else
        {
            if (id == "mute")
                v = 0;
            else if (id == "gate-third")
                v = (s * 3 % 8) < 4 ? 1.0f : 0.0f;
            else if (id == "gate-out")
                v = s % 4 < 2 ? 1.0f - f : 0.0f;
            else if (id == "gate-in")
                v = s % 4 < 2 ? f : 0.0f;
            else if (id == "first-step")
                v = s == 0 ? 1.0f : 0.0f;
            else if (id == "first-fade")
                v = clamp(1.0f - static_cast<float>(s) / 4.0f);
            else if (id == "off-beat")
                v = s % 4 >= 2 ? 1.0f : 0.0f;
            else if (id == "speed-up")
                v = static_cast<float>(speedUp[s]);
            else if (id == "slow-down")
                v = static_cast<float>(slowDown[s]);
            else if (id == "end-fade")
                v = s < 12 ? 1.0f : static_cast<float>(15 - s) / 3.0f;
            else if (id == "end-gate")
                v = s < 12 ? 1.0f : 0.0f;
            else if (id == "drum-loop")
                v = static_cast<float>(drum[s]);
            else if (id == "fade-out-in")
                v = std::abs(2.0f * f - 1.0f);
            else if (id == "gate")
                v = s % 2 == 0 ? 1.0f : .1f;
            else if (id == "pump")
            {
                const float phase = static_cast<float>(s % 4) / 3.0f;
                v = .08f + .92f * phase * phase;
            }
            else if (id == "chop")
                v = (s % 8 == 2 || s % 8 == 4 || s % 8 == 7) ? 0.0f : 1.0f;
            else if (family == "gate")
            {
                constexpr int m[] = {2, 4, 6, 8};
                const int period = m[variant];
                v = s % period < (period + 1) / 2 ? 1.0f : 0.0f;
            }
            else if (family == "pump")
            {
                constexpr int m[] = {2, 4, 8, 16};
                const int period = m[variant];
                v = .12f + .88f * static_cast<float>(s % period) / (period - 1);
            }
            else if (family == "fade")
            {
                const float vals[] = {f, 1.0f - f, f * f, (1.0f - f) * (1.0f - f)};
                v = vals[variant];
            }
            else if (family == "pulse")
            {
                constexpr int period[] = {4, 4, 8, 8}, width[] = {1, 2, 2, 4};
                v = s % period[variant] < width[variant] ? 1.0f : .08f;
            }
            else if (family == "chop")
            {
                constexpr unsigned m[] = {0xf0f0, 0xa5a5, 0xeeee, 0x9090};
                v = static_cast<float>((m[variant] >> s) & 1u);
            }
            else if (family == "duck")
            {
                constexpr int m[] = {4, 4, 8, 8};
                v = s % m[variant] <= variant % 2 ? .15f : 1.0f;
            }
            else if (family == "trem")
                v = .55f + .45f * std::cos(2.0f * pi * f * (variant + 1));
        }
        p.values[static_cast<std::size_t>(s)] = clamp(v);
    }
    return p;
}
} // namespace laggify
