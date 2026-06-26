/**
 * HarmonyOS Measurement Component Unit Tests
 *
 * Tests the pure-logic measurement classes that can be compiled without
 * HarmonyOS platform dependencies (no text layout, no native drawing).
 *
 * Testable classes (self-contained, only need JSON + agenui_measurement.h):
 *   - ImageComponentMeasurement
 *   - DividerComponentMeasurement
 *
 * NOT tested here (require HarmonyOS native_drawing + text typography APIs):
 *   - TextComponentMeasurement, CheckBoxComponentMeasurement,
 *     SliderComponentMeasurement, AudioPlayerComponentMeasurement,
 *     TableComponentMeasurement, TabsComponentMeasurement,
 *     ChoicePickerComponentMeasurement, DateTimeInputComponentMeasurement
 *   These should be tested in the HarmonyOS on-device test environment.
 */

#include <gtest/gtest.h>
#include "agenui_measurement.h"
#include "image_component_measurement.h"
#include "divider_component_measurement.h"

using namespace agenui;
using namespace a2ui;

// ═══════════════════════════════════════════════════════════════════════════════
// ImageComponentMeasurement Tests
// ═══════════════════════════════════════════════════════════════════════════════

class ImageMeasureTest : public ::testing::Test {
protected:
    ImageComponentMeasurement measurer;
    MeasureModes modes;

    void SetUp() override {
        modes.width = {720.0f, 2};   // AtMost 720
        modes.height = {0.0f, 0};    // Undefined
    }
};

// --- styleInfo path ---

TEST_F(ImageMeasureTest, StyleInfo_NumericWidthHeight_ReturnsParsed) {
    std::string json = R"({"styleInfo":"{\"width\":200,\"height\":100}"})";
    auto result = measurer.measure(json, modes);
    EXPECT_EQ(result.calcType, CalcType::Sync);
    EXPECT_FLOAT_EQ(result.width, 200.0f);
    EXPECT_FLOAT_EQ(result.height, 100.0f);
}

TEST_F(ImageMeasureTest, StyleInfo_StringWithUnit_ParsesNumericPart) {
    std::string json = R"({"styleInfo":"{\"width\":\"150vp\",\"height\":\"80vp\"}"})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 150.0f);
    EXPECT_FLOAT_EQ(result.height, 80.0f);
}

TEST_F(ImageMeasureTest, StyleInfo_PercentWidth_ReturnsZero) {
    // Percentage values should NOT be resolved (Yoga handles them)
    std::string json = R"({"styleInfo":"{\"width\":\"50%\",\"height\":100}"})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 0.0f);  // % is not explicit px
    EXPECT_FLOAT_EQ(result.height, 100.0f);
}

TEST_F(ImageMeasureTest, StyleInfo_AutoWidth_ReturnsZero) {
    std::string json = R"({"styleInfo":"{\"width\":\"auto\",\"height\":\"auto\"}"})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 0.0f);
    EXPECT_FLOAT_EQ(result.height, 0.0f);
}

TEST_F(ImageMeasureTest, StyleInfo_EmptyString_ReturnsZero) {
    std::string json = R"({"styleInfo":"{\"width\":\"\",\"height\":\"\"}"})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 0.0f);
    EXPECT_FLOAT_EQ(result.height, 0.0f);
}

// --- styles fallback path ---

TEST_F(ImageMeasureTest, StylesFallback_NumericWidthHeight) {
    std::string json = R"({"styles":{"width":300,"height":150}})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 300.0f);
    EXPECT_FLOAT_EQ(result.height, 150.0f);
}

TEST_F(ImageMeasureTest, StylesFallback_StringWithUnit) {
    std::string json = R"({"styles":{"width":"250px","height":"125px"}})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 250.0f);
    EXPECT_FLOAT_EQ(result.height, 125.0f);
}

TEST_F(ImageMeasureTest, StylesFallback_PercentNotResolved) {
    std::string json = R"({"styles":{"width":"100%","height":"50%"}})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 0.0f);
    EXPECT_FLOAT_EQ(result.height, 0.0f);
}

// --- priority: styleInfo > styles ---

TEST_F(ImageMeasureTest, StyleInfo_TakesPriority_OverStyles) {
    std::string json = R"({"styleInfo":"{\"width\":100,\"height\":50}","styles":{"width":999,"height":999}})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 100.0f);
    EXPECT_FLOAT_EQ(result.height, 50.0f);
}

// --- error handling ---

TEST_F(ImageMeasureTest, InvalidJson_ReturnsZero) {
    std::string json = "not-json-at-all";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 0.0f);
    EXPECT_FLOAT_EQ(result.height, 0.0f);
}

TEST_F(ImageMeasureTest, EmptyJson_ReturnsZero) {
    std::string json = "{}";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 0.0f);
    EXPECT_FLOAT_EQ(result.height, 0.0f);
}

TEST_F(ImageMeasureTest, MissingStyleInfo_MissingStyles_ReturnsZero) {
    std::string json = R"({"id":"img-1","type":"Image"})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 0.0f);
    EXPECT_FLOAT_EQ(result.height, 0.0f);
}

TEST_F(ImageMeasureTest, StyleInfo_InvalidInnerJson_FallsToStyles) {
    std::string json = R"({"styleInfo":"not-json","styles":{"width":120,"height":60}})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 120.0f);
    EXPECT_FLOAT_EQ(result.height, 60.0f);
}

TEST_F(ImageMeasureTest, OnlyWidth_HeightRemainsZero) {
    std::string json = R"({"styleInfo":"{\"width\":200}"})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 200.0f);
    EXPECT_FLOAT_EQ(result.height, 0.0f);
}

TEST_F(ImageMeasureTest, OnlyHeight_WidthRemainsZero) {
    std::string json = R"({"styleInfo":"{\"height\":100}"})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 0.0f);
    EXPECT_FLOAT_EQ(result.height, 100.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// DividerComponentMeasurement Tests
// ═══════════════════════════════════════════════════════════════════════════════

class DividerMeasureTest : public ::testing::Test {
protected:
    DividerComponentMeasurement measurer;
    MeasureModes modes;

    void SetUp() override {
        modes.width = {720.0f, 2};   // AtMost 720
        modes.height = {1000.0f, 2}; // AtMost 1000
    }
};

// --- vertical divider (default) ---

TEST_F(DividerMeasureTest, VerticalDefault_ThicknessAsWidth_HeightFromModes) {
    std::string json = R"({})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 1.0f);     // default thickness
    EXPECT_FLOAT_EQ(result.height, 1000.0f); // from mode AtMost
}

TEST_F(DividerMeasureTest, VerticalExplicitThickness_NumericValue) {
    std::string json = R"({"thickness":3})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 3.0f);
    EXPECT_FLOAT_EQ(result.height, 1000.0f);
}

TEST_F(DividerMeasureTest, VerticalExplicitThickness_StringValue) {
    std::string json = R"({"thickness":"5"})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 5.0f);
    EXPECT_FLOAT_EQ(result.height, 1000.0f);
}

TEST_F(DividerMeasureTest, VerticalDivider_HeightExactly) {
    modes.height = {500.0f, 1}; // Exactly 500
    std::string json = R"({})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 1.0f);
    EXPECT_FLOAT_EQ(result.height, 500.0f);
}

TEST_F(DividerMeasureTest, VerticalDivider_HeightUndefined_ReturnsZero) {
    modes.height = {0.0f, 0}; // Undefined
    std::string json = R"({})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 1.0f);
    EXPECT_FLOAT_EQ(result.height, 0.0f);
}

// --- horizontal divider ---

TEST_F(DividerMeasureTest, HorizontalDivider_ThicknessAsHeight_WidthFromModes) {
    std::string json = R"({"styles":{"axis":"horizontal"}})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 720.0f);  // from mode AtMost
    EXPECT_FLOAT_EQ(result.height, 1.0f);    // default thickness
}

TEST_F(DividerMeasureTest, HorizontalDivider_CustomThickness) {
    std::string json = R"({"thickness":2,"styles":{"axis":"horizontal"}})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 720.0f);
    EXPECT_FLOAT_EQ(result.height, 2.0f);
}

TEST_F(DividerMeasureTest, HorizontalDivider_WidthExactly) {
    modes.width = {400.0f, 1}; // Exactly 400
    std::string json = R"({"styles":{"axis":"horizontal"}})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 400.0f);
    EXPECT_FLOAT_EQ(result.height, 1.0f);
}

TEST_F(DividerMeasureTest, HorizontalDivider_WidthUndefined_ReturnsZero) {
    modes.width = {0.0f, 0}; // Undefined
    std::string json = R"({"styles":{"axis":"horizontal"}})";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 0.0f);
    EXPECT_FLOAT_EQ(result.height, 1.0f);
}

// --- error handling ---

TEST_F(DividerMeasureTest, InvalidJson_DefaultBehavior) {
    std::string json = "invalid-json!!";
    auto result = measurer.measure(json, modes);
    // Invalid JSON → discarded → defaults (vertical, thickness=1)
    EXPECT_FLOAT_EQ(result.width, 1.0f);
    EXPECT_FLOAT_EQ(result.height, 1000.0f);
}

TEST_F(DividerMeasureTest, EmptyJson_DefaultVerticalDivider) {
    std::string json = "{}";
    auto result = measurer.measure(json, modes);
    EXPECT_FLOAT_EQ(result.width, 1.0f);
    EXPECT_FLOAT_EQ(result.height, 1000.0f);
}

TEST_F(DividerMeasureTest, SyncCalcType) {
    std::string json = "{}";
    auto result = measurer.measure(json, modes);
    EXPECT_EQ(result.calcType, CalcType::Sync);
}

TEST_F(DividerMeasureTest, ZeroCountOfLines) {
    std::string json = "{}";
    auto result = measurer.measure(json, modes);
    EXPECT_EQ(result.countOfLines, 0);
}
