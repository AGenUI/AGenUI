#include <gtest/gtest.h>
#include "surface/yoga_node/agenui_yoga_value.h"

using agenui::YogaValue;

// ============================================================================
// Default construction
// ============================================================================

TEST(YogaValue, DefaultConstructed_IsNone) {
    YogaValue v;
    EXPECT_EQ(v.type(), YogaValue::kNone);
    EXPECT_FALSE(v.isValid());
}

// ============================================================================
// Float construction
// ============================================================================

TEST(YogaValue, FloatConstruction_Type) {
    YogaValue v(3.14f);
    EXPECT_EQ(v.type(), YogaValue::kFloat);
    EXPECT_TRUE(v.isValid());
}

TEST(YogaValue, FloatConstruction_Value) {
    YogaValue v(42.0f);
    EXPECT_FLOAT_EQ(v.asFloat(), 42.0f);
}

TEST(YogaValue, FloatConstruction_Zero) {
    YogaValue v(0.0f);
    EXPECT_EQ(v.type(), YogaValue::kFloat);
    EXPECT_TRUE(v.isValid());
    EXPECT_FLOAT_EQ(v.asFloat(), 0.0f);
}

TEST(YogaValue, FloatConstruction_Negative) {
    YogaValue v(-100.5f);
    EXPECT_FLOAT_EQ(v.asFloat(), -100.5f);
}

// ============================================================================
// Bool construction
// ============================================================================

TEST(YogaValue, BoolConstruction_True) {
    YogaValue v(true);
    EXPECT_EQ(v.type(), YogaValue::kBool);
    EXPECT_TRUE(v.isValid());
    EXPECT_TRUE(v.asBool());
}

TEST(YogaValue, BoolConstruction_False) {
    YogaValue v(false);
    EXPECT_EQ(v.type(), YogaValue::kBool);
    EXPECT_TRUE(v.isValid());
    EXPECT_FALSE(v.asBool());
}

// ============================================================================
// String construction
// ============================================================================

TEST(YogaValue, StringConstruction_Copy) {
    std::string s = "auto";
    YogaValue v(s);
    EXPECT_EQ(v.type(), YogaValue::kString);
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.asString(), "auto");
}

TEST(YogaValue, StringConstruction_Move) {
    std::string s = "flex-start";
    YogaValue v(std::move(s));
    EXPECT_EQ(v.type(), YogaValue::kString);
    EXPECT_EQ(v.asString(), "flex-start");
}

TEST(YogaValue, StringConstruction_EmptyString) {
    YogaValue v(std::string(""));
    EXPECT_EQ(v.type(), YogaValue::kString);
    EXPECT_TRUE(v.isValid());  // empty string is still valid (type is kString)
    EXPECT_EQ(v.asString(), "");
}

TEST(YogaValue, StringConstruction_PercentValue) {
    YogaValue v(std::string("100%"));
    EXPECT_EQ(v.asString(), "100%");
}

// ============================================================================
// Cross-type accessor behavior (accessing wrong type)
// ============================================================================

TEST(YogaValue, None_AsFloat_ReturnsZero) {
    YogaValue v;
    EXPECT_FLOAT_EQ(v.asFloat(), 0.0f);
}

TEST(YogaValue, None_AsBool_ReturnsFalse) {
    YogaValue v;
    EXPECT_FALSE(v.asBool());
}

TEST(YogaValue, None_AsString_ReturnsEmpty) {
    YogaValue v;
    EXPECT_EQ(v.asString(), "");
}

TEST(YogaValue, Float_AsBool_ReturnsFalse) {
    YogaValue v(99.0f);
    // Bool field is initialized to false regardless
    EXPECT_FALSE(v.asBool());
}

TEST(YogaValue, Bool_AsFloat_ReturnsZero) {
    YogaValue v(true);
    EXPECT_FLOAT_EQ(v.asFloat(), 0.0f);
}
