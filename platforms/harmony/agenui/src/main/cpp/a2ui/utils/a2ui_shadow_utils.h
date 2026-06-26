#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>

namespace a2ui {

struct DropShadowParams {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float blurRadius = 0.0f;
    float spread = 0.0f;
    std::string colorStr;
    bool valid = false;
};

DropShadowParams parseDropShadow(const std::string& filterVal);

} // namespace a2ui
