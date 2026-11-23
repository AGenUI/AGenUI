// TA*: text-align logical values (start/end) passed through the core to the
// wire (G5 from the RTL adaptation PRD, plan B).
//
// Seam: public ISurfaceManager (direction injection) driven end-to-end through
// the message pipeline (createSurface + updateComponents), asserting the
// *pushed wire JSON* that platform layers consume. text-align is a pure
// render property — core passes it through VERBATIM, including logical
// start/end. Resolving start/end against the writing direction is delegated
// to the platform renderers (they map logical values to native enums using
// the wire's resolved styles["direction"] field). Native mapping ships in a
// follow-up task; until then platforms treat start/end as their default.
//
// Default alignment: the Text spec default is the PHYSICAL "left" — it does
// NOT follow the writing direction (explicit start/end declarations are the
// only direction-aware values). Undeclared text stays left-aligned under rtl
// by design.

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

constexpr const char* kSurfaceId = "ta-surface";
constexpr float kSurfaceW = 600.0f;
constexpr float kSurfaceH = 200.0f;

std::string createSurfaceJson() {
    return std::string(R"({"version":"v0.9","createSurface":{"surfaceId":")") +
           kSurfaceId +
           R"(","catalogId":"https://a2ui.org/specification/v0_9/standard_catalog.json"}})";
}

// root(Column) -> t (Text with optional text-align).
// textAlignDecl is a complete extra key/value pair (R"(,"text-align":"start")")
// or "" (no declaration). The t fragment uses escaped literals so every quote
// is explicit — no raw-string edge ambiguity in the concatenated JSON.
std::string updateComponentsJson(const std::string& textAlignDecl) {
    return std::string(R"({"version":"v0.9","updateComponents":{"surfaceId":")") +
           kSurfaceId +
           R"(","components":[)"
           R"({"id":"root","component":"Column","children":["t"],"styles":{"width":"100%"}},)"
           "{\"id\":\"t\",\"component\":\"Text\",\"text\":\"T\",\"styles\":"
           "{\"width\":\"200px\",\"height\":\"50px\"" +
           textAlignDecl +
           "}}]}}";
}

// Extract styles["text-align"] from a pushed component JSON ("" when absent).
std::string styleTextAlignOf(const std::string& componentJson) {
    auto j = agenui::SerializableData::parse(componentJson);
    return j["styles"]["text-align"].asString("");
}

// Latest styles["text-align"] across add notifications (initial layout pass).
std::string latestAddTextAlign(const agenui::testing::MockMessageListener& listener,
                               const std::string& componentId) {
    std::string align;
    for (const auto& record : listener.componentsAddCalls) {
        for (const auto& msg : record.messages) {
            if (msg.componentId == componentId) {
                align = styleTextAlignOf(msg.component);
            }
        }
    }
    return align;
}

// Latest styles["text-align"] across update notifications.
std::string latestUpdateTextAlign(const agenui::testing::MockMessageListener& listener,
                                  const std::string& componentId) {
    std::string align;
    for (const auto& record : listener.componentsUpdateCalls) {
        for (const auto& msg : record.messages) {
            if (msg.componentId == componentId) {
                align = styleTextAlignOf(msg.component);
            }
        }
    }
    return align;
}

class TextAlignDirectionTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Each SurfaceManager is scoped (destroyed at test end), so no
        // direction state survives a test; just drain the worker thread.
        ::agenui::testing::WaitForWorkerIdle();
    }

    // Build one surface whose Text child declares the given text-align style
    // ("" -> no declaration) and wait for the initial add notifications.
    void buildTree(::agenui::ISurfaceManager* sm,
                   agenui::testing::MockMessageListener& listener,
                   const std::string& textAlignDecl) {
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
        sm->receiveTextChunk(updateComponentsJson(textAlignDecl));
        sm->endTextStream();
        ASSERT_TRUE(listener.waitFor(
            [&]() { return !listener.componentsAddCalls.empty(); }, 3000));
        ::agenui::testing::WaitForWorkerIdle();
    }
};

// TA001: explicit "start" passes through VERBATIM — the wire carries the
// logical value under rtl, and keeps carrying it after flipping back to ltr
// (direction re-publishes coordinates; the alignment value never mutates).
TEST_F(TextAlignDirectionTest, TA001_ExplicitStartPassesThroughVerbatim) {
    agenui::testing::ScopedSurfaceManager scoped;
    auto* sm = scoped.get();
    sm->setLayoutDirection("rtl");

    agenui::testing::MockMessageListener listener;
    buildTree(sm, listener, R"(,"text-align":"start")");

    EXPECT_EQ(latestAddTextAlign(listener, "t"), "start");

    sm->setLayoutDirection("ltr");
    ASSERT_TRUE(listener.waitFor(
        [&]() { return latestUpdateTextAlign(listener, "t") == "start"; }, 3000));
}

// TA002: explicit "end" also passes through verbatim under rtl.
TEST_F(TextAlignDirectionTest, TA002_ExplicitEndPassesThroughVerbatim) {
    agenui::testing::ScopedSurfaceManager scoped;
    auto* sm = scoped.get();
    sm->setLayoutDirection("rtl");

    agenui::testing::MockMessageListener listener;
    buildTree(sm, listener, R"(,"text-align":"end")");

    EXPECT_EQ(latestAddTextAlign(listener, "t"), "end");
}

// TA003: no declaration -> the Text spec default (physical "left") passes
// through unchanged. Direction-aware default alignment is intentionally NOT
// provided: only explicit start/end declarations are direction-aware.
// rtl -> still "left"; the flip to ltr re-publishes coordinates but the
// alignment stays "left" throughout.
TEST_F(TextAlignDirectionTest, TA003_DefaultAlignStaysPhysicalLeft) {
    agenui::testing::ScopedSurfaceManager scoped;
    auto* sm = scoped.get();
    sm->setLayoutDirection("rtl");

    agenui::testing::MockMessageListener listener;
    buildTree(sm, listener, "");

    EXPECT_EQ(latestAddTextAlign(listener, "t"), "left");

    sm->setLayoutDirection("ltr");
    ASSERT_TRUE(listener.waitFor(
        [&]() { return latestUpdateTextAlign(listener, "t") == "left"; }, 3000));
}

}  // namespace
