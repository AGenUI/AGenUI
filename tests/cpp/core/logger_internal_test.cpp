#include <gtest/gtest.h>
#include "agenui_logger_internal.h"

using namespace agenui;

// =============================================================================
// Logger Internal Tests — Logger injection and level management
// =============================================================================

namespace {
// Simple mock logger for testing
class LoggerTestMock : public IRuntimeLogger {
public:
    int logCallCount = 0;
    LogLevel lastLevel = LOG_LEVEL_DEBUG;
    std::string lastTag;
    LogLevel minLevel = LOG_LEVEL_DEBUG;

    void log(LogLevel level, const char* tag, const char* func, int line, const char* format, ...) override {
        ++logCallCount;
        lastLevel = level;
        lastTag = tag ? tag : "";
    }

    LogLevel getMinLevel() const override {
        return minLevel;
    }
};

class LoggerInternalTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean state: restore default logger and reset min level
        setRuntimeLoggerInternal(nullptr);
        setDefaultLogMinLevel(LOG_LEVEL_DEBUG);
    }

    void TearDown() override {
        // Restore default logger after each test
        setRuntimeLoggerInternal(nullptr);
        setDefaultLogMinLevel(LOG_LEVEL_DEBUG);
    }
};

// --- setRuntimeLoggerInternal / getRuntimeLoggerInternal ---

TEST_F(LoggerInternalTest, GetRuntimeLogger_Default_NeverNull) {
    auto* logger = getRuntimeLoggerInternal();
    EXPECT_NE(logger, nullptr);
}

TEST_F(LoggerInternalTest, SetRuntimeLogger_Custom_ReturnsSamePointer) {
    LoggerTestMock mock;
    setRuntimeLoggerInternal(&mock);
    EXPECT_EQ(getRuntimeLoggerInternal(), &mock);
}

TEST_F(LoggerInternalTest, SetRuntimeLogger_Nullptr_RestoresDefault) {
    LoggerTestMock mock;
    setRuntimeLoggerInternal(&mock);
    setRuntimeLoggerInternal(nullptr);
    auto* logger = getRuntimeLoggerInternal();
    EXPECT_NE(logger, nullptr);
    EXPECT_NE(logger, &mock);
}

// --- setDefaultLogMinLevel / getDefaultLogMinLevel ---

TEST_F(LoggerInternalTest, DefaultLogMinLevel_SetAndGet) {
    setDefaultLogMinLevel(LOG_LEVEL_WARN);
    EXPECT_EQ(getDefaultLogMinLevel(), LOG_LEVEL_WARN);
}

TEST_F(LoggerInternalTest, DefaultLogMinLevel_SetMultipleTimes) {
    setDefaultLogMinLevel(LOG_LEVEL_ERROR);
    EXPECT_EQ(getDefaultLogMinLevel(), LOG_LEVEL_ERROR);
    setDefaultLogMinLevel(LOG_LEVEL_DEBUG);
    EXPECT_EQ(getDefaultLogMinLevel(), LOG_LEVEL_DEBUG);
}

// --- LOG_LEVEL macro filtering ---

TEST_F(LoggerInternalTest, LogMacro_AboveMinLevel_Invokes) {
    LoggerTestMock mock;
    mock.minLevel = LOG_LEVEL_DEBUG;
    setRuntimeLoggerInternal(&mock);

    LOG_INFO("TestTag", "message %d", 42);
    EXPECT_EQ(mock.logCallCount, 1);
    EXPECT_EQ(mock.lastTag, "TestTag");
}

TEST_F(LoggerInternalTest, LogMacro_BelowMinLevel_Filtered) {
    LoggerTestMock mock;
    mock.minLevel = LOG_LEVEL_ERROR;
    setRuntimeLoggerInternal(&mock);

    LOG_DEBUG("TestTag", "should be filtered");
    LOG_INFO("TestTag", "should be filtered");
    LOG_WARN("TestTag", "should be filtered");
    EXPECT_EQ(mock.logCallCount, 0);
}

TEST_F(LoggerInternalTest, LogMacro_AtMinLevel_Invokes) {
    LoggerTestMock mock;
    mock.minLevel = LOG_LEVEL_WARN;
    setRuntimeLoggerInternal(&mock);

    LOG_WARN("TestTag", "at threshold");
    EXPECT_EQ(mock.logCallCount, 1);
}

TEST_F(LoggerInternalTest, LogMacro_AllLevels_CountCorrect) {
    LoggerTestMock mock;
    mock.minLevel = LOG_LEVEL_DEBUG;
    setRuntimeLoggerInternal(&mock);

    LOG_DEBUG("T", "d");
    LOG_INFO("T", "i");
    LOG_WARN("T", "w");
    LOG_ERROR("T", "e");
    LOG_FATAL("T", "f");
    EXPECT_EQ(mock.logCallCount, 5);
}

// --- Custom logger isolation ---

TEST_F(LoggerInternalTest, CustomLogger_DefaultMinLevel_Independent) {
    // Changing default min level should NOT affect custom logger behavior
    LoggerTestMock mock;
    mock.minLevel = LOG_LEVEL_DEBUG;
    setRuntimeLoggerInternal(&mock);
    setDefaultLogMinLevel(LOG_LEVEL_FATAL); // This should not affect mock

    LOG_DEBUG("T", "test");
    EXPECT_EQ(mock.logCallCount, 1); // Mock uses its own minLevel
}
} // namespace
