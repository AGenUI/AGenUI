// LB*: Listener add/remove boundary cases.
//
// Documents and locks down the dispatcher's behavior around tricky
// listener patterns: duplicate registration, self-removal during dispatch,
// add-during-dispatch, listeners that re-enter the SDK, and timing
// around init/uninit.

#include <gtest/gtest.h>

#include <vector>

#include "agenui_dispatcher_types.h"
#include "agenui_engine.h"
#include "agenui_message_listener.h"
#include "agenui_surface_manager_interface.h"
#include "support/fixture_loader.h"
#include "support/mock_message_listener.h"
#include "support/scoped_surface_manager.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

// LB001: registering the same listener twice causes each callback to be
// invoked twice (the dispatcher uses emplace_back without dedup).
TEST(ListenerBoundaryTest, LB001_AddSameListenerTwice_FiresTwice) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    ::agenui::testing::MockMessageListener listener;
    sm->addSurfaceEventListener(&listener);
    sm->addSurfaceEventListener(&listener);
    ::agenui::testing::WaitForWorkerIdle();

    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    sm->beginTextStream();
    sm->receiveTextChunk(proto);
    sm->endTextStream();
    EXPECT_TRUE(listener.waitFor(
        [&]() { return listener.createSurfaceCalls.size() >= 2; }, 3000));
    EXPECT_EQ(listener.createSurfaceCalls.size(), 2u);

    sm->removeSurfaceEventListener(&listener);
    sm->removeSurfaceEventListener(&listener);
}

// LB002: add twice + remove once → still fires once (remove uses break
// after first match).
TEST(ListenerBoundaryTest, LB002_AddTwiceRemoveOnce_StillFires) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    ::agenui::testing::MockMessageListener listener;
    sm->addSurfaceEventListener(&listener);
    sm->addSurfaceEventListener(&listener);
    sm->removeSurfaceEventListener(&listener);
    ::agenui::testing::WaitForWorkerIdle();

    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    sm->beginTextStream();
    sm->receiveTextChunk(proto);
    sm->endTextStream();
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.createSurfaceCalls.empty(); }, 3000));
    EXPECT_EQ(listener.createSurfaceCalls.size(), 1u);

    sm->removeSurfaceEventListener(&listener);
}

// LB003: a listener removes ANOTHER listener mid-dispatch.
//
// **DISABLED — surfaces a known engine bug.**
//
// `EventDispatcher::dispatch*()` iterates `_listeners` with a range-based
// for loop while holding a `std::mutex`. The mutex is NOT recursive (see
// `agenui_event_dispatcher.cpp`); on top of that, the loop reads through
// the live container, so when one listener's callback calls
// `removeEventListener(other)`, std::vector::erase invalidates the
// loop's cached `end()`, causing the next iteration to read past the
// (now shorter) buffer. AddressSanitizer flags this as a container-
// overflow at agenui_event_dispatcher.cpp:38.
//
// Fix in the engine should snapshot `_listeners` at the start of each
// dispatch and release the lock before invoking callbacks:
//
//     std::vector<IAGenUIMessageListener*> snapshot;
//     {
//         std::lock_guard<std::mutex> g(_mutex);
//         snapshot = _listeners;          // copy under lock
//     }
//     for (auto* listener : snapshot) {   // iterate the snapshot
//         if (listener) listener->onCreateSurface(msg);
//     }
//
// To enable this regression test once the engine is fixed, drop the
// `DISABLED_` prefix. Run it explicitly via:
//   ./build/host/agenui_concurrency_tests \
//       --gtest_also_run_disabled_tests \
//       --gtest_filter='ListenerBoundaryTest.DISABLED_LB003*'
namespace {
class RemoverListener : public ::agenui::testing::MockMessageListener {
public:
    ::agenui::ISurfaceManager* sm = nullptr;
    ::agenui::IAGenUIMessageListener* target = nullptr;
    void onCreateSurface(const ::agenui::CreateSurfaceMessage& m) override {
        ::agenui::testing::MockMessageListener::onCreateSurface(m);
        if (sm && target) sm->removeSurfaceEventListener(target);
    }
};
}  // namespace

TEST(ListenerBoundaryTest, DISABLED_LB003_ListenerRemovesOther_EngineBugIteratorInvalidation) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);

    ::agenui::testing::MockMessageListener other;
    RemoverListener remover;
    remover.sm = sm.get();
    remover.target = &other;

    sm->addSurfaceEventListener(&remover);
    sm->addSurfaceEventListener(&other);
    ::agenui::testing::WaitForWorkerIdle();

    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    sm->beginTextStream();
    sm->receiveTextChunk(proto);
    sm->endTextStream();
    ::agenui::testing::WaitForWorkerIdle();

    EXPECT_EQ(remover.createSurfaceCalls.size(), 1u);
    // Whether `other` saw the event depends on iteration order, but the
    // suite must not deadlock or crash.
    sm->removeSurfaceEventListener(&remover);
    sm->removeSurfaceEventListener(&other);
}

// LB004: a listener adds ANOTHER listener mid-dispatch. The new one must
// not be invoked for the in-flight event but is registered for future ones.
//
// **DISABLED — surfaces a known engine bug (deadlock).**
//
// `EventDispatcher::dispatch*()` holds a non-recursive `std::mutex` while
// invoking each listener (`agenui_event_dispatcher.cpp:37` etc.). When a
// listener's `onCreateSurface` callback calls back into
// `addSurfaceEventListener` -> `EventDispatcher::addEventListener`
// (`agenui_event_dispatcher.cpp:11`), the same thread tries to acquire
// the same non-recursive mutex a second time and self-deadlocks. The
// worker thread blocks forever on its own mutex, so the test process
// hangs and never returns to gtest.
//
// The same root cause covers LB003 (remove during dispatch) — dispatch
// must not hold the listener mutex across user callbacks at all. The
// engine fix is the snapshot-and-release pattern documented above LB003.
//
// To enable this regression test once the engine is fixed, drop the
// `DISABLED_` prefix. Run it explicitly via:
//   ./build/host/agenui_concurrency_tests \
//       --gtest_also_run_disabled_tests \
//       --gtest_filter='ListenerBoundaryTest.DISABLED_LB004*'
namespace {
class AdderListener : public ::agenui::testing::MockMessageListener {
public:
    ::agenui::ISurfaceManager* sm = nullptr;
    ::agenui::IAGenUIMessageListener* toAdd = nullptr;
    void onCreateSurface(const ::agenui::CreateSurfaceMessage& m) override {
        ::agenui::testing::MockMessageListener::onCreateSurface(m);
        if (sm && toAdd && createSurfaceCalls.size() == 1) {
            sm->addSurfaceEventListener(toAdd);
        }
    }
};
}  // namespace

TEST(ListenerBoundaryTest, DISABLED_LB004_ListenerAddsOther_EngineBugNonRecursiveMutexDeadlock) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);

    ::agenui::testing::MockMessageListener freshListener;
    AdderListener adder;
    adder.sm = sm.get();
    adder.toAdd = &freshListener;

    sm->addSurfaceEventListener(&adder);
    ::agenui::testing::WaitForWorkerIdle();

    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    sm->beginTextStream();
    sm->receiveTextChunk(proto);
    sm->endTextStream();
    ::agenui::testing::WaitForWorkerIdle();

    EXPECT_EQ(adder.createSurfaceCalls.size(), 1u);
    sm->removeSurfaceEventListener(&adder);
    sm->removeSurfaceEventListener(&freshListener);
}

// LB005: remove listener mid-stream (between two chunks). The dispatch
// for the post-removal envelope must not invoke the listener.
TEST(ListenerBoundaryTest, LB005_RemoveListenerMidStream_NoCallbackAfter) {
    auto* engine = ::agenui::testing::GetEngine();
    ASSERT_NE(engine, nullptr);

    auto* sm = engine->createSurfaceManager();
    ASSERT_NE(sm, nullptr);
    ::agenui::testing::MockMessageListener listener;
    sm->addSurfaceEventListener(&listener);

    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    sm->beginTextStream();
    sm->receiveTextChunk(proto.substr(0, proto.size() / 2));
    sm->removeSurfaceEventListener(&listener);
    sm->receiveTextChunk(proto.substr(proto.size() / 2));
    sm->endTextStream();
    ::agenui::testing::WaitForWorkerIdle();

    // Listener removed BEFORE the parser saw the complete envelope, so it
    // must not fire.
    EXPECT_EQ(listener.createSurfaceCalls.size(), 0u);

    engine->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle();
}

// LB006: mass register / mass remove. Final state has zero registered
// listeners; subsequent dispatch must not fire any.
TEST(ListenerBoundaryTest, LB006_MassAddRemove_FinalStateClean) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);

    constexpr int N = 100;
    std::vector<::agenui::testing::MockMessageListener> listeners(N);
    for (int i = 0; i < N; ++i) sm->addSurfaceEventListener(&listeners[i]);
    for (int i = 0; i < N; ++i) sm->removeSurfaceEventListener(&listeners[i]);
    ::agenui::testing::WaitForWorkerIdle();

    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    sm->beginTextStream();
    sm->receiveTextChunk(proto);
    sm->endTextStream();
    ::agenui::testing::WaitForWorkerIdle();

    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(listeners[i].createSurfaceCalls.size(), 0u) << "i=" << i;
    }
}

// LB007: 3 listeners, remove the middle one. The other two still fire.
TEST(ListenerBoundaryTest, LB007_RemoveMiddleListener_NeighborsStillFire) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    ::agenui::testing::MockMessageListener a, b, c;
    sm->addSurfaceEventListener(&a);
    sm->addSurfaceEventListener(&b);
    sm->addSurfaceEventListener(&c);
    sm->removeSurfaceEventListener(&b);
    ::agenui::testing::WaitForWorkerIdle();

    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    sm->beginTextStream();
    sm->receiveTextChunk(proto);
    sm->endTextStream();
    EXPECT_TRUE(a.waitFor([&]() { return !a.createSurfaceCalls.empty(); }, 3000));
    EXPECT_TRUE(c.waitFor([&]() { return !c.createSurfaceCalls.empty(); }, 3000));
    EXPECT_EQ(a.createSurfaceCalls.size(), 1u);
    EXPECT_EQ(b.createSurfaceCalls.size(), 0u);
    EXPECT_EQ(c.createSurfaceCalls.size(), 1u);

    sm->removeSurfaceEventListener(&a);
    sm->removeSurfaceEventListener(&c);
}

// LB008: listener added BEFORE init() runs (cached path), removed AFTER
// init runs. Must NOT receive subsequent events.
TEST(ListenerBoundaryTest, LB008_AddBeforeInit_RemoveAfterInit) {
    auto* engine = ::agenui::testing::GetEngine();
    ASSERT_NE(engine, nullptr);

    auto* sm = engine->createSurfaceManager();
    ASSERT_NE(sm, nullptr);

    ::agenui::testing::MockMessageListener listener;
    // sm hasn't run init() yet (queued on worker). addListener -> cached.
    sm->addSurfaceEventListener(&listener);
    ::agenui::testing::WaitForWorkerIdle();  // init() applies cached listener
    sm->removeSurfaceEventListener(&listener);

    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    sm->beginTextStream();
    sm->receiveTextChunk(proto);
    sm->endTextStream();
    ::agenui::testing::WaitForWorkerIdle();

    EXPECT_EQ(listener.createSurfaceCalls.size(), 0u);

    engine->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle();
}

// LB009: listener added AND removed BEFORE init() runs. Cached listener
// vector must be drained, listener gets nothing.
TEST(ListenerBoundaryTest, LB009_AddAndRemoveBeforeInit_NoFire) {
    auto* engine = ::agenui::testing::GetEngine();
    ASSERT_NE(engine, nullptr);

    auto* sm = engine->createSurfaceManager();
    ASSERT_NE(sm, nullptr);

    ::agenui::testing::MockMessageListener listener;
    sm->addSurfaceEventListener(&listener);
    sm->removeSurfaceEventListener(&listener);
    ::agenui::testing::WaitForWorkerIdle();

    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    sm->beginTextStream();
    sm->receiveTextChunk(proto);
    sm->endTextStream();
    ::agenui::testing::WaitForWorkerIdle();

    EXPECT_EQ(listener.createSurfaceCalls.size(), 0u);
    engine->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle();
}

// LB010: a listener that re-enters the SDK (calls submitUIAction) inside
// its callback. Must not deadlock — submit posts to worker, which is
// already executing the dispatch.
namespace {
class ReentrantListener : public ::agenui::testing::MockMessageListener {
public:
    ::agenui::ISurfaceManager* sm = nullptr;
    void onCreateSurface(const ::agenui::CreateSurfaceMessage& m) override {
        ::agenui::testing::MockMessageListener::onCreateSurface(m);
        if (sm) {
            ::agenui::ActionMessage action;
            action.surfaceId = m.surfaceId;
            action.sourceComponentId = "btn";
            sm->submitUIAction(action);
        }
    }
};
}  // namespace

TEST(ListenerBoundaryTest, LB010_ListenerReentersSDK_NoDeadlock) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    ReentrantListener rl;
    rl.sm = sm.get();
    sm->addSurfaceEventListener(&rl);

    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    sm->beginTextStream();
    sm->receiveTextChunk(proto);
    sm->endTextStream();
    ::agenui::testing::WaitForWorkerIdle();

    EXPECT_GE(rl.createSurfaceCalls.size(), 1u);
    sm->removeSurfaceEventListener(&rl);
}

}  // namespace
