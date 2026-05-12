#pragma once
#include "agenui_measurement.h"
#include <string>

namespace a2ui {

/**
 * @brief Slider component measurement implementation
 *
 * Reads slider-height and thumb-outer-diameter from device component styles (componentStyles["Slider"]),
 * returns intrinsic dimensions (synchronous).
 */
class SliderComponentMeasurement : public agenui::IMeasurement {
public:
    SliderComponentMeasurement() = default;

    agenui::MeasureResult measure(const std::string& paramJson,
                          const agenui::MeasureModes& modes) override;
};

}  // namespace a2ui
