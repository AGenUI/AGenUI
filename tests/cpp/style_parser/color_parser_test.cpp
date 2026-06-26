#include <gtest/gtest.h>
#include "style_parser/agenui_color_parser.h"

using agenui::ColorParser;
using agenui::ColorValue;
using agenui::ColorValueType;
using agenui::GradientType;

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
// Hex colors
// ============================================================================

struct HexCase { std::string input; uint32_t expected; };
class ColorParserHexTest : public ::testing::TestWithParam<HexCase> {};

TEST_P(ColorParserHexTest, ParsesCorrectly) {
    auto [input, expected] = GetParam();
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid(input, argb));
    EXPECT_EQ(argb, expected);
}

INSTANTIATE_TEST_SUITE_P(Hex3, ColorParserHexTest, ::testing::Values(
    HexCase{"#000", 0xFF000000},
    HexCase{"#fff", 0xFFFFFFFF},
    HexCase{"#FFF", 0xFFFFFFFF},
    HexCase{"#f00", 0xFFFF0000},
    HexCase{"#0f0", 0xFF00FF00},
    HexCase{"#00f", 0xFF0000FF},
    HexCase{"#abc", 0xFFAABBCC}
));

INSTANTIATE_TEST_SUITE_P(Hex4, ColorParserHexTest, ::testing::Values(
    HexCase{"#0000", 0x00000000},
    HexCase{"#ffff", 0xFFFFFFFF},
    HexCase{"#f00f", 0xFFFF0000},
    HexCase{"#0f08", 0x8800FF00}
));

INSTANTIATE_TEST_SUITE_P(Hex6, ColorParserHexTest, ::testing::Values(
    HexCase{"#000000", 0xFF000000},
    HexCase{"#ffffff", 0xFFFFFFFF},
    HexCase{"#FFFFFF", 0xFFFFFFFF},
    HexCase{"#ff0000", 0xFFFF0000},
    HexCase{"#00ff00", 0xFF00FF00},
    HexCase{"#0000ff", 0xFF0000FF},
    HexCase{"#aabbcc", 0xFFAABBCC},
    HexCase{"#123456", 0xFF123456}
));

INSTANTIATE_TEST_SUITE_P(Hex8, ColorParserHexTest, ::testing::Values(
    HexCase{"#00000000", 0x00000000},
    HexCase{"#ffffffff", 0xFFFFFFFF},
    HexCase{"#ff000080", 0x80FF0000},
    HexCase{"#00ff00ff", 0xFF00FF00},
    HexCase{"#123456ab", 0xAB123456}
));

// ============================================================================
// rgb() / rgba()
// ============================================================================

struct RgbCase { std::string input; uint32_t expected; };
class ColorParserRgbTest : public ::testing::TestWithParam<RgbCase> {};

TEST_P(ColorParserRgbTest, ParsesCorrectly) {
    auto [input, expected] = GetParam();
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid(input, argb));
    EXPECT_EQ(argb, expected);
}

INSTANTIATE_TEST_SUITE_P(RgbLegacy, ColorParserRgbTest, ::testing::Values(
    RgbCase{"rgb(255, 0, 0)", 0xFFFF0000},
    RgbCase{"rgb(0, 255, 0)", 0xFF00FF00},
    RgbCase{"rgb(0, 0, 255)", 0xFF0000FF},
    RgbCase{"rgb(0, 0, 0)", 0xFF000000},
    RgbCase{"rgb(255, 255, 255)", 0xFFFFFFFF}
));

INSTANTIATE_TEST_SUITE_P(RgbaLegacy, ColorParserRgbTest, ::testing::Values(
    RgbCase{"rgba(255, 0, 0, 1)", 0xFFFF0000},
    RgbCase{"rgba(255, 0, 0, 0)", 0x00FF0000},
    RgbCase{"rgba(255, 0, 0, 0.5)", 0x80FF0000},
    RgbCase{"rgba(0, 0, 0, 0.5)", 0x80000000}
));

INSTANTIATE_TEST_SUITE_P(RgbSpaceSeparated, ColorParserRgbTest, ::testing::Values(
    RgbCase{"rgb(255 0 0)", 0xFFFF0000},
    RgbCase{"rgb(255 0 0 / 0.5)", 0x80FF0000},
    RgbCase{"rgb(0 0 0 / 1)", 0xFF000000}
));

INSTANTIATE_TEST_SUITE_P(RgbPercentage, ColorParserRgbTest, ::testing::Values(
    RgbCase{"rgb(100%, 0%, 0%)", 0xFFFF0000},
    RgbCase{"rgb(0%, 100%, 0%)", 0xFF00FF00},
    RgbCase{"rgb(50%, 50%, 50%)", 0xFF808080}
));

// ============================================================================
// hsl() / hsla()
// ============================================================================

struct HslCase { std::string input; uint32_t expected; };
class ColorParserHslTest : public ::testing::TestWithParam<HslCase> {};

TEST_P(ColorParserHslTest, ParsesCorrectly) {
    auto [input, expected] = GetParam();
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid(input, argb));
    EXPECT_EQ(argb, expected);
}

INSTANTIATE_TEST_SUITE_P(HslBasic, ColorParserHslTest, ::testing::Values(
    HslCase{"hsl(0, 100%, 50%)", 0xFFFF0000},
    HslCase{"hsl(120, 100%, 50%)", 0xFF00FF00},
    HslCase{"hsl(240, 100%, 50%)", 0xFF0000FF},
    HslCase{"hsl(0, 0%, 0%)", 0xFF000000},
    HslCase{"hsl(0, 0%, 100%)", 0xFFFFFFFF}
));

INSTANTIATE_TEST_SUITE_P(HslaBasic, ColorParserHslTest, ::testing::Values(
    HslCase{"hsla(0, 100%, 50%, 0.5)", 0x80FF0000},
    HslCase{"hsla(0, 100%, 50%, 1)", 0xFFFF0000},
    HslCase{"hsla(0, 100%, 50%, 0)", 0x00FF0000}
));

INSTANTIATE_TEST_SUITE_P(HslSpaceSeparated, ColorParserHslTest, ::testing::Values(
    HslCase{"hsl(0 100% 50%)", 0xFFFF0000},
    HslCase{"hsl(120 100% 50%)", 0xFF00FF00},
    HslCase{"hsl(0 100% 50% / 0.5)", 0x80FF0000}
));

// ============================================================================
// hwb()
// ============================================================================

TEST(ColorParserHwb, Pure_Red) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hwb(0 0% 0%)", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
}

TEST(ColorParserHwb, White) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hwb(0 100% 0%)", argb));
    EXPECT_EQ(argb, 0xFFFFFFFF);
}

TEST(ColorParserHwb, Black) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hwb(0 0% 100%)", argb));
    EXPECT_EQ(argb, 0xFF000000);
}

TEST(ColorParserHwb, WithAlpha) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hwb(0 0% 0% / 0.5)", argb));
    EXPECT_EQ(argb, 0x80FF0000);
}

TEST(ColorParserHwb, Gray_WPlusBGe1) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("hwb(0 50% 50%)", argb));
    EXPECT_EQ(argb, 0xFF808080);
}

// ============================================================================
// Named colors
// ============================================================================

struct NamedCase { std::string input; uint32_t expected; };
class ColorParserNamedTest : public ::testing::TestWithParam<NamedCase> {};

TEST_P(ColorParserNamedTest, ParsesCorrectly) {
    auto [input, expected] = GetParam();
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid(input, argb));
    EXPECT_EQ(argb, expected);
}

INSTANTIATE_TEST_SUITE_P(Named, ColorParserNamedTest, ::testing::Values(
    NamedCase{"red", 0xFFFF0000},
    NamedCase{"green", 0xFF008000},
    NamedCase{"blue", 0xFF0000FF},
    NamedCase{"white", 0xFFFFFFFF},
    NamedCase{"black", 0xFF000000},
    NamedCase{"transparent", 0x00000000},
    NamedCase{"coral", 0xFFFF7F50},
    NamedCase{"rebeccapurple", 0xFF663399}
));

TEST(ColorParserNamed, CaseInsensitive) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("RED", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
    ASSERT_TRUE(parseSolid("Red", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
    ASSERT_TRUE(parseSolid("rEd", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
}

// ============================================================================
// currentcolor
// ============================================================================

TEST(ColorParserCurrentColor, Recognized) {
    ColorValue cv;
    ASSERT_TRUE(ColorParser::parse("currentcolor", cv));
    EXPECT_EQ(cv.type, ColorValueType::Solid);
    EXPECT_TRUE(cv.isCurrentColor);
}

TEST(ColorParserCurrentColor, CaseInsensitive) {
    ColorValue cv;
    ASSERT_TRUE(ColorParser::parse("currentColor", cv));
    EXPECT_TRUE(cv.isCurrentColor);
    ASSERT_TRUE(ColorParser::parse("CURRENTCOLOR", cv));
    EXPECT_TRUE(cv.isCurrentColor);
}

// ============================================================================
// Whitespace handling
// ============================================================================

TEST(ColorParserWhitespace, LeadingTrailing) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("  #ff0000  ", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
    ASSERT_TRUE(parseSolid("  red  ", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
}

// ============================================================================
// Invalid inputs
// ============================================================================

struct InvalidCase { std::string input; };
class ColorParserInvalidTest : public ::testing::TestWithParam<InvalidCase> {};

TEST_P(ColorParserInvalidTest, ReturnsFalse) {
    ColorValue cv;
    EXPECT_FALSE(ColorParser::parse(GetParam().input, cv));
}

INSTANTIATE_TEST_SUITE_P(Invalid, ColorParserInvalidTest, ::testing::Values(
    InvalidCase{""},
    InvalidCase{"   "},
    InvalidCase{"#"},
    InvalidCase{"#g0"},
    InvalidCase{"#12345"},
    InvalidCase{"#1234567"},
    InvalidCase{"#123456789"},
    InvalidCase{"notacolor"},
    InvalidCase{"rgb("},
    InvalidCase{"rgb()"},
    InvalidCase{"hsl()"},
    InvalidCase{"hwb()"}
));

TEST(ColorParserRgb, OutOfRangeClamps) {
    uint32_t argb = 0;
    ASSERT_TRUE(parseSolid("rgb(256, 0, 0)", argb));
    EXPECT_EQ(argb, 0xFFFF0000);
}

// ============================================================================
// Linear gradient
// ============================================================================

TEST(ColorParserLinearGradient, BasicTwoStop) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("linear-gradient(red, blue)", cv));
    EXPECT_EQ(cv.gradient.type, GradientType::Linear);
    EXPECT_FALSE(cv.gradient.isRepeating);
    EXPECT_EQ(cv.gradient.colorStops.size(), 2u);
    EXPECT_FLOAT_EQ(cv.gradient.linear.angle, 180.0f);
}

TEST(ColorParserLinearGradient, WithAngle) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("linear-gradient(45deg, red, blue)", cv));
    EXPECT_FLOAT_EQ(cv.gradient.linear.angle, 45.0f);
    EXPECT_EQ(cv.gradient.colorStops.size(), 2u);
}

TEST(ColorParserLinearGradient, WithDirection_ToRight) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("linear-gradient(to right, red, blue)", cv));
    EXPECT_FLOAT_EQ(cv.gradient.linear.angle, 90.0f);
}

TEST(ColorParserLinearGradient, WithDirection_ToTop) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("linear-gradient(to top, red, blue)", cv));
    EXPECT_FLOAT_EQ(cv.gradient.linear.angle, 0.0f);
}

TEST(ColorParserLinearGradient, ThreeStops) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("linear-gradient(red, green, blue)", cv));
    EXPECT_EQ(cv.gradient.colorStops.size(), 3u);
}

TEST(ColorParserLinearGradient, StopWithPosition) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("linear-gradient(red 0%, blue 100%)", cv));
    EXPECT_EQ(cv.gradient.colorStops.size(), 2u);
    EXPECT_TRUE(cv.gradient.colorStops[0].hasPosition);
    EXPECT_FLOAT_EQ(cv.gradient.colorStops[0].position, 0.0f);
    EXPECT_TRUE(cv.gradient.colorStops[1].hasPosition);
    EXPECT_FLOAT_EQ(cv.gradient.colorStops[1].position, 1.0f);
}

TEST(ColorParserLinearGradient, Repeating) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("repeating-linear-gradient(red, blue)", cv));
    EXPECT_TRUE(cv.gradient.isRepeating);
    EXPECT_EQ(cv.gradient.type, GradientType::Linear);
}

// ============================================================================
// Radial gradient
// ============================================================================

TEST(ColorParserRadialGradient, BasicTwoStop) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("radial-gradient(red, blue)", cv));
    EXPECT_EQ(cv.gradient.type, GradientType::Radial);
    EXPECT_EQ(cv.gradient.colorStops.size(), 2u);
}

TEST(ColorParserRadialGradient, Repeating) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("repeating-radial-gradient(red, blue)", cv));
    EXPECT_TRUE(cv.gradient.isRepeating);
    EXPECT_EQ(cv.gradient.type, GradientType::Radial);
}

// ============================================================================
// Conic gradient
// ============================================================================

TEST(ColorParserConicGradient, BasicTwoStop) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("conic-gradient(red, blue)", cv));
    EXPECT_EQ(cv.gradient.type, GradientType::Conic);
    EXPECT_EQ(cv.gradient.colorStops.size(), 2u);
}

TEST(ColorParserConicGradient, Repeating) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("repeating-conic-gradient(red, blue)", cv));
    EXPECT_TRUE(cv.gradient.isRepeating);
    EXPECT_EQ(cv.gradient.type, GradientType::Conic);
}

// ============================================================================
// Gradient edge cases
// ============================================================================

TEST(ColorParserGradient, SingleStop_Fails) {
    ColorValue cv;
    EXPECT_FALSE(ColorParser::parse("linear-gradient(red)", cv));
}

TEST(ColorParserGradient, Empty_Fails) {
    ColorValue cv;
    EXPECT_FALSE(ColorParser::parse("linear-gradient()", cv));
}

TEST(ColorParserGradient, MissingClosingParen_Fails) {
    ColorValue cv;
    EXPECT_FALSE(ColorParser::parse("linear-gradient(red, blue", cv));
}

TEST(ColorParserGradient, CaseInsensitive) {
    ColorValue cv;
    ASSERT_TRUE(parseGradient("Linear-Gradient(red, blue)", cv));
    EXPECT_EQ(cv.gradient.type, GradientType::Linear);
}
