#pragma once
#include "agenui_measurement.h"
#include <string>

namespace a2ui {

/**
 * @brief AudioPlayer component measurement implementation
 *
 * Reads default dimensions from device component styles (componentStyles["AudioPlayer"]["size"]),
 * returns synchronous measurement result (both width and height are size).
 */
class AudioPlayerComponentMeasurement : public agenui::IMeasurement {
public:
    AudioPlayerComponentMeasurement() = default;

    agenui::MeasureResult measure(const std::string& paramJson,
                          const agenui::MeasureModes& modes) override;
};

}  // namespace a2ui
