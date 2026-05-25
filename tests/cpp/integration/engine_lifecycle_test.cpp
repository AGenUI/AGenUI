// E*: AGenUIEngine entry / lifecycle tests.
//
// Architectural contract:
//   AGenUIEngine is a process-level singleton that is init'd ONCE and
//   destroy'd ONCE for the entire process lifetime. The global test
//   Environment (support/test_env.h) takes care of both. Individual
//   tests must NOT call destroyAGenUIEngine() — doing so would tear
//   down the engine that subsequent tests rely on.
//
//   initAGenUIEngine() is documented as idempotent: repeated calls
//   return the same instance. Tests below exercise that contract plus
//   the engine-level configuration APIs.

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_logger_interface.h"
#include "agenui_platform_layout_bridge.h"
#include "support/mock_platform_layout_bridge.h"
#include "support/mock_runtime_logger.h"
#include "support/test_env.h"

namespace {

class EngineLifecycleTest : public ::testing::Test {
    // No SetUp/TearDown: the global Environment owns the engine
    // lifecycle. Tests share a single, always-running engine.
};

// E001: init returns a valid (non-null) engine pointer. With the
// Environment already running, this is a re-entry of init and must
// return the existing instance.
TEST_F(EngineLifecycleTest, E001_InitEngine_ReturnsValidPointer) {
    auto* engine = ::agenui::initAGenUIEngine();
    EXPECT_NE(engine, nullptr);
    EXPECT_EQ(engine, ::agenui::getAGenUIEngine());
}

// E002: init is idempotent — repeated calls return the same instance.
TEST_F(EngineLifecycleTest, E002_InitEngine_RepeatedCall_Idempotent) {
    auto* a = ::agenui::initAGenUIEngine();
    auto* b = ::agenui::initAGenUIEngine();
    auto* c = ::agenui::initAGenUIEngine();
    EXPECT_NE(a, nullptr);
    EXPECT_EQ(a, b);
    EXPECT_EQ(b, c);
}

// E019: setPathConfig accepts valid JSON and returns true.
TEST_F(EngineLifecycleTest, E019_SetPathConfig_ValidJson_Succeeds) {
    auto* engine = ::agenui::getAGenUIEngine();
    ASSERT_NE(engine, nullptr);
    bool ok = engine->setPathConfig(R"({"templateDir":"/tmp/agenui_test_templates"})");
    EXPECT_TRUE(ok);
}

// E020: setPathConfig rejects invalid JSON and returns false.
TEST_F(EngineLifecycleTest, E020_SetPathConfig_InvalidJson_Fails) {
    auto* engine = ::agenui::getAGenUIEngine();
    ASSERT_NE(engine, nullptr);
    bool ok = engine->setPathConfig("not json{{");
    EXPECT_FALSE(ok);
}

// E009: setRuntimeLogger swap & restore. We MUST restore the default
// logger before the mock leaves scope, otherwise the engine would
// keep a dangling pointer for subsequent tests.
TEST_F(EngineLifecycleTest, E009_SetRuntimeLogger_CustomThenRestore) {
    auto* engine = ::agenui::getAGenUIEngine();
    ASSERT_NE(engine, nullptr);

    auto* original = engine->getRuntimeLogger();
    EXPECT_NE(original, nullptr) << "Default logger must never be null";

    auto* custom = ::agenui::testing::MockRuntimeLogger::CreateInstance();
    engine->setRuntimeLogger(custom);
    EXPECT_EQ(engine->getRuntimeLogger(), custom);

    // Restore default by passing nullptr.
    engine->setRuntimeLogger(nullptr);
    EXPECT_NE(engine->getRuntimeLogger(), nullptr);

    delete custom;
}

// E010: setPlatformLayoutBridge get/set symmetric. Restore null
// before the stack-allocated mock dies.
TEST_F(EngineLifecycleTest, E010_PlatformLayoutBridge_GetSetSymmetric) {
    auto* engine = ::agenui::getAGenUIEngine();
    ASSERT_NE(engine, nullptr);

    auto* before = engine->getPlatformLayoutBridge();

    ::agenui::testing::MockPlatformLayoutBridge bridge;
    engine->setPlatformLayoutBridge(&bridge);
    EXPECT_EQ(engine->getPlatformLayoutBridge(), &bridge);

    // Restore previous (typically nullptr) so we don't leave a dangling ref.
    engine->setPlatformLayoutBridge(before);
    EXPECT_EQ(engine->getPlatformLayoutBridge(), before);
}

// E011: getMeasurementManager is non-null on a running engine.
TEST_F(EngineLifecycleTest, E011_GetMeasurementManager_NotNull) {
    auto* engine = ::agenui::getAGenUIEngine();
    ASSERT_NE(engine, nullptr);
    EXPECT_NE(engine->getMeasurementManager(), nullptr);
}

// E013: setDayNightMode tolerates valid and invalid inputs without throwing.
TEST_F(EngineLifecycleTest, E013_SetDayNightMode_AcceptsValidValues) {
    auto* engine = ::agenui::getAGenUIEngine();
    ASSERT_NE(engine, nullptr);
    EXPECT_NO_THROW(engine->setDayNightMode("light"));
    EXPECT_NO_THROW(engine->setDayNightMode("dark"));
    EXPECT_NO_THROW(engine->setDayNightMode("invalid_mode"));
    EXPECT_NO_THROW(engine->setDayNightMode(""));
}

// E015
TEST_F(EngineLifecycleTest, E015_LoadThemeConfig_ValidJson_Succeeds) {
    auto* engine = ::agenui::getAGenUIEngine();
    ASSERT_NE(engine, nullptr);
    std::string err;
    bool ok = engine->loadThemeConfig("{}", err);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(err.empty());
}

// E016
TEST_F(EngineLifecycleTest, E016_LoadThemeConfig_InvalidJson_FailsWithMessage) {
    auto* engine = ::agenui::getAGenUIEngine();
    ASSERT_NE(engine, nullptr);
    std::string err;
    bool ok = engine->loadThemeConfig("not json{{", err);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(err.empty());
}

// E017: TokenParser requires a top-level `designTokens` object.
TEST_F(EngineLifecycleTest, E017_LoadDesignTokenConfig_ValidJson_Succeeds) {
    auto* engine = ::agenui::getAGenUIEngine();
    ASSERT_NE(engine, nullptr);
    std::string err;
    bool ok = engine->loadDesignTokenConfig(
        R"({"designTokens": {"primary": {"light": "#FF0000", "dark": "#00FF00"}}})",
        err);
    EXPECT_TRUE(ok) << err;
}

// E018
TEST_F(EngineLifecycleTest, E018_LoadDesignTokenConfig_InvalidJson_Fails) {
    auto* engine = ::agenui::getAGenUIEngine();
    ASSERT_NE(engine, nullptr);
    std::string err;
    bool ok = engine->loadDesignTokenConfig("##garbage", err);
    EXPECT_FALSE(ok);
}

// NOTE: The following historical cases are intentionally removed because
// they violate the "init once / destroy once per process" architectural
// contract:
//   - E003 GetEngine_BeforeInit_ReturnsNull
//   - E004 DestroyEngine_AfterInit_GetReturnsNull
//   - E005 DestroyEngine_NotInited_Safe
//   - E006 InitDestroyLoop_NoLeak
//   - E007 StartAfterStop_Recreates
//   - E012 DestroyEngine_GetMeasurement_NoCrash
// destroyAGenUIEngine() is the sole responsibility of the global test
// Environment (support/test_env.h::AGenUIEngineEnvironment::TearDown).

}  // namespace
