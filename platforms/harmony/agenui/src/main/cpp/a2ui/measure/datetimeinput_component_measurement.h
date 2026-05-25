#pragma once
#include "agenui_measurement.h"
#include <string>

namespace a2ui {

/**
 * @brief DateTimeInput component measurement implementation
 *
 * Reads height, font, icon dimensions from device component styles (componentStyles["DateTimeInput"]["compact"]),
 * measures display text width via TextMeasureUtils,
 * returns synchronous measurement result.
 */
class DateTimeInputComponentMeasurement : public agenui::IMeasurement {
public:
    DateTimeInputComponentMeasurement() = default;

    agenui::MeasureResult measure(const std::string& paramJson,
                          const agenui::MeasureModes& modes) override;
};

}  // namespace a2ui
