#include "audioplayer_component_measurement.h"
#include "a2ui_platform_layout_bridge.h"
#include "a2ui/utils/a2ui_measure_mode.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cstdlib>

namespace a2ui {

agenui::MeasureResult AudioPlayerComponentMeasurement::measure(
        const std::string& /*paramJson*/,
        const agenui::MeasureModes& modes) {

    // Read default dimensions from device component styles
    float defaultSize = 300.0f;
    const nlohmann::json& apStyles = ::a2ui::getComponentStylesFor("AudioPlayer");
    parseStyleFloat(apStyles, "size", defaultSize);

    float measuredWidth  = defaultSize;
    float measuredHeight = defaultSize;

    // Constrain to modes
    if ((modes.width.mode == kModeExactly || modes.width.mode == kModeAtMost) &&
         modes.width.maxValue > 0.0f) {
        measuredWidth = modes.width.mode == kModeAtMost
            ? std::min(measuredWidth, modes.width.maxValue)
            : modes.width.maxValue;
    }
    if ((modes.height.mode == kModeExactly || modes.height.mode == kModeAtMost) && modes.height.maxValue > 0.0f) {
        measuredHeight = modes.height.mode == kModeAtMost
            ? std::min(measuredHeight, modes.height.maxValue)
            : modes.height.maxValue;
    }

    return {agenui::CalcType::Sync, measuredWidth, measuredHeight, 0};
}

}  // namespace a2ui
