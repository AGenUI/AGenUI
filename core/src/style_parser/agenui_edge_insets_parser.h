#pragma once

#include <string>

namespace agenui {

/**
 * @brief Unit type for edge inset values
 */
enum class EdgeInsetUnit {
    Px,         // Pixel value (default for unitless numbers)
    Percent,    // Percentage value
    Em,         // Relative to font-size of the element
    Rem,        // Relative to font-size of the root element
    Vw,         // 1% of viewport width
    Vh,         // 1% of viewport height
    Vmin,       // 1% of viewport's smaller dimension
    Vmax,       // 1% of viewport's larger dimension
    Cm,         // Centimeters
    Mm,         // Millimeters
    In,         // Inches
    Pt,         // Points (1pt = 1/72 of 1in)
    Pc,         // Picas (1pc = 12pt)
    Auto        // CSS "auto" keyword
};

/**
 * @brief A single edge value with unit and optional calc() expression
 */
struct EdgeInsetValue {
    float value;            // Numeric value (raw percentage for Percent, px for Px, 0 for Auto)
    EdgeInsetUnit unit;     // Unit type
    bool isCalc;            // true if this value is a calc() expression
    std::string calcExpr;   // Raw calc() expression string (valid only when isCalc is true)
};

/**
 * @brief Parsed result of a CSS edge insets shorthand (margin / padding / inset / scroll-margin etc.)
 * @remark Values follow CSS shorthand order: top, right, bottom, left.
 *         The 1~4 value expansion rule is applied during parsing.
 */
struct EdgeInsets {
    EdgeInsetValue top;
    EdgeInsetValue right;
    EdgeInsetValue bottom;
    EdgeInsetValue left;
};

/**
 * @brief CSS edge insets shorthand value parser
 * @remark Parses CSS shorthand strings like "10px", "10px 20px", "10px 20px 30px",
 *         "10px 20px 30px 40px" into four directional values (top, right, bottom, left).
 *         Supports units: px, %, auto, calc(), and unitless numbers (treated as px).
 *         Supports negative values (e.g. "-10px").
 *         Thread-safe: all methods are pure static with no global state.
 */
class EdgeInsetsParser {
public:
    /**
     * @brief Parse a CSS edge insets shorthand string
     * @param cssValue [in] Input CSS shorthand string (e.g. "10px 20% auto 5px")
     * @param result [out] Output parsed edge insets
     * @return true if parsing succeeds, false otherwise
     */
    static bool parse(const std::string& cssValue, EdgeInsets& result);

    /**
     * @brief Parse a CSS inline-axis shorthand string (padding-inline / margin-inline value)
     * @param cssValue [in] Input shorthand string (e.g. "12px", "24px 8px" = start|end)
     * @param isRtl [in] Writing direction: true routes inline-start to the right edge,
     *                   false to the left edge
     * @param result [out] Output insets: left/right carry the physicalized inline edges;
     *                     top/bottom are zeroed (the inline axis has no block component)
     * @return true if parsing succeeds, false otherwise
     * @remark 1 value fills both edges; 2+ values map the first two tokens to
     *         start|end and extra tokens are ignored — mirroring the token
     *         mapping in CSSStyleConverter::applyPaddingInline. Family-agnostic:
     *         the same expansion serves padding-inline and margin-inline.
     */
    static bool parseInline(const std::string& cssValue, bool isRtl, EdgeInsets& result);
};

}  // namespace agenui
