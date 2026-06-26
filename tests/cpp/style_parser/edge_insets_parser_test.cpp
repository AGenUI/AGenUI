#include <gtest/gtest.h>
#include "style_parser/agenui_edge_insets_parser.h"

using agenui::EdgeInsetsParser;
using agenui::EdgeInsets;
using agenui::EdgeInsetValue;
using agenui::EdgeInsetUnit;

// ============================================================================
// Helpers
// ============================================================================

static void expectPx(const EdgeInsetValue& v, float expected) {
    EXPECT_EQ(v.unit, EdgeInsetUnit::Px);
    EXPECT_FLOAT_EQ(v.value, expected);
    EXPECT_FALSE(v.isCalc);
}

static void expectPercent(const EdgeInsetValue& v, float expected) {
    EXPECT_EQ(v.unit, EdgeInsetUnit::Percent);
    EXPECT_FLOAT_EQ(v.value, expected);
    EXPECT_FALSE(v.isCalc);
}

static void expectAuto(const EdgeInsetValue& v) {
    EXPECT_EQ(v.unit, EdgeInsetUnit::Auto);
}

// ============================================================================
// Single value shorthand: all four sides equal
// ============================================================================

TEST(EdgeInsetsParser, SingleValue_Px) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("10px", r));
    expectPx(r.top, 10.0f);
    expectPx(r.right, 10.0f);
    expectPx(r.bottom, 10.0f);
    expectPx(r.left, 10.0f);
}

TEST(EdgeInsetsParser, SingleValue_Unitless) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("10", r));
    expectPx(r.top, 10.0f);
    expectPx(r.right, 10.0f);
    expectPx(r.bottom, 10.0f);
    expectPx(r.left, 10.0f);
}

TEST(EdgeInsetsParser, SingleValue_Percent) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("50%", r));
    expectPercent(r.top, 50.0f);
    expectPercent(r.right, 50.0f);
    expectPercent(r.bottom, 50.0f);
    expectPercent(r.left, 50.0f);
}

TEST(EdgeInsetsParser, SingleValue_Auto) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("auto", r));
    expectAuto(r.top);
    expectAuto(r.right);
    expectAuto(r.bottom);
    expectAuto(r.left);
}

TEST(EdgeInsetsParser, SingleValue_Zero) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("0", r));
    expectPx(r.top, 0.0f);
    expectPx(r.right, 0.0f);
    expectPx(r.bottom, 0.0f);
    expectPx(r.left, 0.0f);
}

// ============================================================================
// Two value shorthand: top/bottom, right/left
// ============================================================================

TEST(EdgeInsetsParser, TwoValues) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("10px 20px", r));
    expectPx(r.top, 10.0f);
    expectPx(r.bottom, 10.0f);
    expectPx(r.right, 20.0f);
    expectPx(r.left, 20.0f);
}

TEST(EdgeInsetsParser, TwoValues_Mixed) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("5% auto", r));
    expectPercent(r.top, 5.0f);
    expectPercent(r.bottom, 5.0f);
    expectAuto(r.right);
    expectAuto(r.left);
}

// ============================================================================
// Three value shorthand: top, right/left, bottom
// ============================================================================

TEST(EdgeInsetsParser, ThreeValues) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("10px 20px 30px", r));
    expectPx(r.top, 10.0f);
    expectPx(r.right, 20.0f);
    expectPx(r.left, 20.0f);
    expectPx(r.bottom, 30.0f);
}

// ============================================================================
// Four value shorthand: top, right, bottom, left
// ============================================================================

TEST(EdgeInsetsParser, FourValues) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("1px 2px 3px 4px", r));
    expectPx(r.top, 1.0f);
    expectPx(r.right, 2.0f);
    expectPx(r.bottom, 3.0f);
    expectPx(r.left, 4.0f);
}

TEST(EdgeInsetsParser, FourValues_Mixed) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("10px 20% auto 5px", r));
    expectPx(r.top, 10.0f);
    expectPercent(r.right, 20.0f);
    expectAuto(r.bottom);
    expectPx(r.left, 5.0f);
}

// ============================================================================
// CSS units
// ============================================================================

struct UnitCase { std::string input; EdgeInsetUnit expectedUnit; float expectedVal; };
class EdgeInsetsUnitTest : public ::testing::TestWithParam<UnitCase> {};

TEST_P(EdgeInsetsUnitTest, ParsesCorrectly) {
    auto [input, expectedUnit, expectedVal] = GetParam();
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse(input, r));
    EXPECT_EQ(r.top.unit, expectedUnit);
    EXPECT_FLOAT_EQ(r.top.value, expectedVal);
}

INSTANTIATE_TEST_SUITE_P(Units, EdgeInsetsUnitTest, ::testing::Values(
    UnitCase{"10px",    EdgeInsetUnit::Px,    10.0f},
    UnitCase{"50%",     EdgeInsetUnit::Percent, 50.0f},
    UnitCase{"2em",     EdgeInsetUnit::Em,    2.0f},
    UnitCase{"1.5rem",  EdgeInsetUnit::Rem,   1.5f},
    UnitCase{"10vw",    EdgeInsetUnit::Vw,    10.0f},
    UnitCase{"10vh",    EdgeInsetUnit::Vh,    10.0f},
    UnitCase{"10vmin",  EdgeInsetUnit::Vmin,  10.0f},
    UnitCase{"10vmax",  EdgeInsetUnit::Vmax,  10.0f},
    UnitCase{"2.54cm",  EdgeInsetUnit::Cm,    2.54f},
    UnitCase{"25.4mm",  EdgeInsetUnit::Mm,    25.4f},
    UnitCase{"1in",     EdgeInsetUnit::In,    1.0f},
    UnitCase{"12pt",    EdgeInsetUnit::Pt,    12.0f},
    UnitCase{"1pc",     EdgeInsetUnit::Pc,    1.0f}
));

// ============================================================================
// Negative values
// ============================================================================

TEST(EdgeInsetsParser, NegativeValues) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("-10px", r));
    expectPx(r.top, -10.0f);
}

TEST(EdgeInsetsParser, NegativePercent) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("-5%", r));
    expectPercent(r.top, -5.0f);
}

// ============================================================================
// calc() expressions
// ============================================================================

TEST(EdgeInsetsParser, CalcExpression) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("calc(100% - 20px)", r));
    EXPECT_TRUE(r.top.isCalc);
    EXPECT_FALSE(r.top.calcExpr.empty());
}

TEST(EdgeInsetsParser, CalcMixed) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("10px calc(50% + 5px)", r));
    expectPx(r.top, 10.0f);
    EXPECT_FALSE(r.top.isCalc);
    EXPECT_TRUE(r.right.isCalc);
}

// ============================================================================
// Floating point values
// ============================================================================

TEST(EdgeInsetsParser, FloatingPoint) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("10.5px", r));
    expectPx(r.top, 10.5f);
}

TEST(EdgeInsetsParser, FloatingPointPercent) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("33.33%", r));
    expectPercent(r.top, 33.33f);
}

// ============================================================================
// Whitespace handling
// ============================================================================

TEST(EdgeInsetsParser, ExtraWhitespace) {
    EdgeInsets r;
    ASSERT_TRUE(EdgeInsetsParser::parse("  10px   20px  ", r));
    expectPx(r.top, 10.0f);
    expectPx(r.right, 20.0f);
}

// ============================================================================
// Invalid inputs
// ============================================================================

struct InvalidEdgeCase { std::string input; };
class EdgeInsetsInvalidTest : public ::testing::TestWithParam<InvalidEdgeCase> {};

TEST_P(EdgeInsetsInvalidTest, ReturnsFalse) {
    EdgeInsets r;
    EXPECT_FALSE(EdgeInsetsParser::parse(GetParam().input, r));
}

INSTANTIATE_TEST_SUITE_P(Invalid, EdgeInsetsInvalidTest, ::testing::Values(
    InvalidEdgeCase{""},
    InvalidEdgeCase{"   "},
    InvalidEdgeCase{"abc"},
    InvalidEdgeCase{"10px 20px 30px 40px 50px"},
    InvalidEdgeCase{"px"},
    InvalidEdgeCase{"%"}
));

// ============================================================================
// Zero-initialized on failure
// ============================================================================

TEST(EdgeInsetsParser, FailureZeroInitializes) {
    EdgeInsets r;
    r.top.value = 999.0f;
    EXPECT_FALSE(EdgeInsetsParser::parse("", r));
    EXPECT_FLOAT_EQ(r.top.value, 0.0f);
    EXPECT_EQ(r.top.unit, EdgeInsetUnit::Px);
}
