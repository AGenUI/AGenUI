#pragma once

#include "agenui_data_value_base.h"
#include "agenui_static_data_value.h"
#include "agenui_data_binding_data_value.h"
#include "agenui_function_call_data_value.h"
#include "agenui_interpolation_expression_data_value.h"
#include "agenui_checks_data_value.h"
#include "agenui_check_rule_data_value.h"
#include "agenui_styles_data_value.h"
#include "agenui_tabs_data_value.h"
#include "agenui_event_action_data_value.h"
#include "agenui_function_call_action_data_value.h"
#include <string>
#include <memory>

namespace agenui {

/**
 * @brief Data value parser
 * @remark Provides static methods for parsing various data value types
 */
class DataValueParser {
public:
    /**
     * @brief Parse a data value (generic)
     * @param context Data value context pointer
     * @param valueJson Data value JSON string
     * @return DataValue smart pointer
     */
    static std::shared_ptr<DataValue> parseDataValue(IDataValueContext* context, const std::string& valueJson);

    /**
     * @brief Parse a checks data value array
     * @param context Data value context pointer
     * @param valueJson JSON array string
     * @return ChecksDataValue smart pointer
     */
    static std::shared_ptr<ChecksDataValue> parseChecksDataValue(IDataValueContext* context, const std::string& valueJson);

    /**
     * @brief Parse a styles data value
     * @param context Data value context pointer
     * @param valueJson JSON string
     * @return StylesDataValue smart pointer
     */
    static std::shared_ptr<StylesDataValue> parseStylesDataValue(IDataValueContext* context, const std::string& valueJson);

    /**
     * @brief Parse a tabs data value
     * @param context Data value context pointer
     * @param valueJson JSON array string, e.g. [{"title": "aaaa", "child": "driving_content"}]
     * @return TabsDataValue smart pointer
     */
    static std::shared_ptr<TabsDataValue> parseTabsDataValue(IDataValueContext* context, const std::string& valueJson);

    /**
     * @brief Parse an event action data value
     * @param context Data value context pointer
     * @param valueJson JSON string
     * @return EventActionDataValue smart pointer
     */
    static std::shared_ptr<EventActionDataValue> parseEventActionDataValue(IDataValueContext* context, const std::string& valueJson);

    /**
     * @brief Parse a function-call action data value
     * @param context Data value context pointer
     * @param valueJson JSON string
     * @return FunctionCallActionDataValue smart pointer
     */
    static std::shared_ptr<FunctionCallActionDataValue> parseFunctionCallActionDataValue(IDataValueContext* context, const std::string& valueJson);

    /**
     * @brief Parse a string into an interpolation expression composed of segments.
     * @param context Data value context pointer
     * @param valueJson A JSON string literal — i.e. a string wrapped in
     *        double quotes, such as the textual form `"hello ${/name}"`
     *        (10 characters, the first and last of which are `"`). This is
     *        a thin convenience wrapper that decodes the JSON string literal
     *        and delegates to parseInterpolationExpressionFromRaw for the
     *        actual `${...}` segmentation.
     * @return InterpolationExpressionDataValue smart pointer, or nullptr when
     *         the input is not a JSON string literal, when JSON parsing fails,
     *         or when the decoded content contains no balanced `${...}`
     *         placeholders (caller should fall back to a plain StaticDataValue
     *         in that case).
     * @remark Follows A2UI v0.9 `formatString` syntax. The result is an
     *         ordered list of segments where each segment is either a
     *         StaticDataValue (literal text, with `\${` unescaped to `${`) or
     *         a DataBindingDataValue (data-model path) / FunctionCallDataValue
     *         (client-side function call).
     * @remark Callers that already hold the decoded inner text (e.g. after
     *         stripping single quotes from an A2UI-style quoted argument)
     *         should call parseInterpolationExpressionFromRaw directly to
     *         avoid a redundant encode-then-decode round trip.
     */
    static std::shared_ptr<InterpolationExpressionDataValue> parseInterpolationExpressionDataValue(IDataValueContext* context, const std::string& valueJson);

    /**
     * @brief Parse an A2UI `formatString` placeholder into a DataBindingDataValue.
     * @param context Data value context pointer
     * @param expressionText A single outer `${...}` placeholder
     *        (e.g. `${/user/firstName}`). Surrounding ASCII whitespace is
     *        tolerated. **Contract: bare inner text is rejected** — callers
     *        must wrap inner expressions as `${...}` before invocation.
     * @return DataBindingDataValue smart pointer when the inner expression is
     *         a JSON Pointer data path per A2UI v0.9 `formatString` rules;
     *         nullptr when the input is not a `${...}` placeholder, when the
     *         inner expression is empty, when it is a function call, when it
     *         is a literal value (quoted string, number, boolean, null), or
     *         otherwise not a well-formed data path.
     * @remark Absolute paths start with `/` (e.g. `/user/firstName`); relative
     *         paths do not (e.g. `firstName`) and are resolved against the
     *         current collection scope by the binder layer.
     */
    static std::shared_ptr<DataBindingDataValue> parseDataBindingDataValueFromExpression(IDataValueContext* context, const std::string& expressionText);

    /**
     * @brief Parse an A2UI `formatString` placeholder into a FunctionCallDataValue.
     * @param context Data value context pointer
     * @param expressionText A single outer `${...}` placeholder
     *        (e.g. `${formatDate(value:${/d)}}`). Surrounding ASCII whitespace
     *        is tolerated. **Contract: bare inner text is rejected** — callers
     *        must wrap inner expressions as `${...}` before invocation.
     * @return FunctionCallDataValue smart pointer when the inner expression
     *         has the form `funcName(argName1: value1, argName2: value2, ...)`
     *         per A2UI v0.9 `formatString` rules; nullptr when the input is
     *         not a `${...}` placeholder or when the inner expression is not
     *         a well-formed function call.
     * @remark Argument values are resolved in priority order by the kind of
     *         DataValue they produce:
     *           1. quoted DynamicString template — e.g. `'hello ${/x}'` or
     *              `"hello ${/x}"`; yields an InterpolationExpressionDataValue.
     *           2. `${funcName(...)}` — yields a FunctionCallDataValue.
     *           3. `${/path}` or `${path}` — yields a DataBindingDataValue.
     *           4. pure literal — single/double-quoted string without
     *              `${...}`, number, boolean, or `null`; yields a
     *              StaticDataValue.
     *         Bare unwrapped forms (e.g. `funcName(...)`, `/path`) are *not*
     *         accepted and fall back to a verbatim StaticDataValue.
     */
    static std::shared_ptr<FunctionCallDataValue> parseFunctionCallDataValueFromExpression(IDataValueContext* context, const std::string& expressionText);

    /**
     * @brief Parse the *already-decoded* DynamicString template body into an
     *        interpolation expression composed of segments.
     * @param context Data value context pointer
     * @param rawValue The decoded DynamicString content — verbatim template
     *        text after any surrounding quotes (single or double) and string
     *        escape sequences have been resolved by the caller. This entry
     *        point performs `${...}` segmentation only; it does not handle
     *        quote stripping or JSON unescaping itself, so callers can feed
     *        in raw template text obtained from any source (JSON string
     *        literal, single-quoted arg value, decoded string, …).
     * @return InterpolationExpressionDataValue smart pointer, or nullptr when
     *         the input contains no `${...}` placeholders, or when every
     *         occurrence of `${` is unbalanced (i.e. no matching `}` exists,
     *         in which case the entire remainder is consumed as literal text
     *         and no placeholder segment is produced).
     * @remark See parseInterpolationExpressionDataValue for the JSON-literal
     *         convenience wrapper that strips the outer JSON quotes before
     *         delegating to this raw-text entry point.
     */
    static std::shared_ptr<InterpolationExpressionDataValue> parseInterpolationExpressionFromRaw(IDataValueContext* context, const std::string& rawValue);

private:
    /**
     * @brief Parse a function-call data value
     * @param context Data value context pointer
     * @param valueJson JSON string
     * @return FunctionCallDataValue smart pointer
     */
    static std::shared_ptr<FunctionCallDataValue> parseFunctionCallDataValue(IDataValueContext* context, const std::string& valueJson);

    /**
     * @brief Parse a data-binding data value
     * @param context Data value context pointer
     * @param valueJson JSON string
     * @return DataBindingDataValue smart pointer
     */
    static std::shared_ptr<DataBindingDataValue> parseDataBindingDataValue(IDataValueContext* context, const std::string& valueJson);

    /**
     * @brief Parse a static data value
     * @param valueJson JSON string
     * @return StaticDataValue smart pointer
     */
    static std::shared_ptr<StaticDataValue> parseStaticDataValue(const std::string& valueJson);

    /**
     * @brief Parse a single CheckRule item
     * @param context Data value context pointer
     * @param itemJson CheckRule JSON string
     * @return CheckRuleDataValue smart pointer, nullptr if parsing fails
     */
    static std::shared_ptr<CheckRuleDataValue> parseCheckRule(IDataValueContext* context, const std::string& itemJson);
};

}  // namespace agenui
