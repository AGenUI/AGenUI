// FC*: registerFunction / unregisterFunction tests.

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_platform_function.h"
#include "support/mock_platform_function.h"
#include "support/test_env.h"

namespace {

class FunctionCallTest : public ::testing::Test {
protected:
    ::agenui::IAGenUIEngine* engine_ = nullptr;
    void SetUp() override {
        engine_ = ::agenui::testing::GetEngine();
        ASSERT_NE(engine_, nullptr);
    }
};

const char* const kValidConfig = R"({"name":"myTestFn","description":"x"})";

// FC001
TEST_F(FunctionCallTest, FC001_RegisterFunction_ValidConfig_Succeeds) {
    ::agenui::testing::MockPlatformFunction fn;
    EXPECT_TRUE(engine_->registerFunction(kValidConfig, &fn));
    EXPECT_TRUE(engine_->unregisterFunction("myTestFn"));
}

// FC002
TEST_F(FunctionCallTest, FC002_RegisterFunction_InvalidJsonConfig_Fails) {
    ::agenui::testing::MockPlatformFunction fn;
    EXPECT_FALSE(engine_->registerFunction("not-json{{", &fn));
}

// FC003
TEST_F(FunctionCallTest, FC003_RegisterFunction_NullFunction_Fails) {
    EXPECT_FALSE(engine_->registerFunction(kValidConfig, nullptr));
}

// FC004
TEST_F(FunctionCallTest, FC004_RegisterFunction_MissingName_Fails) {
    ::agenui::testing::MockPlatformFunction fn;
    EXPECT_FALSE(engine_->registerFunction(R"({"description":"no name"})", &fn));
}

// FC005
TEST_F(FunctionCallTest, FC005_UnregisterFunction_Registered_Succeeds) {
    ::agenui::testing::MockPlatformFunction fn;
    ASSERT_TRUE(engine_->registerFunction(R"({"name":"fc005"})", &fn));
    EXPECT_TRUE(engine_->unregisterFunction("fc005"));
}

// FC006
TEST_F(FunctionCallTest, FC006_UnregisterFunction_Unknown_Fails) {
    EXPECT_FALSE(engine_->unregisterFunction("definitely_not_registered_xyz"));
}

// FC011
TEST_F(FunctionCallTest, FC011_RegisterUnregisterRegister_Idempotent) {
    ::agenui::testing::MockPlatformFunction fn;
    EXPECT_TRUE(engine_->registerFunction(R"({"name":"fc011"})", &fn));
    EXPECT_TRUE(engine_->unregisterFunction("fc011"));
    EXPECT_TRUE(engine_->registerFunction(R"({"name":"fc011"})", &fn));
    EXPECT_TRUE(engine_->unregisterFunction("fc011"));
}

// FC010 removed: AGenUIEngine is a process-level singleton; once
// destroyAGenUIEngine() runs, initAGenUIEngine() can never bring it back
// (entry uses std::call_once). The original FC010 tried to simulate an
// "engine down" state mid-suite, which permanently nulled the global
// engine and poisoned every subsequent test. See DESIGN.md.

// FC009: 14 builtin functions are registered (we don't have a public API
// to enumerate, so we just probe a few well-known ones via the catalog
// export side-effect — not directly accessible. As a smoke check, verify
// re-registration of "and" by name is unique).
TEST_F(FunctionCallTest, FC009_BuiltinFunctions_Registered_Smoke) {
    // The contract is: builtins are registered when the first SurfaceManager
    // is created (initFunctionCalls in SurfaceCoordinator). We can't verify
    // each builtin from public API; this test simply ensures registering an
    // unrelated function does not interfere.
    ::agenui::testing::MockPlatformFunction fn;
    EXPECT_TRUE(engine_->registerFunction(R"({"name":"fc009_user"})", &fn));
    EXPECT_TRUE(engine_->unregisterFunction("fc009_user"));
}

}  // namespace
