// Copyright (c) Alibaba, Inc. and its affiliates.
//
// Centralized font-weight utilities for the Harmony platform.
//
// Supports "bold", "medium", "normal", or numeric values.
#pragma once

#include <arkui/native_type.h>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "../third_party/key_define.h"

namespace a2ui {
namespace font_weight {

// Map a numeric font-weight value to an ArkUI enum (render layer): each level 100-900 maps
// one-to-one to its ArkUI weight (W100..W900); any other value -> NORMAL.
inline int32_t mapNumericToArkUIFontWeight(int numericWeight) {
    switch (numericWeight) {
        case 100: return ARKUI_FONT_WEIGHT_W100;
        case 200: return ARKUI_FONT_WEIGHT_W200;
        case 300: return ARKUI_FONT_WEIGHT_W300;
        case 400: return ARKUI_FONT_WEIGHT_W400;
        case 500: return ARKUI_FONT_WEIGHT_W500;
        case 600: return ARKUI_FONT_WEIGHT_W600;
        case 700: return ARKUI_FONT_WEIGHT_W700;
        case 800: return ARKUI_FONT_WEIGHT_W800;
        case 900: return ARKUI_FONT_WEIGHT_W900;
        default:
            return ARKUI_FONT_WEIGHT_NORMAL;
    }
}

// Parse a font-weight string to ArkUI enum (for render layer).
// Accepts "bold", "medium", "normal", or numeric string ("100".."900").
inline int32_t mapStringToArkUIFontWeight(const std::string& str) {
    if (str.empty()) return ARKUI_FONT_WEIGHT_NORMAL;
    // Keyword aliases (ascending weight)
    if (str == "normal") return ARKUI_FONT_WEIGHT_NORMAL;
    if (str == "medium") return ARKUI_FONT_WEIGHT_MEDIUM;
    if (str == "bold") return ARKUI_FONT_WEIGHT_BOLD;
    // Numeric scale
    const int numWeight = std::atoi(str.c_str());
    if (numWeight > 0) {
        return mapNumericToArkUIFontWeight(numWeight);
    }
    return ARKUI_FONT_WEIGHT_NORMAL;
}

// Parse a font-weight string for the measurement layer.
// Returns NODE_PROPERTY_FONT_BOLD, NODE_PROPERTY_FONT_NORMAL, or raw numeric (100-900).
// The caller (convertToHMLayoutFontWeight) handles the final mapping.
inline int parseStringToMeasureWeight(const std::string& str) {
    if (str.empty()) return NODE_PROPERTY_FONT_NORMAL;
    // Keyword aliases (ascending weight)
    if (str == "normal") return NODE_PROPERTY_FONT_NORMAL;
    if (str == "medium") return NODE_PROPERTY_FONT_MEDIUM;  // mapped to FONT_WEIGHT_500 (medium) downstream
    if (str == "bold") return NODE_PROPERTY_FONT_BOLD;
    // Numeric scale (raw 100-900, mapped downstream by convertToHMLayoutFontWeight)
    int numWeight = std::atoi(str.c_str());
    return (numWeight > 0) ? numWeight : NODE_PROPERTY_FONT_NORMAL;
}

}  // namespace font_weight
}  // namespace a2ui
