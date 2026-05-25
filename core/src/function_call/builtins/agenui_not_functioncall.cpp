#include "agenui_not_functioncall.h"

namespace agenui {

FunctionCallResolution NotFunctionCall::execute(const nlohmann::json& args) {
    if (!args.contains("value")) {
        return FunctionCallResolution::createError("Missing required parameter: value");
    }
    
    const auto& value = args["value"];
    
    if (!value.is_boolean()) {
        return FunctionCallResolution::createError("Parameter 'value' must be a boolean");
    }
    
    return FunctionCallResolution::createSuccess(!value.get<bool>());
}

FunctionCallConfig NotFunctionCall::getConfig() const {
    FunctionCallConfig config;
    config.setName("not");
    config.setDescription("Returns the negation of the input boolean value.");
    config.setReturnType("boolean");
    return config;
}

} // namespace agenui
