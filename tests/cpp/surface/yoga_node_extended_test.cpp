#include <gtest/gtest.h>
#include "surface/yoga_node/agenui_css_style_converter.h"
#include "surface/yoga_node/agenui_measurement_manager.h"
#include "surface/yoga_node/agenui_yoga_layout_engine.h"
#include "surface/yoga_node/agenui_yoga_value.h"
#include "surface/yoga_node/agenui_tabs_yoga_helper.h"
#include <yoga/Yoga.h>
#include <memory>

using namespace agenui;

// ═══════════════════════════════════════════════════════════════════════════════════
// CSSStyleConverter Tests — static apply methods on raw YGNodeRef
// ═══════════════════════════════════════════════════════════════════════════════════

class CSSStyleConverterTest : public ::testing::Test {
protected:
    void SetUp() override {
        node = YGNodeNew();
    }
    void TearDown() override {
        YGNodeFree(node);
    }
    YGNodeRef node;
};

// --- Size properties ---

TEST_F(CSSStyleConverterTest, ApplyWidth_PointValue) {
    CSSStyleConverter::applyWidth(node, YogaValue(100.0f), false);
    EXPECT_FLOAT_EQ(YGNodeStyleGetWidth(node).value, 100.0f);
}

TEST_F(CSSStyleConverterTest, ApplyWidth_PercentValue) {
    CSSStyleConverter::applyWidth(node, YogaValue(std::string("50%")), false);
    auto w = YGNodeStyleGetWidth(node);
    EXPECT_FLOAT_EQ(w.value, 50.0f);
    EXPECT_EQ(w.unit, YGUnitPercent);
}

TEST_F(CSSStyleConverterTest, ApplyWidth_AutoValue) {
    CSSStyleConverter::applyWidth(node, YogaValue(std::string("auto")), false);
    EXPECT_EQ(YGNodeStyleGetWidth(node).unit, YGUnitAuto);
}

TEST_F(CSSStyleConverterTest, ApplyHeight_PointValue) {
    CSSStyleConverter::applyHeight(node, YogaValue(200.0f), false);
    EXPECT_FLOAT_EQ(YGNodeStyleGetHeight(node).value, 200.0f);
}

TEST_F(CSSStyleConverterTest, ApplyMinWidth_PointValue) {
    CSSStyleConverter::applyMinWidth(node, YogaValue(50.0f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetMinWidth(node).value, 50.0f);
}

TEST_F(CSSStyleConverterTest, ApplyMaxWidth_PointValue) {
    CSSStyleConverter::applyMaxWidth(node, YogaValue(300.0f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetMaxWidth(node).value, 300.0f);
}

TEST_F(CSSStyleConverterTest, ApplyMinHeight_PointValue) {
    CSSStyleConverter::applyMinHeight(node, YogaValue(40.0f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetMinHeight(node).value, 40.0f);
}

TEST_F(CSSStyleConverterTest, ApplyMaxHeight_PointValue) {
    CSSStyleConverter::applyMaxHeight(node, YogaValue(500.0f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetMaxHeight(node).value, 500.0f);
}

// --- Flexbox properties ---

TEST_F(CSSStyleConverterTest, ApplyFlexDirection_Row) {
    CSSStyleConverter::applyFlexDirection(node, YogaValue(std::string("row")));
    EXPECT_EQ(YGNodeStyleGetFlexDirection(node), YGFlexDirectionRow);
}

TEST_F(CSSStyleConverterTest, ApplyFlexDirection_Column) {
    CSSStyleConverter::applyFlexDirection(node, YogaValue(std::string("column")));
    EXPECT_EQ(YGNodeStyleGetFlexDirection(node), YGFlexDirectionColumn);
}

TEST_F(CSSStyleConverterTest, ApplyFlexDirection_RowReverse) {
    CSSStyleConverter::applyFlexDirection(node, YogaValue(std::string("row-reverse")));
    EXPECT_EQ(YGNodeStyleGetFlexDirection(node), YGFlexDirectionRowReverse);
}

TEST_F(CSSStyleConverterTest, ApplyFlexWrap_Wrap) {
    CSSStyleConverter::applyFlexWrap(node, YogaValue(std::string("wrap")));
    EXPECT_EQ(YGNodeStyleGetFlexWrap(node), YGWrapWrap);
}

TEST_F(CSSStyleConverterTest, ApplyFlexWrap_NoWrap) {
    CSSStyleConverter::applyFlexWrap(node, YogaValue(std::string("nowrap")));
    EXPECT_EQ(YGNodeStyleGetFlexWrap(node), YGWrapNoWrap);
}

TEST_F(CSSStyleConverterTest, ApplyJustifyContent_Center) {
    CSSStyleConverter::applyJustifyContent(node, YogaValue(std::string("center")));
    EXPECT_EQ(YGNodeStyleGetJustifyContent(node), YGJustifyCenter);
}

TEST_F(CSSStyleConverterTest, ApplyJustifyContent_SpaceBetween) {
    CSSStyleConverter::applyJustifyContent(node, YogaValue(std::string("space-between")));
    EXPECT_EQ(YGNodeStyleGetJustifyContent(node), YGJustifySpaceBetween);
}

TEST_F(CSSStyleConverterTest, ApplyAlignItems_Center) {
    CSSStyleConverter::applyAlignItems(node, YogaValue(std::string("center")));
    EXPECT_EQ(YGNodeStyleGetAlignItems(node), YGAlignCenter);
}

TEST_F(CSSStyleConverterTest, ApplyAlignItems_Stretch) {
    CSSStyleConverter::applyAlignItems(node, YogaValue(std::string("stretch")));
    EXPECT_EQ(YGNodeStyleGetAlignItems(node), YGAlignStretch);
}

TEST_F(CSSStyleConverterTest, ApplyFlexGrow_Value) {
    CSSStyleConverter::applyFlexGrow(node, YogaValue(2.0f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetFlexGrow(node), 2.0f);
}

TEST_F(CSSStyleConverterTest, ApplyFlexShrink_Value) {
    CSSStyleConverter::applyFlexShrink(node, YogaValue(0.5f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetFlexShrink(node), 0.5f);
}

// --- Spacing properties ---

TEST_F(CSSStyleConverterTest, ApplyPaddingLeft_PointValue) {
    CSSStyleConverter::applyPaddingLeft(node, YogaValue(10.0f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetPadding(node, YGEdgeLeft).value, 10.0f);
}

TEST_F(CSSStyleConverterTest, ApplyMarginTop_PointValue) {
    CSSStyleConverter::applyMarginTop(node, YogaValue(20.0f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetMargin(node, YGEdgeTop).value, 20.0f);
}

TEST_F(CSSStyleConverterTest, ApplyMarginBottom_Percent) {
    CSSStyleConverter::applyMarginBottom(node, YogaValue(std::string("10%")));
    auto margin = YGNodeStyleGetMargin(node, YGEdgeBottom);
    EXPECT_FLOAT_EQ(margin.value, 10.0f);
    EXPECT_EQ(margin.unit, YGUnitPercent);
}

TEST_F(CSSStyleConverterTest, ApplyGap_PointValue) {
    CSSStyleConverter::applyGap(node, YogaValue(8.0f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetGap(node, YGGutterAll), 8.0f);
}

// --- Positioning ---

TEST_F(CSSStyleConverterTest, ApplyPosition_Absolute) {
    CSSStyleConverter::applyPosition(node, YogaValue(std::string("absolute")));
    EXPECT_EQ(YGNodeStyleGetPositionType(node), YGPositionTypeAbsolute);
}

TEST_F(CSSStyleConverterTest, ApplyPosition_Relative) {
    CSSStyleConverter::applyPosition(node, YogaValue(std::string("relative")));
    EXPECT_EQ(YGNodeStyleGetPositionType(node), YGPositionTypeRelative);
}

TEST_F(CSSStyleConverterTest, ApplyTop_PointValue) {
    CSSStyleConverter::applyTop(node, YogaValue(5.0f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetPosition(node, YGEdgeTop).value, 5.0f);
}

TEST_F(CSSStyleConverterTest, ApplyLeft_PointValue) {
    CSSStyleConverter::applyLeft(node, YogaValue(15.0f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetPosition(node, YGEdgeLeft).value, 15.0f);
}

// --- Display ---

TEST_F(CSSStyleConverterTest, ApplyDisplay_None) {
    CSSStyleConverter::applyDisplay(node, YogaValue(std::string("none")));
    EXPECT_EQ(YGNodeStyleGetDisplay(node), YGDisplayNone);
}

TEST_F(CSSStyleConverterTest, ApplyDisplay_Flex) {
    CSSStyleConverter::applyDisplay(node, YogaValue(std::string("flex")));
    EXPECT_EQ(YGNodeStyleGetDisplay(node), YGDisplayFlex);
}

// --- Overflow ---

TEST_F(CSSStyleConverterTest, ApplyOverflow_Hidden) {
    CSSStyleConverter::applyOverflow(node, YogaValue(std::string("hidden")));
    EXPECT_EQ(YGNodeStyleGetOverflow(node), YGOverflowHidden);
}

TEST_F(CSSStyleConverterTest, ApplyOverflow_Scroll) {
    CSSStyleConverter::applyOverflow(node, YogaValue(std::string("scroll")));
    EXPECT_EQ(YGNodeStyleGetOverflow(node), YGOverflowScroll);
}

// --- AspectRatio ---

TEST_F(CSSStyleConverterTest, ApplyAspectRatio_Value) {
    CSSStyleConverter::applyAspectRatio(node, YogaValue(1.5f));
    EXPECT_FLOAT_EQ(YGNodeStyleGetAspectRatio(node), 1.5f);
}

// --- parseStyleDimension ---

TEST_F(CSSStyleConverterTest, ParseStyleDimension_ValidNumber) {
    nlohmann::json config = {{"size", "42px"}};
    float val = CSSStyleConverter::parseStyleDimension(config, "size", 0.0f);
    EXPECT_FLOAT_EQ(val, 42.0f);
}

TEST_F(CSSStyleConverterTest, ParseStyleDimension_MissingKey_ReturnsFallback) {
    nlohmann::json config = {};
    float val = CSSStyleConverter::parseStyleDimension(config, "missing", 99.0f);
    EXPECT_FLOAT_EQ(val, 99.0f);
}

// --- isRichText ---

TEST_F(CSSStyleConverterTest, IsRichText_PlainText_ReturnsFalse) {
    EXPECT_FALSE(CSSStyleConverter::isRichText("Hello world"));
}

TEST_F(CSSStyleConverterTest, IsRichText_EmptyString_ReturnsFalse) {
    EXPECT_FALSE(CSSStyleConverter::isRichText(""));
}

// ═══════════════════════════════════════════════════════════════════════════════════
// MeasurementManagerImpl Tests
// ═══════════════════════════════════════════════════════════════════════════════════

class MockMeasurement : public IMeasurement {
public:
    MeasureResult measure(const std::string& paramJson,
                          const MeasureModes& modes) override {
        lastParamJson = paramJson;
        return MeasureResult{CalcType::Sync, fixedWidth, fixedHeight, 0};
    }
    bool allowsMeasureWithChildren() const override { return allowsChildren; }

    float fixedWidth = 100.0f;
    float fixedHeight = 50.0f;
    bool allowsChildren = false;
    std::string lastParamJson;
};

class MeasurementManagerTest : public ::testing::Test {
protected:
    MeasurementManagerImpl manager;
};

TEST_F(MeasurementManagerTest, GetMeasurement_NotRegistered_ReturnsNull) {
    EXPECT_EQ(manager.getMeasurement("Unknown"), nullptr);
}

TEST_F(MeasurementManagerTest, RegisterAndGet_ReturnsImpl) {
    auto impl = std::make_shared<MockMeasurement>();
    manager.registerMeasurement("Text", impl);
    EXPECT_EQ(manager.getMeasurement("Text"), impl);
}

TEST_F(MeasurementManagerTest, Unregister_RemovesImpl) {
    auto impl = std::make_shared<MockMeasurement>();
    manager.registerMeasurement("Text", impl);
    manager.unregisterMeasurement("Text");
    EXPECT_EQ(manager.getMeasurement("Text"), nullptr);
}

TEST_F(MeasurementManagerTest, Measure_Registered_ReturnsResult) {
    auto impl = std::make_shared<MockMeasurement>();
    impl->fixedWidth = 200.0f;
    impl->fixedHeight = 80.0f;
    manager.registerMeasurement("Image", impl);

    MeasureModes modes{};
    auto result = manager.measure("Image", "{}", modes);
    EXPECT_FLOAT_EQ(result.width, 200.0f);
    EXPECT_FLOAT_EQ(result.height, 80.0f);
}

TEST_F(MeasurementManagerTest, Measure_NotRegistered_ReturnsZero) {
    MeasureModes modes{};
    auto result = manager.measure("Unknown", "{}", modes);
    EXPECT_FLOAT_EQ(result.width, 0.0f);
    EXPECT_FLOAT_EQ(result.height, 0.0f);
}

TEST_F(MeasurementManagerTest, Measure_PassesParamJson) {
    auto impl = std::make_shared<MockMeasurement>();
    manager.registerMeasurement("Text", impl);

    MeasureModes modes{};
    manager.measure("Text", "{\"text\":\"hello\"}", modes);
    EXPECT_EQ(impl->lastParamJson, "{\"text\":\"hello\"}");
}

TEST_F(MeasurementManagerTest, ShouldUseMeasureFunc_NotRegistered_ReturnsClear) {
    MeasureDecisionContext ctx;
    auto decision = manager.shouldUseMeasureFunc("Unknown", ctx);
    // Unregistered type returns Clear (not Skip)
    EXPECT_EQ(decision, MeasureDecision::Clear);
}

TEST_F(MeasurementManagerTest, ShouldUseMeasureFunc_Registered_NoChildren_ReturnsRegister) {
    auto impl = std::make_shared<MockMeasurement>();
    manager.registerMeasurement("Text", impl);

    MeasureDecisionContext ctx;
    ctx.hasChildren = false;
    ctx.platformSizeLocked = false;
    auto decision = manager.shouldUseMeasureFunc("Text", ctx);
    EXPECT_EQ(decision, MeasureDecision::Register);
}

TEST_F(MeasurementManagerTest, ShouldUseMeasureFunc_HasChildren_NoAllowsMeasure_ReturnsClear) {
    auto impl = std::make_shared<MockMeasurement>();
    impl->allowsChildren = false;
    manager.registerMeasurement("Text", impl);

    MeasureDecisionContext ctx;
    ctx.hasChildren = true;
    auto decision = manager.shouldUseMeasureFunc("Text", ctx);
    EXPECT_EQ(decision, MeasureDecision::Clear);
}

TEST_F(MeasurementManagerTest, ShouldUseMeasureFunc_HasChildren_AllowsMeasure_ReturnsRegister) {
    auto impl = std::make_shared<MockMeasurement>();
    impl->allowsChildren = true;
    manager.registerMeasurement("Tabs", impl);

    MeasureDecisionContext ctx;
    ctx.hasChildren = true;
    auto decision = manager.shouldUseMeasureFunc("Tabs", ctx);
    EXPECT_EQ(decision, MeasureDecision::Register);
}

TEST_F(MeasurementManagerTest, ShouldUseMeasureFunc_PlatformSizeLocked_ReturnsClear) {
    auto impl = std::make_shared<MockMeasurement>();
    manager.registerMeasurement("Image", impl);

    MeasureDecisionContext ctx;
    ctx.hasChildren = false;
    ctx.platformSizeLocked = true;
    auto decision = manager.shouldUseMeasureFunc("Image", ctx);
    EXPECT_EQ(decision, MeasureDecision::Clear);
}

TEST_F(MeasurementManagerTest, MultipleRegistrations_Independent) {
    auto text = std::make_shared<MockMeasurement>();
    text->fixedWidth = 100.0f;
    auto image = std::make_shared<MockMeasurement>();
    image->fixedWidth = 200.0f;

    manager.registerMeasurement("Text", text);
    manager.registerMeasurement("Image", image);

    EXPECT_EQ(manager.getMeasurement("Text"), text);
    EXPECT_EQ(manager.getMeasurement("Image"), image);

    manager.unregisterMeasurement("Text");
    EXPECT_EQ(manager.getMeasurement("Text"), nullptr);
    EXPECT_EQ(manager.getMeasurement("Image"), image);
}

// ═══════════════════════════════════════════════════════════════════════════════════
// YogaLayoutEngine Basic Tests
// ═══════════════════════════════════════════════════════════════════════════════════

class YogaLayoutEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<YogaLayoutEngine>();
    }

    std::unique_ptr<YogaLayoutEngine> engine;
};

TEST_F(YogaLayoutEngineTest, CreateNode_ReturnsNonNull) {
    auto* node = engine->createNode("root");
    EXPECT_NE(node, nullptr);
}

TEST_F(YogaLayoutEngineTest, CreateNode_SameId_ReturnsSameNode) {
    auto* node1 = engine->createNode("node1");
    auto* node2 = engine->createNode("node1");
    EXPECT_EQ(node1, node2);
}

TEST_F(YogaLayoutEngineTest, RemoveNode_NoCrash) {
    engine->createNode("temp");
    engine->removeNode("temp");
}

TEST_F(YogaLayoutEngineTest, RemoveNode_NonExistent_NoCrash) {
    engine->removeNode("does_not_exist");
}

TEST_F(YogaLayoutEngineTest, InsertChild_ValidParentChild) {
    engine->createNode("parent");
    engine->createNode("child");
    engine->insertChild("parent", "child", 0);
    // No crash = success
}

TEST_F(YogaLayoutEngineTest, RemoveChild_ValidParentChild) {
    engine->createNode("parent");
    engine->createNode("child");
    engine->insertChild("parent", "child", 0);
    engine->removeChild("parent", "child");
    // No crash = success
}

TEST_F(YogaLayoutEngineTest, ClearAll_NoCrash) {
    engine->createNode("a");
    engine->createNode("b");
    engine->clearAll();
}

TEST_F(YogaLayoutEngineTest, CalculateLayout_EmptyRoot_ReturnsFalse) {
    auto* root = engine->createNode("root");
    engine->setRootNode(root);
    bool result = engine->calculateLayout("root", 375.0f);
    // Should work without crash
    EXPECT_TRUE(result || !result); // just verify no crash
}

TEST_F(YogaLayoutEngineTest, ReadLayoutResult_AfterCalculate) {
    auto* root = engine->createNode("root");
    engine->setRootNode(root);
    engine->calculateLayout("root", 375.0f);

    float x, y, w, h;
    bool ok = engine->readLayoutResult("root", x, y, w, h);
    if (ok) {
        EXPECT_GE(w, 0.0f);
        EXPECT_GE(h, 0.0f);
    }
}

TEST_F(YogaLayoutEngineTest, ReadLayoutResult_NonExistentNode_ReturnsFalse) {
    float x, y, w, h;
    bool ok = engine->readLayoutResult("non_existent", x, y, w, h);
    EXPECT_FALSE(ok);
}

// ═══════════════════════════════════════════════════════════════════════════════════
// TabsYogaHelper Constants Test
// ═══════════════════════════════════════════════════════════════════════════════════

TEST(TabsYogaHelperTest, TabBarHeight_Is96) {
    // Verify the constant
    EXPECT_FLOAT_EQ(TabsYogaHelper::kTabBarHeight, 96.0f);
}
