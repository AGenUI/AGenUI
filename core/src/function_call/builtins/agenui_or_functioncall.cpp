#include "agenui_or_functioncall.h"

namespace agenui {

FunctionCallResolution OrFunctionCall::execute(const nlohmann::json& args) {
    if (!args.contains("values")) {
        return FunctionCallResolution::createError("Missing required parameter: values");
    }
    
    const auto& values = args["values"];
    
    if (!values.is_array()) {
        return FunctionCallResolution::createError("Parameter 'values' must be an array");
    }
    
    for (const auto& item : values) {
        if (item.is_boolean() && item.get<bool>()) {
            return FunctionCallResolution::createSuccess(true);
        }
    }
    
    return FunctionCallResolution::createSuccess(false);
}

FunctionCallConfig OrFunctionCall::getConfig() const {
    FunctionCallConfig config;
    config.setName("or");
    config.setDescription("Returns true if any value is true; short-circuits on first true.");
    config.setReturnType("boolean");
    return config;
}

} // namespace agenui
