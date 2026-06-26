#include "a2ui_shadow_utils.h"

namespace a2ui {

DropShadowParams parseDropShadow(const std::string& filterVal) {
    DropShadowParams result;

    const std::string dsPrefix = "drop-shadow(";
    size_t dsStart = filterVal.find(dsPrefix);
    if (dsStart == std::string::npos) return result;
    dsStart += dsPrefix.size();

    size_t dsEnd = filterVal.rfind(')');
    if (dsEnd == std::string::npos || dsEnd < dsStart) return result;

    std::string inner = filterVal.substr(dsStart, dsEnd - dsStart);
    const char* p = inner.c_str();
    char* endPtr = nullptr;

    auto skipSeparators = [](const char*& cursor) {
        while (*cursor == ' ' || *cursor == ',') cursor++;
    };

    auto parseLength = [&](float& outValue) -> bool {
        skipSeparators(p);
        outValue = std::strtof(p, &endPtr);
        if (endPtr == p) return false;
        p = endPtr;
        while (*p && *p != ' ' && *p != ',' && *p != '(') p++;
        return true;
    };

    float vals[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 3; i++) {
        if (!parseLength(vals[i])) return result;
    }

    {
        const char* lookahead = p;
        skipSeparators(lookahead);
        if (*lookahead != '\0' && *lookahead != 'r' && *lookahead != '#' && *lookahead != 't') {
            p = lookahead;
            if (!parseLength(vals[3])) return result;
        } else {
            p = lookahead;
        }
    }

    skipSeparators(p);
    std::string colorStr = p;
    if (colorStr.empty()) return result;

    result.offsetX = vals[0];
    result.offsetY = vals[1];
    result.blurRadius = vals[2];
    result.spread = vals[3];
    result.colorStr = std::move(colorStr);
    result.valid = true;
    return result;
}

} // namespace a2ui
