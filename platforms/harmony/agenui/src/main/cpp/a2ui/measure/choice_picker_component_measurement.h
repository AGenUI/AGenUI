#pragma once

#include "agenui_measurement.h"
#include "text_component_measurement.h"
#include <string>

namespace a2ui {

/**
 * @brief ChoicePicker component measurement class
 *
 * Does not depend on Yoga; directly within measure():
 *  1. Parse options array
 *  2. Measure each item text size using TextMeasurementUtils::measureTextHeight
 *  3. Accumulate or take max based on orientation (horizontal/vertical) to get total height
 */
class ChoicePickerComponentMeasurement : public agenui::IMeasurement {
public:
    ChoicePickerComponentMeasurement() = default;
    ~ChoicePickerComponentMeasurement() = default;

    // IMeasurement
    agenui::MeasureResult measure(const std::string& paramJson,
                          const agenui::MeasureModes& modes) override;

private:
};

}  // namespace a2ui
