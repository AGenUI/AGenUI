#include <gtest/gtest.h>
#include "function_call/builtins/agenui_and_functioncall.h"
#include "function_call/builtins/agenui_or_functioncall.h"
#include "function_call/builtins/agenui_not_functioncall.h"
#include "function_call/builtins/agenui_required_functioncall.h"
#include "function_call/builtins/agenui_length_functioncall.h"
#include "function_call/builtins/agenui_numeric_functioncall.h"
#include "function_call/builtins/agenui_email_functioncall.h"
#include "function_call/builtins/agenui_regex_functioncall.h"
#include "function_call/builtins/agenui_pluralize_functioncall.h"
#include "function_call/builtins/agenui_format_string_functioncall.h"
#include "function_call/builtins/agenui_format_number_functioncall.h"
#include "function_call/builtins/agenui_format_currency_functioncall.h"
#include "function_call/builtins/agenui_format_date_functioncall.h"
#include "function_call/builtins/agenui_parse_token_functioncall.h"
#include "function_call/agenui_functioncall_resolution.h"

using namespace agenui;
using json = nlohmann::json;

// ============================================================================
// Helpers
// ============================================================================

static bool isSuccess(const FunctionCallResolution& r) {
    return r.getStatus() == FunctionCallStatus::Success;
}

static bool isError(const FunctionCallResolution& r) {
    return r.getStatus() == FunctionCallStatus::Error;
}

// ============================================================================
// AndFunctionCall
// ============================================================================

class AndFunctionCallTest : public ::testing::Test {
protected:
    AndFunctionCall fc;
};

TEST_F(AndFunctionCallTest, AllTrue_ReturnsTrue) {
    auto r = fc.execute({{"values", json::array({true, true, true})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(AndFunctionCallTest, OneFalse_ReturnsFalse) {
    auto r = fc.execute({{"values", json::array({true, false, true})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(AndFunctionCallTest, AllFalse_ReturnsFalse) {
    auto r = fc.execute({{"values", json::array({false, false})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(AndFunctionCallTest, EmptyArray_ReturnsTrue) {
    auto r = fc.execute({{"values", json::array()}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(AndFunctionCallTest, NonBooleanItems_Skipped) {
    auto r = fc.execute({{"values", json::array({1, "str", true})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(AndFunctionCallTest, MissingValues_ReturnsError) {
    auto r = fc.execute(json::object());
    ASSERT_TRUE(isError(r));
}

TEST_F(AndFunctionCallTest, ValuesNotArray_ReturnsError) {
    auto r = fc.execute({{"values", "not_array"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(AndFunctionCallTest, SingleTrue_ReturnsTrue) {
    auto r = fc.execute({{"values", json::array({true})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(AndFunctionCallTest, SingleFalse_ReturnsFalse) {
    auto r = fc.execute({{"values", json::array({false})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(AndFunctionCallTest, Config_NameIsAnd) {
    EXPECT_EQ(fc.getConfig().getName(), "and");
}

// ============================================================================
// OrFunctionCall
// ============================================================================

class OrFunctionCallTest : public ::testing::Test {
protected:
    OrFunctionCall fc;
};

TEST_F(OrFunctionCallTest, AllFalse_ReturnsFalse) {
    auto r = fc.execute({{"values", json::array({false, false, false})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(OrFunctionCallTest, OneTrue_ReturnsTrue) {
    auto r = fc.execute({{"values", json::array({false, true, false})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(OrFunctionCallTest, AllTrue_ReturnsTrue) {
    auto r = fc.execute({{"values", json::array({true, true})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(OrFunctionCallTest, EmptyArray_ReturnsFalse) {
    auto r = fc.execute({{"values", json::array()}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(OrFunctionCallTest, NonBooleanItems_Skipped) {
    auto r = fc.execute({{"values", json::array({1, "str", nullptr})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(OrFunctionCallTest, MissingValues_ReturnsError) {
    auto r = fc.execute(json::object());
    ASSERT_TRUE(isError(r));
}

TEST_F(OrFunctionCallTest, ValuesNotArray_ReturnsError) {
    auto r = fc.execute({{"values", 42}});
    ASSERT_TRUE(isError(r));
}

TEST_F(OrFunctionCallTest, Config_NameIsOr) {
    EXPECT_EQ(fc.getConfig().getName(), "or");
}

// ============================================================================
// NotFunctionCall
// ============================================================================

class NotFunctionCallTest : public ::testing::Test {
protected:
    NotFunctionCall fc;
};

TEST_F(NotFunctionCallTest, True_ReturnsFalse) {
    auto r = fc.execute({{"value", true}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(NotFunctionCallTest, False_ReturnsTrue) {
    auto r = fc.execute({{"value", false}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(NotFunctionCallTest, MissingValue_ReturnsError) {
    auto r = fc.execute(json::object());
    ASSERT_TRUE(isError(r));
}

TEST_F(NotFunctionCallTest, NonBoolean_ReturnsError) {
    auto r = fc.execute({{"value", 42}});
    ASSERT_TRUE(isError(r));
}

TEST_F(NotFunctionCallTest, StringValue_ReturnsError) {
    auto r = fc.execute({{"value", "true"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(NotFunctionCallTest, NullValue_ReturnsError) {
    auto r = fc.execute({{"value", nullptr}});
    ASSERT_TRUE(isError(r));
}

TEST_F(NotFunctionCallTest, Config_NameIsNot) {
    EXPECT_EQ(fc.getConfig().getName(), "not");
}

// ============================================================================
// RequiredFunctionCall
// ============================================================================

class RequiredFunctionCallTest : public ::testing::Test {
protected:
    RequiredFunctionCall fc;
};

TEST_F(RequiredFunctionCallTest, NonEmptyString_ReturnsTrue) {
    auto r = fc.execute({{"value", "hello"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(RequiredFunctionCallTest, EmptyString_ReturnsFalse) {
    auto r = fc.execute({{"value", ""}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(RequiredFunctionCallTest, Null_ReturnsFalse) {
    auto r = fc.execute({{"value", nullptr}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(RequiredFunctionCallTest, EmptyArray_ReturnsFalse) {
    auto r = fc.execute({{"value", json::array()}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(RequiredFunctionCallTest, EmptyObject_ReturnsFalse) {
    auto r = fc.execute({{"value", json::object()}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(RequiredFunctionCallTest, NonEmptyArray_ReturnsTrue) {
    auto r = fc.execute({{"value", json::array({1})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(RequiredFunctionCallTest, NonEmptyObject_ReturnsTrue) {
    auto r = fc.execute({{"value", json::object({{"k", "v"}})}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(RequiredFunctionCallTest, NumberValue_ReturnsTrue) {
    auto r = fc.execute({{"value", 0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(RequiredFunctionCallTest, BooleanFalse_ReturnsTrue) {
    auto r = fc.execute({{"value", false}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(RequiredFunctionCallTest, MissingValue_ReturnsError) {
    auto r = fc.execute(json::object());
    ASSERT_TRUE(isError(r));
}

TEST_F(RequiredFunctionCallTest, Config_NameIsRequired) {
    EXPECT_EQ(fc.getConfig().getName(), "required");
}

// ============================================================================
// LengthFunctionCall
// ============================================================================

class LengthFunctionCallTest : public ::testing::Test {
protected:
    LengthFunctionCall fc;
};

TEST_F(LengthFunctionCallTest, NoConstraints_ReturnsTrue) {
    auto r = fc.execute({{"value", "hello"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(LengthFunctionCallTest, MinSatisfied_ReturnsTrue) {
    auto r = fc.execute({{"value", "hello"}, {"min", 3}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(LengthFunctionCallTest, MinNotSatisfied_ReturnsFalse) {
    auto r = fc.execute({{"value", "hi"}, {"min", 5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(LengthFunctionCallTest, MaxSatisfied_ReturnsTrue) {
    auto r = fc.execute({{"value", "hi"}, {"max", 5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(LengthFunctionCallTest, MaxNotSatisfied_ReturnsFalse) {
    auto r = fc.execute({{"value", "toolongstring"}, {"max", 5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(LengthFunctionCallTest, MinAndMaxSatisfied_ReturnsTrue) {
    auto r = fc.execute({{"value", "hello"}, {"min", 3}, {"max", 10}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(LengthFunctionCallTest, ExactlyMinLength_ReturnsTrue) {
    auto r = fc.execute({{"value", "abc"}, {"min", 3}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(LengthFunctionCallTest, ExactlyMaxLength_ReturnsTrue) {
    auto r = fc.execute({{"value", "abc"}, {"max", 3}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(LengthFunctionCallTest, EmptyString_MinZero_ReturnsTrue) {
    auto r = fc.execute({{"value", ""}, {"min", 0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(LengthFunctionCallTest, EmptyString_MinOne_ReturnsFalse) {
    auto r = fc.execute({{"value", ""}, {"min", 1}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(LengthFunctionCallTest, MissingValue_ReturnsError) {
    auto r = fc.execute(json::object());
    ASSERT_TRUE(isError(r));
}

TEST_F(LengthFunctionCallTest, NonStringValue_ReturnsError) {
    auto r = fc.execute({{"value", 123}});
    ASSERT_TRUE(isError(r));
}

TEST_F(LengthFunctionCallTest, Config_NameIsLength) {
    EXPECT_EQ(fc.getConfig().getName(), "length");
}

// ============================================================================
// NumericFunctionCall
// ============================================================================

class NumericFunctionCallTest : public ::testing::Test {
protected:
    NumericFunctionCall fc;
};

TEST_F(NumericFunctionCallTest, NoConstraints_ReturnsTrue) {
    auto r = fc.execute({{"value", 42}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(NumericFunctionCallTest, MinSatisfied_ReturnsTrue) {
    auto r = fc.execute({{"value", 10}, {"min", 5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(NumericFunctionCallTest, MinNotSatisfied_ReturnsFalse) {
    auto r = fc.execute({{"value", 3}, {"min", 5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(NumericFunctionCallTest, MaxSatisfied_ReturnsTrue) {
    auto r = fc.execute({{"value", 3}, {"max", 5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(NumericFunctionCallTest, MaxNotSatisfied_ReturnsFalse) {
    auto r = fc.execute({{"value", 10}, {"max", 5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(NumericFunctionCallTest, MinAndMaxSatisfied_ReturnsTrue) {
    auto r = fc.execute({{"value", 5}, {"min", 1}, {"max", 10}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(NumericFunctionCallTest, ExactlyMin_ReturnsTrue) {
    auto r = fc.execute({{"value", 5}, {"min", 5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(NumericFunctionCallTest, ExactlyMax_ReturnsTrue) {
    auto r = fc.execute({{"value", 5}, {"max", 5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(NumericFunctionCallTest, NegativeValue_WithMin) {
    auto r = fc.execute({{"value", -10}, {"min", -5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(NumericFunctionCallTest, FloatingPoint_MinSatisfied) {
    auto r = fc.execute({{"value", 3.14}, {"min", 3.0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(NumericFunctionCallTest, Zero_NoConstraints) {
    auto r = fc.execute({{"value", 0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(NumericFunctionCallTest, MissingValue_ReturnsError) {
    auto r = fc.execute(json::object());
    ASSERT_TRUE(isError(r));
}

TEST_F(NumericFunctionCallTest, NonNumericValue_ReturnsError) {
    auto r = fc.execute({{"value", "not_a_number"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(NumericFunctionCallTest, Config_NameIsNumeric) {
    EXPECT_EQ(fc.getConfig().getName(), "numeric");
}

// ============================================================================
// EmailFunctionCall
// ============================================================================

class EmailFunctionCallTest : public ::testing::Test {
protected:
    EmailFunctionCall fc;
};

struct EmailCase { std::string input; bool expected; };
class EmailParamTest : public ::testing::TestWithParam<EmailCase> {
protected:
    EmailFunctionCall fc;
};

TEST_P(EmailParamTest, ValidatesCorrectly) {
    auto [input, expected] = GetParam();
    auto r = fc.execute({{"value", input}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), expected);
}

INSTANTIATE_TEST_SUITE_P(ValidEmails, EmailParamTest, ::testing::Values(
    EmailCase{"user@example.com", true},
    EmailCase{"test.name@domain.org", true},
    EmailCase{"user+tag@example.co.uk", true},
    EmailCase{"a@b.cc", true},
    EmailCase{"user123@test.io", true},
    EmailCase{"first.last@company.com", true}
));

INSTANTIATE_TEST_SUITE_P(InvalidEmails, EmailParamTest, ::testing::Values(
    EmailCase{"", false},
    EmailCase{"notanemail", false},
    EmailCase{"@domain.com", false},
    EmailCase{"user@", false},
    EmailCase{"user@.com", false},
    EmailCase{"user@domain", false}
));

TEST_F(EmailFunctionCallTest, MissingValue_ReturnsError) {
    auto r = fc.execute(json::object());
    ASSERT_TRUE(isError(r));
}

TEST_F(EmailFunctionCallTest, NonStringValue_ReturnsError) {
    auto r = fc.execute({{"value", 42}});
    ASSERT_TRUE(isError(r));
}

TEST_F(EmailFunctionCallTest, Config_NameIsEmail) {
    EXPECT_EQ(fc.getConfig().getName(), "email");
}

// ============================================================================
// RegexFunctionCall
// ============================================================================

class RegexFunctionCallTest : public ::testing::Test {
protected:
    RegexFunctionCall fc;
};

TEST_F(RegexFunctionCallTest, MatchingPattern_ReturnsTrue) {
    auto r = fc.execute({{"value", "abc123"}, {"pattern", "^[a-z]+[0-9]+$"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(RegexFunctionCallTest, NonMatchingPattern_ReturnsFalse) {
    auto r = fc.execute({{"value", "123abc"}, {"pattern", "^[a-z]+[0-9]+$"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(RegexFunctionCallTest, EmptyString_MatchesEmptyPattern) {
    auto r = fc.execute({{"value", ""}, {"pattern", "^$"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(RegexFunctionCallTest, EmptyString_NonEmptyPattern_ReturnsFalse) {
    auto r = fc.execute({{"value", ""}, {"pattern", "^.+$"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(RegexFunctionCallTest, PhoneNumberPattern) {
    auto r = fc.execute({{"value", "+1-555-123-4567"}, {"pattern", R"(^\+\d{1,3}-\d{3}-\d{3}-\d{4}$)"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(RegexFunctionCallTest, MissingValue_ReturnsError) {
    auto r = fc.execute({{"pattern", ".*"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(RegexFunctionCallTest, MissingPattern_ReturnsError) {
    auto r = fc.execute({{"value", "abc"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(RegexFunctionCallTest, MissingBoth_ReturnsError) {
    auto r = fc.execute(json::object());
    ASSERT_TRUE(isError(r));
}

TEST_F(RegexFunctionCallTest, NonStringValue_ReturnsError) {
    auto r = fc.execute({{"value", 42}, {"pattern", ".*"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(RegexFunctionCallTest, NonStringPattern_ReturnsError) {
    auto r = fc.execute({{"value", "abc"}, {"pattern", 42}});
    ASSERT_TRUE(isError(r));
}

TEST_F(RegexFunctionCallTest, Config_NameIsRegex) {
    EXPECT_EQ(fc.getConfig().getName(), "regex");
}

// ============================================================================
// PluralizeFunctionCall
// ============================================================================

class PluralizeFunctionCallTest : public ::testing::Test {
protected:
    PluralizeFunctionCall fc;
};

TEST_F(PluralizeFunctionCallTest, ZeroCount_ReturnsZeroForm) {
    auto r = fc.execute({{"value", 0}, {"zero", "no items"}, {"one", "1 item"}, {"other", "items"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "no items");
}

TEST_F(PluralizeFunctionCallTest, OneCount_ReturnsOneForm) {
    auto r = fc.execute({{"value", 1}, {"zero", "no items"}, {"one", "1 item"}, {"other", "items"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "1 item");
}

TEST_F(PluralizeFunctionCallTest, TwoCount_ReturnsTwoForm) {
    auto r = fc.execute({{"value", 2}, {"two", "a pair"}, {"other", "items"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "a pair");
}

TEST_F(PluralizeFunctionCallTest, MultipleCount_ReturnsOtherForm) {
    auto r = fc.execute({{"value", 5}, {"zero", "no items"}, {"one", "1 item"}, {"other", "many items"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "many items");
}

TEST_F(PluralizeFunctionCallTest, ZeroCount_NoZeroForm_FallsToOther) {
    auto r = fc.execute({{"value", 0}, {"other", "items"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "items");
}

TEST_F(PluralizeFunctionCallTest, OneCount_NoOneForm_FallsToOther) {
    auto r = fc.execute({{"value", 1}, {"other", "items"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "items");
}

TEST_F(PluralizeFunctionCallTest, FractionalRoundsToNearest) {
    auto r = fc.execute({{"value", 0.4}, {"zero", "none"}, {"one", "one"}, {"other", "many"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "none");
}

TEST_F(PluralizeFunctionCallTest, FractionalRoundsUp) {
    auto r = fc.execute({{"value", 0.6}, {"zero", "none"}, {"one", "one"}, {"other", "many"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "one");
}

TEST_F(PluralizeFunctionCallTest, NegativeValue_FallsToOther) {
    auto r = fc.execute({{"value", -1}, {"zero", "none"}, {"one", "one"}, {"other", "many"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "many");
}

TEST_F(PluralizeFunctionCallTest, MissingValue_ReturnsError) {
    auto r = fc.execute({{"other", "items"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(PluralizeFunctionCallTest, MissingOther_ReturnsError) {
    auto r = fc.execute({{"value", 5}});
    ASSERT_TRUE(isError(r));
}

TEST_F(PluralizeFunctionCallTest, NonNumericValue_ReturnsError) {
    auto r = fc.execute({{"value", "five"}, {"other", "items"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(PluralizeFunctionCallTest, Config_NameIsPluralize) {
    EXPECT_EQ(fc.getConfig().getName(), "pluralize");
}

// ============================================================================
// FormatStringFunctionCall
// ============================================================================

class FormatStringFunctionCallTest : public ::testing::Test {
protected:
    FormatStringFunctionCall fc;
};

TEST_F(FormatStringFunctionCallTest, StringValue_PassThrough) {
    auto r = fc.execute({{"value", "hello world"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "hello world");
}

TEST_F(FormatStringFunctionCallTest, NumberValue_PassThrough) {
    auto r = fc.execute({{"value", 42}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), 42);
}

TEST_F(FormatStringFunctionCallTest, BooleanValue_PassThrough) {
    auto r = fc.execute({{"value", true}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(FormatStringFunctionCallTest, NullValue_PassThrough) {
    auto r = fc.execute({{"value", nullptr}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_TRUE(r.getValue().is_null());
}

TEST_F(FormatStringFunctionCallTest, EmptyString_PassThrough) {
    auto r = fc.execute({{"value", ""}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "");
}

TEST_F(FormatStringFunctionCallTest, MissingValue_ReturnsError) {
    auto r = fc.execute(json::object());
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatStringFunctionCallTest, Config_NameIsFormatString) {
    EXPECT_EQ(fc.getConfig().getName(), "formatString");
}

// ============================================================================
// FormatNumberFunctionCall
// ============================================================================

class FormatNumberFunctionCallTest : public ::testing::Test {
protected:
    FormatNumberFunctionCall fc;
};

TEST_F(FormatNumberFunctionCallTest, DefaultFormatting) {
    auto r = fc.execute({{"value", 1234.5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "1,234.50");
}

TEST_F(FormatNumberFunctionCallTest, NoGrouping) {
    auto r = fc.execute({{"value", 1234.5}, {"grouping", false}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "1234.50");
}

TEST_F(FormatNumberFunctionCallTest, ZeroDecimals) {
    auto r = fc.execute({{"value", 1234.5}, {"decimals", 0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "1,234");  // 1234.5 rounds to 1234 with 0 decimals
}

TEST_F(FormatNumberFunctionCallTest, FourDecimals) {
    auto r = fc.execute({{"value", 3.14159}, {"decimals", 4}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "3.1416");
}

TEST_F(FormatNumberFunctionCallTest, LargeNumber) {
    auto r = fc.execute({{"value", 1000000}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "1,000,000.00");
}

TEST_F(FormatNumberFunctionCallTest, NegativeNumber) {
    auto r = fc.execute({{"value", -1234.5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "-1,234.50");
}

TEST_F(FormatNumberFunctionCallTest, Zero) {
    auto r = fc.execute({{"value", 0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "0.00");
}

TEST_F(FormatNumberFunctionCallTest, SmallNumber_NoGrouping) {
    auto r = fc.execute({{"value", 42}, {"grouping", false}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "42.00");
}

TEST_F(FormatNumberFunctionCallTest, MissingValue_ReturnsError) {
    auto r = fc.execute(json::object());
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatNumberFunctionCallTest, NonNumericValue_ReturnsError) {
    auto r = fc.execute({{"value", "not_number"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatNumberFunctionCallTest, Config_NameIsFormatNumber) {
    EXPECT_EQ(fc.getConfig().getName(), "formatNumber");
}

// ============================================================================
// FormatCurrencyFunctionCall
// ============================================================================

class FormatCurrencyFunctionCallTest : public ::testing::Test {
protected:
    FormatCurrencyFunctionCall fc;
};

TEST_F(FormatCurrencyFunctionCallTest, DefaultFormatting_USD) {
    auto r = fc.execute({{"value", 1234.5}, {"currency", "$"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "$ 1,234.50");
}

TEST_F(FormatCurrencyFunctionCallTest, DefaultFormatting_EUR) {
    auto r = fc.execute({{"value", 999.99}, {"currency", "EUR"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "EUR 999.99");
}

TEST_F(FormatCurrencyFunctionCallTest, NoGrouping) {
    auto r = fc.execute({{"value", 1234.5}, {"currency", "$"}, {"grouping", false}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "$ 1234.50");
}

TEST_F(FormatCurrencyFunctionCallTest, ZeroDecimals) {
    auto r = fc.execute({{"value", 1234}, {"currency", "$"}, {"decimals", 0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "$ 1,234");
}

TEST_F(FormatCurrencyFunctionCallTest, NegativeAmount) {
    auto r = fc.execute({{"value", -99.99}, {"currency", "$"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "$ -99.99");
}

TEST_F(FormatCurrencyFunctionCallTest, ZeroAmount) {
    auto r = fc.execute({{"value", 0}, {"currency", "$"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "$ 0.00");
}

TEST_F(FormatCurrencyFunctionCallTest, LargeAmount) {
    auto r = fc.execute({{"value", 1000000}, {"currency", "$"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "$ 1,000,000.00");
}

TEST_F(FormatCurrencyFunctionCallTest, MissingValue_ReturnsError) {
    auto r = fc.execute({{"currency", "$"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatCurrencyFunctionCallTest, MissingCurrency_ReturnsError) {
    auto r = fc.execute({{"value", 100}});
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatCurrencyFunctionCallTest, NonNumericValue_ReturnsError) {
    auto r = fc.execute({{"value", "abc"}, {"currency", "$"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatCurrencyFunctionCallTest, NonStringCurrency_ReturnsError) {
    auto r = fc.execute({{"value", 100}, {"currency", 42}});
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatCurrencyFunctionCallTest, Config_NameIsFormatCurrency) {
    EXPECT_EQ(fc.getConfig().getName(), "formatCurrency");
}

// ============================================================================
// FormatDateFunctionCall
// ============================================================================

class FormatDateFunctionCallTest : public ::testing::Test {
protected:
    FormatDateFunctionCall fc;
};

TEST_F(FormatDateFunctionCallTest, ISO8601_YearFormat) {
    auto r = fc.execute({{"value", "2024-01-15T10:30:00"}, {"format", "yyyy"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "2024");
}

TEST_F(FormatDateFunctionCallTest, ISO8601_MonthDayYear) {
    auto r = fc.execute({{"value", "2024-03-15T10:30:00"}, {"format", "MM/dd/yyyy"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "03/15/2024");
}

TEST_F(FormatDateFunctionCallTest, ISO8601_ShortYear) {
    auto r = fc.execute({{"value", "2024-01-15T10:30:00"}, {"format", "yy"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "24");
}

TEST_F(FormatDateFunctionCallTest, ISO8601_SingleDigitMonth) {
    auto r = fc.execute({{"value", "2024-03-05T10:30:00"}, {"format", "M/d/yyyy"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "3/5/2024");
}

TEST_F(FormatDateFunctionCallTest, ISO8601_MonthName) {
    auto r = fc.execute({{"value", "2024-03-15T10:30:00"}, {"format", "MMMM"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "March");
}

TEST_F(FormatDateFunctionCallTest, ISO8601_AbbrevMonthName) {
    auto r = fc.execute({{"value", "2024-03-15T10:30:00"}, {"format", "MMM"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "Mar");
}

TEST_F(FormatDateFunctionCallTest, ISO8601_24HourTime) {
    auto r = fc.execute({{"value", "2024-01-15T14:30:45"}, {"format", "HH:mm:ss"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "14:30:45");
}

TEST_F(FormatDateFunctionCallTest, ISO8601_12HourTime_PM) {
    auto r = fc.execute({{"value", "2024-01-15T14:30:00"}, {"format", "hh:mm a"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "02:30 PM");
}

TEST_F(FormatDateFunctionCallTest, ISO8601_12HourTime_AM) {
    auto r = fc.execute({{"value", "2024-01-15T09:30:00"}, {"format", "hh:mm a"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "09:30 AM");
}

TEST_F(FormatDateFunctionCallTest, ISO8601_DateOnly) {
    auto r = fc.execute({{"value", "2024-06-15"}, {"format", "yyyy-MM-dd"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "2024-06-15");
}

TEST_F(FormatDateFunctionCallTest, UnixTimestamp_Milliseconds) {
    // 2024-01-01 00:00:00 UTC = 1704067200000 ms
    auto r = fc.execute({{"value", 1704067200000}, {"format", "yyyy"}});
    ASSERT_TRUE(isSuccess(r));
    // Year depends on timezone, just check it's a success
    EXPECT_FALSE(r.getValue().get<std::string>().empty());
}

TEST_F(FormatDateFunctionCallTest, MissingValue_ReturnsError) {
    auto r = fc.execute({{"format", "yyyy"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatDateFunctionCallTest, MissingFormat_ReturnsError) {
    auto r = fc.execute({{"value", "2024-01-01"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatDateFunctionCallTest, NonStringFormat_ReturnsError) {
    auto r = fc.execute({{"value", "2024-01-01"}, {"format", 42}});
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatDateFunctionCallTest, BooleanValue_ReturnsError) {
    auto r = fc.execute({{"value", true}, {"format", "yyyy"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatDateFunctionCallTest, Config_NameIsFormatDate) {
    EXPECT_EQ(fc.getConfig().getName(), "formatDate");
}

// ============================================================================
// ParseTokenFunctionCall
// ============================================================================

class ParseTokenFunctionCallTest : public ::testing::Test {
protected:
    ParseTokenFunctionCall fc;
};

TEST_F(ParseTokenFunctionCallTest, UnknownToken_ReturnsSameName) {
    auto r = fc.execute({{"name", "nonexistent.token"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "nonexistent.token");
}

TEST_F(ParseTokenFunctionCallTest, MissingName_ReturnsError) {
    auto r = fc.execute(json::object());
    ASSERT_TRUE(isError(r));
}

TEST_F(ParseTokenFunctionCallTest, NonStringName_ReturnsError) {
    auto r = fc.execute({{"name", 42}});
    ASSERT_TRUE(isError(r));
}

TEST_F(ParseTokenFunctionCallTest, NullName_ReturnsError) {
    auto r = fc.execute({{"name", nullptr}});
    ASSERT_TRUE(isError(r));
}

TEST_F(ParseTokenFunctionCallTest, EmptyStringName_ReturnsSameName) {
    auto r = fc.execute({{"name", ""}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "");
}

TEST_F(ParseTokenFunctionCallTest, Config_NameIsToken) {
    EXPECT_EQ(fc.getConfig().getName(), "token");
}

// ============================================================================
// FunctionCallResolution
// ============================================================================

TEST(FunctionCallResolution, CreateSuccess_StatusIsSuccess) {
    auto r = FunctionCallResolution::createSuccess(42);
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Success);
    EXPECT_EQ(r.getValue(), 42);
}

TEST(FunctionCallResolution, CreateError_StatusIsError) {
    auto r = FunctionCallResolution::createError("oops");
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Error);
    EXPECT_EQ(r.getError(), "oops");
}

TEST(FunctionCallResolution, CreatePending_StatusIsPending) {
    auto r = FunctionCallResolution::createPending("req-123");
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Pending);
    EXPECT_EQ(r.getRequestId(), "req-123");
    EXPECT_TRUE(r.isAsync());
    EXPECT_FALSE(r.isCompleted());
}

TEST(FunctionCallResolution, CreateCompleted_StatusIsCompleted) {
    auto r = FunctionCallResolution::createCompleted("req-123", "done");
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Completed);
    EXPECT_EQ(r.getRequestId(), "req-123");
    EXPECT_EQ(r.getValue(), "done");
    EXPECT_TRUE(r.isAsync());
    EXPECT_TRUE(r.isCompleted());
}

TEST(FunctionCallResolution, ToJson_Success) {
    auto r = FunctionCallResolution::createSuccess("val");
    auto j = r.toJson();
    EXPECT_EQ(j["status"], "success");
    EXPECT_EQ(j["value"], "val");
}

TEST(FunctionCallResolution, ToJson_Error) {
    auto r = FunctionCallResolution::createError("bad");
    auto j = r.toJson();
    EXPECT_EQ(j["status"], "error");
    EXPECT_EQ(j["error"], "bad");
}

TEST(FunctionCallResolution, ToJson_Pending) {
    auto r = FunctionCallResolution::createPending("r1");
    auto j = r.toJson();
    EXPECT_EQ(j["status"], "pending");
    EXPECT_EQ(j["requestId"], "r1");
}

TEST(FunctionCallResolution, IsCompleted_SuccessTrue) {
    EXPECT_TRUE(FunctionCallResolution::createSuccess(1).isCompleted());
}

TEST(FunctionCallResolution, IsCompleted_ErrorTrue) {
    EXPECT_TRUE(FunctionCallResolution::createError("x").isCompleted());
}

TEST(FunctionCallResolution, IsAsync_SuccessFalse) {
    EXPECT_FALSE(FunctionCallResolution::createSuccess(1).isAsync());
}

TEST(FunctionCallResolution, IsAsync_ErrorFalse) {
    EXPECT_FALSE(FunctionCallResolution::createError("x").isAsync());
}
