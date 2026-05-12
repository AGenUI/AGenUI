#pragma once
#include "agenui_measurement.h"
#include <string>

namespace a2ui {

/**
 * @brief Divider component measurement implementation
 *
 * Sets thickness as fixed width or height based on axis direction (vertical / horizontal),
 * the other dimension is auto-allocated by Yoga (return 0), returns synchronous measurement result.
 */
class DividerComponentMeasurement : public agenui::IMeasurement {
public:
    DividerComponentMeasurement() = default;

    agenui::MeasureResult measure(const std::string& paramJson,
                          const agenui::MeasureModes& modes) override;
};

}  // namespace a2ui
