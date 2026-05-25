#include "agenui_edge_insets_parser.h"
#include <cctype>
#include <sstream>
#include <vector>

namespace agenui {

// ============================================================================
// Static helper functions
// ============================================================================

static std::string trimWhitespace(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return str.substr(start, end - start);
}

static std::string toLower(const std::string& str) {
    std::string result = str;
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
    }
    return result;
}

static bool isCalcExpression(const std::string& str) {
    return toLower(str).find("calc(") == 0;
}

// Extract the inner expression from "calc(...)" including the outer "calc()".
static bool extractCalcExpr(const std::string& str, std::string& expr) {
    std::string lower = toLower(str);
    if (lower.find("calc(") != 0) {
        return false;
    }
    if (str.back() != ')') {
        return false;
    }
    expr = str;
    return true;
}

// Split a CSS shorthand string by whitespace, but keep calc() expressions intact.
static std::vector<std::string> splitTokens(const std::string& str) {
    std::vector<std::string> tokens;
    size_t i = 0;
    size_t len = str.size();

    while (i < len) {
        // Skip whitespace
        while (i < len && std::isspace(static_cast<unsigned char>(str[i]))) {
            ++i;
        }
        if (i >= len) {
            break;
        }

        // Check if this token starts with "calc("
        std::string remaining = toLower(str.substr(i));
        if (remaining.find("calc(") == 0) {
            // Find matching closing parenthesis
            int depth = 0;
            size_t start = i;
            while (i < len) {
                if (str[i] == '(') {
                    ++depth;
                } else if (str[i] == ')') {
                    --depth;
                    if (depth == 0) {
                        ++i;
                        break;
                    }
                }
                ++i;
            }
            tokens.push_back(trimWhitespace(str.substr(start, i - start)));
        } else {
            // Normal token: read until whitespace
            size_t start = i;
            while (i < len && !std::isspace(static_cast<unsigned char>(str[i]))) {
                ++i;
            }
            tokens.push_back(str.substr(start, i - start));
        }
    }
    return tokens;
}

// Unit suffix table: sorted longest-first to avoid partial matches (e.g. "vmin" before "vm").
struct UnitEntry {
    const char* suffix;
    size_t len;
    EdgeInsetUnit unit;
};

static const UnitEntry kUnitTable[] = {
    {"vmin", 4, EdgeInsetUnit::Vmin},
    {"vmax", 4, EdgeInsetUnit::Vmax},
    {"rem",  3, EdgeInsetUnit::Rem},
    {"px",   2, EdgeInsetUnit::Px},
    {"em",   2, EdgeInsetUnit::Em},
    {"vw",   2, EdgeInsetUnit::Vw},
    {"vh",   2, EdgeInsetUnit::Vh},
    {"cm",   2, EdgeInsetUnit::Cm},
    {"mm",   2, EdgeInsetUnit::Mm},
    {"in",   2, EdgeInsetUnit::In},
    {"pt",   2, EdgeInsetUnit::Pt},
    {"pc",   2, EdgeInsetUnit::Pc},
};

static const size_t kUnitTableSize = sizeof(kUnitTable) / sizeof(kUnitTable[0]);

// Try to parse a numeric string strictly: all characters must be consumed.
static bool parseStrictFloat(const std::string& str, float& value) {
    if (str.empty()) {
        return false;
    }
    char* endPtr = nullptr;
    value = std::strtof(str.c_str(), &endPtr);
    if (endPtr == str.c_str()) {
        return false;
    }
    // Reject trailing non-whitespace characters
    while (*endPtr != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*endPtr))) {
            return false;
        }
        ++endPtr;
    }
    return true;
}

static void initEdgeInsetValue(EdgeInsetValue& val) {
    val.value = 0.0f;
    val.unit = EdgeInsetUnit::Px;
    val.isCalc = false;
    val.calcExpr.clear();
}

// Parse a single edge value token into EdgeInsetValue.
static bool parseSingleValue(const std::string& token, EdgeInsetValue& result) {
    initEdgeInsetValue(result);

    std::string trimmed = trimWhitespace(token);
    if (trimmed.empty()) {
        return false;
    }

    // "auto" keyword
    if (toLower(trimmed) == "auto") {
        result.unit = EdgeInsetUnit::Auto;
        return true;
    }

    // calc() expression
    if (isCalcExpression(trimmed)) {
        std::string expr;
        if (!extractCalcExpr(trimmed, expr)) {
            return false;
        }
        result.isCalc = true;
        result.calcExpr = expr;
        return true;
    }

    // Percentage: e.g. "50%", "-10%"
    if (!trimmed.empty() && trimmed.back() == '%') {
        std::string numStr = trimmed.substr(0, trimmed.size() - 1);
        float val = 0.0f;
        if (!parseStrictFloat(numStr, val)) {
            return false;
        }
        result.value = val;
        result.unit = EdgeInsetUnit::Percent;
        return true;
    }

    // Try matching known unit suffixes (longest match first)
    std::string lower = toLower(trimmed);
    for (size_t u = 0; u < kUnitTableSize; ++u) {
        const UnitEntry& entry = kUnitTable[u];
        if (lower.size() > entry.len && lower.substr(lower.size() - entry.len) == entry.suffix) {
            std::string numStr = trimmed.substr(0, trimmed.size() - entry.len);
            float val = 0.0f;
            if (!parseStrictFloat(numStr, val)) {
                return false;
            }
            result.value = val;
            result.unit = entry.unit;
            return true;
        }
    }

    // Unitless number: treated as px (e.g. "10", "-3.5", "0")
    float val = 0.0f;
    if (!parseStrictFloat(trimmed, val)) {
        return false;
    }
    result.value = val;
    result.unit = EdgeInsetUnit::Px;
    return true;
}

// ============================================================================
// Public API
// ============================================================================

bool EdgeInsetsParser::parse(const std::string& cssValue, EdgeInsets& result) {
    // Zero-initialize result so callers get a clean state on failure
    initEdgeInsetValue(result.top);
    initEdgeInsetValue(result.right);
    initEdgeInsetValue(result.bottom);
    initEdgeInsetValue(result.left);

    std::string input = trimWhitespace(cssValue);
    if (input.empty()) {
        return false;
    }

    std::vector<std::string> tokens = splitTokens(input);
    if (tokens.empty() || tokens.size() > 4) {
        return false;
    }

    // Parse each token
    std::vector<EdgeInsetValue> values;
    values.reserve(tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        EdgeInsetValue val;
        if (!parseSingleValue(tokens[i], val)) {
            return false;
        }
        values.push_back(val);
    }

    // CSS shorthand expansion: 1~4 values -> top, right, bottom, left
    if (values.size() == 1) {
        result.top = values[0];
        result.right = values[0];
        result.bottom = values[0];
        result.left = values[0];
    } else if (values.size() == 2) {
        result.top = values[0];
        result.bottom = values[0];
        result.right = values[1];
        result.left = values[1];
    } else if (values.size() == 3) {
        result.top = values[0];
        result.right = values[1];
        result.left = values[1];
        result.bottom = values[2];
    } else {
        result.top = values[0];
        result.right = values[1];
        result.bottom = values[2];
        result.left = values[3];
    }

    return true;
}

}  // namespace agenui
