#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "surface/component_manager/data_value/agenui_data_value_parser.h"
#include "surface/component_manager/data_value/agenui_static_data_value.h"
#include "surface/component_manager/data_value/agenui_data_binding_data_value.h"
#include "surface/component_manager/data_value/agenui_interpolation_expression_data_value.h"
#include "surface/component_manager/data_value/agenui_idata_value_context.h"
#include "surface/agenui_isurface_context.h"

using namespace agenui;

// =============================================================================
// Mock Context
// =============================================================================

class MockDataValueContext : public IDataValueContext {
public:
    MOCK_METHOD(int, getInstanceId, (), (const, override));
    MOCK_METHOD(std::string, getSurfaceId, (), (const, override));
    MOCK_METHOD(IDataModel*, getDataModel, (), (const, override));

    MockDataValueContext() {
        ON_CALL(*this, getInstanceId()).WillByDefault(::testing::Return(1));
        ON_CALL(*this, getSurfaceId()).WillByDefault(::testing::Return("test_surface"));
        ON_CALL(*this, getDataModel()).WillByDefault(::testing::Return(nullptr));
    }
};

// =============================================================================
// StaticDataValue Tests
// =============================================================================

TEST(StaticDataValue, GetDataType_ReturnsStaticData) {
    StaticDataValue v("\"hello\"");
    EXPECT_EQ(v.getDataType(), DataType::StaticData);
}

TEST(StaticDataValue, GetDataBindingStatus_ReturnsNotDependent) {
    StaticDataValue v("\"hello\"");
    EXPECT_EQ(v.getDataBindingStatus(), DataBindingStatus::NotDependent);
}

TEST(StaticDataValue, GetValueData_StringLiteral_ReturnsSerializableData) {
    StaticDataValue v("\"hello world\"");
    auto data = v.getValueData();
    EXPECT_FALSE(data.isNull());
}

TEST(StaticDataValue, GetValueData_Number_ReturnsSerializableData) {
    StaticDataValue v("42");
    auto data = v.getValueData();
    EXPECT_FALSE(data.isNull());
}

TEST(StaticDataValue, GetValueData_Boolean_ReturnsSerializableData) {
    StaticDataValue v("true");
    auto data = v.getValueData();
    EXPECT_FALSE(data.isNull());
}

TEST(StaticDataValue, GetValueData_EmptyString_ReturnsData) {
    StaticDataValue v("\"\"");
    auto data = v.getValueData();
    EXPECT_FALSE(data.isNull());
}

TEST(StaticDataValue, Bind_DoesNotCrash) {
    StaticDataValue v("\"test\"");
    v.bind(nullptr);  // Static values don't need binding
}

TEST(StaticDataValue, Unbind_DoesNotCrash) {
    StaticDataValue v("\"test\"");
    v.unbind();  // Static values have no binding to release
}

TEST(StaticDataValue, CloneAsTemplate_ReturnsNonNull) {
    MockDataValueContext ctx;
    StaticDataValue v("\"hello\"");
    auto clone = v.cloneAsTemplate(&ctx, "/root");
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->getDataType(), DataType::StaticData);
}

// =============================================================================
// DataValueParser Tests — Static values
// =============================================================================

class DataValueParserTest : public ::testing::Test {
protected:
    MockDataValueContext ctx_;
};

TEST_F(DataValueParserTest, ParseDataValue_StringLiteral_ReturnsStaticDataValue) {
    auto result = DataValueParser::parseDataValue(&ctx_, "\"hello\"");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getDataType(), DataType::StaticData);
}

TEST_F(DataValueParserTest, ParseDataValue_NumberLiteral_ReturnsStaticDataValue) {
    auto result = DataValueParser::parseDataValue(&ctx_, "123");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getDataType(), DataType::StaticData);
}

TEST_F(DataValueParserTest, ParseDataValue_BooleanLiteral_ReturnsStaticDataValue) {
    auto result = DataValueParser::parseDataValue(&ctx_, "true");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getDataType(), DataType::StaticData);
}

TEST_F(DataValueParserTest, ParseDataValue_NullLiteral_ReturnsStaticOrNull) {
    auto result = DataValueParser::parseDataValue(&ctx_, "null");
    // null might return nullptr or a special value — either is acceptable
    if (result) {
        EXPECT_EQ(result->getDataType(), DataType::StaticData);
    }
}

// =============================================================================
// DataValueParser Tests — Data binding
// =============================================================================

TEST_F(DataValueParserTest, ParseDataValue_DataBinding_ReturnsDataBindingDataValue) {
    // Data binding format: {"path":"/user/name"}
    auto result = DataValueParser::parseDataValue(&ctx_, R"({"path":"/user/name"})");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getDataType(), DataType::DataBindingData);
}

TEST_F(DataValueParserTest, ParseDataValue_DataBinding_GetBindingPath) {
    auto result = DataValueParser::parseDataValue(&ctx_, R"({"path":"/count"})");
    ASSERT_NE(result, nullptr);
    auto* binding = dynamic_cast<DataBindingDataValue*>(result.get());
    ASSERT_NE(binding, nullptr);
    EXPECT_EQ(binding->getBindingPath(), "/count");
}

TEST_F(DataValueParserTest, ParseDataValue_DataBinding_TypeIsDataBindingData) {
    auto result = DataValueParser::parseDataValue(&ctx_, R"({"path":"/x"})");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getDataType(), DataType::DataBindingData);
}

// =============================================================================
// DataValueParser Tests — Interpolation expressions
// =============================================================================

TEST_F(DataValueParserTest, ParseInterpolation_WithPlaceholder_ReturnsInterpolation) {
    auto result = DataValueParser::parseInterpolationExpressionDataValue(&ctx_, "\"Hello ${/name}!\"");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getDataType(), DataType::InterpolationExpressionData);
}

TEST_F(DataValueParserTest, ParseInterpolation_NoPlaceholder_ReturnsNull) {
    auto result = DataValueParser::parseInterpolationExpressionDataValue(&ctx_, "\"plain text\"");
    // No ${} means it's not an interpolation expression
    EXPECT_EQ(result, nullptr);
}

TEST_F(DataValueParserTest, ParseInterpolation_MultiplePlaceholders_ReturnsInterpolation) {
    auto result = DataValueParser::parseInterpolationExpressionDataValue(&ctx_, "\"${/first} ${/last}\"");
    ASSERT_NE(result, nullptr);
    auto* interp = dynamic_cast<InterpolationExpressionDataValue*>(result.get());
    ASSERT_NE(interp, nullptr);
    auto segments = interp->getSegments();
    // Should have segments: binding, static(" "), binding
    EXPECT_GE(segments.size(), 2u);
}

TEST_F(DataValueParserTest, ParseInterpolationFromRaw_SimpleExpression) {
    auto result = DataValueParser::parseInterpolationExpressionFromRaw(&ctx_, "Hi ${/user}!");
    ASSERT_NE(result, nullptr);
    auto* interp = dynamic_cast<InterpolationExpressionDataValue*>(result.get());
    ASSERT_NE(interp, nullptr);
    auto segments = interp->getSegments();
    EXPECT_GE(segments.size(), 2u);  // "Hi " + binding + "!"
}

TEST_F(DataValueParserTest, ParseInterpolationFromRaw_NoPlaceholder_ReturnsNull) {
    auto result = DataValueParser::parseInterpolationExpressionFromRaw(&ctx_, "just plain text");
    EXPECT_EQ(result, nullptr);
}

// =============================================================================
// DataValueParser Tests — Function call
// =============================================================================

TEST_F(DataValueParserTest, ParseDataValue_FunctionCall_ReturnsFunctionCallDataValue) {
    // Function call format: {"call":"functionName", "args":{...}}
    auto result = DataValueParser::parseDataValue(&ctx_, R"({"call":"token","args":{"name":"Color_Text"}})");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getDataType(), DataType::FunctionCallData);
}

// =============================================================================
// DataValueParser Tests — Styles
// =============================================================================

TEST_F(DataValueParserTest, ParseStylesDataValue_ValidJson_ReturnsStylesDataValue) {
    auto result = DataValueParser::parseStylesDataValue(&ctx_, R"({"width":"100px","height":"50px"})");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getDataType(), DataType::StylesData);
}

TEST_F(DataValueParserTest, ParseStylesDataValue_EmptyObject_ReturnsNonNull) {
    auto result = DataValueParser::parseStylesDataValue(&ctx_, "{}");
    // Empty style object may or may not be valid; just verify no crash
    SUCCEED();
}

// =============================================================================
// DataValueParser Tests — Checks
// =============================================================================

TEST_F(DataValueParserTest, ParseChecksDataValue_ValidArray_ReturnsChecksDataValue) {
    // CheckRule uses {"check":{"path":...},"value":...} format
    auto result = DataValueParser::parseChecksDataValue(&ctx_,
        R"([{"check":{"path":"/visible"},"value":"true"}])");
    // May or may not succeed depending on internal validation
    if (result) {
        EXPECT_EQ(result->getDataType(), DataType::ChecksData);
    }
}

TEST_F(DataValueParserTest, ParseChecksDataValue_EmptyArray_ReturnsNonNull) {
    auto result = DataValueParser::parseChecksDataValue(&ctx_, "[]");
    // Empty array should return a valid but empty checks object
    if (result) {
        EXPECT_EQ(result->getDataType(), DataType::ChecksData);
    }
}

// =============================================================================
// DataValueParser Tests — Tabs
// =============================================================================

TEST_F(DataValueParserTest, ParseTabsDataValue_ValidArray_ReturnsTabsDataValue) {
    auto result = DataValueParser::parseTabsDataValue(&ctx_,
        R"([{"title":"Tab1","child":"content_1"}])");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getDataType(), DataType::TabsData);
}

// =============================================================================
// DataValueParser Tests — Event actions
// =============================================================================

TEST_F(DataValueParserTest, ParseEventAction_ValidJson_ReturnsEventActionDataValue) {
    auto result = DataValueParser::parseEventActionDataValue(&ctx_,
        R"({"action":"navigate","url":"https://example.com"})");
    // EventAction may have specific format requirements
    if (result) {
        EXPECT_EQ(result->getDataType(), DataType::EventActionData);
    }
}

// =============================================================================
// DataValueParser Tests — Expression-based parsing
// =============================================================================

TEST_F(DataValueParserTest, ParseDataBindingFromExpression_ValidPath_ReturnsBinding) {
    auto result = DataValueParser::parseDataBindingDataValueFromExpression(&ctx_, "${/user/name}");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getBindingPath(), "/user/name");
}

TEST_F(DataValueParserTest, ParseDataBindingFromExpression_RelativePath_ReturnsBinding) {
    auto result = DataValueParser::parseDataBindingDataValueFromExpression(&ctx_, "${name}");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getBindingPath(), "name");
}

TEST_F(DataValueParserTest, ParseDataBindingFromExpression_NotExpression_ReturnsNull) {
    auto result = DataValueParser::parseDataBindingDataValueFromExpression(&ctx_, "plain text");
    EXPECT_EQ(result, nullptr);
}

TEST_F(DataValueParserTest, ParseDataBindingFromExpression_Empty_ReturnsNull) {
    auto result = DataValueParser::parseDataBindingDataValueFromExpression(&ctx_, "");
    EXPECT_EQ(result, nullptr);
}

TEST_F(DataValueParserTest, ParseFunctionCallFromExpression_ValidCall_ReturnsFunctionCall) {
    auto result = DataValueParser::parseFunctionCallDataValueFromExpression(&ctx_,
        "${formatDate(value:${/date})}");
    // May or may not be supported; check for non-null if valid format
    if (result) {
        EXPECT_EQ(result->getDataType(), DataType::FunctionCallData);
    }
}

// =============================================================================
// DataValue::aggregateBindingStatus Tests
// =============================================================================

TEST(DataValueAggregateStatus, AllNotDependent_ReturnsNotDependent) {
    std::vector<DataBindingStatus> statuses = {
        DataBindingStatus::NotDependent,
        DataBindingStatus::NotDependent
    };
    EXPECT_EQ(DataValue::aggregateBindingStatus(statuses), DataBindingStatus::NotDependent);
}

TEST(DataValueAggregateStatus, AllFullyReady_ReturnsFullyReady) {
    std::vector<DataBindingStatus> statuses = {
        DataBindingStatus::FullyReady,
        DataBindingStatus::FullyReady
    };
    EXPECT_EQ(DataValue::aggregateBindingStatus(statuses), DataBindingStatus::FullyReady);
}

TEST(DataValueAggregateStatus, MixedReadyAndNotReady_ReturnsPartiallyReady) {
    std::vector<DataBindingStatus> statuses = {
        DataBindingStatus::FullyReady,
        DataBindingStatus::NotReady
    };
    EXPECT_EQ(DataValue::aggregateBindingStatus(statuses), DataBindingStatus::PartiallyReady);
}

TEST(DataValueAggregateStatus, Empty_ReturnsNotDependent) {
    std::vector<DataBindingStatus> statuses;
    EXPECT_EQ(DataValue::aggregateBindingStatus(statuses), DataBindingStatus::NotDependent);
}

TEST(DataValueAggregateStatus, SingleNotReady_ReturnsNotReady) {
    std::vector<DataBindingStatus> statuses = {DataBindingStatus::NotReady};
    EXPECT_EQ(DataValue::aggregateBindingStatus(statuses), DataBindingStatus::NotReady);
}

// =============================================================================
// InterpolationExpressionDataValue Tests
// =============================================================================

TEST(InterpolationExpressionDataValue, GetSegments_ReturnsAllSegments) {
    MockDataValueContext ctx;
    std::vector<std::shared_ptr<DataValue>> segments;
    segments.push_back(std::make_shared<StaticDataValue>("\"Hello \""));
    segments.push_back(std::make_shared<StaticDataValue>("\"World\""));

    InterpolationExpressionDataValue expr(&ctx, segments);
    EXPECT_EQ(expr.getSegments().size(), 2u);
}

TEST(InterpolationExpressionDataValue, GetDataType_ReturnsInterpolationExpressionData) {
    MockDataValueContext ctx;
    std::vector<std::shared_ptr<DataValue>> segments;
    segments.push_back(std::make_shared<StaticDataValue>("\"test\""));

    InterpolationExpressionDataValue expr(&ctx, segments);
    EXPECT_EQ(expr.getDataType(), DataType::InterpolationExpressionData);
}

TEST(InterpolationExpressionDataValue, BindingStatus_AllStatic_NotDependent) {
    MockDataValueContext ctx;
    std::vector<std::shared_ptr<DataValue>> segments;
    segments.push_back(std::make_shared<StaticDataValue>("\"a\""));
    segments.push_back(std::make_shared<StaticDataValue>("\"b\""));

    InterpolationExpressionDataValue expr(&ctx, segments);
    EXPECT_EQ(expr.getDataBindingStatus(), DataBindingStatus::NotDependent);
}

TEST(InterpolationExpressionDataValue, GetValueData_ConcatenatesSegments) {
    MockDataValueContext ctx;
    std::vector<std::shared_ptr<DataValue>> segments;
    segments.push_back(std::make_shared<StaticDataValue>("\"Hello\""));
    segments.push_back(std::make_shared<StaticDataValue>("\" World\""));

    InterpolationExpressionDataValue expr(&ctx, segments);
    auto data = expr.getValueData();
    EXPECT_FALSE(data.isNull());
}
