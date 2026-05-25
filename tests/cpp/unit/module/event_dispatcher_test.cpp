// ED*: Event-dispatch tests through the public ISurfaceManager API.
//
// These tests verify listener registration and callback delivery using
// only the public IAGenUIEngine + ISurfaceManager interfaces, never
// touching the internal EventDispatcher directly.

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
#include "support/mock_message_listener.h"
#include "support/scoped_surface_manager.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

class EventDispatcherBlackBoxTest : public ::testing::Test {
protected:
    ::agenui::testing::ScopedSurfaceManager sm;

    void SetUp() override {
        ASSERT_TRUE(sm);
    }
};

// ED001: addSurfaceEventListener with nullptr is safely ignored.
TEST_F(EventDispatcherBlackBoxTest, ED001_AddNullListener_Ignored) {
    EXPECT_NO_THROW(sm->addSurfaceEventListener(nullptr));
}

// ED002: removeSurfaceEventListener with nullptr is safely ignored.
TEST_F(EventDispatcherBlackBoxTest, ED002_RemoveNullListener_Ignored) {
    EXPECT_NO_THROW(sm->removeSurfaceEventListener(nullptr));
}

// ED003: Multiple listeners registered on the same ISurfaceManager all
// receive the onCreateSurface callback when a createSurface protocol is
// delivered through the public receiveTextChunk API.
TEST_F(EventDispatcherBlackBoxTest, ED003_CreateSurface_AllListenersInvoked) {
    ::agenui::testing::MockMessageListener a, b;
    sm->addSurfaceEventListener(&a);
    sm->addSurfaceEventListener(&b);

    std::string json = R"({"version":"v0.9","createSurface":{"surfaceId":"ED003","catalogId":"test","theme":{},"sendDataModel":false,"animated":true}})";

    sm->beginTextStream();
    sm->receiveTextChunk(json);
    sm->endTextStream();

    EXPECT_TRUE(a.waitFor(
        [&]() { return !a.createSurfaceCalls.empty(); }, 2000));
    EXPECT_TRUE(b.waitFor(
        [&]() { return !b.createSurfaceCalls.empty(); }, 2000));

    EXPECT_EQ(a.createSurfaceCalls.size(), 1u);
    EXPECT_EQ(b.createSurfaceCalls.size(), 1u);

    sm->removeSurfaceEventListener(&a);
    sm->removeSurfaceEventListener(&b);
}

// ED004: After removeSurfaceEventListener, no further callbacks are
// delivered to the removed listener.
TEST_F(EventDispatcherBlackBoxTest, ED004_RemoveListener_NoFurtherCallbacks) {
    ::agenui::testing::MockMessageListener listener;
    sm->addSurfaceEventListener(&listener);
    sm->removeSurfaceEventListener(&listener);

    std::string json = R"({"version":"v0.9","createSurface":{"surfaceId":"ED004","catalogId":"test","theme":{},"sendDataModel":false,"animated":true}})";

    sm->beginTextStream();
    sm->receiveTextChunk(json);
    sm->endTextStream();
    ::agenui::testing::WaitForWorkerIdle();

    EXPECT_EQ(listener.createSurfaceCalls.size(), 0u);
}

// ED005: Components update callbacks (onComponentsAdd / onComponentsUpdate)
// are correctly dispatched to registered listeners via the public API.
TEST_F(EventDispatcherBlackBoxTest, ED005_UpdateComponents_ListenersReceiveAdd) {
    ::agenui::testing::MockMessageListener listener;
    sm->addSurfaceEventListener(&listener);

    // Step 1: createSurface
    std::string create = R"({"version":"v0.9","createSurface":{"surfaceId":"ED005","catalogId":"test","theme":{},"sendDataModel":false,"animated":true}})";
    sm->beginTextStream();
    sm->receiveTextChunk(create);
    sm->endTextStream();
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.createSurfaceCalls.empty(); }, 2000));
    listener.clear();

    // Step 2: updateComponents
    std::string update = R"({"version":"v0.9","updateComponents":{"surfaceId":"ED005","version":"1","components":[{"id":"c1","component":"Text","text":"hello"}]}})";
    sm->beginTextStream();
    sm->receiveTextChunk(update);
    sm->endTextStream();
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));

    sm->removeSurfaceEventListener(&listener);
}

}  // namespace
