#pragma once

#include "text_component_measurement.h"
#include "hm_text_measure_utils.h"
#include "a2ui/third_party/key_define.h"
#include "nlohmann/json.hpp"
#include <string>
#include <cstdlib>

namespace a2ui {

/**
 * @brief Common style attribute parsing utility: reads float attribute from json object
 * Supports both number and string formats; keeps out unchanged if not present
 */
inline void parseStyleFloat(const nlohmann::json& styles, const char* key, float& out) {
    if (!styles.contains(key)) return;
    const auto& v = styles[key];
    if (v.is_number()) { out = v.get<float>(); return; }
    if (v.is_string()) { out = static_cast<float>(std::atof(v.get<std::string>().c_str())); }
}

/**
 * @brief Text measurement utility class
 */
class TextMeasurementUtils {
public:
    /**
     * @brief Build simple measurement parameters (no JSON parsing needed)
     */
    static void buildSimpleParam(const std::string& text,
                                 float fontSize,
                                 TextMeasureParam& outParam,
                                 int fontWeight = NODE_PROPERTY_FONT_NORMAL);

    /**
     * @brief Measure text height (width mode AtMost, height mode Undefined)
     */
    static float measureTextHeight(const std::string& text,
                                   float maxWidth,
                                   float fontSize = 0.0f,
                                   int fontWeight = 0);

    /**
     * @brief Convenience interface: directly measure height from TextMeasureParam
     * Wraps baseLine/ascent/descent temporary variables to reduce caller boilerplate
     */
    static float doMeasureHeight(const TextMeasureParam& param,
                                 float maxWidth,
                                 MeasureMode widthMode = MeasureModeAtMost);
};

}  // namespace a2ui
