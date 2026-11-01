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
constexpr int resizePopupWidth = 440;
constexpr int resizePopupHeight = 228;

struct EditorPoint { int x, y; };
// On standalone startup, put at least the window's top-left on the current
// work area even when a saved size is too large to fit in full.
inline EditorPoint standaloneWindowPosition(int x, int y, int width, int height,
                                            int workX, int workY, int workWidth, int workHeight) noexcept
{
    if (width <= 0 || height <= 0 || workWidth <= 0 || workHeight <= 0) return { x, y };
    const auto rightmost = workX + std::max(0, workWidth - width);
    const auto bottommost = workY + std::max(0, workHeight - height);
    return { std::clamp(x, workX, rightmost), std::clamp(y, workY, bottommost) };
}
// Center on the visible portion of an oversized/off-screen editor, so the menu
// is still reachable after choosing a large size on a smaller display.
inline EditorPoint resizePopupPosition(int canvasWidth, int canvasHeight,
                                       int visibleX, int visibleY, int visibleWidth, int visibleHeight) noexcept
{
    const bool hasVisibleArea = visibleWidth > 0 && visibleHeight > 0;
    const int centreX = hasVisibleArea ? visibleX + visibleWidth / 2 : canvasWidth / 2;
    const int centreY = hasVisibleArea ? visibleY + visibleHeight / 2 : canvasHeight / 2;
    return { std::clamp(centreX - resizePopupWidth / 2, 0, std::max(0, canvasWidth - resizePopupWidth)),
             std::clamp(centreY - resizePopupHeight / 2, 0, std::max(0, canvasHeight - resizePopupHeight)) };
}

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
