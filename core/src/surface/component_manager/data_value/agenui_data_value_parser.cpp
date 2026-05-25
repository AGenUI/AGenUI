#include "agenui_data_value_parser.h"
#include "nlohmann/json.hpp"
#include <cctype>
#include <optional>
#include "agenui_logger_internal.h"
#include "surface/component_manager/data_value/agenui_tabs_data_value.h"

namespace agenui {

std::shared_ptr<DataValue> DataValueParser::parseDataValue(IDataValueContext* context, const std::string& valueJson) {
    auto functionCallValue = parseFunctionCallDataValue(context, valueJson);
    if (functionCallValue) {
        return functionCallValue;
    }

    auto dataBindingValue = parseDataBindingDataValue(context, valueJson);
    if (dataBindingValue) {
        return dataBindingValue;
    }

    auto interpolationExpressionValue = parseInterpolationExpressionDataValue(context, valueJson);
    if (interpolationExpressionValue) {
        return interpolationExpressionValue;
    }

    return parseStaticDataValue(valueJson);
}

namespace {

// Trim leading/trailing ASCII whitespace.
std::string trimWhitespace(const std::string& input) {
    size_t start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = input.find_last_not_of(" \t\r\n");
    return input.substr(start, end - start + 1);
}

// True iff the string is a non-empty C-style identifier (letters/digits/_,
// not starting with a digit). Used to validate a function name.
bool isValidIdentifier(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    char head = text.front();
    if (!std::isalpha(static_cast<unsigned char>(head)) && head != '_') {
        return false;
    }
    for (size_t i = 1; i < text.size(); ++i) {
        char ch = text[i];
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            return false;
        }
    }
    return true;
}

// Walk `text` starting at `from` and collect the position of every occurrence
// of one of the `targets` characters that appears at the top level — i.e.
// outside any `(...)` group, outside any `${...}` block, and outside any
// single/double quoted string literal. Honors `\${` and intra-quote escapes.
// When `stopOnFirst` is true, returns immediately after the first hit.
std::vector<size_t> findTopLevelChars(const std::string& text,
                                      size_t from,
                                      const std::string& targets,
                                      bool stopOnFirst) {
    std::vector<size_t> hits;
    int parenDepth = 0;
    int braceDepth = 0;
    char quote = '\0';

    for (size_t i = from; i < text.size(); ++i) {
        char ch = text[i];

        if (quote != '\0') {
            if (ch == '\\' && i + 1 < text.size()) {
                ++i;
                continue;
            }
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\\' && i + 2 < text.size() && text[i + 1] == '$' && text[i + 2] == '{') {
            i += 2;
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '$' && i + 1 < text.size() && text[i + 1] == '{') {
            ++braceDepth;
            ++i;
            continue;
        }

        if (braceDepth > 0) {
            if (ch == '}') {
                --braceDepth;
            }
            continue;
        }

        // Record the hit before adjusting parenthesis depth so the opening
        // `(` of an argument list itself can be reported at top level.
        if (parenDepth == 0 && targets.find(ch) != std::string::npos) {
            hits.push_back(i);
            if (stopOnFirst) {
                return hits;
            }
        }

        if (ch == '(') {
            ++parenDepth;
            continue;
        }
        if (ch == ')') {
            if (parenDepth > 0) {
                --parenDepth;
            }
            continue;
        }
    }
    return hits;
}

// Decode a single/double-quoted string literal in the form `'...'` or `"..."`.
// Returns the unquoted, unescaped content on success; std::nullopt when the
// input is not a well-formed quoted literal (mismatched quotes, unterminated,
// dangling backslash, etc.). Supported escapes mirror JSON: \\ \' \" \/ \n \t
// \r \b \f. Unknown escapes are passed through unchanged for tolerance.
std::optional<std::string> tryDecodeQuotedLiteral(const std::string& text) {
    if (text.size() < 2) {
        return std::nullopt;
    }
    char quote = text.front();
    if ((quote != '\'' && quote != '"') || text.back() != quote) {
        return std::nullopt;
    }

    std::string decoded;
    decoded.reserve(text.size() - 2);
    for (size_t i = 1; i + 1 < text.size(); ++i) {
        char ch = text[i];
        if (ch == '\\') {
            if (i + 2 >= text.size()) {
                return std::nullopt;  // dangling backslash before closing quote
            }
            char next = text[i + 1];
            switch (next) {
                case '\\': decoded.push_back('\\'); break;
                case '\'': decoded.push_back('\''); break;
                case '"':  decoded.push_back('"');  break;
                case '/':  decoded.push_back('/');  break;
                case 'n':  decoded.push_back('\n'); break;
                case 't':  decoded.push_back('\t'); break;
                case 'r':  decoded.push_back('\r'); break;
                case 'b':  decoded.push_back('\b'); break;
                case 'f':  decoded.push_back('\f'); break;
                default:   decoded.push_back(next); break;  // tolerant passthrough
            }
            ++i;
            continue;
        }
        // A bare matching quote inside the literal is invalid.
        if (ch == quote) {
            return std::nullopt;
        }
        decoded.push_back(ch);
    }
    return decoded;
}

// Try to parse a callable-argument value text as a pure literal per the
// A2UI v0.9 `formatString` argument grammar:
//   - quoted string: 'abc' / "abc"   -> StaticDataValue (decoded text)
//   - number:        12, -3.14, +1e2 -> StaticDataValue holding the JSON number
//   - boolean:       true / false     -> StaticDataValue holding the JSON boolean
//   - null:          null              -> StaticDataValue holding JSON null
// Quoted-string template forms (i.e. content containing `${...}`) are
// resolved by tryParseInterpolationExpressionFromQuotedArgValue, which the
// caller invokes earlier in the dispatch chain. By the time this function
// runs, any quoted string is guaranteed to be a pure literal with no
// placeholders.
// Returns nullptr when the text is not recognized as any literal form; the
// caller is then expected to fall through to the fallback branch.
std::shared_ptr<StaticDataValue> tryParseLiteralArgValue(const std::string& argValueText) {
    std::string trimmed = trimWhitespace(argValueText);
    if (trimmed.empty()) {
        return nullptr;
    }

    char head = trimmed.front();

    // Quoted string literal — pure literal (no `${...}` placeholders, since
    // those are intercepted earlier by the quoted-template parser).
    if (head == '\'' || head == '"') {
        auto decoded = tryDecodeQuotedLiteral(trimmed);
        if (!decoded.has_value()) {
            return nullptr;
        }
        // StaticDataValue stores its value as a JSON literal, so re-encode
        // the decoded text into `"..."` form for downstream consumers.
        return std::make_shared<StaticDataValue>(nlohmann::json(*decoded).dump());
    }

    // Boolean / null literals.
    if (trimmed == "true" || trimmed == "false" || trimmed == "null") {
        return std::make_shared<StaticDataValue>(trimmed);
    }

    // Number literal: optional sign, digits or leading dot, validated via JSON parse.
    bool looksLikeNumber = (std::isdigit(static_cast<unsigned char>(head)) != 0) ||
                           ((head == '+' || head == '-' || head == '.') &&
                            trimmed.size() > 1 &&
                            (std::isdigit(static_cast<unsigned char>(trimmed[1])) != 0 ||
                             trimmed[1] == '.'));
    if (looksLikeNumber) {
        nlohmann::json parsed = nlohmann::json::parse(trimmed, nullptr, false);
        if (!parsed.is_discarded() && parsed.is_number()) {
            // Re-dump to canonicalize (e.g. `+1` -> `1`).
            return std::make_shared<StaticDataValue>(parsed.dump());
        }
    }

    return nullptr;
}

// Try to parse a callable-argument value text as a quoted DynamicString
// template — a string literal whose decoded content contains `${...}`
// placeholders (e.g. `'hello ${/name}'`, `"Total ${count}"`).
// Steps:
//   1. Require the input to be a quoted literal (single or double quotes).
//   2. Decode the quoted literal to obtain the raw template content.
//   3. Hand the decoded inner text to parseInterpolationExpressionFromRaw,
//      which performs the `${...}` segmentation.
// Returns nullptr when the input is not a quoted literal, when decoding
// fails, or when the decoded content carries no `${...}` placeholders — in
// the last case the caller falls through to the pure-literal branch.
std::shared_ptr<InterpolationExpressionDataValue>
tryParseInterpolationExpressionFromQuotedArgValue(IDataValueContext* context,
                                                  const std::string& argValueText) {
    std::string trimmed = trimWhitespace(argValueText);
    if (trimmed.empty()) {
        return nullptr;
    }
    char head = trimmed.front();
    if (head != '\'' && head != '"') {
        return nullptr;
    }
    auto decoded = tryDecodeQuotedLiteral(trimmed);
    if (!decoded.has_value()) {
        return nullptr;
    }
    return DataValueParser::parseInterpolationExpressionFromRaw(context, *decoded);
}

// Split `argsText` on top-level commas; whitespace-only entries are dropped.
std::vector<std::string> splitTopLevelArgs(const std::string& argsText) {
    std::vector<std::string> parts;
    std::vector<size_t> commaPositions = findTopLevelChars(argsText, 0, ",", /*stopOnFirst=*/false);

    size_t cursor = 0;
    for (size_t pos : commaPositions) {
        parts.emplace_back(argsText.substr(cursor, pos - cursor));
        cursor = pos + 1;
    }
    parts.emplace_back(argsText.substr(cursor));

    return parts;
}

// Locate the matching closing brace for a `${` block whose opening `{` is at
// position `openBracePos` in `text`. Honors nested `${...}` blocks, escaped
// `\${` inside the block, and single/double quoted string literals (with the
// usual `\` escape inside quotes). Returns the index of the matching `}` on
// success, or std::string::npos when the block is unbalanced.
size_t findMatchingClose(const std::string& text, size_t openBracePos) {
    int depth = 1;
    char quote = '\0';

    for (size_t i = openBracePos + 1; i < text.size(); ++i) {
        char ch = text[i];

        if (quote != '\0') {
            if (ch == '\\' && i + 1 < text.size()) {
                ++i;  // skip the escaped character inside the quoted string
                continue;
            }
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\\' && i + 2 < text.size() && text[i + 1] == '$' && text[i + 2] == '{') {
            // Escaped `\${` inside the expression — does not open a nested block.
            i += 2;
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '$' && i + 1 < text.size() && text[i + 1] == '{') {
            ++depth;
            ++i;
            continue;
        }

        if (ch == '}') {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }
    return std::string::npos;
}

// Strip a single outer `${...}` wrapper (with optional surrounding ASCII
// whitespace) from `text`. Returns the trimmed inner expression on success,
// or std::nullopt when the input is not a well-formed single-outer-`${...}`
// placeholder — e.g. empty, missing `${` / `}`, or side-by-side placeholders
// such as `${a}${b}` where the matching close of the first `${` is not the
// final character.
std::optional<std::string> unwrapPlaceholder(const std::string& text) {
    std::string trimmed = trimWhitespace(text);
    if (trimmed.size() < 3 || trimmed[0] != '$' || trimmed[1] != '{' ||
        trimmed.back() != '}' ||
        findMatchingClose(trimmed, 1) != trimmed.size() - 1) {
        return std::nullopt;
    }
    std::string inner = trimWhitespace(trimmed.substr(2, trimmed.size() - 3));
    if (inner.empty()) {
        return std::nullopt;
    }
    return inner;
}

}  // namespace

std::shared_ptr<InterpolationExpressionDataValue> DataValueParser::parseInterpolationExpressionDataValue(IDataValueContext* context, const std::string& valueJson) {
    // Public entry point: the input must be a JSON string literal — i.e.
    // wrapped in double quotes. A bare unquoted string is rejected so callers
    // cannot accidentally feed raw text through this entry point.
    // This wrapper only decodes the JSON string and delegates the actual
    // template segmentation to parseInterpolationExpressionFromRaw, which
    // operates on the already-decoded content. Callers that already hold the
    // decoded inner text (e.g. after stripping single quotes) can call the
    // raw entry point directly instead of re-encoding.
    if (valueJson.size() < 2 || valueJson.front() != '"' || valueJson.back() != '"') {
        return nullptr;
    }

    nlohmann::json json = nlohmann::json::parse(valueJson, nullptr, false);

    if (json.is_discarded() || !json.is_string()) {
        return nullptr;
    }

    return parseInterpolationExpressionFromRaw(context, json.get<std::string>());
}

std::shared_ptr<InterpolationExpressionDataValue> DataValueParser::parseInterpolationExpressionFromRaw(IDataValueContext* context, const std::string& rawValue) {
    // Contract: `rawValue` is the already-decoded DynamicString content — i.e.
    // the verbatim template text the author wrote, with any surrounding
    // quotes (single or double) and string-escape sequences already resolved
    // by the caller. This parser only performs `${...}` segmentation; it
    // does not know or care how the text was quoted in the source.

    // This layer performs segmentation only: it splits the input into a
    // sequence of literal segments and `${...}` placeholder segments, where
    // every segment is stored as a StaticDataValue holding the verbatim text.
    // A separate resolution pass is responsible for turning each placeholder's
    // inner text into a concrete DataValue subtype (DataBindingDataValue,
    // FunctionCallDataValue, nested InterpolationExpressionDataValue, …).
    //
    // Segmentation rules (per A2UI v0.9 `formatString`):
    //   - `${...}` opens a placeholder; the matching `}` is located via
    //     brace-depth counting that honors nested `${...}`, single/double
    //     quoted string literals, and `\${` escapes inside the expression.
    //   - `\${` outside a placeholder escapes the marker and contributes the
    //     literal characters `${` to the surrounding text segment.
    //   - An unbalanced `${` (no matching `}`) is treated as literal text.

    std::vector<std::shared_ptr<DataValue>> segments;
    std::string literalBuffer;

    auto flushLiteral = [&]() {
        if (literalBuffer.empty()) {
            return;
        }
        // StaticDataValue::_value is a JSON literal (it goes through
        // SerializableData::parse on read), so re-escape the unescaped literal
        // text and wrap it in quotes before constructing the segment.
        segments.emplace_back(std::make_shared<StaticDataValue>(nlohmann::json(literalBuffer).dump()));
        literalBuffer.clear();
    };

    size_t cursor = 0;
    bool foundPlaceholder = false;

    while (cursor < rawValue.size()) {
        char ch = rawValue[cursor];

        // Escape sequence `\${` → literal `${` (does not open a placeholder).
        if (ch == '\\' && cursor + 2 < rawValue.size() &&
            rawValue[cursor + 1] == '$' && rawValue[cursor + 2] == '{') {
            literalBuffer.append("${");
            cursor += 3;
            continue;
        }

        // Real `${...}` placeholder.
        if (ch == '$' && cursor + 1 < rawValue.size() && rawValue[cursor + 1] == '{') {
            size_t openBracePos = cursor + 1;
            size_t closePos = findMatchingClose(rawValue, openBracePos);

            if (closePos == std::string::npos) {
                // Unbalanced — the rest of the string is treated as literal text.
                literalBuffer.append(rawValue, cursor, std::string::npos);
                break;
            }

            foundPlaceholder = true;
            flushLiteral();

            // Resolve the placeholder by trying the expression-form parsers
            // in order — function call first, then data binding, finally fall
            // back to a StaticDataValue that preserves the placeholder verbatim
            // (including the surrounding `${` and `}`). The parsers below
            // expect the wrapped `${...}` form per their contract, so we pass
            // the full placeholder token rather than the inner text.
            std::string placeholderToken = rawValue.substr(cursor, closePos - cursor + 1);

            if (auto functionCall = parseFunctionCallDataValueFromExpression(context, placeholderToken)) {
                segments.emplace_back(functionCall);
            } else if (auto dataBinding = parseDataBindingDataValueFromExpression(context, placeholderToken)) {
                segments.emplace_back(dataBinding);
            } else {
                segments.emplace_back(std::make_shared<StaticDataValue>(
                    nlohmann::json(placeholderToken).dump()));
            }

            cursor = closePos + 1;
            continue;
        }

        literalBuffer.push_back(ch);
        ++cursor;
    }

    flushLiteral();

    // Only treat the input as an interpolation expression when at least one
    // placeholder was successfully extracted. Pure literals (including those
    // that only contain `\${` escapes) fall through to StaticDataValue.
    if (!foundPlaceholder || segments.empty()) {
        return nullptr;
    }

    return std::make_shared<InterpolationExpressionDataValue>(context, segments);
}

std::shared_ptr<DataBindingDataValue> DataValueParser::parseDataBindingDataValueFromExpression(IDataValueContext* context, const std::string& expressionText) {
    // Contract: input MUST be a single outer `${...}` placeholder (optionally
    // surrounded by ASCII whitespace). Bare inner text (e.g. `/a/b`) is
    // rejected — the caller is responsible for wrapping it as `${/a/b}`.
    auto inner = unwrapPlaceholder(expressionText);
    if (!inner.has_value()) {
        return nullptr;
    }
    std::string trimmed = *inner;

    // Reject function calls — they are identified by the presence of `()`
    // (per A2UI v0.9 `formatString` spec).
    if (trimmed.find('(') != std::string::npos) {
        return nullptr;
    }

    // Reject string literals (quoted with single or double quotes).
    char firstChar = trimmed.front();
    if (firstChar == '\'' || firstChar == '"') {
        return nullptr;
    }

    // Reject primitive literals: numbers, booleans, null.
    if (trimmed == "true" || trimmed == "false" || trimmed == "null") {
        return nullptr;
    }
    bool looksLikeNumber = (std::isdigit(static_cast<unsigned char>(firstChar)) != 0) ||
                           ((firstChar == '+' || firstChar == '-' || firstChar == '.') &&
                            trimmed.size() > 1 &&
                            std::isdigit(static_cast<unsigned char>(trimmed[1])) != 0);
    if (looksLikeNumber) {
        nlohmann::json parsed = nlohmann::json::parse(trimmed, nullptr, false);
        if (!parsed.is_discarded() && parsed.is_number()) {
            return nullptr;
        }
    }

    // Anything else is treated as a JSON Pointer data path. Both absolute
    // (`/foo/bar`) and relative (`foo/bar`) forms are valid; relative paths
    // are resolved against the current collection scope by the binder layer.
    return std::make_shared<DataBindingDataValue>(context, trimmed);
}

std::shared_ptr<FunctionCallDataValue> DataValueParser::parseFunctionCallDataValueFromExpression(IDataValueContext* context, const std::string& expressionText) {
    // Contract: input MUST be a single outer `${...}` placeholder (optionally
    // surrounded by ASCII whitespace). Bare inner text (e.g. `now()`) is
    // rejected — the caller is responsible for wrapping it as `${now()}`.
    auto inner = unwrapPlaceholder(expressionText);
    if (!inner.has_value()) {
        return nullptr;
    }
    std::string trimmed = *inner;
    if (trimmed.back() != ')') {
        return nullptr;
    }

    // Find the top-level `(` that opens the argument list. Function name occupies
    // everything before that `(`.
    std::vector<size_t> openParens = findTopLevelChars(trimmed, 0, "(", /*stopOnFirst=*/true);
    if (openParens.empty()) {
        return nullptr;
    }
    size_t openParenPos = openParens.front();
    if (openParenPos == 0) {
        return nullptr;
    }

    std::string functionName = trimWhitespace(trimmed.substr(0, openParenPos));
    if (!isValidIdentifier(functionName)) {
        return nullptr;
    }

    // Reject text after the closing `)` — only whitespace is allowed (already
    // trimmed away above), so the final `)` must be the very last character of
    // the trimmed expression.
    std::string argsText = trimmed.substr(openParenPos + 1, trimmed.size() - openParenPos - 2);

    std::map<std::string, std::shared_ptr<DataValue>> args;

    // An empty argument list (e.g. `now()`) is valid.
    std::string argsBody = trimWhitespace(argsText);
    if (!argsBody.empty()) {
        std::vector<std::string> argEntries = splitTopLevelArgs(argsText);
        for (const std::string& entry : argEntries) {
            std::string entryTrimmed = trimWhitespace(entry);
            if (entryTrimmed.empty()) {
                return nullptr;
            }

            // Split on the first top-level `:` to separate the argument name
            // from its value expression.
            std::vector<size_t> colonPositions = findTopLevelChars(entry, 0, ":", /*stopOnFirst=*/true);
            if (colonPositions.empty()) {
                return nullptr;
            }
            size_t colonPos = colonPositions.front();

            std::string argName = trimWhitespace(entry.substr(0, colonPos));
            std::string argValueText = entry.substr(colonPos + 1);
            if (!isValidIdentifier(argName)) {
                return nullptr;
            }

            // Resolve the argument value in priority order per the A2UI v0.9
            // `formatString` argument grammar. The candidate parsers are
            // grouped by the *kind* of DataValue they produce:
            //
            //   Expression family (yields dynamically-resolved DataValue):
            //     1. quoted DynamicString template — `'hello ${/x}'` etc.,
            //        produces an InterpolationExpressionDataValue
            //     2. `${funcName(...)}` — FunctionCallDataValue
            //     3. `${/path}` / `${path}` — DataBindingDataValue
            //
            //   Literal family (yields StaticDataValue):
            //     4. pure literal — quoted string without `${...}`, number,
            //        boolean, null
            //
            // Anything else (bare `funcName(...)`, bare `/path`, unknown
            // syntax, …) falls back to a StaticDataValue holding the verbatim
            // text, since it is not a well-formed arg value per the spec.
            std::shared_ptr<DataValue> argValue;
            if (auto interpArg = tryParseInterpolationExpressionFromQuotedArgValue(context, argValueText)) {
                argValue = interpArg;
            } else if (auto functionCallArg = parseFunctionCallDataValueFromExpression(context, argValueText)) {
                argValue = functionCallArg;
            } else if (auto dataBindingArg = parseDataBindingDataValueFromExpression(context, argValueText)) {
                argValue = dataBindingArg;
            } else if (auto literalArg = tryParseLiteralArgValue(argValueText)) {
                argValue = literalArg;
            } else {
                argValue = std::make_shared<StaticDataValue>(
                    nlohmann::json(argValueText).dump());
            }

            args[argName] = argValue;
        }
    }

    return std::make_shared<FunctionCallDataValue>(context, functionName, args);
}

std::shared_ptr<FunctionCallDataValue> DataValueParser::parseFunctionCallDataValue(IDataValueContext* context, const std::string& valueJson) {
    auto json = nlohmann::json::parse(valueJson, nullptr, false);

    if (json.is_discarded() || !json.is_object()) {
        return nullptr;
    }

    if (!json.contains("call")) {
        return nullptr;
    }

    if (!json["call"].is_string()) {
        return nullptr;
    }

    std::string functionName = json["call"].get<std::string>();

    std::map<std::string, std::shared_ptr<DataValue>> args;

    if (json.contains("args")) {
        const auto& argsJson = json["args"];

        if (!argsJson.is_object()) {
            return nullptr;
        }

        for (auto it = argsJson.begin(); it != argsJson.end(); ++it) {
            std::string paramName = it.key();
            std::string argValueJson = it.value().dump();
            auto argValue = parseDataValue(context, argValueJson);

            if (!argValue) {
                continue;
            }

            args[paramName] = argValue;
        }
    }

    return std::make_shared<FunctionCallDataValue>(context, functionName, args);
}

std::shared_ptr<DataBindingDataValue> DataValueParser::parseDataBindingDataValue(IDataValueContext* context, const std::string& valueJson) {
    auto json = nlohmann::json::parse(valueJson, nullptr, false);

    if (json.is_discarded()) {
        return nullptr;
    }

    if (json.is_object() && json.contains("path")) {
        AGENUI_LOG("%s", valueJson.c_str());
        std::string path = json["path"].get<std::string>();
        return std::make_shared<DataBindingDataValue>(context, path);
    }

    return nullptr;
}

std::shared_ptr<StaticDataValue> DataValueParser::parseStaticDataValue(const std::string& valueJson) {
    if (valueJson.empty()) {
        return std::make_shared<StaticDataValue>("");
    }

    auto jsonObj = nlohmann::json::parse(valueJson, nullptr, false);
    if (jsonObj.is_discarded()) {
        return std::make_shared<StaticDataValue>(valueJson);
    }

    return std::make_shared<StaticDataValue>(jsonObj.dump());
}

std::shared_ptr<CheckRuleDataValue> DataValueParser::parseCheckRule(IDataValueContext* context, const std::string& itemJson) {
    auto item = nlohmann::json::parse(itemJson, nullptr, false);

    if (item.is_discarded() || !item.is_object()) {
        return nullptr;
    }

    // Strictly follow CheckRule definition in common_types.json: {condition: DynamicBoolean, message: string}
    if (!item.contains("condition") || !item.contains("message")) {
        return nullptr;
    }

    std::string message = item["message"].is_string() ? item["message"].get<std::string>() : "";
    auto condition = parseDataValue(context, item["condition"].dump());

    if (!condition) {
        return nullptr;
    }

    return std::make_shared<CheckRuleDataValue>(context, condition, message);
}

std::shared_ptr<ChecksDataValue> DataValueParser::parseChecksDataValue(IDataValueContext* context, const std::string& valueJson) {
    auto json = nlohmann::json::parse(valueJson, nullptr, false);

    if (json.is_discarded() || !json.is_array()) {
        return nullptr;
    }

    std::vector<std::shared_ptr<CheckRuleDataValue>> rules;
    for (const auto& item : json) {
        auto rule = parseCheckRule(context, item.dump());
        if (rule) {
            rules.push_back(rule);
        }
    }

    if (rules.empty()) {
        return nullptr;
    }

    return std::make_shared<ChecksDataValue>(context, rules);
}

std::shared_ptr<StylesDataValue> DataValueParser::parseStylesDataValue(IDataValueContext* context, const std::string& valueJson) {
    auto json = nlohmann::json::parse(valueJson, nullptr, false);

    if (json.is_discarded() || !json.is_object()) {
        return nullptr;
    }

    std::map<std::string, std::shared_ptr<DataValue>> styles;
    for (auto it = json.begin(); it != json.end(); ++it) {
        std::string styleName = it.key();
        std::string styleValueJson = it.value().dump();
        auto styleValue = parseDataValue(context, styleValueJson);

        if (!styleValue) {
            continue;
        }

        styles[styleName] = styleValue;
    }

    if (styles.empty()) {
        return nullptr;
    }

    return std::make_shared<StylesDataValue>(context, styles);
}

std::shared_ptr<TabsDataValue> DataValueParser::parseTabsDataValue(IDataValueContext* context, const std::string& valueJson) {
    auto json = nlohmann::json::parse(valueJson, nullptr, false);

    if (json.is_discarded() || !json.is_array()) {
        return nullptr;
    }

    std::vector<TabItem> tabs;
    for (const auto& itemJson : json) {
        if (!itemJson.is_object()) {
            continue;
        }

        if (!itemJson.contains("title") || !itemJson.contains("child")) {
            continue;
        }

        std::string titleJson = itemJson["title"].dump();
        auto titleValue = parseDataValue(context, titleJson);

        if (!titleValue) {
            continue;
        }

        std::string child;
        if (itemJson["child"].is_string()) {
            child = itemJson["child"].get<std::string>();
        }

        TabItem tab;
        tab.title = titleValue;
        tab.child = child;
        tabs.emplace_back(tab);
    }

    if (tabs.empty()) {
        return nullptr;
    }

    return std::make_shared<TabsDataValue>(context, tabs);
}

std::shared_ptr<EventActionDataValue> DataValueParser::parseEventActionDataValue(IDataValueContext* context, const std::string& valueJson) {
    auto json = nlohmann::json::parse(valueJson, nullptr, false);

    if (json.is_discarded() || !json.is_object() || !json.contains("event")) {
        return nullptr;
    }

    const auto& eventJson = json["event"];

    if (!eventJson.is_object() || !eventJson.contains("name")) {
        return nullptr;
    }

    if (!eventJson["name"].is_string()) {
        return nullptr;
    }

    std::string eventName = eventJson["name"].get<std::string>();

    std::map<std::string, std::shared_ptr<DataValue>> contextMap;

    if (eventJson.contains("context")) {
        const auto& contextJson = eventJson["context"];

        if (!contextJson.is_object()) {
            return nullptr;
        }

        for (auto it = contextJson.begin(); it != contextJson.end(); ++it) {
            std::string contextKey = it.key();
            std::string contextValueJson = it.value().dump();
            auto contextValue = parseDataValue(context, contextValueJson);

            if (!contextValue) {
                continue;
            }

            contextMap[contextKey] = contextValue;
        }
    }

    return std::make_shared<EventActionDataValue>(context, eventName, contextMap);
}

std::shared_ptr<FunctionCallActionDataValue> DataValueParser::parseFunctionCallActionDataValue(IDataValueContext* context, const std::string& valueJson) {
    auto json = nlohmann::json::parse(valueJson, nullptr, false);

    if (json.is_discarded() || !json.is_object() || !json.contains("functionCall")) {
        return nullptr;
    }

    std::string functionCallJson = json["functionCall"].dump();

    auto functionCallValue = parseFunctionCallDataValue(context, functionCallJson);

    if (!functionCallValue) {
        return nullptr;
    }

    return std::make_shared<FunctionCallActionDataValue>(context, functionCallValue);
}

}  // namespace agenui
