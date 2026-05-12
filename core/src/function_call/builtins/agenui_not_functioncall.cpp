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
    config.setSync(true);
    nlohmann::json params = {
        {"type", "object"},
        {"properties", {
            {"value", {
                {"type", "DynamicBoolean"},
                {"description", "The boolean value to negate."}
            }}
        }},
        {"required", nlohmann::json::array({"value"})}
    };
    config.setParameters(params);
    return config;
}

} // namespace agenui
