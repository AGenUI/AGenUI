// Sanity test: verify the test harness, gtest linkage, and engine entry point.
//
// If this file fails to compile or run, nothing else will.

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "support/test_env.h"

namespace {

TEST(SanityTest, GTestRuns) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(SanityTest, EngineIsRunning) {
    auto* engine = ::agenui::testing::GetEngine();
    ASSERT_NE(engine, nullptr) << "Engine not initialized by test environment";
    // MeasurementManager is created at engine start; non-null is a strong
    // signal that start() ran end-to-end.
    EXPECT_NE(engine->getMeasurementManager(), nullptr);
}

}  // namespace
