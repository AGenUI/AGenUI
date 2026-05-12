#pragma once
#include "agenui_measurement.h"
#include "hm_text_measure_utils.h"
#include "nlohmann/json.hpp"
#include <string>
#include <climits>

namespace a2ui {

/**
 * @brief Text component measurement implementation (Text / RichText)
 *
 * Parses text attributes from paramJson, directly calls TextMeasureUtils::doMeasure and returns dimensions.
 */
class TextComponentMeasurement : public agenui::IMeasurement {
public:
    TextComponentMeasurement() = default;

    agenui::MeasureResult measure(const std::string& paramJson,
                          const agenui::MeasureModes& modes) override;

    /**
     * @brief Parse json to construct TextMeasureParam (public static method, reusable by other measurement classes)
     *
     * @param json       JSON object from stringify() (attributes flattened at root level, styles in styles sub-object)
     * @param outText    Output: parsed text content
     * @param outParam   Output: constructed TextMeasureParam
     * @return true if parsing succeeds, false if text is empty
     */
    static bool buildParam(const nlohmann::json& json,
                           std::string& outText,
                           TextMeasureParam& outParam);
};

}  // namespace a2ui
