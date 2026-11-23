#pragma once

#include <cctype>
#include <cstdint>
#include <string>

namespace a2ui::writing_direction {

enum class Direction : int32_t {
    Ltr = 0,
    Rtl = 1
};

enum class TextAlignment {
    Left,
    Center,
    Right
};

inline Direction resolve(const std::string& value) {
    return value == "rtl" ? Direction::Rtl : Direction::Ltr;
}

inline TextAlignment physicalTextAlignment(const std::string& value, Direction direction) {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return TextAlignment::Left;
    }
    const auto end = value.find_first_of(" \t\r\n", start);
    std::string token = value.substr(start, end == std::string::npos ? end : end - start);
    for (char& character : token) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    if (token == "center") {
        return TextAlignment::Center;
    }
    if (token == "right") {
        return TextAlignment::Right;
    }
    if (token == "start") {
        return direction == Direction::Rtl ? TextAlignment::Right : TextAlignment::Left;
    }
    if (token == "end") {
        return direction == Direction::Rtl ? TextAlignment::Left : TextAlignment::Right;
    }
    return TextAlignment::Left;
}

}
