// RTL layout correctness tests (G1/G2 from the RTL adaptation PRD).
//
// Seam: CSSStyleConverter public static apply functions + raw Yoga layout
// results. Expected coordinates are derived from CSS logical-property
// semantics under the writing direction resolved by Yoga (start/end follow
// the *inherited* direction, per CSS Writing Modes), independent of the
// implementation under test.

#include <gtest/gtest.h>
#include "surface/yoga_node/agenui_css_style_converter.h"
#include "surface/yoga_node/agenui_yoga_layout_engine.h"
#include "surface/yoga_node/agenui_yoga_node.h"
#include "surface/yoga_node/agenui_component_snapshot_wrapper.h"
#include "surface/virtual_dom/agenui_virtual_dom.h"
#include "surface/virtual_dom/agenui_virtual_dom_node.h"
#include "surface/agenui_isurface_context.h"
#include <yoga/Yoga.h>
#include <memory>
#include <string>
#include <vector>

using namespace agenui;

namespace {

constexpr float kRootWidth = 300.0f;
constexpr float kRootHeight = 100.0f;

// Minimal RAII guard so a failed assertion never leaks Yoga nodes.
struct YogaTree {
    std::vector<YGNodeRef> nodes;
    ~YogaTree() {
        for (auto* n : nodes) {
            YGNodeFree(n);
        }
    }
    YGNodeRef make() {
        nodes.push_back(YGNodeNew());
        return nodes.back();
    }
};

float px(float v) { return v; }

// No-op IVirtualDOMObserver: the RTL tests assert layout results directly on
// the Yoga nodes, so notifications are intentionally discarded.
class SilentObserver : public IVirtualDOMObserver {
public:
    void onNodeUpdate(const std::string&, const std::string&, const std::string&) override {}
    void onNodeAdded(const std::string&, const std::string&, const std::string&) override {}
    void onNodeRemoved(const std::string&, const std::string&) override {}
};

// Fixed-size surface context: width 300, height 100.
class FixedSurfaceContext : public ISurfaceContext {
public:
    int getInstanceId() const override { return 1; }
    std::string getSurfaceId() const override { return "rtl_test_surface"; }
    IDataModel* getDataModel() const override { return nullptr; }
    float getSurfaceWidth() override { return kRootWidth; }
    float getSurfaceHeight() override { return kRootHeight; }
};

ComponentSnapshot makeSnap(const std::string& id, const std::string& component,
                           std::vector<std::string> children = {}) {
    ComponentSnapshot s;
    s.id = id;
    s.component = component;
    s.children = std::move(children);
    return s;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════════════
// TC-1-1: RTL row mirrors child order (root declares direction: rtl)
// ═══════════════════════════════════════════════════════════════════════════════════

TEST(RtlLayoutTest, RtlRow_MirrorsChildOrder) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef a = t.make();
    YGNodeRef b = t.make();
    YGNodeRef c = t.make();

    // Direction comes from the layout call's ownerDirection (the production
    // path: manager default; no node carries a style direction).
    CSSStyleConverter::applyFlexDirection(root, YogaValue(std::string("row")));
    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);
    for (auto* child : {a, b, c}) {
        YGNodeStyleSetWidth(child, 100.0f);
        YGNodeStyleSetHeight(child, 50.0f);
        YGNodeInsertChild(root, child, YGNodeGetChildCount(root));
    }

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionRTL);

    // First child A ends up on the physical right: left = 300 - 100 = 200.
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(a)), 200.0f);
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(b)), 100.0f);
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(c)), 0.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════════
// TC-1-2: inset-inline-start resolves against the *inherited* rtl direction
// (rtl3a scenario from css_yoga_edge/direction_rtl_mixed.json)
// ═══════════════════════════════════════════════════════════════════════════════════

TEST(RtlLayoutTest, InsetInlineStart_InheritsRtl_AnchorsRightEdge) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef child = t.make();

    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);

    CSSStyleConverter::applyPosition(child, YogaValue(std::string("absolute")));
    CSSStyleConverter::applyWidth(child, YogaValue(100.0f), false);
    CSSStyleConverter::applyHeight(child, YogaValue(50.0f), false);
    // Child declares no direction of its own; the logical start edge must
    // resolve to the physical RIGHT edge because the tree is rtl.
    CSSStyleConverter::applyInsetInlineStart(child, YogaValue(40.0f));
    YGNodeInsertChild(root, child, 0);

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionRTL);

    // start(right) offset 40, width 100 -> left = 300 - 100 - 40 = 160.
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(child)), 160.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════════
// TC-1-3: margin-inline-start resolves against the inherited rtl direction
// ═══════════════════════════════════════════════════════════════════════════════════

TEST(RtlLayoutTest, MarginInlineStart_InheritsRtl_MarginOnRight) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef a = t.make();
    YGNodeRef b = t.make();

    CSSStyleConverter::applyFlexDirection(root, YogaValue(std::string("row")));
    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);

    CSSStyleConverter::applyWidth(a, YogaValue(100.0f), false);
    CSSStyleConverter::applyHeight(a, YogaValue(50.0f), false);
    CSSStyleConverter::applyMarginInlineStart(a, YogaValue(40.0f));

    CSSStyleConverter::applyWidth(b, YogaValue(100.0f), false);
    CSSStyleConverter::applyHeight(b, YogaValue(50.0f), false);

    YGNodeInsertChild(root, a, 0);
    YGNodeInsertChild(root, b, 1);

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionRTL);

    // RTL row starts from the right: 40 margin, then A, then B.
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(a)), 160.0f);  // 300 - 40 - 100
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(b)), 60.0f);   // 300 - 40 - 100 - 100
}

// ═══════════════════════════════════════════════════════════════════════════════════
// TC-1-4: padding-inline-start resolves against the inherited rtl direction
// ═══════════════════════════════════════════════════════════════════════════════════

TEST(RtlLayoutTest, PaddingInlineStart_InheritsRtl_PadRight) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef child = t.make();

    CSSStyleConverter::applyFlexDirection(root, YogaValue(std::string("row")));
    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);
    CSSStyleConverter::applyPaddingInlineStart(root, YogaValue(40.0f));

    CSSStyleConverter::applyWidth(child, YogaValue(100.0f), false);
    CSSStyleConverter::applyHeight(child, YogaValue(50.0f), false);
    YGNodeInsertChild(root, child, 0);

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionRTL);

    // Content box shrinks from the right: child.left = 300 - 40 - 100 = 160.
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(child)), 160.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════════
// TC-1-6 (regression guard): LTR logical properties keep current behavior
// ═══════════════════════════════════════════════════════════════════════════════════

TEST(RtlLayoutTest, InsetInlineStart_Ltr_AnchorsLeftEdge) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef child = t.make();

    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);

    CSSStyleConverter::applyPosition(child, YogaValue(std::string("absolute")));
    CSSStyleConverter::applyWidth(child, YogaValue(100.0f), false);
    CSSStyleConverter::applyHeight(child, YogaValue(50.0f), false);
    CSSStyleConverter::applyInsetInlineStart(child, YogaValue(40.0f));
    YGNodeInsertChild(root, child, 0);

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);

    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(child)), 40.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════════
// TC-1-10: inline-end resolves to the physical LEFT edge under inherited rtl
// ═══════════════════════════════════════════════════════════════════════════════════

TEST(RtlLayoutTest, InsetInlineEnd_InheritsRtl_AnchorsLeftEdge) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef child = t.make();

    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);

    CSSStyleConverter::applyPosition(child, YogaValue(std::string("absolute")));
    CSSStyleConverter::applyWidth(child, YogaValue(100.0f), false);
    CSSStyleConverter::applyHeight(child, YogaValue(50.0f), false);
    CSSStyleConverter::applyInsetInlineEnd(child, YogaValue(40.0f));
    YGNodeInsertChild(root, child, 0);

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionRTL);

    // end(left) offset 40 -> left = 40.
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(child)), 40.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════════
// TC-1-8 (G1): default root direction (engine-level) drives layout when the
// root declares no direction of its own.
//
// Seam: ILayoutDelegate public interface (createNode/insertChild/
// setDefaultRootDirection/calculateLayout/readLayoutResult) — no internals.
// ═══════════════════════════════════════════════════════════════════════════════════

TEST(RtlLayoutTest, RootDirection_DefaultRtl_MirrorsRow) {
    YogaLayoutEngine engine;
    YogaNode* root = engine.createNode("root");
    YogaNode* a = engine.createNode("a");
    YogaNode* b = engine.createNode("b");
    YogaNode* c = engine.createNode("c");
    ASSERT_TRUE(root && a && b && c);
    engine.setRootNode(root);

    // Row container; NO direction declared anywhere in the tree. The writing
    // direction must come from the engine's default root direction.
    CSSStyleConverter::applyFlexDirection(root->get(), YogaValue(std::string("row")));
    for (auto* child : {a, b, c}) {
        YGNodeStyleSetWidth(child->get(), 100.0f);
        YGNodeStyleSetHeight(child->get(), 50.0f);
    }
    engine.insertChild("root", "a", 0);
    engine.insertChild("root", "b", 1);
    engine.insertChild("root", "c", 2);

    engine.setDefaultRootDirection(LayoutDirection::RTL);
    ASSERT_TRUE(engine.calculateLayout("root", kRootWidth));

    // Mirrored row: first child on the physical right.
    float ax, ay, aw, ah;
    float bx, by, bw, bh;
    float cx, cy, cw, ch;
    ASSERT_TRUE(engine.readLayoutResult("a", ax, ay, aw, ah));
    ASSERT_TRUE(engine.readLayoutResult("b", bx, by, bw, bh));
    ASSERT_TRUE(engine.readLayoutResult("c", cx, cy, cw, ch));
    EXPECT_FLOAT_EQ(px(ax), 200.0f);
    EXPECT_FLOAT_EQ(px(bx), 100.0f);
    EXPECT_FLOAT_EQ(px(cx), 0.0f);
}

// TC-1-8c (guard): with no default configured the engine stays LTR (legacy
// behavior for every existing surface).
TEST(RtlLayoutTest, RootDirection_DefaultLtr_KeepsLegacyBehavior) {
    YogaLayoutEngine engine;
    YogaNode* root = engine.createNode("root");
    YogaNode* a = engine.createNode("a");
    YogaNode* b = engine.createNode("b");
    engine.setRootNode(root);
    CSSStyleConverter::applyFlexDirection(root->get(), YogaValue(std::string("row")));
    for (auto* child : {a, b}) {
        YGNodeStyleSetWidth(child->get(), 100.0f);
        YGNodeStyleSetHeight(child->get(), 50.0f);
    }
    engine.insertChild("root", "a", 0);
    engine.insertChild("root", "b", 1);
    ASSERT_TRUE(engine.calculateLayout("root", kRootWidth));
    float ax, ay, aw, ah;
    ASSERT_TRUE(engine.readLayoutResult("a", ax, ay, aw, ah));
    EXPECT_FLOAT_EQ(px(ax), 0.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════════
// camelCase alias + percent value through the full convertToYoga dispatch path
// ═══════════════════════════════════════════════════════════════════════════════════

TEST(RtlLayoutTest, ConvertToYoga_CamelCaseLogicalProperty_InheritsRtl) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef child = t.make();

    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);

    auto snap = std::make_shared<ComponentSnapshot>();
    snap->id = "child";
    snap->styles["position"] = SerializableData::parse("\"absolute\"");
    snap->styles["width"] = SerializableData::parse("\"100px\"");
    snap->styles["height"] = SerializableData::parse("\"50px\"");
    // camelCase alias for inset-inline-start, percentage form.
    snap->styles["insetInlineStart"] = SerializableData::parse("\"10%\"");
    ComponentSnapshotWrapper wrapper(snap);
    CSSStyleConverter::convertToYoga(wrapper, child, false /*clearAfterConvert*/);

    YGNodeInsertChild(root, child, 0);
    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionRTL);

    // 10% of 300 = 30 from the physical right edge: left = 300 - 100 - 30 = 170.
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(child)), 170.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════════
// TC-1-9 (G1): Tabs two-pass layout stays consistent with the surface-level
// default root direction (no DSL direction anywhere).
//
// Seam: VirtualDOM public interface (setLayoutDirection + updateNode) with
// the real Yoga engine underneath; assertions on Yoga layout results.
// ═══════════════════════════════════════════════════════════════════════════════════

TEST(RtlLayoutTest, Tabs_SecondLayout_DefaultRootDirection_StaysRtl) {
    SilentObserver observer;
    FixedSurfaceContext ctx;
    VirtualDOM vdom(&observer, &ctx, nullptr);
    vdom.setLayoutDirection(LayoutDirection::RTL);

    // root(Column) -> tabs(Tabs) -> content(Row) -> [x, y]
    vdom.updateNode(makeSnap("root", "Column", {"tabs"}));
    vdom.updateNode(makeSnap("tabs", "Tabs", {"content"}));
    vdom.updateNode(makeSnap("content", "Row", {"x", "y"}));

    ComponentSnapshot xs = makeSnap("x", "Text");
    xs.styles["width"]  = SerializableData::parse("\"100px\"");
    xs.styles["height"] = SerializableData::parse("\"50px\"");
    vdom.updateNode(xs);

    ComponentSnapshot ys = makeSnap("y", "Text");
    ys.styles["width"]  = SerializableData::parse("\"100px\"");
    ys.styles["height"] = SerializableData::parse("\"50px\"");
    vdom.updateNode(ys);

    auto root = vdom.getRoot();
    ASSERT_NE(root, nullptr);
    auto tabs = root->findChild("tabs");
    ASSERT_NE(tabs, nullptr);
    // findChild only inspects direct children; walk one level at a time.
    auto content = tabs->findChild("content");
    ASSERT_NE(content, nullptr);
    auto x = content->findChild("x");
    auto y = content->findChild("y");
    ASSERT_NE(x, nullptr);
    ASSERT_NE(y, nullptr);

    // The Tabs minHeight fix-up (kTabBarHeight 96 + content 50) must have run
    // and the secondary layout applied it: tabs height = 146.
    EXPECT_FLOAT_EQ(px(tabs->getYogaNodeObj()->getLayoutHeight()), 146.0f);

    // Tabs children are pinned below the tab bar.
    EXPECT_FLOAT_EQ(px(content->getYogaNodeObj()->getLayoutTop()), 96.0f);

    // And the RTL mirroring survived BOTH layout passes: first child on the
    // physical right. content spans the full surface width (300).
    EXPECT_FLOAT_EQ(px(x->getYogaNodeObj()->getLayoutLeft()), 200.0f);
    EXPECT_FLOAT_EQ(px(y->getYogaNodeObj()->getLayoutLeft()), 100.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════════
// Inline shorthands (margin-inline / padding-inline / inset-inline), CSS
// Writing Modes 1-2 value forms: one value -> both inline edges; two values
// -> start | end. The edges stay logical (YGEdgeStart/End); Yoga resolves
// them against the layout direction.
// ═══════════════════════════════════════════════════════════════════════════════════

TEST(RtlLayoutTest, MarginInline_TwoValues_Ltr_StartThenEnd) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef a = t.make();
    YGNodeRef b = t.make();

    CSSStyleConverter::applyFlexDirection(root, YogaValue(std::string("row")));
    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);
    // Two-value form: start | end.
    CSSStyleConverter::applyMarginInline(a, YogaValue(std::string("40px 20px")));
    for (auto* child : {a, b}) {
        YGNodeStyleSetWidth(child, 100.0f);
        YGNodeStyleSetHeight(child, 50.0f);
        YGNodeInsertChild(root, child, YGNodeGetChildCount(root));
    }

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);

    // LTR: start -> physical left. a's start margin 40 puts it at 40; a's
    // end margin 20 pushes b to 40 + 100 + 20 = 160.
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(a)), 40.0f);
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(b)), 160.0f);
}

TEST(RtlLayoutTest, MarginInline_TwoValues_Rtl_Mirrored) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef a = t.make();
    YGNodeRef b = t.make();

    CSSStyleConverter::applyFlexDirection(root, YogaValue(std::string("row")));
    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);
    CSSStyleConverter::applyMarginInline(a, YogaValue(std::string("40px 20px")));
    for (auto* child : {a, b}) {
        YGNodeStyleSetWidth(child, 100.0f);
        YGNodeStyleSetHeight(child, 50.0f);
        YGNodeInsertChild(root, child, YGNodeGetChildCount(root));
    }

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionRTL);

    // RTL: start -> physical right. a sits at 300 - 40 - 100 = 160; a's end
    // margin 20 lands on its physical left, so b's right edge = 160 - 20 ->
    // b.left = 40.
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(a)), 160.0f);
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(b)), 40.0f);
}

TEST(RtlLayoutTest, PaddingInline_SingleValue_PadsBothInlineEdges) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef a = t.make();
    YGNodeRef b = t.make();

    CSSStyleConverter::applyFlexDirection(root, YogaValue(std::string("row")));
    CSSStyleConverter::applyJustifyContent(root, YogaValue(std::string("flex-end")));
    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);
    // Single-value form: both inline edges take it.
    CSSStyleConverter::applyPaddingInline(root, YogaValue(std::string("40px")));
    for (auto* child : {a, b}) {
        YGNodeStyleSetWidth(child, 100.0f);
        YGNodeStyleSetHeight(child, 50.0f);
        YGNodeInsertChild(root, child, YGNodeGetChildCount(root));
    }

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);

    // Content box = 300 - 40 (start) - 40 (end) = 220; flex-end packs [a,b]
    // against its right edge at 300 - 40 = 260 -> a.left = 260 - 200 = 60.
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(a)), 60.0f);
}

TEST(RtlLayoutTest, InsetInline_TwoValues_AnchorStartAndStretchToEnd) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef child = t.make();

    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);

    CSSStyleConverter::applyPosition(child, YogaValue(std::string("absolute")));
    CSSStyleConverter::applyHeight(child, YogaValue(50.0f), false);
    // Two-value form with no explicit width: the absolute child stretches
    // between the two inline insets.
    CSSStyleConverter::applyInsetInline(child, YogaValue(std::string("40px 20px")));
    YGNodeInsertChild(root, child, 0);

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);

    // start = 40 anchors the left edge; width = 300 - 40 - 20 = 240 proves
    // the second value landed on the end edge.
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(child)), 40.0f);
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetWidth(child)), 240.0f);
}

TEST(RtlLayoutTest, ConvertToYoga_MarginInlineShorthand_Rtl) {
    YogaTree t;
    YGNodeRef root = t.make();
    YGNodeRef a = t.make();
    YGNodeRef b = t.make();

    CSSStyleConverter::applyFlexDirection(root, YogaValue(std::string("row")));
    YGNodeStyleSetWidth(root, kRootWidth);
    YGNodeStyleSetHeight(root, kRootHeight);

    auto snap = std::make_shared<ComponentSnapshot>();
    snap->id = "a";
    snap->styles["width"] = SerializableData::parse("\"100px\"");
    snap->styles["height"] = SerializableData::parse("\"50px\"");
    // kebab-case shorthand key goes through the dispatch table.
    snap->styles["margin-inline"] = SerializableData::parse("\"40px 20px\"");
    ComponentSnapshotWrapper wrapper(snap);
    CSSStyleConverter::convertToYoga(wrapper, a, false /*clearAfterConvert*/);

    YGNodeStyleSetWidth(b, 100.0f);
    YGNodeStyleSetHeight(b, 50.0f);
    YGNodeInsertChild(root, a, 0);
    YGNodeInsertChild(root, b, 1);

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionRTL);

    // Dispatch entry + RTL: start margin 40 on the physical right ->
    // a.left = 160; a's end margin 20 on its left -> b.left = 40.
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(a)), 160.0f);
    EXPECT_FLOAT_EQ(px(YGNodeLayoutGetLeft(b)), 40.0f);
}
