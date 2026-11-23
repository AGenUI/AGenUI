#pragma once

#include <algorithm>

namespace a2ui::list_layout {

inline float trailingSpacing(float padding, float margin) {
    return std::max(0.0f, padding) + std::max(0.0f, margin);
}

inline float leadingSpacing(float position, float size, float containerSize, bool isRtlLayout) {
    return std::max(0.0f, isRtlLayout ? containerSize - position - size : position);
}

inline float itemSpacing(float position, float size, float previousPosition, float previousSize,
                         bool isRtlLayout) {
    return std::max(0.0f, isRtlLayout ? previousPosition - position - size
                                     : position - previousPosition - previousSize);
}

}
