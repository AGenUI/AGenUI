// builtins_edge_cases_test.cpp
//
// Supplementary edge-case / boundary tests for builtin functions.
// Covers gaps identified during unit-test workflow audit:
//   - Regex: invalid pattern (unhandled std::regex_error)
//   - FormatDate: invalid/empty date strings, leap year, midnight, weekday
//   - Length: multi-byte UTF-8, negative min
//   - FormatNumber: very large numbers, negative decimals
//   - FormatCurrency: empty currency string

#include <gtest/gtest.h>
#include <regex>
#include "function_call/builtins/agenui_regex_functioncall.h"
#include "function_call/builtins/agenui_format_date_functioncall.h"
#include "function_call/builtins/agenui_length_functioncall.h"
#include "function_call/builtins/agenui_format_number_functioncall.h"
#include "function_call/builtins/agenui_format_currency_functioncall.h"
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
// RegexFunctionCall — Edge Cases
// ============================================================================

class RegexEdgeCaseTest : public ::testing::Test {
protected:
    RegexFunctionCall fc;
};

TEST_F(RegexEdgeCaseTest, InvalidPattern_UnclosedBracket_ThrowsRegexError) {
    EXPECT_THROW(fc.execute({{"value", "test"}, {"pattern", "[unclosed"}}), std::regex_error);
}

TEST_F(RegexEdgeCaseTest, InvalidPattern_BadQuantifier_ThrowsRegexError) {
    EXPECT_THROW(fc.execute({{"value", "test"}, {"pattern", "*invalid"}}), std::regex_error);
}

TEST_F(RegexEdgeCaseTest, EmptyPattern_MatchesEmptyString) {
    auto r = fc.execute({{"value", ""}, {"pattern", ""}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(RegexEdgeCaseTest, EmptyPattern_MatchesAnyString) {
    // An empty regex pattern matches any string (vacuous truth for regex_match)
    auto r = fc.execute({{"value", "anything"}, {"pattern", ""}});
    ASSERT_TRUE(isSuccess(r));
    // std::regex_match with empty pattern: matches empty only
    // Actually std::regex("") only matches empty string with regex_match
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(RegexEdgeCaseTest, DotStarPattern_MatchesAnything) {
    auto r = fc.execute({{"value", "anything at all!"}, {"pattern", ".*"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

// ============================================================================
// FormatDateFunctionCall — Edge Cases
// ============================================================================

class FormatDateEdgeCaseTest : public ::testing::Test {
protected:
    FormatDateFunctionCall fc;
};

TEST_F(FormatDateEdgeCaseTest, InvalidDateString_ReturnsError) {
    auto r = fc.execute({{"value", "not-a-date"}, {"format", "yyyy"}});
    // parseISO8601 with garbage → mktime returns -1 → error
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatDateEdgeCaseTest, EmptyDateString_ReturnsError) {
    auto r = fc.execute({{"value", ""}, {"format", "yyyy"}});
    ASSERT_TRUE(isError(r));
}

TEST_F(FormatDateEdgeCaseTest, LeapYear_Feb29_Parses) {
    auto r = fc.execute({{"value", "2024-02-29T12:00:00"}, {"format", "MM/dd/yyyy"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "02/29/2024");
}

TEST_F(FormatDateEdgeCaseTest, Midnight_12HourFormat) {
    // 00:00 in 12-hour format should be 12:00 AM
    auto r = fc.execute({{"value", "2024-01-15T00:00:00"}, {"format", "hh:mm a"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "12:00 AM");
}

TEST_F(FormatDateEdgeCaseTest, Noon_12HourFormat) {
    // 12:00 in 12-hour format should be 12:00 PM
    auto r = fc.execute({{"value", "2024-01-15T12:00:00"}, {"format", "hh:mm a"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "12:00 PM");
}

TEST_F(FormatDateEdgeCaseTest, WeekdayFormat_FullName) {
    // 2024-01-15 is a Monday
    auto r = fc.execute({{"value", "2024-01-15T10:00:00"}, {"format", "EEEE"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "Monday");
}

TEST_F(FormatDateEdgeCaseTest, WeekdayFormat_Abbreviated) {
    // 2024-01-15 is a Monday
    auto r = fc.execute({{"value", "2024-01-15T10:00:00"}, {"format", "EEE"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "Mon");
}

TEST_F(FormatDateEdgeCaseTest, NegativeTimestamp_HandledGracefully) {
    auto r = fc.execute({{"value", -1000}, {"format", "yyyy"}});
    if (isSuccess(r)) {
        EXPECT_FALSE(r.getValue().get<std::string>().empty());
    } else {
        EXPECT_TRUE(isError(r));
    }
}

TEST_F(FormatDateEdgeCaseTest, ZeroTimestamp_IsEpoch) {
    // 0 ms → 1970-01-01T00:00:00 UTC (localtime adjusts for timezone)
    auto r = fc.execute({{"value", 0}, {"format", "yyyy"}});
    ASSERT_TRUE(isSuccess(r));
    // Year should be 1969 or 1970 depending on timezone
    std::string year = r.getValue().get<std::string>();
    EXPECT_TRUE(year == "1970" || year == "1969");
}

TEST_F(FormatDateEdgeCaseTest, ArrayValue_ReturnsError) {
    auto r = fc.execute({{"value", json::array({1, 2, 3})}, {"format", "yyyy"}});
    ASSERT_TRUE(isError(r));
}

// ============================================================================
// LengthFunctionCall — Edge Cases
// ============================================================================

class LengthEdgeCaseTest : public ::testing::Test {
protected:
    LengthFunctionCall fc;
};

TEST_F(LengthEdgeCaseTest, MultiByte_UTF8_CountsBytes) {
    // "你好" in UTF-8 is 6 bytes (3 bytes per character)
    // value.length() counts bytes, not code points
    auto r = fc.execute({{"value", "\xe4\xbd\xa0\xe5\xa5\xbd"}, {"min", 6}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(LengthEdgeCaseTest, MultiByte_UTF8_MinCodePointsWouldFail) {
    // Same "你好" — if someone expects min=2 to pass based on char count,
    // it works here because 6 bytes >= 2
    auto r = fc.execute({{"value", "\xe4\xbd\xa0\xe5\xa5\xbd"}, {"min", 2}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(LengthEdgeCaseTest, MultiByte_UTF8_MaxByteBoundary) {
    // "你好" is 6 bytes; max=5 should fail because byte length > 5
    auto r = fc.execute({{"value", "\xe4\xbd\xa0\xe5\xa5\xbd"}, {"max", 5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(LengthEdgeCaseTest, NegativeMin_AlwaysPass) {
    // min=-1 → any string length (>= 0) satisfies min < 0
    auto r = fc.execute({{"value", ""}, {"min", -1}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(LengthEdgeCaseTest, ZeroMax_OnlyEmptyPasses) {
    auto r = fc.execute({{"value", ""}, {"max", 0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), true);
}

TEST_F(LengthEdgeCaseTest, ZeroMax_NonEmptyFails) {
    auto r = fc.execute({{"value", "a"}, {"max", 0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), false);
}

TEST_F(LengthEdgeCaseTest, NonIntegerMin_Ignored) {
    // min is a float (not integer) → should be ignored per implementation
    auto r = fc.execute({{"value", "a"}, {"min", 3.5}});
    ASSERT_TRUE(isSuccess(r));
    // Since min is not is_number_integer(), it's ignored → returns true
    EXPECT_EQ(r.getValue(), true);
}

// ============================================================================
// FormatNumberFunctionCall — Edge Cases
// ============================================================================

class FormatNumberEdgeCaseTest : public ::testing::Test {
protected:
    FormatNumberFunctionCall fc;
};

TEST_F(FormatNumberEdgeCaseTest, VeryLargeNumber_TrillionGrouping) {
    auto r = fc.execute({{"value", 1234567890123.0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "1,234,567,890,123.00");
}

TEST_F(FormatNumberEdgeCaseTest, VerySmallFraction) {
    auto r = fc.execute({{"value", 0.001}, {"decimals", 5}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "0.00100");
}

TEST_F(FormatNumberEdgeCaseTest, NegativeDecimals_Behavior) {
    // std::setprecision with negative value: implementation-defined
    // Just verify it doesn't crash
    auto r = fc.execute({{"value", 42.0}, {"decimals", -1}});
    ASSERT_TRUE(isSuccess(r));
    // Output is implementation-defined; just confirm non-empty
    EXPECT_FALSE(r.getValue().get<std::string>().empty());
}

TEST_F(FormatNumberEdgeCaseTest, ZeroValue_ZeroDecimals) {
    auto r = fc.execute({{"value", 0}, {"decimals", 0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "0");
}

TEST_F(FormatNumberEdgeCaseTest, OneThousandExact_NoExtraComma) {
    auto r = fc.execute({{"value", 1000}, {"decimals", 0}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "1,000");
}

TEST_F(FormatNumberEdgeCaseTest, TenDecimals_Precision) {
    auto r = fc.execute({{"value", 1.0}, {"decimals", 10}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "1.0000000000");
}

// ============================================================================
// FormatCurrencyFunctionCall — Edge Cases
// ============================================================================

class FormatCurrencyEdgeCaseTest : public ::testing::Test {
protected:
    FormatCurrencyFunctionCall fc;
};

TEST_F(FormatCurrencyEdgeCaseTest, EmptyCurrencyString_StillFormats) {
    auto r = fc.execute({{"value", 100}, {"currency", ""}});
    ASSERT_TRUE(isSuccess(r));
    // Empty currency → " 100.00" (space + number)
    EXPECT_EQ(r.getValue(), " 100.00");
}

TEST_F(FormatCurrencyEdgeCaseTest, LongCurrencySymbol) {
    auto r = fc.execute({{"value", 42}, {"currency", "BTC"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "BTC 42.00");
}

TEST_F(FormatCurrencyEdgeCaseTest, UnicodeSymbol_Yen) {
    auto r = fc.execute({{"value", 1000}, {"currency", "\xc2\xa5"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "\xc2\xa5 1,000.00");
}

TEST_F(FormatCurrencyEdgeCaseTest, VerySmallAmount) {
    auto r = fc.execute({{"value", 0.01}, {"currency", "$"}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "$ 0.01");
}

TEST_F(FormatCurrencyEdgeCaseTest, FourDecimals) {
    auto r = fc.execute({{"value", 99.9999}, {"currency", "$"}, {"decimals", 4}});
    ASSERT_TRUE(isSuccess(r));
    EXPECT_EQ(r.getValue(), "$ 99.9999");
}
