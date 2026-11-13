// UA*: SubmitUIAction / SubmitUIDataModel tests.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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

// ---------------------------------------------------------------------------
// Static write-back (syncState) tests.
//
// Host custom components (AmapCheckBox / Switch) call syncState({"value":...})
// after the user toggles the control. Before the StaticData branch of
// ComponentModel::syncValue existed, the core ignored literal attributes: the
// UI changed but the core/VDOM baseline did not, so a later business push of
// the original value produced no downstream diff and the UI never restored.
// These tests exercise the real protocol path:
//   submitUIDataModel -> Surface::syncUIToData -> ComponentModel::syncValue
//   -> dirty flush -> VDOM diff -> onComponentsUpdate listener callback.
// See plan 三端静态状态回写修复.
// ---------------------------------------------------------------------------

std::string MakeCreateSurfaceJson(const std::string& surfaceId) {
    return R"({"version":"v0.9","createSurface":{"surfaceId":")" + surfaceId +
           R"(","catalogId":"https://a2ui.org/specification/v0_9/standard_catalog.json","theme":{},"sendDataModel":false,"animated":true}})";
}

std::string MakeUpdateComponentsJson(const std::string& surfaceId, const std::string& componentsArrayJson) {
    return R"({"version":"v0.9","updateComponents":{"surfaceId":")" + surfaceId +
           R"(","components":)" + componentsArrayJson + "}}";
}

std::string MakeUpdateDataModelJson(const std::string& surfaceId, const std::string& path, const std::string& valueJson) {
    return R"({"version":"v0.9","updateDataModel":{"surfaceId":")" + surfaceId +
           R"(","path":")" + path + R"(","value":)" + valueJson + "}}";
}

void DeliverProtocol(::agenui::ISurfaceManager* sm, const std::string& payload) {
    sm->beginTextStream();
    sm->receiveTextChunk(payload);
    sm->endTextStream();
}

// All "value" entries found in downstream update diffs for one component.
// Reads the listener under its lock and returns plain copies.
std::vector<std::string> CollectedUpdateValues(::agenui::testing::MockMessageListener& listener,
                                               const std::string& surfaceId,
                                               const std::string& componentId) {
    return listener.withLock([&](const ::agenui::testing::MockMessageListener& l) {
        std::vector<std::string> values;
        for (const auto& record : l.componentsUpdateCalls) {
            if (record.surfaceId != surfaceId) continue;
            for (const auto& msg : record.messages) {
                if (msg.componentId != componentId) continue;
                auto json = nlohmann::json::parse(msg.component, nullptr, false);
                if (!json.is_discarded() && json.contains("value")) {
                    values.push_back(json["value"].dump());
                }
            }
        }
        return values;
    });
}

// Submits a syncState write-back through the public ISurfaceManager API.
void SubmitSyncState(::agenui::ISurfaceManager* sm,
                     const std::string& surfaceId,
                     const std::string& componentId,
                     const std::string& changeJson) {
    ::agenui::SyncUIToDataMessage msg;
    msg.surfaceId = surfaceId;
    msg.componentId = componentId;
    msg.change = changeJson;
    sm->submitUIDataModel(msg);
    ::agenui::testing::WaitForWorkerIdle();
}

// Waits until at least one "value" entry appears in downstream update diffs
// for the component. Uses the polling WaitFor helper instead of
// MockMessageListener::waitFor: the latter holds the listener mutex while
// evaluating its predicate, and CollectedUpdateValues takes that same mutex
// via withLock — calling it inside waitFor would self-deadlock.
bool WaitForUpdateValue(::agenui::testing::MockMessageListener& listener,
                        const std::string& surfaceId,
                        const std::string& componentId,
                        int timeoutMillis = 2000) {
    return ::agenui::testing::WaitFor(
        [&]() {
            return !CollectedUpdateValues(listener, surfaceId, componentId)
                        .empty();
        },
        timeoutMillis);
}

// Creates a surface and renders one component; waits until the first frame
// was added on the platform side. Returns false on timeout.
// NOTE: the component id must be "root" — VirtualDOM::updateNode only mounts
// the root id directly; anything else lands in the orphan table until some
// mounted node references it, so no onComponentsAdd would ever fire.
bool RenderSingleComponent(::agenui::ISurfaceManager* sm,
                           ::agenui::testing::MockMessageListener& listener,
                           const std::string& surfaceId,
                           const std::string& componentJson) {
    DeliverProtocol(sm, MakeCreateSurfaceJson(surfaceId));
    if (!listener.waitFor([&]() { return !listener.createSurfaceCalls.empty(); },
                          2000)) {
        return false;
    }

    DeliverProtocol(sm, MakeUpdateComponentsJson(surfaceId, "[" + componentJson + "]"));
    return listener.waitFor([&]() {
        for (const auto& record : listener.componentsAddCalls) {
            if (record.surfaceId != surfaceId) continue;
            for (const auto& msg : record.messages) {
                if (msg.componentId == "root" || msg.componentId == "cbp_shared") {
                    return true;
                }
            }
        }
        return false;
    }, 2000);
}

}  // namespace

class UIActionTest : public ::testing::Test {
protected:
    ::agenui::testing::ScopedSurfaceManager sm;
    ::agenui::testing::MockMessageListener listener;

    void SetUp() override {
        ASSERT_TRUE(sm);
        sm->addSurfaceEventListener(&listener);
    }

    void TearDown() override {
        if (sm) sm->removeSurfaceEventListener(&listener);
    }
};

// UA001: surfaceId / componentId empty path triggers no callback (logged
// only). We verify safety here, not behavior on real components.
TEST_F(UIActionTest, UA001_SubmitUIAction_NoCrash) {
    ::agenui::ActionMessage msg;
    msg.surfaceId = "nonexistent";
    msg.sourceComponentId = "src1";
    msg.contextJson = "{}";
    EXPECT_NO_THROW(sm->submitUIAction(msg));
    ::agenui::testing::WaitForWorkerIdle();
}

// UA002
TEST_F(UIActionTest, UA002_SubmitUIAction_EmptyMessage_NoCrash) {
    ::agenui::ActionMessage msg;
    EXPECT_NO_THROW(sm->submitUIAction(msg));
    ::agenui::testing::WaitForWorkerIdle();
}

// UA003: SubmitUIDataModel against unknown surface is logged but doesn't
// crash.
TEST_F(UIActionTest, UA003_SubmitUIDataModel_NoCrash) {
    ::agenui::SyncUIToDataMessage msg;
    msg.surfaceId = "nonexistent";
    msg.componentId = "comp1";
    msg.change = R"({"value":1})";
    EXPECT_NO_THROW(sm->submitUIDataModel(msg));
    ::agenui::testing::WaitForWorkerIdle();
}

// UA004: action against an existing surface (created via protocol).
TEST_F(UIActionTest, UA004_SubmitUIAction_OnExistingSurface_NoCrash) {
    auto create = ::agenui::testing::LoadFixtureOrEmpty(
        "protocol/create_surface.json");
    ASSERT_FALSE(create.empty());
    sm->beginTextStream();
    sm->receiveTextChunk(create);
    sm->endTextStream();
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.createSurfaceCalls.empty(); }, 2000));

    ::agenui::ActionMessage msg;
    msg.surfaceId = "test-surface-1";
    msg.sourceComponentId = "non_existent_component";
    msg.contextJson = "{}";
    EXPECT_NO_THROW(sm->submitUIAction(msg));
    ::agenui::testing::WaitForWorkerIdle();
}

// UA005: submitUIDataModel ignores empty surfaceId without callback.
TEST_F(UIActionTest, UA005_SubmitUIDataModel_EmptySurfaceId_NoCallback) {
    ::agenui::SyncUIToDataMessage msg;
    msg.surfaceId = "";
    msg.componentId = "c";
    msg.change = "{}";
    EXPECT_NO_THROW(sm->submitUIDataModel(msg));
    ::agenui::testing::WaitForWorkerIdle();
    EXPECT_EQ(listener.totalCalls(), 0);
}

// UA101: the core write-back loop for a literal AmapCheckBox value:
// business push -> user toggle -> business re-push of the ORIGINAL value must
// produce a real downstream diff (the exact gap this fix closes).
TEST_F(UIActionTest, UA101_StaticWriteBack_BusinessRepublishRestores) {
    ASSERT_TRUE(RenderSingleComponent(
        sm.get(), listener, "test-surface-1",
        R"({"id":"root","component":"AmapCheckBox","value":false})"));

    // User toggles on: UI write-back must reach the downstream diff.
    listener.clear();
    SubmitSyncState(sm.get(), "test-surface-1", "root", R"({"value":true})");
    EXPECT_TRUE(WaitForUpdateValue(listener, "test-surface-1", "root"));
    {
        auto values = CollectedUpdateValues(listener, "test-surface-1", "root");
        ASSERT_FALSE(values.empty());
        EXPECT_EQ(values.back(), "true");
    }

    // Business re-pushes the original value: must restore (downstream diff
    // with value=false). Before the fix this diff never happened.
    listener.clear();
    SubmitSyncState(sm.get(), "test-surface-1", "root", R"({"value":false})");
    EXPECT_TRUE(WaitForUpdateValue(listener, "test-surface-1", "root"));
    {
        auto values = CollectedUpdateValues(listener, "test-surface-1", "root");
        ASSERT_FALSE(values.empty());
        EXPECT_EQ(values.back(), "false");
    }

    // No user interaction in between: repeating the same business value must
    // deduplicate to no value diff (other layout/style messages are allowed).
    listener.clear();
    SubmitSyncState(sm.get(), "test-surface-1", "root", R"({"value":false})");
    EXPECT_TRUE(CollectedUpdateValues(listener, "test-surface-1", "root").empty());
}

// UA102: Switch, both bool directions, multiple loops. The second switch
// renders on its own surface because a surface's VDOM has a single root.
TEST_F(UIActionTest, UA102_StaticWriteBack_SwitchBothDirectionsAndLoops) {
    ASSERT_TRUE(RenderSingleComponent(
        sm.get(), listener, "test-surface-1",
        R"({"id":"root","component":"Switch","value":false})"));

    // Two identical loops in the false -> true direction.
    for (int round = 0; round < 2; ++round) {
        listener.clear();
        SubmitSyncState(sm.get(), "test-surface-1", "root", R"({"value":true})");
        EXPECT_TRUE(WaitForUpdateValue(listener, "test-surface-1", "root"));
        EXPECT_EQ(CollectedUpdateValues(listener, "test-surface-1", "root").back(), "true");

        listener.clear();
        SubmitSyncState(sm.get(), "test-surface-1", "root", R"({"value":false})");
        EXPECT_TRUE(WaitForUpdateValue(listener, "test-surface-1", "root"));
        EXPECT_EQ(CollectedUpdateValues(listener, "test-surface-1", "root").back(), "false");
    }

    // Opposite direction with a second switch on a separate surface,
    // starting at true: user turns it off, business re-pushes true to restore.
    ASSERT_TRUE(RenderSingleComponent(
        sm.get(), listener, "test-surface-2",
        R"({"id":"root","component":"Switch","value":true})"));

    listener.clear();
    SubmitSyncState(sm.get(), "test-surface-2", "root", R"({"value":false})");
    EXPECT_TRUE(WaitForUpdateValue(listener, "test-surface-2", "root"));
    EXPECT_EQ(CollectedUpdateValues(listener, "test-surface-2", "root").back(), "false");

    listener.clear();
    SubmitSyncState(sm.get(), "test-surface-2", "root", R"({"value":true})");
    EXPECT_TRUE(WaitForUpdateValue(listener, "test-surface-2", "root"));
    EXPECT_EQ(CollectedUpdateValues(listener, "test-surface-2", "root").back(), "true");
}

// UA103: without user interaction, repeated business values produce no value
// diff at all (same-value dedup must survive the new static branch).
TEST_F(UIActionTest, UA103_StaticWriteBack_RepeatedBusinessValue_NoDiff) {
    ASSERT_TRUE(RenderSingleComponent(
        sm.get(), listener, "test-surface-1",
        R"({"id":"root","component":"AmapCheckBox","value":false})"));

    listener.clear();
    for (int i = 0; i < 3; ++i) {
        SubmitSyncState(sm.get(), "test-surface-1", "root", R"({"value":false})");
    }
    EXPECT_TRUE(CollectedUpdateValues(listener, "test-surface-1", "root").empty());

    // A malformed change payload is ignored without breaking the channel.
    SubmitSyncState(sm.get(), "test-surface-1", "root", "not-json");
    EXPECT_TRUE(CollectedUpdateValues(listener, "test-surface-1", "root").empty());
}

// UA104: same surfaceId / componentId under two different SurfaceManagers
// stay isolated; a write-back in one must not affect the other.
TEST_F(UIActionTest, UA104_StaticWriteBack_IsolatedAcrossManagers) {
    ::agenui::testing::ScopedSurfaceManager sm1;
    ::agenui::testing::ScopedSurfaceManager sm2;
    ASSERT_TRUE(sm1);
    ASSERT_TRUE(sm2);

    ::agenui::testing::MockMessageListener listener1;
    ::agenui::testing::MockMessageListener listener2;
    sm1->addSurfaceEventListener(&listener1);
    sm2->addSurfaceEventListener(&listener2);

    const std::string surfaceId = "iso-surface";
    ASSERT_TRUE(RenderSingleComponent(
        sm1.get(), listener1, surfaceId,
        R"({"id":"root","component":"AmapCheckBox","value":false})"));
    ASSERT_TRUE(RenderSingleComponent(
        sm2.get(), listener2, surfaceId,
        R"({"id":"root","component":"AmapCheckBox","value":false})"));

    // Toggle both on: each manager must emit its own diff.
    listener1.clear();
    listener2.clear();
    SubmitSyncState(sm1.get(), surfaceId, "root", R"({"value":true})");
    SubmitSyncState(sm2.get(), surfaceId, "root", R"({"value":true})");
    EXPECT_TRUE(WaitForUpdateValue(listener1, surfaceId, "root"));
    EXPECT_TRUE(WaitForUpdateValue(listener2, surfaceId, "root"));

    // Business restores manager 1 only: manager 2 must stay silent.
    listener1.clear();
    listener2.clear();
    SubmitSyncState(sm1.get(), surfaceId, "root", R"({"value":false})");
    EXPECT_TRUE(WaitForUpdateValue(listener1, surfaceId, "root"));
    EXPECT_EQ(CollectedUpdateValues(listener1, surfaceId, "root").back(), "false");
    EXPECT_TRUE(CollectedUpdateValues(listener2, surfaceId, "root").empty());

    sm1->removeSurfaceEventListener(&listener1);
    sm2->removeSurfaceEventListener(&listener2);
}

// UA105: regression for path-bound values (the DataBindingData channel must
// keep working unchanged, including shared-path fan-out).
TEST_F(UIActionTest, UA105_BindingWriteBack_PathBindingAndSharedPath) {
    const std::string surfaceId = "test-surface-1";
    DeliverProtocol(sm.get(), MakeCreateSurfaceJson(surfaceId));
    ASSERT_TRUE(listener.waitFor(
        [&]() { return !listener.createSurfaceCalls.empty(); }, 2000));

    // Seed the DataModel before the components bind to it.
    DeliverProtocol(sm.get(), MakeUpdateDataModelJson(surfaceId, "/state/cb", "false"));
    ::agenui::testing::WaitForWorkerIdle();

    // The root AmapCheckBox references the Switch as a child so both attach
    // to the VDOM tree (only "root" mounts directly); they share /state/cb.
    DeliverProtocol(sm.get(), MakeUpdateComponentsJson(
        surfaceId,
        R"([{"id":"root","component":"AmapCheckBox","value":{"path":"/state/cb"},"children":["cbp_shared"]},)"
        R"({"id":"cbp_shared","component":"Switch","value":{"path":"/state/cb"}}])"));
    ASSERT_TRUE(listener.waitFor([&]() {
        for (const auto& record : listener.componentsAddCalls) {
            if (record.surfaceId != surfaceId) continue;
            bool has_root = false;
            bool has_shared = false;
            for (const auto& msg : record.messages) {
                if (msg.componentId == "root") has_root = true;
                if (msg.componentId == "cbp_shared") has_shared = true;
            }
            if (has_root && has_shared) return true;
        }
        return false;
    }, 2000));

    // UI write-back on the bound attribute goes through the DataModel and
    // fans out to BOTH components sharing the path.
    listener.clear();
    SubmitSyncState(sm.get(), surfaceId, "root", R"({"value":true})");
    EXPECT_TRUE(WaitForUpdateValue(listener, surfaceId, "cbp_shared"));
    EXPECT_FALSE(CollectedUpdateValues(listener, surfaceId, "root").empty());
    EXPECT_EQ(CollectedUpdateValues(listener, surfaceId, "root").back(), "true");
    EXPECT_EQ(CollectedUpdateValues(listener, surfaceId, "cbp_shared").back(), "true");

    // Business writes the old value back to the path: both components
    // receive the restore diff through the existing binding channel.
    listener.clear();
    DeliverProtocol(sm.get(), MakeUpdateDataModelJson(surfaceId, "/state/cb", "false"));
    EXPECT_TRUE(::agenui::testing::WaitFor([&]() {
        auto v1 = CollectedUpdateValues(listener, surfaceId, "root");
        auto v2 = CollectedUpdateValues(listener, surfaceId, "cbp_shared");
        return !v1.empty() && v1.back() == "false" &&
               !v2.empty() && v2.back() == "false";
    }));
}

