#pragma once

#include <cstdlib>
#include <string>

#include <nlohmann/json.hpp>

#include "log/a2ui_capi_log.h"
#include "style_parser/agenui_edge_insets_parser.h"

namespace a2ui {

inline float parseFloat(const std::string& s, float fallback) {
    if (s.empty()) return fallback;
    char* end = nullptr;
    float val = std::strtof(s.c_str(), &end);
    return (end != s.c_str()) ? val : fallback;
}

inline float parseStyleDimension(const nlohmann::json& styles, const char* key, float fallback) {
    if (!styles.is_object() || !styles.contains(key)) {
        return fallback;
    }
    const auto& value = styles[key];
    if (value.is_number()) {
        return value.get<float>();
    }
    if (value.is_string()) {
        return parseFloat(value.get<std::string>(), fallback);
    }
    return fallback;
}

inline float parseCssLength(const nlohmann::json& val, float fallback) {
    if (val.is_number()) {
        float f = val.get<float>();
        return f >= 0.0f ? f : fallback;
    }
    if (val.is_string()) {
        std::string s = val.get<std::string>();
        if (s.size() > 2 && s.compare(s.size() - 2, 2, "px") == 0) {
            s.resize(s.size() - 2);
        }
        float f = parseFloat(s, fallback);
        return f >= 0.0f ? f : fallback;
    }
    return fallback;
}

inline std::string extractStringValue(const nlohmann::json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_object() && value.contains("literalString") && value["literalString"].is_string()) {
        return value["literalString"].get<std::string>();
    }
    return "";
}

inline bool extractBooleanValue(const nlohmann::json& value) {
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_string()) {
        return value.get<std::string>() == "true";
    }
    if (value.is_object() && value.contains("literalBoolean") && value["literalBoolean"].is_boolean()) {
        return value["literalBoolean"].get<bool>();
    }
    return false;
}

inline std::string extractUrlFromCssUrl(const std::string& value) {
    if (value.empty()) {
        return "";
    }

    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t')) {
        start++;
    }

    if (value.compare(start, 4, "url(") != 0) {
        return value;
    }

    size_t parenStart = start + 3;
    while (parenStart < value.size() && value[parenStart] != '(') {
        parenStart++;
    }
    if (parenStart >= value.size()) {
        return value;
    }
    parenStart++;

    while (parenStart < value.size() && (value[parenStart] == ' ' || value[parenStart] == '\t')) {
        parenStart++;
    }

    size_t parenEnd = value.rfind(')');
    if (parenEnd == std::string::npos || parenEnd <= parenStart) {
        return value;
    }

    std::string inner = value.substr(parenStart, parenEnd - parenStart);

    size_t innerEnd = inner.size();
    while (innerEnd > 0 && (inner[innerEnd - 1] == ' ' || inner[innerEnd - 1] == '\t')) {
        innerEnd--;
    }
    inner = inner.substr(0, innerEnd);

    if (inner.size() >= 2) {
        if ((inner[0] == '"' && inner[inner.size() - 1] == '"') ||
            (inner[0] == '\'' && inner[inner.size() - 1] == '\'')) {
            inner = inner.substr(1, inner.size() - 2);
        }
    }

    return inner;
}

/**
 * Resolve CSS `margin` shorthand + `margin-*` overrides into four a2ui-px
 * edge values. Negative results are clamped to 0. When the styles object has
 * no `margin*` keys, all four outputs are 0.
 *
 * Mirrors padding_utils::resolveUserPadding in structure and parser usage.
 */
inline void resolveUserMargin(const nlohmann::json& styles,
                           float& top, float& right, float& bottom, float& left) {
    top = right = bottom = left = 0.0f;
    if (styles.is_null() || !styles.is_object()) return;

    // Shorthand: `margin`
    auto it = styles.find("margin");
    if (it != styles.end()) {
        if (it->is_number()) {
            const float v = it->get<float>();
            const float clamped = v > 0.0f ? v : 0.0f;
            top = right = bottom = left = clamped;
        } else if (it->is_string()) {
            const std::string& raw = it->get_ref<const std::string&>();
            ::agenui::EdgeInsets parsed;
            if (::agenui::EdgeInsetsParser::parse(raw, parsed)) {
                auto sidePx = [](const ::agenui::EdgeInsetValue& s) -> float {
                    if (s.isCalc) return 0.0f;
                    return (s.unit == ::agenui::EdgeInsetUnit::Px) ? s.value : 0.0f;
                };
                top    = sidePx(parsed.top);
                right  = sidePx(parsed.right);
                bottom = sidePx(parsed.bottom);
                left   = sidePx(parsed.left);
            }
        }
    }

    // Per-edge overrides take precedence over the shorthand.
    float v = 0.0f;
    if ((it = styles.find("margin-top")) != styles.end() && parseCssLength(*it, 0.0f) >= 0.0f) {
        v = parseCssLength(*it, 0.0f); top = v;
    }
    if ((it = styles.find("margin-right")) != styles.end()) {
        v = parseCssLength(*it, 0.0f); right = v;
    }
    if ((it = styles.find("margin-bottom")) != styles.end()) {
        v = parseCssLength(*it, 0.0f); bottom = v;
    }
    if ((it = styles.find("margin-left")) != styles.end()) {
        v = parseCssLength(*it, 0.0f); left = v;
    }

    if (top < 0.0f) top = 0.0f;
    if (right < 0.0f) right = 0.0f;
    if (bottom < 0.0f) bottom = 0.0f;
    if (left < 0.0f) left = 0.0f;
}

} // namespace a2ui
