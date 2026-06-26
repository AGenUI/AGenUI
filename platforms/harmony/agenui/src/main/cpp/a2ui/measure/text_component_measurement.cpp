#include "text_component_measurement.h"
#include "hm_text_measure_utils.h"
#include "nlohmann/json.hpp"
#include "a2ui/third_party/key_define.h"
#include "a2ui/utils/a2ui_measure_mode.h"
#include <climits>
#include <cstdlib>
#include <algorithm>

namespace a2ui {

namespace {

// Resolve a W3C `line-height` value to an absolute a2ui-px line box height.
// Parsing rules (kept in lockstep with iOS `extractLineHeight`, Android
// `StyleHelper` and the render-side `text_component.cpp`):
//   - JSON number                    -> unitless multiplier (`value * fontSize`)
//   - JSON string ending with "px"   -> absolute px value
//   - JSON string without unit       -> unitless multiplier (`atof(value) * fontSize`)
// Returns 0 when the style is absent or invalid.
float resolveLineHeightPx(const nlohmann::json& lhVal, float fontSize) {
    if (lhVal.is_number()) {
        float mult = static_cast<float>(lhVal.get<double>());
        return (mult > 0.0f && fontSize > 0.0f) ? mult * fontSize : 0.0f;
    }
    if (lhVal.is_string()) {
        const std::string& s = lhVal.get_ref<const std::string&>();
        if (s.empty()) {
            return 0.0f;
        }
        const float raw = static_cast<float>(std::atof(s.c_str()));
        if (raw <= 0.0f) {
            return 0.0f;
        }
        const bool hasPxSuffix = s.size() >= 2 &&
                                 s.compare(s.size() - 2, 2, "px") == 0;
        if (hasPxSuffix) {
            return raw;
        }
        return fontSize > 0.0f ? raw * fontSize : 0.0f;
    }
    return 0.0f;
}

} // namespace

agenui::MeasureResult TextComponentMeasurement::measure(const std::string& paramJson,
                                                  const agenui::MeasureModes& modes) {
    std::string text;
    TextMeasureParam param;
    using json = nlohmann::json;
    json j = json::parse(paramJson, nullptr, false);
    if (j.is_discarded()) return agenui::MeasureResult{agenui::CalcType::Sync, 0.0f, 0.0f, 0};
    if (!buildParam(j, text, param)) {
        return agenui::MeasureResult{agenui::CalcType::Sync, 0.0f, 0.0f, 0};
    }

    auto toMode = [](int m) -> MeasureMode {
        switch (m) {
            case a2ui::kModeExactly:     return MeasureModeExactly;
            case a2ui::kModeAtMost:      return MeasureModeAtMost;
            default:                     return MeasureModeUndefined;
        }
    };

    float baseLine = 0.f, ascent = 0.f, descent = 0.f;
    auto r = TextMeasureUtils::doMeasure(
        param,
        modes.width.maxValue,  toMode(modes.width.mode),
        modes.height.maxValue, toMode(modes.height.mode),
        baseLine, ascent, descent);

    return agenui::MeasureResult{agenui::CalcType::Sync, r.width, r.height, r.lines};
}

bool TextComponentMeasurement::buildParam(const nlohmann::json& j,
                                           std::string& outText,
                                           TextMeasureParam& outParam) {
    using json = nlohmann::json;
    if (j.is_discarded() || j.is_null()) return false;

    // ---- Extract text content (attributes flattened at root level: text / label) ----
    // stringify() flattens attributes to root level without "attrs" wrapper
    if (j.contains("text") && j["text"].is_string()) {
        outText = j["text"].get<std::string>();
    } else if (j.contains("label") && j["label"].is_string()) {
        outText = j["label"].get<std::string>();
    }
    if (outText.empty()) return false;

    // ---- Default parameters ----
    outParam.text             = outText.c_str();
    outParam.fontSize         = 24;
    outParam.fontWeight       = NODE_PROPERTY_FONT_NORMAL;
    outParam.fontStyle        = NODE_PROPERTY_FONT_NORMAL;
    outParam.textAlign        = TEXT_ALIGN_LEFT_TOP;
    outParam.isMultLineHeight = true;
    outParam.lineHeight       = 1.0f;
    outParam.maxLines         = INT_MAX;
    outParam.id               = 0;
    outParam.textOverflow     = NODE_PROPERTY_TEXT_OVERFLOW_UNDEFINED;
    outParam.isRichtext       = false;  // TODO: Can be determined by CSSStyleConverter::isRichText
    outParam.fontFamily       = "";
    outParam.extras           = "";
    outParam.letter_spacing   = 0.0f;
    outParam.ctx_id           = 0;

    // ---- Helper lambda: get field reference from root level (flattened attributes) or styles ----
    // Return pointer instead of by-value to avoid copying json objects
    static const json s_null{};
    auto getValue = [&](const std::string& key) -> const json& {
        if (j.contains("styles") && j["styles"].contains(key)) return j["styles"][key];
        if (j.contains(key)) return j[key];
        return s_null;
    };
    
    // font-size
    const json& fsVal0 = getValue("font-size");
    const json& fsVal  = (!fsVal0.is_null() && !fsVal0.is_discarded()) ? fsVal0 : getValue("fontSize");
    if (fsVal.is_number()) {
        outParam.fontSize = fsVal.get<int>();
    } else if (fsVal.is_string()) {
        outParam.fontSize = std::atoi(fsVal.get<std::string>().c_str());
    }
    
    // font-weight: supports "bold", "normal", or numeric (>=500 is bold)
    const json& fwVal0 = getValue("font-weight");
    const json& fwVal  = (!fwVal0.is_null() && !fwVal0.is_discarded()) ? fwVal0 : getValue("fontWeight");
    if (fwVal.is_number()) {
        outParam.fontWeight = fwVal.get<int>();
    } else if (fwVal.is_string()) {
        outParam.fontWeight = font_weight::parseStringToMeasureWeight(fwVal.get<std::string>());
    }
    
    // font-style
    const json& fstVal = getValue("font-style");
    if (fstVal.is_number()) {
        outParam.fontStyle = fstVal.get<int>();
    } else if (fstVal.is_string()) {
        const std::string fst = fstVal.get<std::string>();
        if      (fst == "italic") outParam.fontStyle = NODE_PROPERTY_FONT_ITALIC;
        else if (fst == "normal") outParam.fontStyle = NODE_PROPERTY_FONT_NORMAL;
        else                      outParam.fontStyle = std::atoi(fst.c_str());
    }
    
    // font-family
    static thread_local std::string s_fontFamily;
    const json& ffVal = getValue("font-family");
    if (ffVal.is_string()) {
        s_fontFamily = ffVal.get<std::string>();
        outParam.fontFamily = s_fontFamily.c_str();
    }
    
    // text-align
    const json& taVal = getValue("text-align");
    if (taVal.is_string()) {
        const std::string ta = taVal.get<std::string>();
        if      (ta == "left top")        outParam.textAlign = TEXT_ALIGN_LEFT_TOP;
        else if (ta == "left" ||
                 ta == "left center")     outParam.textAlign = TEXT_ALIGN_LEFT_V_CENTER;
        else if (ta == "left bottom")     outParam.textAlign = TEXT_ALIGN_LEFT_BOTTOM;
        else if (ta == "center top")      outParam.textAlign = TEXT_ALIGN_TOP_H_CENTER;
        else if (ta == "center" ||
                 ta == "center center")   outParam.textAlign = TEXT_ALIGN_CENTER;
        else if (ta == "center bottom")   outParam.textAlign = TEXT_ALIGN_BOTTOM_H_CENTER;
        else if (ta == "right top")       outParam.textAlign = TEXT_ALIGN_RIGHT_TOP;
        else if (ta == "right" ||
                 ta == "right center")    outParam.textAlign = TEXT_ALIGN_RIGHT_V_CENTER;
        else if (ta == "right bottom")    outParam.textAlign = TEXT_ALIGN_RIGHT_BOTTOM;
    }
    
    // letter-spacing
    const json& lsVal = getValue("letter-spacing");
    if (lsVal.is_number()) {
        outParam.letter_spacing = static_cast<float>(lsVal.get<double>());
    } else if (lsVal.is_string()) {
        outParam.letter_spacing = static_cast<float>(std::atof(lsVal.get<std::string>().c_str()));
    }
    
    // line-height:
    // `outParam.lineHeight` is stored as a unit-less multiplier relative to
    // `outParam.fontSize` (see `TextMeasureUtils::doMeasure`). We resolve the
    // raw style value to an absolute a2ui-px height via `resolveLineHeightPx`
    // (shared rule with iOS/Android and the render-side `text_component.cpp`)
    // and then divide by fontSize to store the multiplier.
    const json& lhVal = getValue("line-height");
    const float resolvedLineHeightPx =
        resolveLineHeightPx(lhVal, static_cast<float>(outParam.fontSize));
    if (resolvedLineHeightPx > 0.0f && outParam.fontSize > 0) {
        outParam.lineHeight = resolvedLineHeightPx / static_cast<float>(outParam.fontSize);
    }
    
    // line-clamp -> maxLines
    const json& lcVal = getValue("line-clamp");
    if (lcVal.is_number()) {
        int clamp = lcVal.get<int>();
        outParam.maxLines = clamp <= 0 ? INT_MAX : clamp;
    } else if (lcVal.is_string()) {
        int clamp = std::atoi(lcVal.get<std::string>().c_str());
        outParam.maxLines = clamp <= 0 ? INT_MAX : clamp;
    }
    
    // text-overflow
    const json& toVal = getValue("text-overflow");
    if (toVal.is_number()) {
        outParam.textOverflow = toVal.get<int>();
    } else if (toVal.is_string()) {
        const std::string tov = toVal.get<std::string>();
        if      (tov == "ellipsis") outParam.textOverflow = NODE_PROPERTY_TEXT_OVERFLOW_ELLIPSIS;
        else if (tov == "clip")     outParam.textOverflow = NODE_PROPERTY_TEXT_OVERFLOW_CLIP;
        else                         outParam.textOverflow = std::atoi(tov.c_str());
    }
    
    // white-space -> isMultLineHeight
    const json& wsVal = getValue("white-space");
    if (wsVal.is_string()) {
        const std::string ws = wsVal.get<std::string>();
        outParam.isMultLineHeight = (ws != "nowrap" && ws != "pre");
    }

    return true;
}

}  // namespace a2ui
