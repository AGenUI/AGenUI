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
}

#define LOG_LEVEL(level, tag, format, ...)                                                \
    do {                                                                                  \
        if (gRuntimeLogger) {                                                             \
            gRuntimeLogger->log(level, tag, __FUNCTION__, __LINE__, format, ##__VA_ARGS__); \
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
