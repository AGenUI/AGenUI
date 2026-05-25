// EC*: AGenUIEngine init / API stress (NOT lifecycle chaos).
//
// Architectural contract:
//   AGenUIEngine is a process-level singleton. initAGenUIEngine() is
//   called ONCE by the global test Environment; destroyAGenUIEngine()
//   is called ONCE at process exit. Tests MUST NOT destroy the engine
//   mid-suite — doing so would tear down the instance subsequent
//   tests rely on.
//
//   What is still legitimate to stress here:
//     * Calling initAGenUIEngine() many times (must be idempotent and
//       always return the same instance).
//     * Calling initAGenUIEngine() concurrently from multiple threads
//       (a contract violation in production, but the engine entry
//       must still degrade safely without crashing or returning
//       different pointers).
//     * Hammering engine-level config APIs in tight loops.
//
// The historical destroy/reinit-based chaos cases (EC001-EC005,
// EC007-EC009) were intentionally removed when the engine contract
// was clarified to "init/destroy at most once per process".

#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "support/test_env.h"

namespace {

class EngineChaosTest : public ::testing::Test {
    // No SetUp/TearDown: the global Environment owns engine lifecycle.
};

// EC006: init is idempotent — repeated calls return the same instance.
TEST_F(EngineChaosTest, EC006_InitMultipleTimes_SameInstance) {
    auto* a = ::agenui::initAGenUIEngine();
    auto* b = ::agenui::initAGenUIEngine();
    auto* c = ::agenui::initAGenUIEngine();
    auto* d = ::agenui::initAGenUIEngine();
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a, b);
    EXPECT_EQ(b, c);
    EXPECT_EQ(c, d);
    EXPECT_EQ(::agenui::getAGenUIEngine(), a);
}

// EC007: hammer engine-level config APIs in a tight loop without ever
// destroying the engine. Validates that repeated re-configuration is
// stable (no crash, no corruption visible to subsequent tests).
TEST_F(EngineChaosTest, EC007_RepeatedConfigCalls_Safe) {
    auto* engine = ::agenui::getAGenUIEngine();
    ASSERT_NE(engine, nullptr);

    constexpr int kIterations = 50;
    for (int i = 0; i < kIterations; ++i) {
        std::string err;
        EXPECT_TRUE(engine->loadThemeConfig("{}", err));
        engine->loadDesignTokenConfig(R"({"designTokens":{}})", err);
        engine->setDayNightMode(i % 2 == 0 ? "light" : "dark");

        auto* sm = engine->createSurfaceManager();
        ASSERT_NE(sm, nullptr);
        engine->destroySurfaceManager(sm);
    }
}

// EC010: contract-violating concurrent init from multiple threads. The
// SDK contract says all engine APIs run on the main thread; this test
// asserts the entry layer doesn't crash and always hands back the
// SAME (already-running) instance even under racy callers. (TSan may
// still report races on the entry-layer mutex / atomic — those are
// expected and informational.)
TEST_F(EngineChaosTest, EC010_InitFromMultipleThreads_SameInstance) {
    auto* expected = ::agenui::getAGenUIEngine();
    ASSERT_NE(expected, nullptr) << "engine must already be running";

    constexpr int kThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> mismatchCount{0};
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&]() {
            auto* engine = ::agenui::initAGenUIEngine();
            if (engine != expected) {
                ++mismatchCount;
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(mismatchCount.load(), 0)
        << "concurrent init must always return the singleton";
    EXPECT_EQ(::agenui::getAGenUIEngine(), expected);
}

}  // namespace
