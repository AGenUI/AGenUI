#include "datetimeinput_component_measurement.h"
#include "a2ui_platform_layout_bridge.h"
#include "hm_text_measure_utils.h"
#include "a2ui/third_party/key_define.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <climits>
#include <cstdlib>

namespace a2ui {

agenui::MeasureResult DateTimeInputComponentMeasurement::measure(
        const std::string& paramJson,
        const agenui::MeasureModes& modes) {

    // ---- Read device component styles ----
    const nlohmann::json& dtiStyles = ::a2ui::getComponentStylesFor("DateTimeInput");
    const nlohmann::json compactStyles =
        dtiStyles.contains("compact") && dtiStyles["compact"].is_object()
            ? dtiStyles["compact"]
            : nlohmann::json::object();

    auto parseDim = [](const nlohmann::json& obj, const char* key, float fallback) -> float {
        if (!obj.contains(key)) return fallback;
        const auto& v = obj[key];
        if (v.is_number()) return v.get<float>();
        if (v.is_string()) return static_cast<float>(std::atof(v.get<std::string>().c_str()));
        return fallback;
    };

    const float compactHeight      = parseDim(compactStyles, "height",           56.0f);
    const float fontSize           = parseDim(compactStyles, "font-size",         24.0f);
    const float iconSize           = parseDim(compactStyles, "icon-size",         24.0f);
    const float iconSpacing        = parseDim(compactStyles, "icon-spacing",       6.0f);
    const float paddingVertical    = parseDim(compactStyles, "padding-vertical",  12.0f);
    const float paddingHorizontal  = parseDim(compactStyles, "padding-horizontal",24.0f);

    // ---- Parse paramJson: extract value / placeholder ----
    std::string displayText = "Select Date";
    bool showIcon = true;
    bool hasValue = false;

    auto placeholder = compactStyles.find("placeholder-text");
    if (placeholder != compactStyles.end() && placeholder->is_string()) {
        displayText = placeholder->get<std::string>();
    }

    nlohmann::json j = nlohmann::json::parse(paramJson, nullptr, false);
    if (!j.is_discarded()) {
        // stringify() flattens attributes to root level without "attrs" wrapper
        auto tryGet = [&](const nlohmann::json& node, const std::string& key) -> std::string {
            if (node.contains(key)) {
                const auto& v = node[key];
                if (v.is_string()) return v.get<std::string>();
            }
            return "";
        };

        // value is at root level (attributes flattened)
        std::string value = tryGet(j, "value");

        if (!value.empty()) {
            displayText = value;
            showIcon    = false;
            hasValue    = true;
        }

        // placeholder is at root level
        if (!hasValue) {
            std::string ph = tryGet(j, "placeholder");
            if (!ph.empty()) displayText = ph;
        }
    }

    // ---- Measure text ----
    float textWidth  = fontSize * 3.0f;
    float textHeight = fontSize;

    {
        const float reservedWidth = paddingHorizontal * 2.0f +
                                    (showIcon ? (iconSpacing + iconSize) : 0.0f);
        float availWidth = 0.0f;
        MeasureMode wMode = MeasureModeUndefined;

        if (modes.width.mode == 1 /*Exactly*/ || modes.width.mode == 2 /*AtMost*/) {
            wMode = (modes.width.mode == 1) ? MeasureModeExactly : MeasureModeAtMost;
            availWidth = std::max(0.0f, modes.width.maxValue - reservedWidth);
        }

        TextMeasureParam param;
        param.text             = displayText.c_str();
        param.fontSize         = static_cast<int>(fontSize);
        param.fontWeight       = hasValue ? NODE_PROPERTY_FONT_BOLD : NODE_PROPERTY_FONT_NORMAL;
        param.fontStyle        = NODE_PROPERTY_FONT_NORMAL;
        param.textAlign        = TEXT_ALIGN_LEFT_V_CENTER;
        param.isMultLineHeight = false;
        param.lineHeight       = 1.0f;
        param.maxLines         = 1;
        param.id               = 0;
        param.textOverflow     = NODE_PROPERTY_TEXT_OVERFLOW_CLIP;
        param.isRichtext       = false;
        param.fontFamily       = "";
        param.extras           = "";
        param.letter_spacing   = 0.0f;
        param.ctx_id           = 0;

        float baseLine = 0.f, ascent = 0.f, descent = 0.f;
        auto r = TextMeasureUtils::doMeasure(
            param,
            availWidth, wMode,
            0.0f,       MeasureModeUndefined,
            baseLine, ascent, descent);
        textWidth  = r.width;
        textHeight = r.height;
    }

    // ---- Calculate final dimensions ----
    float measuredWidth = textWidth + paddingHorizontal * 2.0f;
    if (showIcon) measuredWidth += iconSpacing + iconSize;

    float measuredHeight = std::max(compactHeight, textHeight + paddingVertical * 2.0f);
    if (showIcon) measuredHeight = std::max(measuredHeight, iconSize + paddingVertical * 2.0f);

    // Constrain to modes
    if ((modes.width.mode == 1 || modes.width.mode == 2) && modes.width.maxValue > 0.0f) {
        measuredWidth = modes.width.mode == 2
            ? std::min(measuredWidth, modes.width.maxValue)
            : modes.width.maxValue;
    }
    if ((modes.height.mode == 1 || modes.height.mode == 2) && modes.height.maxValue > 0.0f) {
        measuredHeight = modes.height.mode == 2
            ? std::min(measuredHeight, modes.height.maxValue)
            : modes.height.maxValue;
    }

    return {agenui::CalcType::Sync, measuredWidth, measuredHeight, 0};
}

}  // namespace a2ui
