// Recording IRuntimeLogger for tests.

#pragma once

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "agenui_logger_interface.h"

namespace agenui {
namespace testing {

class MockRuntimeLogger : public ::agenui::IRuntimeLogger {
public:
    struct Entry {
        ::agenui::LogLevel level;
        std::string tag;
        std::string func;
        int line;
        std::string formatted;
    };

    void log(::agenui::LogLevel level, const char* tag, const char* func,
             int line, const char* format, ...) override {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back({level,
                            tag ? tag : "",
                            func ? func : "",
                            line,
                            std::string(buffer)});
    }

    std::vector<Entry> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    // Construct on heap so AGenUIEngine can hold non-owning raw pointer.
    // Test code owns the pointer and must delete via this concrete subclass.
    static MockRuntimeLogger* CreateInstance() {
        return new MockRuntimeLogger();
    }

    ~MockRuntimeLogger() override = default;

private:
    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
};

}  // namespace testing
}  // namespace agenui
