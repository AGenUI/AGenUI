// Regression test for the shared_ptr-based SurfaceManager lookup.
//
// The engine owns SurfaceManagers via shared_ptr. An earlier lookup returned
// `it->second.get()` and released _surfaceManagersMutex, so a concurrent
// destroySurfaceManager() could drop the last reference — on the worker thread,
// when the posted uninit lambda is destroyed — before the caller dereferenced
// the raw pointer. With the double-thread shape below that reproduced as a
// heap-use-after-free under ASan, and SIGSEGV in 25 of 25 plain-build runs.
//
// findSurfaceManagerShared() now copies the shared_ptr under the same mutex
// that guards the erase, so a caller gets either a live reference or nullptr.
// These tests must stay clean under ASan and never crash in a plain build.
// The raw-pointer lookup was removed rather than deprecated, so an unsafe call
// site is now a compile error.

#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

// Resolve instanceId -> shared_ptr, then use it, while another thread destroys
// the same instance. Mirrors the JNI/NAPI call shape.
TEST(FindSurfaceManagerSharedTest, SharedLookupVsConcurrentDestroy_NoUseAfterFree) {
    auto* engine = ::agenui::testing::GetEngine();
    ASSERT_NE(engine, nullptr);

    for (int round = 0; round < 400; ++round) {
        auto* created = engine->createSurfaceManager();
        if (!created) continue;

        const int instanceId = created->getInstanceId();
        std::atomic<bool> go{false};

        // Thread A: the JNI "use" side — lookup then call.
        std::thread user([&]() {
            while (!go.load(std::memory_order_acquire)) { /* spin */ }
            for (int i = 0; i < 64; ++i) {
                // === exactly findSurfaceManagerByInstanceId() post-fix ===
                std::shared_ptr<::agenui::ISurfaceManager> sm =
                    engine->findSurfaceManagerShared(instanceId);
                if (!sm) return;  // correctly gated
                // Shared ownership held for the whole call: cannot dangle.
                sm->beginTextStream();
                sm->receiveTextChunk("{\"version\":\"v0.9\"}");
                sm->endTextStream();
            }
        });

        // Thread B: the JNI "destroy" side.
        std::thread destroyer([&]() {
            while (!go.load(std::memory_order_acquire)) { /* spin */ }
            engine->destroySurfaceManager(created);
        });

        go.store(true, std::memory_order_release);
        user.join();
        destroyer.join();
    }

    ::agenui::testing::WaitForWorkerIdle(15000);
}

// Same race against the other lookup on this path: ThreadManager::
// getMessageThread(), reached indirectly by every SurfaceManager API. When it
// returned a raw IThread*, destroyThread() could `stop(); delete thread;` while
// a poster still held that pointer, so the poster's _queueMutex / _condition
// access touched freed memory. This drives destroy against live posting.
TEST(FindSurfaceManagerSharedTest, PostVsConcurrentDestroy_NoUseAfterFree) {
    auto* engine = ::agenui::testing::GetEngine();
    ASSERT_NE(engine, nullptr);

    for (int round = 0; round < 50; ++round) {
        auto* sm = engine->createSurfaceManager();
        if (!sm) continue;
        const int instanceId = sm->getInstanceId();

        std::atomic<bool> stop{false};
        std::thread poster([&]() {
            while (!stop.load(std::memory_order_acquire)) {
                auto handle = engine->findSurfaceManagerShared(instanceId);
                if (!handle) continue;
                handle->beginTextStream();
                handle->receiveTextChunk("{\"version\":\"v0.9\"}");
                handle->endTextStream();
            }
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        engine->destroySurfaceManager(sm);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        stop.store(true, std::memory_order_release);
        poster.join();
    }

    ::agenui::testing::WaitForWorkerIdle(15000);
}

}  // namespace
