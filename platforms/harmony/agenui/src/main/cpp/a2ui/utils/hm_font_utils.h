#pragma once

#include <string>
#include <sstream>

#include "a2ui/font/a2ui_font_registry.h"
#include "a2ui/utils/a2ui_string_utils.h"
#include "a2ui/utils/hm_generic_font_resolver.h"

namespace a2ui {

inline std::string trimFontToken(const std::string& value) {
    size_t start = 0;
    size_t end = value.size();
    while (start < end && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

inline std::string stripFontQuotes(const std::string& value) {
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

inline const std::string& harmonyDefaultFontFamily() {
    return HmGenericFontResolver::instance().defaultFamily();
}

inline std::string resolveGenericHarmonyFamily(const std::string& lower) {
    return HmGenericFontResolver::instance().resolve(lower);
}

/// Resolve a CSS font-family value (may contain comma-separated fallback list)
/// through generic names, FontRegistry, then transparent passthrough to ArkUI.
inline std::string resolveHarmonyFontFamily(const std::string& rawFamily) {
    if (rawFamily.empty()) {
        return harmonyDefaultFontFamily();
    }

    std::string lastCandidate;
    std::istringstream stream(rawFamily);
    std::string candidate;
    while (std::getline(stream, candidate, ',')) {
        std::string name = trimFontToken(stripFontQuotes(trimFontToken(candidate)));
        if (name.empty()) continue;

        std::string lower = toLowerAscii(name);

        // 1. Generic family map (CSS keywords: monospace, serif, etc.)
        std::string generic = resolveGenericHarmonyFamily(lower);
        if (!generic.empty()) {
            return generic;
        }

        // 2. FontRegistry lookup (custom registered fonts)
        std::string registered = FontRegistry::instance().resolve(name);
        if (!registered.empty()) {
            return registered;
        }

        // 3. Save as last candidate for transparent passthrough.
        //    System fonts like "HarmonyOS Sans Mono" or "Noto Serif" are
        //    neither CSS keywords nor in FontRegistry — save the original
        //    name (not lower-cased) so ArkUI can resolve the system font.
        lastCandidate = name;
    }

    // Pass through the last candidate to let ArkUI resolve system fonts.
    if (!lastCandidate.empty()) {
        return lastCandidate;
    }
    return harmonyDefaultFontFamily();
}

/// Legacy API kept for backward compatibility — callers that only need
/// simple normalization (no fallback list) can still use this.
inline std::string normalizeHarmonyFontFamily(const std::string& rawFamily) {
    return resolveHarmonyFontFamily(rawFamily);
}

} // namespace a2ui
