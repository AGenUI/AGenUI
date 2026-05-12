#pragma once

#include "agenui_measurement.h"
#include "text_component_measurement.h"
#include <string>

namespace a2ui {

/**
 * @brief CheckBox component measurement class
 *
 * Does not depend on Yoga; directly within measure():
 *  1. Measure text height using TextMeasurementUtils::measureTextHeight
 *  2. Return (maxWidth, max(checkboxSize, textHeight))
 */
class CheckBoxComponentMeasurement : public agenui::IMeasurement {
public:
    CheckBoxComponentMeasurement() = default;
    ~CheckBoxComponentMeasurement() = default;

    // IMeasurement
    agenui::MeasureResult measure(const std::string& paramJson,
                          const agenui::MeasureModes& modes) override;

private:
};

}  // namespace a2ui
