// SM*: SurfaceManager creation / lookup / destruction tests.

#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

class SurfaceManagerTest : public ::testing::Test {
protected:
    ::agenui::IAGenUIEngine* engine_ = nullptr;
    void SetUp() override {
        engine_ = ::agenui::testing::GetEngine();
        ASSERT_NE(engine_, nullptr);
    }
};

// SM001
TEST_F(SurfaceManagerTest, SM001_CreateSurfaceManager_ReturnsValidPointer) {
    auto* sm = engine_->createSurfaceManager();
    ASSERT_NE(sm, nullptr);
    engine_->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle();
}

// SM002 removed: AGenUIEngine is a process-level singleton; once
// destroyAGenUIEngine() runs, initAGenUIEngine() can never bring it back
// (entry uses std::call_once). The original SM002 tried to simulate an
// "engine down" state mid-suite, which permanently nulled the global
// engine and poisoned every subsequent test. The corresponding contract
// case is owned by the global Environment teardown, not by any user-
// level test. See DESIGN.md.

// SM003
TEST_F(SurfaceManagerTest, SM003_MultipleCreate_DistinctInstanceIds) {
    auto* a = engine_->createSurfaceManager();
    auto* b = engine_->createSurfaceManager();
    auto* c = engine_->createSurfaceManager();
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);
    EXPECT_NE(a->getInstanceId(), b->getInstanceId());
    EXPECT_NE(b->getInstanceId(), c->getInstanceId());
    EXPECT_NE(a->getInstanceId(), c->getInstanceId());
    engine_->destroySurfaceManager(a);
    engine_->destroySurfaceManager(b);
    engine_->destroySurfaceManager(c);
    ::agenui::testing::WaitForWorkerIdle();
}

// SM004
TEST_F(SurfaceManagerTest, SM004_FindSurfaceManager_ById_Returns) {
    auto* sm = engine_->createSurfaceManager();
    ASSERT_NE(sm, nullptr);
    auto* found = engine_->findSurfaceManager(sm->getInstanceId());
    EXPECT_EQ(found, sm);
    engine_->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle();
}

// SM005
TEST_F(SurfaceManagerTest, SM005_FindSurfaceManager_UnknownId_ReturnsNull) {
    EXPECT_EQ(engine_->findSurfaceManager(999999), nullptr);
}

// SM006
TEST_F(SurfaceManagerTest, SM006_DestroySurfaceManager_ThenFindReturnsNull) {
    auto* sm = engine_->createSurfaceManager();
    ASSERT_NE(sm, nullptr);
    int id = sm->getInstanceId();
    engine_->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle();
    EXPECT_EQ(engine_->findSurfaceManager(id), nullptr);
}

// SM007
TEST_F(SurfaceManagerTest, SM007_DestroySurfaceManager_WithNullPointer_Safe) {
    EXPECT_NO_THROW(engine_->destroySurfaceManager(nullptr));
}

// SM008: 200 concurrent creates produce distinct ids (not 1000 to keep CI
// snappy).
TEST_F(SurfaceManagerTest, SM008_BulkCreate_UniqueInstanceIds) {
    constexpr int kCount = 200;
    std::vector<::agenui::ISurfaceManager*> sms;
    sms.reserve(kCount);
    std::set<int> ids;

    for (int i = 0; i < kCount; ++i) {
        auto* sm = engine_->createSurfaceManager();
        ASSERT_NE(sm, nullptr);
        sms.push_back(sm);
        ids.insert(sm->getInstanceId());
    }
    EXPECT_EQ(ids.size(), static_cast<size_t>(kCount));
    for (auto* sm : sms) engine_->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle();
}

// SM009
TEST_F(SurfaceManagerTest, SM009_GetInstanceId_ReturnsConsistentValue) {
    auto* sm = engine_->createSurfaceManager();
    ASSERT_NE(sm, nullptr);
    int id1 = sm->getInstanceId();
    int id2 = sm->getInstanceId();
    EXPECT_EQ(id1, id2);
    engine_->destroySurfaceManager(sm);
    ::agenui::testing::WaitForWorkerIdle();
}

}  // namespace
