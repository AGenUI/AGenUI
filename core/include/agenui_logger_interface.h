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

    /**
     * @brief Minimum log level the implementation wants to receive.
     *
     * Messages with `level < getMinLevel()` are filtered out at the call site
     * (the variadic formatting is skipped entirely), so this is a hot-path hook.
     *
     * Default implementation returns LOG_LEVEL_DEBUG (no filtering), keeping
     * existing implementations source-compatible. Concrete loggers should
     * override this and back it with a thread-safe field (e.g. std::atomic)
     * so the level can be changed at runtime by the SDK consumer.
     */
    virtual LogLevel getMinLevel() const { return LOG_LEVEL_DEBUG; }
};

} // namespace agenui
