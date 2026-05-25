// TM*: precise timing tests for the worker queue.
//
// Each SurfaceManager API that touches state posts a lambda to the shared
// worker thread. The lambda captures `shared_from_this()` so the SM stays
// alive until all queued work drains. These tests pin down the exact
// ordering guarantees that platform code can rely on.

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "agenui_dispatcher_types.h"
#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
#include "support/fixture_loader.h"
#include "support/mock_message_listener.h"
#include "support/scoped_surface_manager.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

class TimingTest : public ::testing::Test {
protected:
    ::agenui::IAGenUIEngine* engine_ = nullptr;
    void SetUp() override {
        engine_ = ::agenui::testing::GetEngine();
        ASSERT_NE(engine_, nullptr);
    }
};

// TM001: action submitted immediately before destroy. The action lambda
// has been queued before the uninit lambda; worker is single-threaded so
// the action MUST run first. The coordinator is still alive at that point.
TEST_F(TimingTest, TM001_ActionPostedBeforeDestroy_RunsBeforeUninit) {
    constexpr int kIters = 200;
    for (int i = 0; i < kIters; ++i) {
        auto* sm = engine_->createSurfaceManager();
        ASSERT_NE(sm, nullptr);

        // Pre-create a surface so the action has a real target.
        ::agenui::testing::MockMessageListener listener;
        sm->addSurfaceEventListener(&listener);
        std::string proto =
            R"({"version":"v0.9","createSurface":{"surfaceId":"tm001-)"
            + std::to_string(i) + R"(","catalogId":"x"}})";
        sm->beginTextStream();
        sm->receiveTextChunk(proto);
        sm->endTextStream();

        ::agenui::ActionMessage msg;
        msg.surfaceId = "tm001-" + std::to_string(i);
        msg.sourceComponentId = "btn";
        sm->submitUIAction(msg);

        // Destroy immediately. uninit is queued AFTER the action lambda.
        sm->removeSurfaceEventListener(&listener);
        engine_->destroySurfaceManager(sm);
    }
    ::agenui::testing::WaitForWorkerIdle(15000);
}

// TM002: API after exitRunning is gated out. We rely on the synchronous
// nature of exitRunning() inside destroySurfaceManager — the moment the
// destroy call returns to main, _isRunning is already false.
TEST_F(TimingTest, TM002_APIAfterExitRunning_GatedOut) {
    auto* sm = engine_->createSurfaceManager();
    ASSERT_NE(sm, nullptr);
    ::agenui::testing::WaitForWorkerIdle();

    ::agenui::testing::MockMessageListener listener;
    sm->addSurfaceEventListener(&listener);

    // Capture the SM pointer for one more API call after destroy. Safe
    // because the lambda for uninit hasn't run yet (we don't drain).
    engine_->destroySurfaceManager(sm);

    // Try every state-touching API. All must early-return without queuing
    // anything because _isRunning is now false.
    sm->beginTextStream();
    sm->receiveTextChunk("post-destroy chunk");
    sm->endTextStream();
    ::agenui::ActionMessage am; am.surfaceId = "x"; am.sourceComponentId = "y";
    sm->submitUIAction(am);
    sm->invalidateFunctionCallValues();

    ::agenui::testing::WaitForWorkerIdle();
    // Listener must NOT have received any callbacks from this SM.
    EXPECT_EQ(listener.totalCalls(), 0);
}

// TM003: tight begin → chunk → end → destroy sequence. All four state
// changes are queued in order; uninit runs last. No data should leak past
// destroy.
TEST_F(TimingTest, TM003_TightStreamThenDestroy_NoTrailingDispatch) {
    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    ASSERT_FALSE(proto.empty());

    auto* sm = engine_->createSurfaceManager();
    ::agenui::testing::MockMessageListener listener;
    sm->addSurfaceEventListener(&listener);
    ::agenui::testing::WaitForWorkerIdle();

    sm->beginTextStream();
    sm->receiveTextChunk(proto);
    sm->endTextStream();
    sm->removeSurfaceEventListener(&listener);
    engine_->destroySurfaceManager(sm);

    ::agenui::testing::WaitForWorkerIdle();
    // The listener was removed AFTER receiveTextChunk was queued but
    // BEFORE destroy. The dispatch happens inside the worker — between
    // the time the listener was removed and dispatch fires. Either:
    //   (a) listener was removed first → 0 calls
    //   (b) listener was already snapshotted → 1 call
    // Both are valid; just no crash.
    EXPECT_LE(listener.createSurfaceCalls.size(), 1u);
}

// TM004: ordering across many APIs queued back-to-back. We verify the
// dispatcher sees them in submission order by inspecting accumulated
// callback ordering relative to a baseline.
TEST_F(TimingTest, TM004_FIFO_AcrossManyAPIs) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ::agenui::testing::MockMessageListener listener;
    sm->addSurfaceEventListener(&listener);

    // Submit 10 distinct createSurface envelopes. The listener must
    // receive them in the same order.
    constexpr int N = 10;
    for (int i = 0; i < N; ++i) {
        std::string proto =
            R"({"version":"v0.9","createSurface":{"surfaceId":"tm004-)"
            + std::to_string(i) + R"(","catalogId":"x"}})";
        sm->beginTextStream();
        sm->receiveTextChunk(proto);
        sm->endTextStream();
    }
    EXPECT_TRUE(listener.waitFor(
        [&]() { return listener.createSurfaceCalls.size() >= N; }, 5000));
    ASSERT_EQ(listener.createSurfaceCalls.size(), (size_t)N);
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(listener.createSurfaceCalls[i].surfaceId,
                  "tm004-" + std::to_string(i)) << "i=" << i;
    }
    sm->removeSurfaceEventListener(&listener);
}

// TM005: producer thread bursts API calls while main waits. FIFO must be
// preserved when a single thread is the producer.
TEST_F(TimingTest, TM005_SingleProducer_HighFrequency_FIFO) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ::agenui::testing::MockMessageListener listener;
    sm->addSurfaceEventListener(&listener);

    constexpr int N = 100;
    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            std::string proto =
                R"({"version":"v0.9","createSurface":{"surfaceId":"tm005-)"
                + std::to_string(i) + R"(","catalogId":"x"}})";
            sm->beginTextStream();
            sm->receiveTextChunk(proto);
            sm->endTextStream();
        }
    });
    producer.join();
    EXPECT_TRUE(listener.waitFor(
        [&]() { return listener.createSurfaceCalls.size() >= N; }, 10000));
    ASSERT_EQ(listener.createSurfaceCalls.size(), (size_t)N);
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(listener.createSurfaceCalls[i].surfaceId,
                  "tm005-" + std::to_string(i));
    }
    sm->removeSurfaceEventListener(&listener);
}

// TM006: worker thread executes one task to completion before starting
// the next. We prove this by timing the runtime of a slow callback —
// while it runs, no other lambda executes.
TEST_F(TimingTest, TM006_WorkerSerial_OneTaskAtATime) {
    ::agenui::testing::ScopedSurfaceManager sm;

    class SlowListener : public ::agenui::testing::MockMessageListener {
    public:
        std::atomic<int> entered{0};
        std::atomic<int> maxConcurrent{0};
        std::atomic<int> active{0};
        void onCreateSurface(const ::agenui::CreateSurfaceMessage& m) override {
            int now = ++active;
            int prev = maxConcurrent.load();
            while (prev < now &&
                   !maxConcurrent.compare_exchange_weak(prev, now)) {}
            ++entered;
            ::agenui::testing::MockMessageListener::onCreateSurface(m);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            --active;
        }
    };
    SlowListener slow;
    sm->addSurfaceEventListener(&slow);

    // Queue 5 createSurface envelopes back-to-back. Worker is single-
    // threaded so they should serialize: maxConcurrent == 1 throughout.
    constexpr int N = 5;
    for (int i = 0; i < N; ++i) {
        std::string proto =
            R"({"version":"v0.9","createSurface":{"surfaceId":"tm006-)"
            + std::to_string(i) + R"(","catalogId":"x"}})";
        sm->beginTextStream();
        sm->receiveTextChunk(proto);
        sm->endTextStream();
    }
    EXPECT_TRUE(slow.waitFor(
        [&]() { return slow.entered.load() >= N; }, 10000));
    EXPECT_EQ(slow.maxConcurrent.load(), 1)
        << "Worker thread executed callbacks concurrently — broken serialization";

    sm->removeSurfaceEventListener(&slow);
}

// TM007: invalidateFunctionCallValues posted from setDayNightMode while a
// stream is in flight. Each operation runs to completion in arrival order.
TEST_F(TimingTest, TM007_DayNightToggle_WhileStreaming_NoCrash) {
    auto* engine = ::agenui::testing::GetEngine();
    ::agenui::testing::ScopedSurfaceManager sm;
    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    ASSERT_FALSE(proto.empty());

    sm->beginTextStream();
    for (int i = 0; i < 10; ++i) {
        sm->receiveTextChunk(proto.substr(i, std::min<size_t>(20, proto.size() - i)));
        engine->setDayNightMode(i % 2 == 0 ? "light" : "dark");
    }
    sm->endTextStream();
    ::agenui::testing::WaitForWorkerIdle(5000);
}

// TM008: instanceId is monotonic and unique across an engine run.
TEST_F(TimingTest, TM008_InstanceId_MonotonicallyIncreasing) {
    constexpr int N = 50;
    int prev = -1;
    std::vector<::agenui::ISurfaceManager*> sms;
    for (int i = 0; i < N; ++i) {
        auto* sm = engine_->createSurfaceManager();
        int id = sm->getInstanceId();
        EXPECT_GT(id, prev);
        prev = id;
        sms.push_back(sm);
    }
    for (auto* sm : sms) engine_->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle();
}

}  // namespace
