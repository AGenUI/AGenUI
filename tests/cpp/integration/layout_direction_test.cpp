// LD*: surface-level layout direction injection (G3 from the RTL adaptation
// PRD; direction is exposed on ISurfaceManager only — no engine-level entry).
//
// Seam: public ISurfaceManager API driven end-to-end through the message
// pipeline (createSurface + updateComponents), asserting the *pushed wire
// JSON* that platform layers consume. Expected coordinates come from
// CSS/Yoga row mirroring semantics: under rtl a 100-wide first child of
// a 600-wide row sits at x = 600 - 100 = 500; under ltr at x = 0.

#include <gtest/gtest.h>

#include <string>

#include "agenui_dispatcher_types.h"
#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_render_info_types.h"
#include "agenui_surface_manager_interface.h"
#include "support/mock_message_listener.h"
#include "support/scoped_surface_manager.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"
#include "surface/agenui_serializable_data.h"

namespace {

constexpr const char* kSurfaceId = "dir-surface";
constexpr float kSurfaceW = 600.0f;
constexpr float kSurfaceH = 200.0f;

std::string createSurfaceJson() {
    return std::string(R"({"version":"v0.9","createSurface":{"surfaceId":")") +
           kSurfaceId +
           R"(","catalogId":"https://a2ui.org/specification/v0_9/standard_catalog.json"}})";
}

// root(Row) -> [a, b]; fixed 100x50 children; NO direction declared anywhere
// (unless rootDirection injects one into the root's styles).
std::string updateComponentsJson(const std::string& rootDirection = "") {
    const std::string rootStyleExtra =
        rootDirection.empty() ? "" : (",\"direction\":\"" + rootDirection + "\"");
    return std::string(R"({"version":"v0.9","updateComponents":{"surfaceId":")") +
           kSurfaceId +
           R"(","components":[)"
           R"({"id":"root","component":"Row","children":["a","b"],"styles":{"width":"100%")" +
           rootStyleExtra +
           R"(}},)"
           R"({"id":"a","component":"Text","text":"A","styles":{"width":"100px","height":"50px"}},)"
           R"({"id":"b","component":"Text","text":"B","styles":{"width":"100px","height":"50px"}}]}})";
}

// Extract styles.x from a pushed component JSON ("" -> -1).
float styleXOf(const std::string& componentJson) {
    auto j = agenui::SerializableData::parse(componentJson);
    return static_cast<float>(j["styles"]["x"].asDouble(-1.0));
}

// Latest styles.x pushed for componentId across update notifications; -1 if
// the component was never pushed.
float latestUpdateX(const agenui::testing::MockMessageListener& listener,
                    const std::string& componentId) {
    float x = -1.0f;
    for (const auto& record : listener.componentsUpdateCalls) {
        for (const auto& msg : record.messages) {
            if (msg.componentId == componentId) {
                x = styleXOf(msg.component);
            }
        }
    }
    return x;
}

// Latest resolved direction across update notifications; "" when never pushed.
// Direction is a MESSAGE-LEVEL attribute (ComponentsUpdateMessage.direction),
// not part of the component styles.
std::string latestUpdateDirection(
    const agenui::testing::MockMessageListener& listener,
    const std::string& componentId) {
    std::string dir;
    for (const auto& record : listener.componentsUpdateCalls) {
        for (const auto& msg : record.messages) {
            if (msg.componentId == componentId) {
                dir = msg.direction;
            }
        }
    }
    return dir;
}

// Latest resolved direction across add notifications; "" when never pushed.
// Direction is a MESSAGE-LEVEL attribute (ComponentsAddMessage.direction),
// not part of the component styles.
std::string latestAddDirection(
    const agenui::testing::MockMessageListener& listener,
    const std::string& componentId) {
    std::string dir;
    for (const auto& record : listener.componentsAddCalls) {
        for (const auto& msg : record.messages) {
            if (msg.componentId == componentId) {
                dir = msg.direction;
            }
        }
    }
    return dir;
}

// Latest styles.x across add notifications (initial layout pass).
float latestAddX(const agenui::testing::MockMessageListener& listener,
                 const std::string& componentId) {
    float x = -1.0f;
    for (const auto& record : listener.componentsAddCalls) {
        for (const auto& msg : record.messages) {
            if (msg.componentId == componentId) {
                x = styleXOf(msg.component);
            }
        }
    }
    return x;
}

class LayoutDirectionTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Each SurfaceManager is scoped (destroyed at test end), so no
        // process-wide direction state survives a test; just drain the
        // worker thread before the next suite runs.
        ::agenui::testing::WaitForWorkerIdle();
    }

    // Build one surface with the Row tree and a fixed 600x200 size.
    // rootDirection optionally declares a direction on the root node's DSL.
    void buildTree(::agenui::ISurfaceManager* sm,
                   agenui::testing::MockMessageListener& listener,
                   const std::string& rootDirection = "") {
        sm->addSurfaceEventListener(&listener);
        sm->beginTextStream();
        sm->receiveTextChunk(createSurfaceJson());
        sm->endTextStream();
        ASSERT_TRUE(listener.waitFor(
            [&]() { return !listener.createSurfaceCalls.empty(); }, 3000));

        ::agenui::SurfaceLayoutInfo info;
        info.surfaceId = kSurfaceId;
        info.width = kSurfaceW;
        info.height = kSurfaceH;
        sm->onSurfaceSizeChanged(info);
        ::agenui::testing::WaitForWorkerIdle();

        sm->beginTextStream();
        sm->receiveTextChunk(updateComponentsJson(rootDirection));
        sm->endTextStream();
        ASSERT_TRUE(listener.waitFor(
            [&]() { return !listener.componentsAddCalls.empty(); }, 3000));
        ::agenui::testing::WaitForWorkerIdle();
    }
};

// LD001: surface-level rtl flips the pushed layout and ltr flips it back.
TEST_F(LayoutDirectionTest, LD001_SurfaceRtl_MirrorsRowAndPushesNewLayout) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    agenui::testing::MockMessageListener listener;
    buildTree(sm.get(), listener);

    // Legacy default: LTR, first child at x=0.
    ASSERT_FLOAT_EQ(latestAddX(listener, "a"), 0.0f);

    sm->setLayoutDirection("rtl");
    ASSERT_TRUE(listener.waitFor(
        [&]() { return latestUpdateX(listener, "a") >= 499.5f; }, 3000));
    EXPECT_FLOAT_EQ(latestUpdateX(listener, "a"), kSurfaceW - 100.0f);

    // Flip back: coordinates return to the LTR layout.
    sm->setLayoutDirection("ltr");
    ASSERT_TRUE(listener.waitFor(
        [&]() {
            float x = latestUpdateX(listener, "a");
            return x >= 0.0f && x < 0.5f;
        },
        3000));
    EXPECT_FLOAT_EQ(latestUpdateX(listener, "a"), 0.0f);

    sm->removeSurfaceEventListener(&listener);
}

// LD002: direction is per-SurfaceManager — instances are isolated from each
// other. A rtl manager mirrors while a freshly created one stays LTR (the
// legacy default), proving no hidden cross-instance/global state.
TEST_F(LayoutDirectionTest, LD002_SurfaceManagers_IsolatedPerInstance) {
    ::agenui::testing::ScopedSurfaceManager sm1;
    ASSERT_TRUE(sm1);
    agenui::testing::MockMessageListener listener1;
    buildTree(sm1.get(), listener1);

    sm1->setLayoutDirection("rtl");
    ASSERT_TRUE(listener1.waitFor(
        [&]() { return latestUpdateX(listener1, "a") >= 499.5f; }, 3000));
    EXPECT_FLOAT_EQ(latestUpdateX(listener1, "a"), kSurfaceW - 100.0f);

    // A second manager created afterwards starts from the untouched LTR
    // default — sm1's direction must not leak into it.
    ::agenui::testing::ScopedSurfaceManager sm2;
    ASSERT_TRUE(sm2);
    agenui::testing::MockMessageListener listener2;
    buildTree(sm2.get(), listener2);
    EXPECT_FLOAT_EQ(latestAddX(listener2, "a"), 0.0f);

    sm1->removeSurfaceEventListener(&listener1);
    sm2->removeSurfaceEventListener(&listener2);
}

// LD004: invalid direction strings are ignored (logged) and change nothing.
TEST_F(LayoutDirectionTest, LD004_InvalidDirection_Ignored) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    agenui::testing::MockMessageListener listener;
    buildTree(sm.get(), listener);
    listener.clear();

    sm->setLayoutDirection("diagonal");
    ::agenui::testing::WaitForWorkerIdle();

    // No update may be pushed with a flipped coordinate (the legacy LTR
    // layout stays untouched).
    EXPECT_FLOAT_EQ(latestUpdateX(listener, "a"), -1.0f);

    sm->removeSurfaceEventListener(&listener);
}

// LD005: every ComponentsAdd/Update message carries the RESOLVED per-node
// direction as a MESSAGE-LEVEL attribute — layout output on the message
// envelope, never a component style. A direction flip forces an update for
// every node even when its own layout is byte-identical (direction is a
// render input, not just coordinate output).
TEST_F(LayoutDirectionTest, LD005_MessagesCarryResolvedDirection) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    sm->setLayoutDirection("rtl");
    ::agenui::testing::WaitForWorkerIdle();

    agenui::testing::MockMessageListener listener;
    buildTree(sm.get(), listener);

    // First push: every node reports the resolved rtl (root and children).
    EXPECT_EQ(latestAddDirection(listener, "root"), "rtl");
    EXPECT_EQ(latestAddDirection(listener, "a"), "rtl");
    EXPECT_EQ(latestAddDirection(listener, "b"), "rtl");

    // Flip back to ltr: updates carry the new resolved value. Notification
    // order is root -> a -> b (same recursive pass), so waiting on the LAST
    // child guarantees the whole batch has been pushed.
    sm->setLayoutDirection("ltr");
    ASSERT_TRUE(listener.waitFor(
        [&]() { return latestUpdateDirection(listener, "b") == "ltr"; }, 3000));
    EXPECT_EQ(latestUpdateDirection(listener, "root"), "ltr");
    EXPECT_EQ(latestUpdateDirection(listener, "a"), "ltr");
    EXPECT_EQ(latestUpdateDirection(listener, "b"), "ltr");

    sm->removeSurfaceEventListener(&listener);
}

// LD006: re-setting the ACTIVE direction must not push anything. (A full
// no-op also skips the wasted re-layout cycle in the manager — though the
// push-free guarantee is what this test can observe from the outside.)
// The flip-after tail proves the dedup check didn't corrupt the stored
// state (a real change still re-publishes).
TEST_F(LayoutDirectionTest, LD006_SameDirection_NoRepublish) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    agenui::testing::MockMessageListener listener;
    buildTree(sm.get(), listener);

    sm->setLayoutDirection("rtl");
    ASSERT_TRUE(listener.waitFor(
        [&]() { return latestUpdateX(listener, "a") >= 499.5f; }, 3000));
    ::agenui::testing::WaitForWorkerIdle();  // drain the whole rtl batch
    listener.clear();

    // Same value again: nothing may be pushed for any component.
    sm->setLayoutDirection("rtl");
    ::agenui::testing::WaitForWorkerIdle();
    EXPECT_FLOAT_EQ(latestUpdateX(listener, "a"), -1.0f);

    // And a real flip afterwards still re-publishes.
    sm->setLayoutDirection("ltr");
    ASSERT_TRUE(listener.waitFor(
        [&]() {
            float x = latestUpdateX(listener, "a");
            return x >= 0.0f && x < 0.5f;
        },
        3000));

    sm->removeSurfaceEventListener(&listener);
}

// LD007: DSL-declared direction is NOT supported — the surface manager is
// the single direction entry point. A root-node "direction":"rtl" style is
// an ordinary unrecognized style (passes through untouched, never consumed):
// layout stays at the manager's LTR default, and only setLayoutDirection
// flips the tree.
TEST_F(LayoutDirectionTest, LD007_RootDslDirection_Ignored_ManagerIsSoleEntry) {
    ::agenui::testing::ScopedSurfaceManager sm;
    ASSERT_TRUE(sm);
    agenui::testing::MockMessageListener listener;
    buildTree(sm.get(), listener, "rtl");  // root DSL: rtl (must be ignored)

    // SM is at its LTR default: the DSL declaration must NOT mirror.
    EXPECT_FLOAT_EQ(latestAddX(listener, "a"), 0.0f);
    ::agenui::testing::WaitForWorkerIdle();
    listener.clear();

    // The manager remains the sole entry: flipping it mirrors the row.
    sm->setLayoutDirection("rtl");
    ASSERT_TRUE(listener.waitFor(
        [&]() { return latestUpdateX(listener, "a") >= 499.5f; }, 3000));
    EXPECT_FLOAT_EQ(latestUpdateX(listener, "a"), kSurfaceW - 100.0f);

    sm->removeSurfaceEventListener(&listener);
}

}  // namespace
