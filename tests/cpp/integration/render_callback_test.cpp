// RC*: render / surface size callback tests.

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_render_info_types.h"
#include "agenui_surface_manager_interface.h"
#include "support/fixture_loader.h"
#include "support/mock_message_listener.h"
#include "support/scoped_surface_manager.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

// RC001
TEST(RenderCallbackTest, RC001_OnRenderFinish_PostedToWorker_NoCrash) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    ::agenui::ComponentRenderInfo info;
    info.surfaceId = "no-such";
    info.componentId = "x";
    info.type = "Text";
    info.width = 10.f;
    info.height = 10.f;
    EXPECT_NO_THROW(sm->onRenderFinish(info));
    ::agenui::testing::WaitForWorkerIdle();
}

// RC002
TEST(RenderCallbackTest, RC002_OnSurfaceSizeChanged_PostedToWorker_NoCrash) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    ::agenui::SurfaceLayoutInfo info;
    info.surfaceId = "no-such";
    info.width = 200.f;
    info.height = 300.f;
    EXPECT_NO_THROW(sm->onSurfaceSizeChanged(info));
    ::agenui::testing::WaitForWorkerIdle();
}

// RC003
TEST(RenderCallbackTest, RC003_TabsIndexChange_SafeOnUnknownSurface) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    ::agenui::ComponentRenderInfo info;
    info.surfaceId = "missing";
    info.componentId = "tabs1";
    info.type = "TabsIndexChange";
    info.selectedIndex = 2;
    EXPECT_NO_THROW(sm->onRenderFinish(info));
    ::agenui::testing::WaitForWorkerIdle();
}

}  // namespace
