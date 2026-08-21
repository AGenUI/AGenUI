// MSM*: Multi-SurfaceManager coexistence tests.
//
// Verifies that several SurfaceManager instances can carry independent
// streams, listeners, and lifecycles without cross-contamination.

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
#include "support/fixture_loader.h"
#include "support/mock_message_listener.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

class MultiSMTest : public ::testing::Test {
protected:
    ::agenui::IAGenUIEngine* engine_ = nullptr;
    void SetUp() override {
        engine_ = ::agenui::testing::GetEngine();
        ASSERT_NE(engine_, nullptr);
    }
};

// MSM001: 5 SMs, each receives a distinct createSurface. Each listener
// only observes events from its own SM.
TEST_F(MultiSMTest, MSM001_FiveSMs_ReceiveDistinctProtocols) {
    constexpr int N = 5;
    std::vector<::agenui::ISurfaceManager*> sms;
    std::vector<std::unique_ptr<::agenui::testing::MockMessageListener>> listeners;
    for (int i = 0; i < N; ++i) {
        sms.push_back(engine_->createSurfaceManager());
        listeners.push_back(std::make_unique<::agenui::testing::MockMessageListener>());
        sms[i]->addSurfaceEventListener(listeners[i].get());
    }
    for (int i = 0; i < N; ++i) {
        std::string proto =
            R"({"version":"v0.9","createSurface":{"surfaceId":"msm001-)"
            + std::to_string(i) + R"(","catalogId":"x"}})";
        sms[i]->beginTextStream();
        sms[i]->receiveTextChunk(proto);
        sms[i]->endTextStream();
    }
    for (int i = 0; i < N; ++i) {
        EXPECT_TRUE(listeners[i]->waitFor(
            [&]() { return !listeners[i]->createSurfaceCalls.empty(); }, 3000));
        EXPECT_EQ(listeners[i]->createSurfaceCalls.size(), 1u);
        if (!listeners[i]->createSurfaceCalls.empty()) {
            EXPECT_EQ(listeners[i]->createSurfaceCalls.front().surfaceId,
                      "msm001-" + std::to_string(i));
        }
    }
    for (int i = 0; i < N; ++i) {
        sms[i]->removeSurfaceEventListener(listeners[i].get());
        engine_->destroySurfaceManager(sms[i]);
    }
    ::agenui::testing::WaitForWorkerIdle();
}

// MSM002: same surfaceId across two SMs is independent (each SM has its
// own coordinator/surfaces map).
TEST_F(MultiSMTest, MSM002_SameSurfaceIdAcrossSMs_Isolated) {
    auto* a = engine_->createSurfaceManager();
    auto* b = engine_->createSurfaceManager();
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    ::agenui::testing::MockMessageListener la, lb;
    a->addSurfaceEventListener(&la);
    b->addSurfaceEventListener(&lb);

    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    ASSERT_FALSE(proto.empty());

    a->beginTextStream(); a->receiveTextChunk(proto); a->endTextStream();
    b->beginTextStream(); b->receiveTextChunk(proto); b->endTextStream();

    EXPECT_TRUE(la.waitFor([&]() { return !la.createSurfaceCalls.empty(); }, 2000));
    EXPECT_TRUE(lb.waitFor([&]() { return !lb.createSurfaceCalls.empty(); }, 2000));
    EXPECT_EQ(la.createSurfaceCalls.size(), 1u);
    EXPECT_EQ(lb.createSurfaceCalls.size(), 1u);

    a->removeSurfaceEventListener(&la);
    b->removeSurfaceEventListener(&lb);
    engine_->destroySurfaceManager(a);
    engine_->destroySurfaceManager(b);
    ::agenui::testing::WaitForWorkerIdle();
}

// MSM003: a single shared listener across N SMs receives all events.
TEST_F(MultiSMTest, MSM003_SharedListener_ReceivesEverything) {
    constexpr int N = 4;
    std::vector<::agenui::ISurfaceManager*> sms;
    ::agenui::testing::MockMessageListener shared;
    for (int i = 0; i < N; ++i) {
        sms.push_back(engine_->createSurfaceManager());
        sms[i]->addSurfaceEventListener(&shared);
    }
    for (int i = 0; i < N; ++i) {
        std::string proto =
            R"({"version":"v0.9","createSurface":{"surfaceId":"msm003-)"
            + std::to_string(i) + R"(","catalogId":"x"}})";
        sms[i]->beginTextStream();
        sms[i]->receiveTextChunk(proto);
        sms[i]->endTextStream();
    }
    EXPECT_TRUE(shared.waitFor(
        [&]() { return shared.createSurfaceCalls.size() >= (size_t)N; }, 5000));
    EXPECT_EQ(shared.createSurfaceCalls.size(), (size_t)N);
    for (int i = 0; i < N; ++i) {
        sms[i]->removeSurfaceEventListener(&shared);
        engine_->destroySurfaceManager(sms[i]);
    }
    ::agenui::testing::WaitForWorkerIdle();
}

// MSM004: destroy half of the SMs while the other half still has pending
// stream data — the survivors must finish without disturbance.
TEST_F(MultiSMTest, MSM004_DestroyHalfWhileOthersStream) {
    constexpr int N = 10;
    std::vector<::agenui::ISurfaceManager*> sms;
    std::vector<std::unique_ptr<::agenui::testing::MockMessageListener>> listeners;
    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    ASSERT_FALSE(proto.empty());

    for (int i = 0; i < N; ++i) {
        sms.push_back(engine_->createSurfaceManager());
        listeners.push_back(std::make_unique<::agenui::testing::MockMessageListener>());
        sms[i]->addSurfaceEventListener(listeners[i].get());
    }
    // Begin all + half the data
    for (auto* sm : sms) {
        sm->beginTextStream();
        sm->receiveTextChunk(proto.substr(0, proto.size() / 2));
    }
    // Tear down odd indices mid-stream. Listeners must NOT get callbacks
    // from destroyed SMs.
    for (int i = 0; i < N; i += 2) {
        sms[i]->removeSurfaceEventListener(listeners[i].get());
        engine_->destroySurfaceManager(sms[i]);
    }
    // Continue streaming on the survivors.
    for (int i = 1; i < N; i += 2) {
        sms[i]->receiveTextChunk(proto.substr(proto.size() / 2));
        sms[i]->endTextStream();
    }
    // Survivors must observe their createSurface; destroyed ones must not.
    for (int i = 1; i < N; i += 2) {
        EXPECT_TRUE(listeners[i]->waitFor(
            [&]() { return !listeners[i]->createSurfaceCalls.empty(); }, 5000));
    }
    for (int i = 0; i < N; i += 2) {
        EXPECT_EQ(listeners[i]->createSurfaceCalls.size(), 0u);
    }
    for (int i = 1; i < N; i += 2) {
        sms[i]->removeSurfaceEventListener(listeners[i].get());
        engine_->destroySurfaceManager(sms[i]);
    }
    ::agenui::testing::WaitForWorkerIdle();
}

// MSM005: mass create then mass destroy. Stress test for instanceId and
// shared_ptr churn.
TEST_F(MultiSMTest, MSM005_MassCreate_MassDestroy) {
    constexpr int N = 200;
    std::vector<::agenui::ISurfaceManager*> sms;
    sms.reserve(N);
    for (int i = 0; i < N; ++i) {
        auto* sm = engine_->createSurfaceManager();
        ASSERT_NE(sm, nullptr);
        sms.push_back(sm);
    }
    // All N must have unique IDs and be findable.
    for (auto* sm : sms) {
        EXPECT_EQ(engine_->findSurfaceManagerShared(sm->getInstanceId()).get(), sm);
    }
    for (auto* sm : sms) engine_->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle(15000);
}

// MSM006: byte-interleaved streams on N SMs. Each SM's parser must
// reassemble its own envelope without confusing other SMs' bytes.
TEST_F(MultiSMTest, MSM006_ByteInterleavedStreams_Isolated) {
    constexpr int N = 4;
    std::vector<::agenui::ISurfaceManager*> sms;
    std::vector<std::unique_ptr<::agenui::testing::MockMessageListener>> listeners;
    for (int i = 0; i < N; ++i) {
        sms.push_back(engine_->createSurfaceManager());
        listeners.push_back(std::make_unique<::agenui::testing::MockMessageListener>());
        sms[i]->addSurfaceEventListener(listeners[i].get());
    }

    std::vector<std::string> protos;
    for (int i = 0; i < N; ++i) {
        protos.push_back(
            R"({"version":"v0.9","createSurface":{"surfaceId":"msm006-)"
            + std::to_string(i) + R"(","catalogId":"x"}})");
    }
    for (auto* sm : sms) sm->beginTextStream();
    size_t maxLen = 0;
    for (const auto& p : protos) maxLen = std::max(maxLen, p.size());
    for (size_t pos = 0; pos < maxLen; ++pos) {
        for (int i = 0; i < N; ++i) {
            if (pos < protos[i].size()) {
                sms[i]->receiveTextChunk(std::string(1, protos[i][pos]));
            }
        }
    }
    for (auto* sm : sms) sm->endTextStream();

    for (int i = 0; i < N; ++i) {
        EXPECT_TRUE(listeners[i]->waitFor(
            [&]() { return !listeners[i]->createSurfaceCalls.empty(); }, 5000));
        if (!listeners[i]->createSurfaceCalls.empty()) {
            EXPECT_EQ(listeners[i]->createSurfaceCalls.front().surfaceId,
                      "msm006-" + std::to_string(i));
        }
    }
    for (int i = 0; i < N; ++i) {
        sms[i]->removeSurfaceEventListener(listeners[i].get());
        engine_->destroySurfaceManager(sms[i]);
    }
    ::agenui::testing::WaitForWorkerIdle();
}

}  // namespace
