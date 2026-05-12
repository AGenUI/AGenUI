#include "divider_component_measurement.h"
#include "nlohmann/json.hpp"
#include <cstdlib>
#include <string>

namespace a2ui {

agenui::MeasureResult DividerComponentMeasurement::measure(
        const std::string& paramJson,
        const agenui::MeasureModes& modes) {

    // ---- Parse thickness and axis ----
    float thicknessPx = 1.0f;
    std::string axis  = "vertical";  // Default to vertical divider

    nlohmann::json j = nlohmann::json::parse(paramJson, nullptr, false);
    if (!j.is_discarded()) {
        // stringify() flattens attributes to root level without "attrs" wrapper
        auto parseThickness = [&](const nlohmann::json& node) {
            if (node.contains("thickness")) {
                const auto& v = node["thickness"];
                if (v.is_number()) thicknessPx = v.get<float>();
                else if (v.is_string()) thicknessPx = static_cast<float>(std::atof(v.get<std::string>().c_str()));
            }
        };
        parseThickness(j);  // thickness is at root level
    
        // axis is in styles sub-object
        if (j.contains("styles") && j["styles"].is_object()) {
            const auto& styles = j["styles"];
            if (styles.contains("axis") && styles["axis"].is_string()) {
                axis = styles["axis"].get<std::string>();
            }
        }
    }

    // ---- Calculate dimensions based on axis ----
    // vertical (vertical divider): thickness as width, height fills (return 0 for Yoga to allocate)
    // horizontal (horizontal divider): thickness as height, width fills
    float measuredWidth  = 0.0f;
    float measuredHeight = 0.0f;

    if (axis == "vertical") {
        measuredWidth  = thicknessPx;
        // Height: prefer Exactly/AtMost constraints, otherwise return 0 (Yoga fills parent)
        if (modes.height.mode == 1 /*Exactly*/) {
            measuredHeight = modes.height.maxValue;
        } else if (modes.height.mode == 2 /*AtMost*/ && modes.height.maxValue > 0.0f) {
            measuredHeight = modes.height.maxValue;
        }
    } else {
        measuredHeight = thicknessPx;
        // Width: prefer Exactly/AtMost constraints, otherwise return 0 (Yoga fills)
        if (modes.width.mode == 1 /*Exactly*/) {
            measuredWidth = modes.width.maxValue;
        } else if (modes.width.mode == 2 /*AtMost*/ && modes.width.maxValue > 0.0f) {
            measuredWidth = modes.width.maxValue;
        }
    }

    return {agenui::CalcType::Sync, measuredWidth, measuredHeight, 0};
}

}  // namespace a2ui
