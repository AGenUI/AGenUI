#pragma once

#include <string>

namespace agenui {

/**
 * @brief Function call execution status
 */
enum class FunctionCallStatus {
    Success,    // Call succeeded
    Error,      // Call failed
    Pending,    // Async request submitted, awaiting callback result
    Completed   // Async execution completed
};

/**
 * @brief Function call result
 * 
 * Encapsulates the result of a platform function call,
 * including execution status, return data, and error information.
 */
struct FunctionCallResult {
    FunctionCallStatus status = FunctionCallStatus::Error;  // Execution status, default Error
    std::string data;    // Return data on success (JSON string)
    std::string error;   // Error message on failure
};

/**
 * @brief Context information for a function call
 * 
 * Provides the engine and surface context in which the function call is invoked.
 */
struct FunctionCallContext {
    int instanceId;              // Instance unique identifier
    std::string surfaceId;       // Surface unique identifier
};

/**
 * @brief Platform function interface
 * 
 * Defines a unified interface for invoking platform-side functions.
 * Platform layer (iOS/Android/Harmony) should implement this interface to:
 * 1. Handle synchronous function calls
 */
class IPlatformFunction {
public:
    virtual ~IPlatformFunction() = default;
    
    /**
     * @brief Invoke the function synchronously
     * 
     * @param params Parameters as a JSON string
     * @return FunctionCallResult containing the execution result
     * 
     * @note This method blocks until the function call completes.
     */
    virtual FunctionCallResult callSync(const FunctionCallContext& context,
                                         const std::string& params) = 0;
};

} // namespace agenui
