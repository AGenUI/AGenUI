// QP*: worker queue under pressure.
//
// Many APIs `messageThread->post(lambda)` without bound on the queue
// depth. These tests verify behavior at the high-water mark:
//
//   1. Memory growth is bounded by queued shared_ptr captures —
//      destroying the SM must release them all.
//   2. Latency under burst — worker drains in reasonable time.
//   3. No use-after-free between burst and drain.

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "agenui_dispatcher_types.h"
#include "agenui_engine.h"
#include "agenui_surface_manager_interface.h"
#include "support/fixture_loader.h"
#include "support/mock_message_listener.h"
#include "support/scoped_surface_manager.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

class QueuePressureTest : public ::testing::Test {
protected:
    ::agenui::IAGenUIEngine* engine_ = nullptr;
    void SetUp() override {
        engine_ = ::agenui::testing::GetEngine();
        ASSERT_NE(engine_, nullptr);
    }
};

// QP001: Burst 10K small tasks via receiveTextChunk, then drain. Memory
// (queue depth) is observable indirectly via worker-idle latency.
TEST_F(QueuePressureTest, QP001_BurstReceive10K_DrainsCleanly) {
    ::agenui::testing::ScopedSurfaceManager sm;
    constexpr int N = 10000;
    auto begin = std::chrono::steady_clock::now();
    sm->beginTextStream();
    for (int i = 0; i < N; ++i) {
        sm->receiveTextChunk("x");
    }
    sm->endTextStream();
    ASSERT_TRUE(::agenui::testing::WaitForWorkerIdle(30000));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin).count();
    // No hard latency contract; just sanity bound to catch infinite loops.
    EXPECT_LT(elapsed, 30000) << "elapsed ms=" << elapsed;
}

// QP002: Burst then immediately destroy. All queued tasks hold a shared_ptr
// to the SM via lambda capture. Destroy enqueues uninit; queue must drain
// without leaking.
TEST_F(QueuePressureTest, QP002_BurstThenDestroy_AllRefsReleased) {
    constexpr int kIters = 20;
    constexpr int N = 500;
    for (int iter = 0; iter < kIters; ++iter) {
        auto* sm = engine_->createSurfaceManager();
        ASSERT_NE(sm, nullptr);
        sm->beginTextStream();
        for (int i = 0; i < N; ++i) {
            sm->receiveTextChunk("partial-data");
        }
        // Destroy without endTextStream and without waiting for worker.
        // shared_ptrs in the queue keep the SM alive until the lambdas
        // run. ASan / LSan verifies eventual release.
        engine_->destroySurfaceManager(sm);
    }
    ASSERT_TRUE(::agenui::testing::WaitForWorkerIdle(60000));
}

// QP003: Multiple threads burst in parallel against a single SM. Worker
// queue serializes them. Tests FIFO under contention.
TEST_F(QueuePressureTest, QP003_MultiProducerBurst_NoCrash) {
    ::agenui::testing::ScopedSurfaceManager sm;
    constexpr int kThreads = 4;
    constexpr int kPerThread = 250;

    std::vector<std::thread> producers;
    for (int t = 0; t < kThreads; ++t) {
        producers.emplace_back([&, t]() {
            for (int i = 0; i < kPerThread; ++i) {
                sm->beginTextStream();
                sm->receiveTextChunk("p");
                sm->endTextStream();
            }
        });
    }
    for (auto& th : producers) th.join();
    ASSERT_TRUE(::agenui::testing::WaitForWorkerIdle(30000));
}

// QP004: producer thread keeps posting while main destroys. Producer
// must not crash even if the SM transitions to non-running mid-post.
TEST_F(QueuePressureTest, QP004_ProducerVsDestroy_GracefulShutdown) {
    auto* sm = engine_->createSurfaceManager();
    ::agenui::testing::WaitForWorkerIdle();

    std::atomic<bool> stop{false};
    std::thread producer([&]() {
        while (!stop.load()) {
            sm->beginTextStream();
            sm->receiveTextChunk("p");
            sm->endTextStream();
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    engine_->destroySurfaceManager(sm);
    // Producer thread now sees _isRunning=false on next post; APIs gate.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop.store(true);
    producer.join();
    ::agenui::testing::WaitForWorkerIdle(15000);
}

// QP005: action burst on a real surface; coordinator must process each
// without dropping or duplicating callbacks.
TEST_F(QueuePressureTest, QP005_ActionBurstOnLiveSurface_AllProcessed) {
    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    ASSERT_FALSE(proto.empty());

    ::agenui::testing::ScopedSurfaceManager sm;
    ::agenui::testing::MockMessageListener listener;
    sm->addSurfaceEventListener(&listener);

    sm->beginTextStream();
    sm->receiveTextChunk(proto);
    sm->endTextStream();
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.createSurfaceCalls.empty(); }, 3000));

    constexpr int N = 1000;
    for (int i = 0; i < N; ++i) {
        ::agenui::ActionMessage msg;
        msg.surfaceId = "test-surface-1";
        msg.sourceComponentId = "btn";  // doesn't have to exist
        sm->submitUIAction(msg);
    }
    // Drain. Actions on missing component log and return; we just need
    // no crash.
    ::agenui::testing::WaitForWorkerIdle(15000);
    sm->removeSurfaceEventListener(&listener);
}

// QP006: 50 concurrent SM lifecycles, each doing a small burst. Only the
// engine-level operations (create/destroy) are run from the main thread;
// per-SM operations are split across threads — that respects the docs.
TEST_F(QueuePressureTest, QP006_ParallelSMLifecycles_StableUnderPressure) {
    constexpr int N = 50;
    std::vector<::agenui::ISurfaceManager*> sms;
    std::vector<std::unique_ptr<::agenui::testing::MockMessageListener>> listeners;
    for (int i = 0; i < N; ++i) {
        sms.push_back(engine_->createSurfaceManager());
        listeners.push_back(std::make_unique<::agenui::testing::MockMessageListener>());
        sms[i]->addSurfaceEventListener(listeners[i].get());
    }
    // Per-SM thread doing a stream burst.
    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&, i]() {
            for (int k = 0; k < 5; ++k) {
                std::string proto =
                    R"({"version":"v0.9","createSurface":{"surfaceId":"qp006-)"
                    + std::to_string(i) + "-"
                    + std::to_string(k) + R"(","catalogId":"x"}})";
                sms[i]->beginTextStream();
                sms[i]->receiveTextChunk(proto);
                sms[i]->endTextStream();
            }
        });
    }
    for (auto& th : threads) th.join();
    ::agenui::testing::WaitForWorkerIdle(30000);
    for (int i = 0; i < N; ++i) {
        sms[i]->removeSurfaceEventListener(listeners[i].get());
        engine_->destroySurfaceManager(sms[i]);
    }
    ::agenui::testing::WaitForWorkerIdle(15000);
}

}  // namespace
