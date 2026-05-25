#pragma once

#include "function_call/agenui_ifunctioncall.h"
#include "function_call/agenui_functioncall_resolution.h"
#include "function_call/agenui_functioncall_config.h"

namespace agenui {

/**
 * @brief FormatString functionCall — returns the result of string interpolation.
 *
 * Note: actual interpolation (parsing `${...}` expressions, accessing the
 * data model, type conversion, etc.) is performed upstream by the
 * DataValueParser / InterpolationExpressionDataValue pipeline. This
 * functionCall only returns the final pre-resolved value.
 */
class FormatStringFunctionCall : public IFunctionCall {
public:
    FormatStringFunctionCall() = default;
    
    FunctionCallResolution execute(const nlohmann::json& args) override;
    FunctionCallConfig getConfig() const override;
};

} // namespace agenui
