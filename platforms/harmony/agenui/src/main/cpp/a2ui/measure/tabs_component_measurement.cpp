#include "tabs_component_measurement.h"
#include "a2ui_platform_layout_bridge.h"
#include "text_component_measurement.h"
#include "hm_text_measure_utils.h"
#include "a2ui/third_party/key_define.h"
#include "a2ui/utils/a2ui_measure_mode.h"

#include "nlohmann/json.hpp"
#include <algorithm>
#include <cstdlib>

namespace a2ui {

// ==================== IMeasurement::measure ====================

agenui::MeasureResult TabsComponentMeasurement::measure(
        const std::string& paramJson,
        const agenui::MeasureModes& modes) {

    float maxWidth = modes.width.maxValue > 0.0f ? modes.width.maxValue : kDefaultMaxWidth;

    // ---- Read Tabs style configuration from g_component_styles ----
    // TabBar fixed height on render side is 96.0f (a2ui units, corresponding to 48vp)
    // Use text measurement max value and take max with fixed height to align with render side
    float tabBarFixedHeight = 96.0f;  // Consistent with bar.setHeight(96.0f) in tabs_component.cpp
    float tabFontSize         = 32.0f; 
    float tabFontSizeSelected = 32.0f;
    bool  tabFontWeightBold         = false;
    bool  tabFontWeightSelectedBold = true;   // Default selected bold

    const nlohmann::json& tabsStyles = ::a2ui::getComponentStylesFor("Tabs");
    if (tabsStyles.is_object()) {
        parseStyleFloat(tabsStyles, "tab-font-size",          tabFontSize);
        parseStyleFloat(tabsStyles, "tab-font-size-selected", tabFontSizeSelected);

        if (tabsStyles.contains("tab-font-weight") && tabsStyles["tab-font-weight"].is_string()) {
            tabFontWeightBold = (font_weight::mapStringToArkUIFontWeight(tabsStyles["tab-font-weight"].get<std::string>()) == ARKUI_FONT_WEIGHT_BOLD);
        }
        if (tabsStyles.contains("tab-font-weight-selected") && tabsStyles["tab-font-weight-selected"].is_string()) {
            tabFontWeightSelectedBold = (font_weight::mapStringToArkUIFontWeight(tabsStyles["tab-font-weight-selected"].get<std::string>()) == ARKUI_FONT_WEIGHT_BOLD);
        }
    }

    // ---- Parse tabs array (attributes flattened at root level, not wrapped in "attrs") ----
    std::vector<std::string> tabs;
    nlohmann::json j = nlohmann::json::parse(paramJson, nullptr, false);
    if (!j.is_discarded() && j.contains("tabs") && j["tabs"].is_array()) {
        for (const auto& item : j["tabs"]) {
            if (item.is_object() && item.contains("title")) {
                const auto& t = item["title"];
                tabs.push_back(t.is_string() ? t.get<std::string>() : t.dump());
            } else if (item.is_string()) {
                tabs.push_back(item.get<std::string>());
            } else {
                tabs.push_back(item.dump());
            }
        }
    }

    if (tabs.empty()) {
        // No tabs data: return tabBar height only (content area height determined by child components, 0 here)
        return {agenui::CalcType::Sync, maxWidth, tabBarFixedHeight, 0};
    }

    // ---- Measure tab text height via buildParam, take the larger of normal/selected ----
    float tabWidth = maxWidth / static_cast<float>(tabs.size());
    
    // Reuse a json object, modify fields in-place to avoid constructing temporary objects each iteration
    nlohmann::json tabStyleJson = nlohmann::json::object();
    tabStyleJson["font-size"]   = tabFontSize;
    tabStyleJson["font-weight"] = tabFontWeightBold ? "bold" : "normal";
    tabStyleJson["text"]        = "";  // Placeholder; overwritten in-place within loop
    
    float measuredTabBarHeight = 0.0f;
    for (const auto& text : tabs) {
        // Normal state
        {
            tabStyleJson["font-size"]   = tabFontSize;
            tabStyleJson["font-weight"] = tabFontWeightBold ? "bold" : "normal";
            tabStyleJson["text"]        = text;
            std::string outText;
            TextMeasureParam param;
            if (TextComponentMeasurement::buildParam(tabStyleJson, outText, param)) {
                float baseLine = 0.f, ascent = 0.f, descent = 0.f;
                auto r = TextMeasureUtils::doMeasure(
                    param, tabWidth, MeasureModeAtMost,
                    0.0f, MeasureModeUndefined,
                    baseLine, ascent, descent);
                measuredTabBarHeight = std::max(measuredTabBarHeight, r.height);
            }
        }
        // Selected state (typically bold, wider/taller font)
        {
            tabStyleJson["font-size"]   = tabFontSizeSelected;
            tabStyleJson["font-weight"] = tabFontWeightSelectedBold ? "bold" : "normal";
            tabStyleJson["text"]        = text;
            std::string outText;
            TextMeasureParam param;
            if (TextComponentMeasurement::buildParam(tabStyleJson, outText, param)) {
                float baseLine = 0.f, ascent = 0.f, descent = 0.f;
                auto r = TextMeasureUtils::doMeasure(
                    param, tabWidth, MeasureModeAtMost,
                    0.0f, MeasureModeUndefined,
                    baseLine, ascent, descent);
                measuredTabBarHeight = std::max(measuredTabBarHeight, r.height);
            }
        }
    }

    // TabBar final height = max(render-side fixed value, text measurement value)
    // Ensure alignment with render-side setHeight(96.0f), no less than fixed value
    float tabBarHeight = std::max(tabBarFixedHeight, measuredTabBarHeight);

    // Tabs measureFunc returns tabBar height only
    // Note: when Tabs has Yoga child nodes, Yoga will not call this measureFunc,
    // Tabs overall height is determined by style properties (height/flex).
    // Content area height is corrected by TabsLayoutPatcher (= Tabs total height - tabBarHeight).
    return {agenui::CalcType::Sync, maxWidth, tabBarHeight, 0};
}

}  // namespace a2ui
