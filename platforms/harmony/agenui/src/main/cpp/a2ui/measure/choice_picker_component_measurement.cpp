#include "choice_picker_component_measurement.h"
#include "a2ui_platform_layout_bridge.h"
#include "text_measurement_utils.h"
#include "text_component_measurement.h"

#include "nlohmann/json.hpp"
#include <algorithm>

namespace a2ui {

// ==================== IMeasurement::measure ====================

agenui::MeasureResult ChoicePickerComponentMeasurement::measure(
        const std::string& paramJson,
        const agenui::MeasureModes& modes) {

    float maxWidth = modes.width.maxValue > 0.0f ? modes.width.maxValue : 720.0f;

    // Parse options and orientation
    std::vector<std::string> options;
    bool horizontal = false;
    // Default dimensions from render layer (aligned with choicepicker_component.cpp)
    float checkboxSize  = 32.0f;  // Actual checkbox size
    float checkboxMar   = 8.0f;   // Checkbox margin (render layer setMargin(8,8,8,8))
    float textMargin    = 16.0f;  // Text left padding
    float rowPadding    = 16.0f;  // Each row setPadding(16): 16 top and bottom
    float choiceGap     = 40.0f;  // Item gap
    float textFontSize  = 32.0f;
    std::string textFontWeight = "normal";

    // Read default styles from g_component_styles
    const nlohmann::json& cpStyles = ::a2ui::getComponentStylesFor("ChoicePicker");
    if (cpStyles.is_object()) {
        parseStyleFloat(cpStyles, "checkbox-size", checkboxSize);
        parseStyleFloat(cpStyles, "text-margin",   textMargin);
        parseStyleFloat(cpStyles, "choice-gap",    choiceGap);
        parseStyleFloat(cpStyles, "text-size",     textFontSize);
        if (cpStyles.contains("text-weight") && cpStyles["text-weight"].is_string()) {
            textFontWeight = cpStyles["text-weight"].get<std::string>();
        }
    }

    // Per stringify flatten convention, options is at root level (not under attrs)
    nlohmann::json j = nlohmann::json::parse(paramJson, nullptr, false);
    if (!j.is_discarded()) {
        // stringify flatten convention: options directly at root level
        const nlohmann::json* opts = nullptr;
        if (j.contains("options") && j["options"].is_array()) {
            opts = &j["options"];
        }
        if (opts) {
            for (const auto& item : *opts) {
                if (item.is_object() && item.contains("label")) {
                    const auto& lv = item["label"];
                    std::string label = lv.is_string() ? lv.get<std::string>() : lv.dump();
                    // Support DynamicString format: {"literalString": "xxx"}
                    if (lv.is_object() && lv.contains("literalString") && lv["literalString"].is_string()) {
                        label = lv["literalString"].get<std::string>();
                    }
                    options.push_back(label);
                } else if (item.is_string()) {
                    options.push_back(item.get<std::string>());
                } else {
                    options.push_back(item.dump());
                }
            }
        }

        if (j.contains("styles") && j["styles"].is_object()) {
            const auto& styles = j["styles"];
            if (styles.contains("orientation")) {
                std::string ori = styles["orientation"].is_string()
                    ? styles["orientation"].get<std::string>() : styles["orientation"].dump();
                horizontal = (ori == "horizontal");
            }
        }
    }

    if (options.empty()) {
        return {agenui::CalcType::Sync, maxWidth, 0.0f, 0};
    }

    // Build text style JSON (consistent with Table's cellStyleJson)
    // Pre-allocate a "text" key; modify in-place within loop to avoid per-iteration copy
    nlohmann::json styleJson = nlohmann::json::object();
    styleJson["font-size"]   = textFontSize;
    styleJson["font-weight"] = textFontWeight;
    styleJson["text"]        = "";  // Placeholder; overwritten in-place within loop

    // Available text width: total - checkbox size - checkbox margin (both sides) - text left margin
    const float checkboxTotalW = checkboxSize + checkboxMar * 2.0f;
    float itemWidth = maxWidth - rowPadding * 2.0f;
    if (horizontal && options.size() > 0) {
        itemWidth = (maxWidth - rowPadding * 2.0f
                     - choiceGap * (static_cast<float>(options.size()) - 1.0f))
                    / static_cast<float>(options.size());
    }
    float textAvailWidth = itemWidth - checkboxTotalW - textMargin;
    if (textAvailWidth < 1.0f) textAvailWidth = 1.0f;

    const float checkboxH = checkboxSize + checkboxMar * 2.0f;  // Checkbox height including margin
    float totalHeight = 0.0f;
    bool  firstItem   = true;

    for (const auto& text : options) {
        float contentH = checkboxH;  // fallback
        if (!text.empty()) {
            styleJson["text"] = text;  // Overwrite in-place, no copy
            std::string outText;
            TextMeasureParam param;
            if (TextComponentMeasurement::buildParam(styleJson, outText, param)) {
                float baseLine = 0.f, ascent = 0.f, descent = 0.f;
                auto r = TextMeasureUtils::doMeasure(
                    param,
                    textAvailWidth, MeasureModeAtMost,
                    0.0f,           MeasureModeUndefined,
                    baseLine, ascent, descent);
                contentH = std::max(checkboxH, r.height);
            }
        }
        const float itemH = contentH + rowPadding * 2.0f;
        if (horizontal) {
            totalHeight = std::max(totalHeight, itemH);
        } else {
            if (!firstItem) totalHeight += choiceGap;
            totalHeight += itemH;
            firstItem = false;
        }
    }

    return {agenui::CalcType::Sync, maxWidth, totalHeight, 0};
}

}  // namespace a2ui
