#pragma once

#include <cstdlib>
#include <string>

#include <nlohmann/json.hpp>

#include "log/a2ui_capi_log.h"

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

} // namespace a2ui
