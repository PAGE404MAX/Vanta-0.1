#pragma once

#include <algorithm>
#include <cmath>

namespace vanta
{
constexpr int editorWidth = 1000;
constexpr int vstEditorHeight = 720;
constexpr int standaloneEditorHeight = 820;
constexpr int minEditorScale = 60;
constexpr int maxEditorScale = 150;
constexpr int defaultEditorScale = 100;

// Work area and editor bounds are logical pixels. hostScale represents the
// additional transform the host applies to the editor (not the user's scale).
inline int fitEditorScaleToWorkArea(int baseHeight, int workWidth, int workHeight, double hostScale) noexcept
{
    if (baseHeight <= 0 || workWidth <= 0 || workHeight <= 0 || !std::isfinite(hostScale) || hostScale <= 0.0)
        return defaultEditorScale;
    // Reserve room for a title bar, host wrapper and screen edges. The OS work
    // area already excludes the taskbar. At the 60% minimum a tiny screen may
    // still be too small to show the entire interface.
    const auto availableWidth = std::max(1.0, static_cast<double>(workWidth) - 32.0);
    const auto availableHeight = std::max(1.0, static_cast<double>(workHeight) - 96.0);
    const auto fit = std::min(availableWidth / (editorWidth * hostScale), availableHeight / (baseHeight * hostScale));
    const auto bounded = std::clamp(fit, minEditorScale / 100.0, maxEditorScale / 100.0);
    return std::clamp(static_cast<int>(std::floor(bounded * 20.0)) * 5, minEditorScale, maxEditorScale);
}
}
