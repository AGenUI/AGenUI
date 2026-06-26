#include "slider_component_measurement.h"
#include "a2ui_platform_layout_bridge.h"
#include "a2ui/utils/a2ui_measure_mode.h"
#include <algorithm>

namespace a2ui {

agenui::MeasureResult SliderComponentMeasurement::measure(const std::string& /*paramJson*/,
                                                    const agenui::MeasureModes& modes) {

    const nlohmann::json& sliderStyles = ::a2ui::getComponentStylesFor("Slider");

    float sliderHeight       = 48.0f;
    float thumbOuterDiameter = sliderHeight;
    parseStyleFloat(sliderStyles, "slider-height",        sliderHeight);
    parseStyleFloat(sliderStyles, "thumb-outer-diameter", thumbOuterDiameter);

    float measuredWidth = 0.0f;
    if ((modes.width.mode == kModeExactly || modes.width.mode == kModeAtMost) &&
         modes.width.maxValue > 0.0f) {
        measuredWidth = modes.width.maxValue;
    }

    float measuredHeight = std::max(sliderHeight, thumbOuterDiameter);
    if ((modes.height.mode == kModeExactly || modes.height.mode == kModeAtMost) && modes.height.maxValue > 0.0f) {
        measuredHeight = modes.height.mode == kModeAtMost
            ? std::min(measuredHeight, modes.height.maxValue)
            : modes.height.maxValue;
    }

    return agenui::MeasureResult{agenui::CalcType::Sync, measuredWidth, measuredHeight, 0};
}

}  // namespace a2ui
