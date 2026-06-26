#include "image_component_measurement.h"

namespace a2ui {

/**
 * Image measurement: returns Yoga's parent constraints directly.
 *
 * Yoga calls this only when it cannot resolve Image size from styles alone
 * (at least one axis is auto). The modes encode everything needed:
 *   EXACTLY (1) — size determined by parent/styles, echo it back
 *   AT_MOST (2) — parent gives upper bound, use as placeholder for loader downsampling
 *   UNDEFINED (0) — no constraint, return 0 (unknown until image loads)
 *
 * After image loads, the platform reports intrinsic size via notifyRenderFinish,
 * triggering a Yoga re-layout with the real dimensions.
 */
agenui::MeasureResult ImageComponentMeasurement::measure(const std::string& /*paramJson*/,
                                                   const agenui::MeasureModes& modes) {
    float w = (modes.width.mode == 1 || modes.width.mode == 2) && modes.width.maxValue > 0.0f
            ? modes.width.maxValue : 0.0f;
    float h = (modes.height.mode == 1 || modes.height.mode == 2) && modes.height.maxValue > 0.0f
            ? modes.height.maxValue : 0.0f;
    return agenui::MeasureResult{agenui::CalcType::Sync, w, h, 0};
}

}  // namespace a2ui
