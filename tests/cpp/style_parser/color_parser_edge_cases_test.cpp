#include <gtest/gtest.h>
#include "style_parser/agenui_color_parser.h"

using agenui::ColorParser;
using agenui::ColorValue;
using agenui::ColorValueType;

// ============================================================================
// Helpers
// ============================================================================

static bool parseSolid(const std::string& css, uint32_t& argb) {
    ColorValue cv;
    if (!ColorParser::parse(css, cv)) return false;
    if (cv.type != ColorValueType::Solid) return false;
    argb = cv.solidColor;
    return true;
}

static bool parseGradient(const std::string& css, ColorValue& cv) {
    if (!ColorParser::parse(css, cv)) return false;
    return cv.type == ColorValueType::Gradient;
}

// ============================================================================
// RGB: Negative channel values => clamp to 0
// ============================================================================

TEST(ColorParserRgbClamp, NegativeRed_ClampsToZero) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("rgb(-10, 0, 0)", argb));
    EXPECT_EQ(argb, 0xFF000000);
}

TEST(ColorParserRgbClamp, NegativeAll_ClampsToZero) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("rgb(-1, -1, -1)", argb));
    EXPECT_EQ(argb, 0xFF000000);
}

TEST(ColorParserRgbClamp, LargeValue_ClampsTo255) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("rgb(300, 500, 1000)", argb));
    EXPECT_EQ(argb, 0xFFFFFFFF);
}

// ============================================================================
// RGBA: Alpha clamping
// ============================================================================

TEST(ColorParserAlphaClamp, AlphaGreaterThan1_ClampsTo1) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("rgba(255, 0, 0, 2.0)", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
}

TEST(ColorParserAlphaClamp, AlphaNegative_ClampsTo0) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("rgba(255, 0, 0, -0.5)", argb));
    EXPECT_EQ(argb, 0x00FF0000);
}

TEST(ColorParserAlphaClamp, AlphaPercentOver100_ClampsTo1) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("rgb(255 0 0 / 150%)", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
}

TEST(ColorParserAlphaClamp, AlphaPercentNegative_ClampsTo0) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("rgb(255 0 0 / -50%)", argb));
    EXPECT_EQ(argb, 0x00FF0000);
}

// ============================================================================
// HSL: Hue wrapping
// ============================================================================

TEST(ColorParserHslWrap, Hue360_WrapsTo0_EqualsRed) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hsl(360, 100%, 50%)", argb));
    EXPECT_EQ(argb, 0xFFFF0000);  // same as hsl(0, 100%, 50%)
}

TEST(ColorParserHslWrap, Hue720_WrapsTo0_EqualsRed) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hsl(720, 100%, 50%)", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
}

TEST(ColorParserHslWrap, NegativeHue_WrapsCorrectly) {
    // -120 => 240 => blue
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hsl(-120, 100%, 50%)", argb));
    EXPECT_EQ(argb, 0xFF0000FF);
}

TEST(ColorParserHslWrap, Hue480_WrapsTo120_EqualsGreen) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hsl(480, 100%, 50%)", argb));
    EXPECT_EQ(argb, 0xFF00FF00);  // 480 - 360 = 120 => green
}

// ============================================================================
// HSL: Saturation/lightness clamping
// ============================================================================

TEST(ColorParserHslClamp, SaturationOver100_ClampsTo100) {
    uint32_t argb1 = 0, argb2 = 0;
    ASSERT_TRUE(parseSolid("hsl(0, 200%, 50%)", argb1));
    ASSERT_TRUE(parseSolid("hsl(0, 100%, 50%)", argb2));
    EXPECT_EQ(argb1, argb2);
}

TEST(ColorParserHslClamp, LightnessOver100_ClampsTo100) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hsl(0, 100%, 200%)", argb));
    EXPECT_EQ(argb, 0xFFFFFFFF);  // lightness 100% = white
}

// ============================================================================
// HSL: Hue with unit variations (CSS Color Level 4)
// ============================================================================

TEST(ColorParserHslUnits, HueInDeg) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hsl(120deg, 100%, 50%)", argb));
    EXPECT_EQ(argb, 0xFF00FF00);
}

TEST(ColorParserHslUnits, HueInTurn) {
    // 0.333turn ~ 120deg => green
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hsl(0.3333turn, 100%, 50%)", argb));
    // approximately green (120 deg), may have small rounding
    uint8_t r = (argb >> 16) & 0xFF;
    uint8_t g = (argb >> 8) & 0xFF;
    EXPECT_LE(r, 2);
    EXPECT_GE(g, 253);
}

TEST(ColorParserHslUnits, HueInRad) {
    // PI rad = 180 deg => cyan
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hsl(3.14159rad, 100%, 50%)", argb));
    uint8_t r = (argb >> 16) & 0xFF;
    uint8_t b = argb & 0xFF;
    EXPECT_LE(r, 2);   // ~0
    EXPECT_GE(b, 253); // ~255
}

// ============================================================================
// CSS Color Level 4: "none" keyword
// ============================================================================

TEST(ColorParserNone, AlphaNone_FullyTransparent) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("rgb(255 0 0 / none)", argb));
    EXPECT_EQ(argb, 0x00FF0000);
}

TEST(ColorParserNone, HslHueNone_TreatedAsZero) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hsl(none, 100%, 50%)", argb));
    EXPECT_EQ(argb, 0xFFFF0000);  // hue 0 = red
}

// ============================================================================
// Gradient with hex color stops
// ============================================================================

TEST(ColorParserGradientHex, LinearWithHexStops) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("linear-gradient(#ff0000, #0000ff)", cv));
    EXPECT_EQ(cv.gradient.colorStops.size(), 2u);
    EXPECT_EQ(cv.gradient.colorStops[0].color, 0xFFFF0000u);
    EXPECT_EQ(cv.gradient.colorStops[1].color, 0xFF0000FFu);
}

TEST(ColorParserGradientHex, LinearWithHexAndPosition) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("linear-gradient(#ff0000 0%, #00ff00 50%, #0000ff 100%)", cv));
    EXPECT_EQ(cv.gradient.colorStops.size(), 3u);
    EXPECT_TRUE(cv.gradient.colorStops[1].hasPosition);
    EXPECT_FLOAT_EQ(cv.gradient.colorStops[1].position, 0.5f);
}

TEST(ColorParserGradientHex, RadialWithRgbaStop) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("radial-gradient(rgba(255,0,0,0.5), blue)", cv));
    EXPECT_EQ(cv.gradient.colorStops.size(), 2u);
    EXPECT_EQ(cv.gradient.colorStops[0].color, 0x80FF0000u);
}

// ============================================================================
// Edge: Very long / unusual but valid inputs
// ============================================================================

TEST(ColorParserEdge, ManySpacesInsideRgb) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("rgb(  255  ,  0  ,  0  )", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
}

TEST(ColorParserEdge, TabsInRgb) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("rgb(\t255\t,\t0\t,\t0\t)", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
}
