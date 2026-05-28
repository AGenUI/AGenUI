#include "agenui_yoga_value.h"

namespace agenui {

YogaValue::YogaValue()
    : _type(kNone), _float(0.0f), _bool(false), _string() {
}

YogaValue::YogaValue(float value)
    : _type(kFloat), _float(value), _bool(false), _string() {
}

YogaValue::YogaValue(bool value)
    : _type(kBool), _float(0.0f), _bool(value), _string() {
}

YogaValue::YogaValue(const std::string& value)
    : _type(kString), _float(0.0f), _bool(false), _string(value) {
}

YogaValue::YogaValue(std::string&& value) noexcept
    : _type(kString), _float(0.0f), _bool(false), _string(std::move(value)) {
}

float YogaValue::asFloat() const {
    return _float;
}

bool YogaValue::asBool() const {
    return _bool;
}

const std::string& YogaValue::asString() const {
    return _string;
}

}  // namespace agenui
