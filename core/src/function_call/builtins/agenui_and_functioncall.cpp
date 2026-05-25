#include "agenui_and_functioncall.h"

namespace agenui {

FunctionCallResolution AndFunctionCall::execute(const nlohmann::json& args) {
    if (!args.contains("values")) {
        return FunctionCallResolution::createError("Missing required parameter: values");
    }
    
    const auto& values = args["values"];
    
    if (!values.is_array()) {
        return FunctionCallResolution::createError("Parameter 'values' must be an array");
    }
    
    for (const auto& item : values) {
        if (item.is_boolean() && !item.get<bool>()) {
            return FunctionCallResolution::createSuccess(false);
        }
    }
    
    return FunctionCallResolution::createSuccess(true);
}

FunctionCallConfig AndFunctionCall::getConfig() const {
    FunctionCallConfig config;
    config.setName("and");
    config.setDescription("Returns true if all values are true; short-circuits on first false.");
    config.setReturnType("boolean");
    return config;
}

} // namespace agenui
