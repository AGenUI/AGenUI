#pragma once

#include "function_call/agenui_ifunctioncall.h"
#include "function_call/agenui_functioncall_resolution.h"
#include "function_call/agenui_functioncall_config.h"

namespace agenui {

/**
 * @brief Logical AND functionCall
 *
 * Returns true if all values in the array are true; short-circuits on first false.
 */
class AndFunctionCall : public IFunctionCall {
public:
    FunctionCallResolution execute(const nlohmann::json& args) override;
    FunctionCallConfig getConfig() const override;
};

} // namespace agenui
