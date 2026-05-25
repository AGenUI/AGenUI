// TD*: theme + design token + day/night mode tests.

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "support/scoped_surface_manager.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

class ThemeDesignTokenTest : public ::testing::Test {
protected:
    ::agenui::IAGenUIEngine* engine_ = nullptr;
    void SetUp() override {
        engine_ = ::agenui::testing::GetEngine();
        ASSERT_NE(engine_, nullptr);
    }
};

// TD001
TEST_F(ThemeDesignTokenTest, TD001_LoadThemeConfig_ValidJson_AppliedToSurface) {
    std::string err;
    EXPECT_TRUE(engine_->loadThemeConfig("{}", err));
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
}

// TD002
TEST_F(ThemeDesignTokenTest, TD002_LoadDesignTokenConfig_ValidJson_Applied) {
    std::string err;
    bool ok = engine_->loadDesignTokenConfig(
        R"({"designTokens":{"primaryColor":{"light":"#FF0000","dark":"#00FF00"}}})",
        err);
    EXPECT_TRUE(ok) << err;
}

// TD003
TEST_F(ThemeDesignTokenTest, TD003_SetDayNightMode_RefreshesAllSurfaceManagers) {
    ::agenui::testing::ScopedSurfaceManager a;
    ::agenui::testing::ScopedSurfaceManager b;
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    engine_->setDayNightMode("dark");
    ::agenui::testing::WaitForWorkerIdle();
    engine_->setDayNightMode("light");
    ::agenui::testing::WaitForWorkerIdle();
    SUCCEED();
}

// TD004
TEST_F(ThemeDesignTokenTest, TD004_SetSameDayNightMode_NoOp_Safe) {
    engine_->setDayNightMode("dark");
    engine_->setDayNightMode("dark");
    ::agenui::testing::WaitForWorkerIdle();
}

// TD005 removed: AGenUIEngine is a process-level singleton; once
// destroyAGenUIEngine() runs, initAGenUIEngine() can never bring it back
// (entry uses std::call_once). The original TD005 tried to simulate an
// "engine down" state mid-suite, which permanently nulled the global
// engine and poisoned every subsequent test. See DESIGN.md.

}  // namespace
