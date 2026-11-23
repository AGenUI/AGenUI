#pragma once

#include <cctype>
#include <cstdint>
#include <string>

namespace agenui {

/**
 * @brief Platform-agnostic writing direction for layout resolution.
 *
 * Shared vocabulary across the SurfaceManager API, VirtualDOM and layout
 * delegate layers. Deliberately free of Yoga types: custom layout engines
 * (Skia, Taffy, ArkUI CAPI ...) must not be forced to include Yoga headers.
 *
 * The surface-level direction (ISurfaceManager::setLayoutDirection) is the
 * SINGLE direction source for the whole tree: node-level DSL "direction"
 * styles are not supported and never reach the layout engine.
 * Default: LTR.
 */
enum class LayoutDirection : std::uint8_t {
    LTR,
    RTL,
};

/**
 * @brief Serialize a direction to its wire form ("ltr"/"rtl").
 *
 * Single producer-side entry for the direction wire vocabulary so future
 * values stay in sync across all layers.
 */
inline const char* toLayoutDirectionString(LayoutDirection direction) {
    return direction == LayoutDirection::RTL ? "rtl" : "ltr";
}

/**
 * @brief Parse a wire direction string.
 *
 * Case-insensitive and tolerant of surrounding whitespace ("RTL", " rtl "
 * are accepted), so host-derived inputs don't silently fail.
 *
 * @param value Raw input string.
 * @param out Parsed direction (untouched when parsing fails).
 * @return false when the input is not a recognized direction.
 */
inline bool parseLayoutDirection(const std::string& value, LayoutDirection& out) {
    auto isSpace = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && isSpace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    while (end > begin && isSpace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    std::string token = value.substr(begin, end - begin);
    for (char& c : token) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (token == "ltr") {
        out = LayoutDirection::LTR;
        return true;
    }
    if (token == "rtl") {
        out = LayoutDirection::RTL;
        return true;
    }
    return false;
}

}  // namespace agenui
