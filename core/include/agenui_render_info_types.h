#pragma once
#include <string>

namespace agenui {

/**
 * @brief Component render info reported back from the platform layer.
 */
struct ComponentRenderInfo {
    std::string surfaceId;   // Component surface ID
    std::string componentId; // Component ID
    std::string type;        // Component type
    float width = 0.0f;      // Width, in a2ui logical units, i.e., pv * 2
    float height = 0.0f;     // Height, in a2ui logical units, i.e., pv * 2
    int selectedIndex = -1;  // Selected index passed on Tabs switch (-1 for non-Tabs switch events)
};

/**
 * @brief Surface layout info reported back from the platform layer.
 */
struct SurfaceLayoutInfo {
    std::string surfaceId;   // Surface ID
    float width = 0.0f;      // Width, in a2ui logical units, i.e., pv * 2
    float height = 0.0f;     // Height, in a2ui logical units, i.e., pv * 2
};

} // namespace agenui
