// Copyright (c) Alibaba, Inc. and its affiliates.
//
// Centralized font-weight utilities for the Harmony platform.
//
// Supports "bold", "normal", or numeric values.
// Rule: numeric >= 500 maps to BOLD, otherwise NORMAL.
#pragma once

#include <arkui/native_type.h>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "../third_party/key_define.h"

namespace a2ui {
namespace font_weight {

// Map a numeric font-weight value to BOLD or NORMAL (ArkUI render layer).
// Rule: >= 500 is BOLD, < 500 is NORMAL.
inline int32_t mapNumericToArkUIFontWeight(int numericWeight) {
    return numericWeight >= 500 ? ARKUI_FONT_WEIGHT_BOLD : ARKUI_FONT_WEIGHT_NORMAL;
}

// Parse a font-weight string to ArkUI enum (for render layer).
// Accepts "bold", "normal", or numeric string ("100".."900").
// Returns ARKUI_FONT_WEIGHT_BOLD or ARKUI_FONT_WEIGHT_NORMAL.
inline int32_t mapStringToArkUIFontWeight(const std::string& str) {
    if (str.empty()) return ARKUI_FONT_WEIGHT_NORMAL;
    if (str == "bold") return ARKUI_FONT_WEIGHT_BOLD;
    if (str == "normal") return ARKUI_FONT_WEIGHT_NORMAL;
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
    if (str == "bold") return NODE_PROPERTY_FONT_BOLD;
    if (str == "normal") return NODE_PROPERTY_FONT_NORMAL;
    int numWeight = std::atoi(str.c_str());
    return (numWeight > 0) ? numWeight : NODE_PROPERTY_FONT_NORMAL;
}

}  // namespace font_weight
}  // namespace a2ui
