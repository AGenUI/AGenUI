#include <gtest/gtest.h>
#include "surface/yoga_node/agenui_yoga_internal_parse.h"

using agenui::yoga_internal::parseCssFloat;
using agenui::yoga_internal::parsePercent;

// ============================================================================
// parseCssFloat: Normal inputs
// ============================================================================

TEST(YogaInternalParse, CssFloat_IntegerPixel) {
    bool ok = false;
    float v = parseCssFloat("8px", &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(v, 8.0f);
}

TEST(YogaInternalParse, CssFloat_DecimalPixel) {
    bool ok = false;
    float v = parseCssFloat("1.5px", &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(v, 1.5f);
}

TEST(YogaInternalParse, CssFloat_NegativeValue) {
    bool ok = false;
    float v = parseCssFloat("-3px", &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(v, -3.0f);
}

TEST(YogaInternalParse, CssFloat_PureNumber_NoUnit) {
    bool ok = false;
    float v = parseCssFloat("42", &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(v, 42.0f);
}

TEST(YogaInternalParse, CssFloat_VpUnit) {
    bool ok = false;
    float v = parseCssFloat("12vp", &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(v, 12.0f);
}

TEST(YogaInternalParse, CssFloat_Zero) {
    bool ok = false;
    float v = parseCssFloat("0", &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(YogaInternalParse, CssFloat_PositiveSign) {
    bool ok = false;
    float v = parseCssFloat("+5rem", &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(v, 5.0f);
}

// ============================================================================
// parseCssFloat: Edge cases / Invalid inputs
// ============================================================================

TEST(YogaInternalParse, CssFloat_EmptyString_Fails) {
    bool ok = true;
    float v = parseCssFloat("", &ok);
    EXPECT_FALSE(ok);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(YogaInternalParse, CssFloat_OnlyUnit_Fails) {
    bool ok = true;
    float v = parseCssFloat("px", &ok);
    EXPECT_FALSE(ok);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(YogaInternalParse, CssFloat_NonNumericString_Fails) {
    bool ok = true;
    float v = parseCssFloat("auto", &ok);
    EXPECT_FALSE(ok);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(YogaInternalParse, CssFloat_NullOkParam_DoesNotCrash) {
    // ok = nullptr => no crash, just returns value
    float v = parseCssFloat("10px", nullptr);
    EXPECT_FLOAT_EQ(v, 10.0f);
}

TEST(YogaInternalParse, CssFloat_NullOkParam_InvalidInput) {
    float v = parseCssFloat("", nullptr);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

// ============================================================================
// parsePercent: Normal inputs
// ============================================================================

TEST(YogaInternalParse, Percent_50Percent) {
    bool ok = false;
    float v = parsePercent("50%", &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(v, 50.0f);
}

TEST(YogaInternalParse, Percent_100Percent) {
    bool ok = false;
    float v = parsePercent("100%", &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(v, 100.0f);
}

TEST(YogaInternalParse, Percent_DecimalPercent) {
    bool ok = false;
    float v = parsePercent("33.3%", &ok);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(v, 33.3f, 0.01f);
}

TEST(YogaInternalParse, Percent_ZeroPercent) {
    bool ok = false;
    float v = parsePercent("0%", &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(YogaInternalParse, Percent_NegativePercent) {
    bool ok = false;
    float v = parsePercent("-25%", &ok);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(v, -25.0f);
}

// ============================================================================
// parsePercent: Edge cases / Invalid inputs
// ============================================================================

TEST(YogaInternalParse, Percent_NoPercentSign_Fails) {
    bool ok = true;
    float v = parsePercent("50", &ok);
    EXPECT_FALSE(ok);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(YogaInternalParse, Percent_EmptyString_Fails) {
    bool ok = true;
    float v = parsePercent("", &ok);
    EXPECT_FALSE(ok);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(YogaInternalParse, Percent_OnlyPercentSign_Fails) {
    bool ok = true;
    float v = parsePercent("%", &ok);
    EXPECT_FALSE(ok);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(YogaInternalParse, Percent_PixelUnit_Fails) {
    bool ok = true;
    float v = parsePercent("50px", &ok);
    EXPECT_FALSE(ok);
    EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(YogaInternalParse, Percent_NullOkParam_DoesNotCrash) {
    float v = parsePercent("75%", nullptr);
    EXPECT_FLOAT_EQ(v, 75.0f);
}
