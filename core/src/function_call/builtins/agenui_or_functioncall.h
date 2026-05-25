#pragma once

#include "function_call/agenui_ifunctioncall.h"
#include "function_call/agenui_functioncall_resolution.h"
#include "function_call/agenui_functioncall_config.h"

namespace agenui {

/**
 * @brief Logical OR functionCall
 *
 * Returns true if any value in the array is true; short-circuits on first true.
 */
class OrFunctionCall : public IFunctionCall {
public:
    FunctionCallResolution execute(const nlohmann::json& args) override;
    FunctionCallConfig getConfig() const override;
};

} // namespace agenui
