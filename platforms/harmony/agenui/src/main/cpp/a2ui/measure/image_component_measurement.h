#pragma once
#include "agenui_measurement.h"
#include <string>

namespace a2ui {

/**
 * @brief Image component measurement implementation (Image / Icon)
 *
 * Reads pre-saved width/height from styleInfo in paramJson,
 * returns style dimensions directly (synchronous).
 *
 * Note: saveImageStyleInfo() must be called before convertToYoga,
 * serializing styles["width/height"] into layout.styleInfo,
 * then injected into the "styleInfo" field of paramJson by buildParamJson.
 */
class ImageComponentMeasurement : public agenui::IMeasurement {
public:
    ImageComponentMeasurement() = default;

    agenui::MeasureResult measure(const std::string& paramJson,
                          const agenui::MeasureModes& modes) override;
};

}  // namespace a2ui
