#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace a2ui {

inline std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

} // namespace a2ui
