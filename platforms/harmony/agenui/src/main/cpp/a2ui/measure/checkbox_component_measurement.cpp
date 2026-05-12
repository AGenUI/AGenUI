#include "checkbox_component_measurement.h"
#include "a2ui_platform_layout_bridge.h"
#include "text_measurement_utils.h"

#include "nlohmann/json.hpp"
#include <algorithm>

namespace a2ui {

// ==================== IMeasurement::measure ====================

agenui::MeasureResult CheckBoxComponentMeasurement::measure(
        const std::string& paramJson,
        const agenui::MeasureModes& modes) {

    float maxWidth    = modes.width.maxValue > 0.0f ? modes.width.maxValue : 720.0f;
    float checkboxSize = 32.0f;
    float textMargin   = 16.0f;
    float textFontSize = 32.0f;

    // Read default styles from g_component_styles
    const nlohmann::json& cbStyles = ::a2ui::getComponentStylesFor("CheckBox");
    if (cbStyles.is_object()) {
        parseStyleFloat(cbStyles, "checkbox-size", checkboxSize);
        parseStyleFloat(cbStyles, "text-margin",   textMargin);
        parseStyleFloat(cbStyles, "text-size",     textFontSize);
    }

    // Parse text content
    // stringify() flattens attributes to root level without "attrs" wrapper
    std::string text;
    nlohmann::json j = nlohmann::json::parse(paramJson, nullptr, false);
    if (!j.is_discarded()) {
        if (j.contains("text")) {
            const auto& tv = j["text"];
            text = tv.is_string() ? tv.get<std::string>() : tv.dump();
        }
        if (j.contains("styles")) {
            const auto& styles = j["styles"];
            if (styles.contains("font-size") && styles["font-size"].is_number()) {
                textFontSize = styles["font-size"].get<float>();
            }
        }
    }

    // Measure text height
    float textHeight = checkboxSize;  // fallback
    if (!text.empty()) {
        float textAvailWidth = maxWidth - checkboxSize - textMargin;
        if (textAvailWidth < 1.0f) textAvailWidth = 1.0f;
        textHeight = TextMeasurementUtils::measureTextHeight(text, textAvailWidth, textFontSize);
    }

    float totalHeight = std::max(checkboxSize, textHeight);
    return {agenui::CalcType::Sync, maxWidth, totalHeight, 0};
}

}  // namespace a2ui
