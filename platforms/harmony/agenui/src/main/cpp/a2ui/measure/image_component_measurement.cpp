#include "image_component_measurement.h"
#include "nlohmann/json.hpp"
#include <string>
#include <cstdlib>

namespace a2ui {

// Parse json field to float, supports number and unit-string (e.g. "100vp")
static float parseImageDimension(const nlohmann::json& val) {
    if (val.is_number()) return static_cast<float>(val.get<double>());
    if (val.is_string()) {
        const std::string& s = val.get<std::string>();
        if (s.empty()) return 0.0f;
        // Direct atof; unrecognized unit suffixes are automatically ignored
        return static_cast<float>(std::atof(s.c_str()));
    }
    return 0.0f;
}

agenui::MeasureResult ImageComponentMeasurement::measure(const std::string& paramJson,
                                                   const agenui::MeasureModes& /*modes*/) {
    using json = nlohmann::json;
    json j = json::parse(paramJson, nullptr, false);
    if (j.is_discarded()) {
        return agenui::MeasureResult{agenui::CalcType::Sync, 0.0f, 0.0f, 0};
    }

    float w = 0.0f, h = 0.0f;

    // Helper: returns true only for explicit pixel values (no %, no "auto", no empty).
    // Percentage and "auto" values must NOT be resolved here — Yoga handles them via
    // aspect-ratio / parent-width constraints.  Returning a wrong numeric guess would
    // shadow Yoga's own calculation and produce incorrect sizes.
    auto isExplicitPx = [](const nlohmann::json& val) -> bool {
        if (val.is_number()) return true;
        if (!val.is_string()) return false;
        const std::string& s = val.get<std::string>();
        if (s.empty() || s == "auto") return false;
        if (s.back() == '%') return false;
        return true;
    };

    // Read from styleInfo (pre-Yoga-clear snapshot of the raw style strings)
    if (j.contains("styleInfo") && j["styleInfo"].is_string()) {
        json styleInfo = json::parse(j["styleInfo"].get<std::string>(), nullptr, false);
        if (!styleInfo.is_discarded()) {
            if (styleInfo.contains("width") && isExplicitPx(styleInfo["width"]))
                w = parseImageDimension(styleInfo["width"]);
            if (styleInfo.contains("height") && isExplicitPx(styleInfo["height"]))
                h = parseImageDimension(styleInfo["height"]);
        }
    }

    // Fallback: read from styles directly (only explicit px values)
    if (w == 0.0f && h == 0.0f && j.contains("styles")) {
        const auto& styles = j["styles"];
        if (styles.contains("width") && isExplicitPx(styles["width"]))
            w = parseImageDimension(styles["width"]);
        if (styles.contains("height") && isExplicitPx(styles["height"]))
            h = parseImageDimension(styles["height"]);
    }

    // If no explicit pixel size was found, return {0, 0} so Yoga uses its own
    // constraints (aspect-ratio, percent-width, flex) to determine the final size.
    return agenui::MeasureResult{agenui::CalcType::Sync, w, h, 0};
}

}  // namespace a2ui
