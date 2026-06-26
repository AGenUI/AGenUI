/**
 * Unit tests for a2ui::UnitConverter (HarmonyOS unit conversion utilities).
 *
 * Tests the pure math conversion functions:
 * - a2uiToVp: a2ui / 2
 * - vpToA2ui: vp * 2
 * - pxToA2ui: px / density * 2
 * - pxToVp:   px / density
 * - a2uiToPx: a2ui / 2 * density
 */
#include <gtest/gtest.h>
#include <atomic>
#include <cmath>

// The header declares `extern std::atomic<float> gDensityForUI;` — we define it here for testing.
std::atomic<float> gDensityForUI{1.0f};

#include "a2ui_unit_utils.h"

using a2ui::UnitConverter;

class UnitConverterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset to 1.0 density by default
        gDensityForUI.store(1.0f);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// a2uiToVp: value / 2
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(UnitConverterTest, A2uiToVp_Zero) {
    EXPECT_FLOAT_EQ(UnitConverter::a2uiToVp(0.0f), 0.0f);
}

TEST_F(UnitConverterTest, A2uiToVp_PositiveEven) {
    EXPECT_FLOAT_EQ(UnitConverter::a2uiToVp(360.0f), 180.0f);
}

TEST_F(UnitConverterTest, A2uiToVp_PositiveOdd) {
    EXPECT_FLOAT_EQ(UnitConverter::a2uiToVp(1.0f), 0.5f);
}

TEST_F(UnitConverterTest, A2uiToVp_Negative) {
    EXPECT_FLOAT_EQ(UnitConverter::a2uiToVp(-100.0f), -50.0f);
}

TEST_F(UnitConverterTest, A2uiToVp_Large) {
    EXPECT_FLOAT_EQ(UnitConverter::a2uiToVp(10000.0f), 5000.0f);
}

TEST_F(UnitConverterTest, A2uiToVp_SmallFraction) {
    EXPECT_FLOAT_EQ(UnitConverter::a2uiToVp(0.01f), 0.005f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// vpToA2ui: value * 2
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(UnitConverterTest, VpToA2ui_Zero) {
    EXPECT_FLOAT_EQ(UnitConverter::vpToA2ui(0.0f), 0.0f);
}

TEST_F(UnitConverterTest, VpToA2ui_Positive) {
    EXPECT_FLOAT_EQ(UnitConverter::vpToA2ui(100.0f), 200.0f);
}

TEST_F(UnitConverterTest, VpToA2ui_Negative) {
    EXPECT_FLOAT_EQ(UnitConverter::vpToA2ui(-50.0f), -100.0f);
}

TEST_F(UnitConverterTest, VpToA2ui_Fraction) {
    EXPECT_FLOAT_EQ(UnitConverter::vpToA2ui(0.5f), 1.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// pxToA2ui: px / density * 2
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(UnitConverterTest, PxToA2ui_Density1) {
    gDensityForUI.store(1.0f);
    EXPECT_FLOAT_EQ(UnitConverter::pxToA2ui(100.0f), 200.0f);
}

TEST_F(UnitConverterTest, PxToA2ui_Density2) {
    gDensityForUI.store(2.0f);
    EXPECT_FLOAT_EQ(UnitConverter::pxToA2ui(100.0f), 100.0f);
}

TEST_F(UnitConverterTest, PxToA2ui_Density3) {
    gDensityForUI.store(3.0f);
    EXPECT_NEAR(UnitConverter::pxToA2ui(300.0f), 200.0f, 0.001f);
}

TEST_F(UnitConverterTest, PxToA2ui_Zero) {
    gDensityForUI.store(2.0f);
    EXPECT_FLOAT_EQ(UnitConverter::pxToA2ui(0.0f), 0.0f);
}

TEST_F(UnitConverterTest, PxToA2ui_FractionalDensity) {
    gDensityForUI.store(1.5f);
    // 300 / 1.5 * 2 = 400
    EXPECT_NEAR(UnitConverter::pxToA2ui(300.0f), 400.0f, 0.001f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// pxToVp: px / density
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(UnitConverterTest, PxToVp_Density1) {
    gDensityForUI.store(1.0f);
    EXPECT_FLOAT_EQ(UnitConverter::pxToVp(100.0f), 100.0f);
}

TEST_F(UnitConverterTest, PxToVp_Density3) {
    gDensityForUI.store(3.0f);
    EXPECT_NEAR(UnitConverter::pxToVp(300.0f), 100.0f, 0.001f);
}

TEST_F(UnitConverterTest, PxToVp_Zero) {
    gDensityForUI.store(2.0f);
    EXPECT_FLOAT_EQ(UnitConverter::pxToVp(0.0f), 0.0f);
}

TEST_F(UnitConverterTest, PxToVp_Negative) {
    gDensityForUI.store(2.0f);
    EXPECT_FLOAT_EQ(UnitConverter::pxToVp(-100.0f), -50.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// a2uiToPx: a2ui / 2 * density
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(UnitConverterTest, A2uiToPx_Density1) {
    gDensityForUI.store(1.0f);
    EXPECT_FLOAT_EQ(UnitConverter::a2uiToPx(200.0f), 100.0f);
}

TEST_F(UnitConverterTest, A2uiToPx_Density3) {
    gDensityForUI.store(3.0f);
    EXPECT_FLOAT_EQ(UnitConverter::a2uiToPx(200.0f), 300.0f);
}

TEST_F(UnitConverterTest, A2uiToPx_Zero) {
    gDensityForUI.store(2.0f);
    EXPECT_FLOAT_EQ(UnitConverter::a2uiToPx(0.0f), 0.0f);
}

TEST_F(UnitConverterTest, A2uiToPx_Negative) {
    gDensityForUI.store(2.0f);
    EXPECT_FLOAT_EQ(UnitConverter::a2uiToPx(-200.0f), -200.0f);
}

TEST_F(UnitConverterTest, A2uiToPx_FractionalDensity) {
    gDensityForUI.store(2.5f);
    // 200 / 2 * 2.5 = 250
    EXPECT_FLOAT_EQ(UnitConverter::a2uiToPx(200.0f), 250.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Round-trip consistency
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(UnitConverterTest, RoundTrip_A2ui_Vp_A2ui) {
    float original = 123.456f;
    float roundTripped = UnitConverter::vpToA2ui(UnitConverter::a2uiToVp(original));
    EXPECT_NEAR(roundTripped, original, 0.001f);
}

TEST_F(UnitConverterTest, RoundTrip_Px_A2ui_Px) {
    gDensityForUI.store(2.0f);
    float original = 256.0f;
    float roundTripped = UnitConverter::a2uiToPx(UnitConverter::pxToA2ui(original));
    EXPECT_NEAR(roundTripped, original, 0.001f);
}

TEST_F(UnitConverterTest, RoundTrip_Px_Vp_Px) {
    gDensityForUI.store(3.0f);
    float original = 333.0f;
    float roundTripped = UnitConverter::pxToVp(original) * gDensityForUI.load();
    EXPECT_NEAR(roundTripped, original, 0.001f);
}
