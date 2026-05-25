// LR*: lifecycle race tests.

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
#include "support/mock_message_listener.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

// LR003: serialized create/destroy from a single thread (the documented
// contract). We do NOT call engine->createSurfaceManager from multiple
// threads — engine-level multi-instance map is not protected by a mutex
// and the contract states all engine APIs are called from the main thread.
//
// The thread-safety of a single SurfaceManager's APIs is exercised by
// the tests in thread_safety_test.cpp.
TEST(LifecycleRaceTest, LR003_SerialCreateDestroySurfaceManagers) {
    auto* engine = ::agenui::testing::GetEngine();
    ASSERT_NE(engine, nullptr);

    constexpr int kIters = 50;
    for (int k = 0; k < kIters; ++k) {
        auto* sm = engine->createSurfaceManager();
        ASSERT_NE(sm, nullptr);
        engine->destroySurfaceManager(sm);
    }
    ::agenui::testing::WaitForWorkerIdle();
}

// LR006: addListener while init() is in flight on the worker thread.
TEST(LifecycleRaceTest, LR006_AddListener_DuringInit) {
    auto* engine = ::agenui::testing::GetEngine();
    constexpr int kRounds = 20;
    for (int i = 0; i < kRounds; ++i) {
        ::agenui::testing::MockMessageListener listener;
        auto* sm = engine->createSurfaceManager();
        ASSERT_NE(sm, nullptr);
        // Race: add listener immediately, before init() lambda runs on the
        // worker thread. The SurfaceManager caches it for now and applies
        // it once init completes.
        sm->addSurfaceEventListener(&listener);
        ::agenui::testing::WaitForWorkerIdle();
        sm->removeSurfaceEventListener(&listener);
        engine->destroySurfaceManager(sm);
        ::agenui::testing::WaitForWorkerIdle();
    }
}

}  // namespace
