#include "agenui_format_string_functioncall.h"
#include "agenui_logger_internal.h"
#include <sstream>

namespace agenui {

FunctionCallResolution FormatStringFunctionCall::execute(const nlohmann::json& args) {
    if (!args.contains("value")) {
        return FunctionCallResolution::createError("Missing required parameter: value");
    }
    
    const auto& value = args["value"];
    AGENUI_LOG("value: %s", value.dump().c_str());

    return FunctionCallResolution::createSuccess(value);
}

FunctionCallConfig FormatStringFunctionCall::getConfig() const {
    FunctionCallConfig config;
    config.setName("formatString");
    config.setDescription("Performs string interpolation of data model values and other functions in the catalog functions list and returns the resulting string. The value string can contain interpolated expressions in the ${expression} format.");
    config.setReturnType("string");
    return config;
}

} // namespace agenui
