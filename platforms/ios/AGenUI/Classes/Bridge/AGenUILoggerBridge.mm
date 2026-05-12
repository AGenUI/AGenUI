//
//  AGenUIPerfLoggerBridge.mm
//  AGenUI
//
//  Created by acoder-ai-infra on 2026/4/29.
//

#import "AGenUILoggerBridge.h"
#import <AGenUI-Swift.h>
#include "agenui_logger_interface.h"

// MARK: - C++ Wrapper Class
class RuntimeLoggerImpl : public agenui::IRuntimeLogger {
public:
    RuntimeLoggerImpl(Logger *swiftLogger) : _swiftLogger(swiftLogger) {}
    
    void log(agenui::LogLevel level, const char* tag, const char* func, int line, const char* format, ...) {
        if (_swiftLogger.delegate == nil) {
            return;
        }
        // Format the message using variadic arguments
        va_list args;
        va_start(args, format);
        NSString *message = [[NSString alloc] initWithFormat:[NSString stringWithUTF8String:format] arguments:args];
        va_end(args);
        
        [_swiftLogger.delegate onLogWithLevel:(Level)level
                                          tag:(tag ? [NSString stringWithUTF8String:tag] : @"")
                                         func:(func ? [NSString stringWithUTF8String:func] : @"")
                                         line:line
                                      message:(message ? message : @"")];
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
