#pragma once
#include "agenui_logger_interface.h"


namespace agenui {
extern IRuntimeLogger* gRuntimeLogger;

/**
 * @brief Internal helper: set the global runtime logger.
 * Only called by AGenUIEngine::setRuntimeLogger. Pass nullptr to restore the built-in default.
 */
void setRuntimeLoggerInternal(IRuntimeLogger* logger);

/**
 * @brief Internal helper: get the global runtime logger (never null).
 */
IRuntimeLogger* getRuntimeLoggerInternal();

/**
 * @brief Set the minimum log level of the built-in default logger.
 *
 * Only affects the default logger (used when no custom IRuntimeLogger is
 * injected). Custom loggers expose their own threshold via getMinLevel().
 * Thread-safe.
 */
void setDefaultLogMinLevel(LogLevel level);

/**
 * @brief Get the minimum log level of the built-in default logger.
 */
LogLevel getDefaultLogMinLevel();
}

// Capture gRuntimeLogger into a local to avoid TOCTOU between the threshold check
// and the log() call (the global pointer can be swapped out from another thread).
// Threshold check happens before the variadic format → filtered-out messages skip
// the entire vsnprintf cost.
#define LOG_LEVEL(level, tag, format, ...)                                                \
    do {                                                                                  \
        agenui::IRuntimeLogger* _agenui_logger = agenui::gRuntimeLogger;                  \
        if (_agenui_logger && (level) >= _agenui_logger->getMinLevel()) {                 \
            _agenui_logger->log(level, tag, __FUNCTION__, __LINE__, format, ##__VA_ARGS__); \
        }                                                                                 \
    } while (0);

#define LOG_DEBUG(tag, format, ...) \
    LOG_LEVEL(LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#define LOG_INFO(tag, format, ...) \
    LOG_LEVEL(LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#define LOG_WARN(tag, format, ...) \
    LOG_LEVEL(LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)
#define LOG_ERROR(tag, format, ...) \
    LOG_LEVEL(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#define LOG_FATAL(tag, format, ...) \
    LOG_LEVEL(LOG_LEVEL_FATAL, tag, format, ##__VA_ARGS__)
#define LOG_PERFORMANCE(tag, format, ...) \
    LOG_LEVEL(LOG_LEVEL_PERFORMANCE, tag, format, ##__VA_ARGS__)


#define AGENUI_LOG(fmt, ...) LOG_DEBUG("AGenUI", fmt, ##__VA_ARGS__)
#define AGENUI_PERFORMANCE_LOG(tag, fmt, ...) LOG_PERFORMANCE(tag, fmt, ##__VA_ARGS__)
