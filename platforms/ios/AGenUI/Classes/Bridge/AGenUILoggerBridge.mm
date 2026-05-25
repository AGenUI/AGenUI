//
//  AGenUIPerfLoggerBridge.mm
//  AGenUI
//
//  Created by acoder-ai-infra on 2026/4/29.
//

#import "AGenUILoggerBridge.h"
#import <AGenUI-Swift.h>
#include "agenui_logger_interface.h"
#include <cstdarg>
#include <cstdio>

// MARK: - C++ Wrapper Class
class RuntimeLoggerImpl : public agenui::IRuntimeLogger {
public:
    RuntimeLoggerImpl(Logger *swiftLogger) : _swiftLogger(swiftLogger) {}

    // Bridge the Swift `Logger.shared.minimumLevel` value into the C++ filter so
    // callers in the C++ core skip variadic formatting for filtered-out levels.
    agenui::LogLevel getMinLevel() const override {
        return static_cast<agenui::LogLevel>(_swiftLogger.minimumLevel);
    }

    void log(agenui::LogLevel level, const char* tag, const char* func, int line, const char* format, ...) override {
        // Respect the global enable flag so users can fully silence the SDK.
        if (!_swiftLogger.isEnabled) {
            return;
        }

        // Use vsnprintf + NSUTF8StringEncoding instead of NSString's format
        // APIs: their `%s` decodes the C string as MacRoman on Apple, which
        // mojibakes UTF-8 multi-byte payloads (e.g. Chinese).
        va_list args;
        va_start(args, format);

        va_list argsCopy;
        va_copy(argsCopy, args);
        int needed = vsnprintf(nullptr, 0, format, argsCopy);
        va_end(argsCopy);

        NSString *message = nil;
        if (needed > 0) {
            char *buffer = new char[static_cast<size_t>(needed) + 1];
            buffer[0] = '\0';
            vsnprintf(buffer, static_cast<size_t>(needed) + 1, format, args);
            message = [[NSString alloc] initWithBytes:buffer
                                               length:static_cast<NSUInteger>(needed)
                                             encoding:NSUTF8StringEncoding];
            delete[] buffer;
        }
        va_end(args);
        if (message == nil) {
            message = @"";
        }

        NSString *tagStr  = (tag  ? [NSString stringWithUTF8String:tag]  : @"");
        NSString *funcStr = (func ? [NSString stringWithUTF8String:func] : @"");
        NSString *msgStr  = (message ? message : @"");

        if (_swiftLogger.delegate != nil) {
            [_swiftLogger.delegate onLogWithLevel:(Level)level
                                              tag:tagStr
                                             func:funcStr
                                             line:line
                                          message:msgStr];
            return;
        }

        // No custom logger registered → fallback to NSLog so the SDK is never silent by default.
        // Format aligned with Android's logToAndroidLog: "[LEVEL] [tag] [func@line] message".
        NSString *levelStr = nil;
        switch (level) {
            case agenui::LOG_LEVEL_DEBUG:       levelStr = @"DEBUG";       break;
            case agenui::LOG_LEVEL_INFO:        levelStr = @"INFO";        break;
            case agenui::LOG_LEVEL_WARN:        levelStr = @"WARN";        break;
            case agenui::LOG_LEVEL_ERROR:       levelStr = @"ERROR";       break;
            case agenui::LOG_LEVEL_FATAL:       levelStr = @"FATAL";       break;
            case agenui::LOG_LEVEL_PERFORMANCE: levelStr = @"PERF";        break;
            default:                            levelStr = @"DEBUG";       break;
        }
        NSString *logTag = (tagStr.length > 0 ? tagStr : @"AGenUI");
        if (funcStr.length > 0) {
            NSLog(@"[%@] [%@] [%@@%d] %@", levelStr, logTag, funcStr, line, msgStr);
        } else {
            NSLog(@"[%@] [%@] %@", levelStr, logTag, msgStr);
        }
    }
    
private:
    Logger* _swiftLogger;
};

// MARK: - AGenUIPerfLoggerBridge Implementation

@implementation AGenUILoggerBridge {
    RuntimeLoggerImpl *_runtimeLogerImpl;
}

- (instancetype)init {
    if (self = [super init]) {
        _runtimeLogerImpl = new RuntimeLoggerImpl(Logger.shared);
    }
    return self;
}

- (void)dealloc {
    if (_runtimeLogerImpl) {
        delete _runtimeLogerImpl;
        _runtimeLogerImpl = nullptr;
    }
}

- (void *)cppRumtimeLoggerPointer {
    return _runtimeLogerImpl;
}

@end
