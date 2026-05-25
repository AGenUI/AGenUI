#pragma once

#include "agenui_functioncall_config.h"
#include "function_call/agenui_functioncall_resolution.h"
#include <memory>
#include "nlohmann/json.hpp"

namespace agenui {

/**
 * @brief Abstract base class for all functionCalls
 *
 * Defines the interface that every functionCall must implement.
 * Register new functionCalls in C++ by subclassing this class.
 */
class IFunctionCall {
public:
    virtual ~IFunctionCall() = default;
    
    /**
     * @brief Execute the functionCall
     * @param args FunctionCall arguments (JSON object)
     * @return Execution result
     */
    virtual FunctionCallResolution execute(const nlohmann::json& args) = 0;

    /**
     * @brief Get the functionCall configuration
     * @return FunctionCallConfig object
     */
    virtual FunctionCallConfig getConfig() const = 0;

    /**
     * @brief Get the functionCall name
     * @return FunctionCall name
     */
    virtual std::string getName() const {
        return getConfig().getName();
    }

    /**
     * @brief Get the fully qualified functionCall name (including namespace)
     * @return Full name
     */
    virtual std::string getFullName() const {
        return getConfig().getFullName();
    }

};

/**
 * @brief FunctionCall smart pointer type
 */
using FunctionCallPtr = std::shared_ptr<IFunctionCall>;

} // namespace agenui
