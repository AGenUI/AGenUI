// SMC*: SurfaceManager lifecycle chaos.
//
// Single-thread but operationally chaotic patterns: rapid create/destroy,
// scrambled order, mid-stream destroy, malformed sequences, randomized
// op mix. Run under ASan + UBSan. Subset is also exercised by TSan via
// the agenui_tsan_tests target.

#include <algorithm>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "agenui_dispatcher_types.h"
#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_render_info_types.h"
#include "agenui_surface_manager_interface.h"
#include "support/fixture_loader.h"
#include "support/mock_message_listener.h"
#include "support/scoped_surface_manager.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

class SMChaosTest : public ::testing::Test {
protected:
    ::agenui::IAGenUIEngine* engine_ = nullptr;
    void SetUp() override {
        engine_ = ::agenui::testing::GetEngine();
        ASSERT_NE(engine_, nullptr);
    }
    void TearDown() override {
        ::agenui::testing::WaitForWorkerIdle(5000);
    }
};

// SMC001: rapid create/destroy — many small lifecycles back-to-back.
TEST_F(SMChaosTest, SMC001_Rapid_CreateDestroy_1000_NoLeak) {
    constexpr int kCount = 1000;
    for (int i = 0; i < kCount; ++i) {
        auto* sm = engine_->createSurfaceManager();
        ASSERT_NE(sm, nullptr) << "iter " << i;
        engine_->destroySurfaceManager(sm);
    }
    ::agenui::testing::WaitForWorkerIdle(10000);
}

// SMC002: batch-create then destroy in REVERSE order. Worker queue must
// process init() in arrival order and uninit() in destroy order without
// leaking shared_ptrs.
TEST_F(SMChaosTest, SMC002_BatchCreate_ReverseOrderDestroy) {
    constexpr int kCount = 100;
    std::vector<::agenui::ISurfaceManager*> sms;
    sms.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        sms.push_back(engine_->createSurfaceManager());
        ASSERT_NE(sms.back(), nullptr);
    }
    for (int i = kCount - 1; i >= 0; --i) {
        engine_->destroySurfaceManager(sms[i]);
    }
}

// SMC003: batch-create then destroy in SHUFFLED order.
TEST_F(SMChaosTest, SMC003_BatchCreate_ScrambledDestroy) {
    constexpr int kCount = 100;
    std::vector<::agenui::ISurfaceManager*> sms;
    for (int i = 0; i < kCount; ++i) {
        sms.push_back(engine_->createSurfaceManager());
    }
    std::mt19937 rng(0xC0FFEE);
    std::shuffle(sms.begin(), sms.end(), rng);
    for (auto* sm : sms) engine_->destroySurfaceManager(sm);
}

// SMC004: destroy in the middle of an active stream — uninit must clean
// up partial buffers without crashing.
TEST_F(SMChaosTest, SMC004_DestroyMidStream_NoCrash) {
    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    ASSERT_FALSE(proto.empty());

    constexpr int kCount = 50;
    for (int i = 0; i < kCount; ++i) {
        auto* sm = engine_->createSurfaceManager();
        sm->beginTextStream();
        sm->receiveTextChunk(proto.substr(0, proto.size() / 2));
        // Destroy without endTextStream. Buffer half-filled.
        engine_->destroySurfaceManager(sm);
    }
}

// SMC005: many beginTextStream without matching endTextStream. Each
// begin must reset the buffer and not leak.
TEST_F(SMChaosTest, SMC005_BeginWithoutEnd_Chain) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    for (int i = 0; i < 200; ++i) {
        sm->beginTextStream();
        sm->receiveTextChunk("partial-data-not-a-full-envelope");
        // intentional: no endTextStream
    }
    sm->endTextStream();
}

// SMC006: endTextStream without prior begin must be safe.
TEST_F(SMChaosTest, SMC006_EndWithoutBegin_Safe) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    for (int i = 0; i < 50; ++i) {
        sm->endTextStream();
    }
}

// SMC007: chunk arriving before begin enters compatibility mode.
TEST_F(SMChaosTest, SMC007_ChunkBeforeBegin_NoCrash) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    for (int i = 0; i < 50; ++i) {
        sm->receiveTextChunk("orphan-chunk");
    }
    sm->endTextStream();
}

// SMC008: pseudo-random mix of every public SM API. Exercises operation
// orderings that hand-written tests would never cover.
TEST_F(SMChaosTest, SMC008_RandomizedMixedOps_NoCrash) {
    auto create_proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    ASSERT_FALSE(create_proto.empty());

    std::mt19937 rng(0xDEADBEEF);
    std::vector<::agenui::ISurfaceManager*> active;

    constexpr int kOps = 1000;
    for (int i = 0; i < kOps; ++i) {
        // Bound the live set to 32 to cap memory.
        if (active.size() > 32) {
            engine_->destroySurfaceManager(active.front());
            active.erase(active.begin());
        }

        int op = rng() % 10;
        switch (op) {
        case 0: {  // create
            auto* sm = engine_->createSurfaceManager();
            if (sm) active.push_back(sm);
            break;
        }
        case 1: {  // destroy random
            if (!active.empty()) {
                size_t idx = rng() % active.size();
                engine_->destroySurfaceManager(active[idx]);
                active.erase(active.begin() + idx);
            }
            break;
        }
        case 2:
            if (!active.empty()) active[rng() % active.size()]->beginTextStream();
            break;
        case 3:
            if (!active.empty()) active[rng() % active.size()]->receiveTextChunk("x");
            break;
        case 4:
            if (!active.empty())
                active[rng() % active.size()]->receiveTextChunk(create_proto);
            break;
        case 5:
            if (!active.empty()) active[rng() % active.size()]->endTextStream();
            break;
        case 6: {  // submitUIAction
            if (!active.empty()) {
                ::agenui::ActionMessage msg;
                msg.surfaceId = "test-surface-1";
                msg.sourceComponentId = "btn";
                active[rng() % active.size()]->submitUIAction(msg);
            }
            break;
        }
        case 7: {  // submitUIDataModel
            if (!active.empty()) {
                ::agenui::SyncUIToDataMessage msg;
                msg.surfaceId = "test-surface-1";
                msg.componentId = "in";
                msg.change = R"({"value":1})";
                active[rng() % active.size()]->submitUIDataModel(msg);
            }
            break;
        }
        case 8: {  // onRenderFinish
            if (!active.empty()) {
                ::agenui::ComponentRenderInfo info;
                info.surfaceId = "test-surface-1";
                info.componentId = "btn";
                info.type = "Text";
                info.width = 10.0f;
                info.height = 10.0f;
                active[rng() % active.size()]->onRenderFinish(info);
            }
            break;
        }
        case 9: {  // onSurfaceSizeChanged
            if (!active.empty()) {
                ::agenui::SurfaceLayoutInfo info;
                info.surfaceId = "test-surface-1";
                info.width = 720.0f;
                info.height = 1280.0f;
                active[rng() % active.size()]->onSurfaceSizeChanged(info);
            }
            break;
        }
        }
    }
    // Drain
    for (auto* sm : active) engine_->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle(15000);
}

// SMC009: send a giant burst of envelopes, then destroy. All queued
// lambdas in the worker must hold shared_ptrs that get cleanly released.
TEST_F(SMChaosTest, SMC009_StreamBurstThenDestroy) {
    auto proto = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    ASSERT_FALSE(proto.empty());

    auto* sm = engine_->createSurfaceManager();
    ASSERT_NE(sm, nullptr);
    sm->beginTextStream();
    for (int i = 0; i < 500; ++i) {
        sm->receiveTextChunk(proto);
    }
    // Destroy without endTextStream: queued tasks still hold shared_ptr.
    engine_->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle(15000);
}

// SMC010: submit a UIAction in the gap between createSurfaceManager and
// init() running on the worker thread. The action lambda must observe a
// fully-constructed coordinator (or be safely ignored).
TEST_F(SMChaosTest, SMC010_ActionRightAfterCreate_BeforeInitRuns) {
    constexpr int kIters = 200;
    for (int i = 0; i < kIters; ++i) {
        auto* sm = engine_->createSurfaceManager();
        ASSERT_NE(sm, nullptr);

        ::agenui::ActionMessage msg;
        msg.surfaceId = "no-such";
        msg.sourceComponentId = "btn";
        sm->submitUIAction(msg);

        ::agenui::SyncUIToDataMessage data;
        data.surfaceId = "no-such";
        data.componentId = "in";
        data.change = "{}";
        sm->submitUIDataModel(data);

        engine_->destroySurfaceManager(sm);
    }
}

}  // namespace
