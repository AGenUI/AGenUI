#pragma once

#include <cstdint>
#include <string>

#include <arkui/native_type.h>

namespace a2ui {

inline int32_t mapFlexJustifyContent(const std::string& justify) {
    if (justify == "center") {
        return ARKUI_FLEX_ALIGNMENT_CENTER;
    } else if (justify == "end") {
        return ARKUI_FLEX_ALIGNMENT_END;
    } else if (justify == "spaceBetween") {
        return ARKUI_FLEX_ALIGNMENT_SPACE_BETWEEN;
    } else if (justify == "spaceAround") {
        return ARKUI_FLEX_ALIGNMENT_SPACE_AROUND;
    } else if (justify == "spaceEvenly") {
        return ARKUI_FLEX_ALIGNMENT_SPACE_EVENLY;
    }
    return ARKUI_FLEX_ALIGNMENT_START;
}

inline int32_t mapFlexAlignItems(const std::string& align) {
    if (align == "center") {
        return ARKUI_ITEM_ALIGNMENT_CENTER;
    } else if (align == "end") {
        return ARKUI_ITEM_ALIGNMENT_END;
    } else if (align == "stretch") {
        return ARKUI_ITEM_ALIGNMENT_STRETCH;
    }
    return ARKUI_ITEM_ALIGNMENT_START;
}

} // namespace a2ui
