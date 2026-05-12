#pragma once
#include <string>

namespace agenui {
enum LogLevel {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_PERFORMANCE,
};

/**
 * @brief Runtime logger interface (abstract interface)
 *
 * Follows the Dependency Inversion Principle (DIP):
 * - C++ modules depend on this abstract interface, not on any concrete implementation
 * - Concrete implementation is injected at the platform layer (Swift/ObjC/Kotlin/ArkTS)
 *
 * Used for runtime diagnostic logging (DEBUG/INFO/WARN/ERROR/FATAL),
 * paired with IPerfLogger which handles performance metrics.
 */
class IRuntimeLogger {
protected:
    virtual ~IRuntimeLogger() = default;

public:
    virtual void log(LogLevel level, const char* tag, const char* func, int line, const char* format, ...) = 0;
};

} // namespace agenui
