#pragma once

#include <algorithm>

#include "text_measurement_utils.h"
#include "../utils/a2ui_parse_utils.h"

namespace a2ui {

constexpr float kFormLabelFontSize = 32.0f;
constexpr float kFormLabelSpacing = 16.0f;

inline std::string formLabelText(const nlohmann::json& properties) {
    return properties.is_object() && properties.contains("label")
        ? extractStringValue(properties["label"]) : "";
}

// Includes the gap below a non-empty title. Shared by measurement and native layout.
inline float measureFormLabel(const nlohmann::json& properties, float width) {
    const auto text = formLabelText(properties);
    return text.empty() ? 0.0f : TextMeasurementUtils::measureTextHeight(
        text, std::max(1.0f, width), kFormLabelFontSize) + kFormLabelSpacing;
}

} // namespace a2ui
