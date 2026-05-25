#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace agenui {

/**
 * @brief Gradient type enumeration
 */
enum class GradientType {
    Linear,
    Radial,
    Conic
};

/**
 * @brief Unit type for color stop positions
 */
enum class StopUnit {
    Percent,    // Percentage (default)
    Px,         // Pixels (for linear-gradient)
    Deg,        // Degrees (for conic-gradient)
    Grad,       // Gradians
    Rad,        // Radians
    Turn        // Turns
};

/**
 * @brief Color stop point in a gradient
 */
struct ColorStop {
    uint32_t color;         // ARGB format
    float position;         // Position value (percentage 0.0~1.0, or raw value for px/deg/etc.)
    float positionEnd;      // Second position value for double-position syntax
    StopUnit unit;          // Unit of position value
    StopUnit unitEnd;       // Unit of positionEnd value
    bool hasPosition;       // Whether position is explicitly specified
    bool hasPositionEnd;    // Whether a second position is specified (double-position stop)
    bool isHint;            // true if this is a color hint (no color, position only)
    bool isCurrentColor;    // true if the color is "currentcolor" keyword
    bool positionIsCalc;    // true if position is a calc() expression
    std::string positionCalcExpr; // raw calc() expression string (if positionIsCalc)
};

/**
 * @brief Linear gradient parameters
 */
struct LinearGradientParams {
    float angle;            // Angle in degrees: 0 = to top, 90 = to right, 180 = to bottom
    bool angleIsCalc;       // true if angle is a calc() expression
    std::string angleCalcExpr; // raw calc() expression (if angleIsCalc)
};

/**
 * @brief Radial gradient shape
 */
enum class RadialShape { Circle, Ellipse };

/**
 * @brief Radial gradient size keyword
 */
enum class RadialSize { ClosestSide, ClosestCorner, FarthestSide, FarthestCorner };

/**
 * @brief Radial gradient parameters
 */
struct RadialGradientParams {
    RadialShape shape;      // Default: Ellipse
    RadialSize size;        // Default: FarthestCorner
    float centerX;          // Normalized 0.0~1.0 if centerXIsPx==false, raw px if true
    float centerY;          // Normalized 0.0~1.0 if centerYIsPx==false, raw px if true
    float radiusX;          // Explicit radius X (px or %)
    float radiusY;          // Explicit radius Y (px or %, equals radiusX for circle)
    bool hasExplicitSize;   // Whether explicit size is used instead of keyword
    bool radiusXIsPercent;  // Radius X unit: true=percentage, false=px
    bool radiusYIsPercent;  // Radius Y unit: true=percentage, false=px
    bool centerXIsPx;       // true if centerX is in px, false if normalized (keyword/%)
    bool centerYIsPx;       // true if centerY is in px, false if normalized (keyword/%)
    bool radiusXIsCalc;     // true if radiusX is a calc() expression
    bool radiusYIsCalc;     // true if radiusY is a calc() expression
    std::string radiusXCalcExpr; // raw calc() expression (if radiusXIsCalc)
    std::string radiusYCalcExpr; // raw calc() expression (if radiusYIsCalc)
};

/**
 * @brief Conic gradient parameters
 */
struct ConicGradientParams {
    float startAngle;       // Start angle in degrees, default 0
    float centerX;          // Normalized 0.0~1.0 if centerXIsPx==false, raw px if true
    float centerY;          // Normalized 0.0~1.0 if centerYIsPx==false, raw px if true
    bool centerXIsPx;       // true if centerX is in px, false if normalized (keyword/%)
    bool centerYIsPx;       // true if centerY is in px, false if normalized (keyword/%)
    bool startAngleIsCalc;  // true if startAngle is a calc() expression
    std::string startAngleCalcExpr; // raw calc() expression (if startAngleIsCalc)
};

/**
 * @brief Parsed gradient information
 */
struct GradientInfo {
    GradientType type;
    bool isRepeating = false;       // Whether this is a repeating gradient
    std::string colorInterpolationMethod; // e.g. "oklch", "srgb", "lab", etc.
    std::vector<ColorStop> colorStops;
    LinearGradientParams linear;    // Valid when type == Linear
    RadialGradientParams radial;    // Valid when type == Radial
    ConicGradientParams conic;      // Valid when type == Conic
};

/**
 * @brief Color value type: solid color or gradient
 */
enum class ColorValueType {
    Solid,
    Gradient
};

/**
 * @brief Unified parsed color value (solid color or gradient)
 */
struct ColorValue {
    ColorValueType type;
    uint32_t solidColor = 0;        // ARGB format, valid when type == Solid
    bool isCurrentColor = false;    // true if the color is "currentcolor" keyword
    GradientInfo gradient;          // Valid when type == Gradient
};

/**
 * @brief Unified CSS color value parser
 * @remark Supports both solid colors and gradients.
 *         Solid: #hex, rgb(), rgba(), hsl(), hsla(), hwb(), named colors, currentcolor
 *         Gradient: linear-gradient, radial-gradient, conic-gradient (+ repeating variants)
 *         Thread-safe: all methods are pure static with no global state.
 */
class ColorParser {
public:
    /**
     * @brief Parse a CSS color value string (solid color or gradient)
     * @param cssValue Input CSS color value string (e.g. "red", "#ff0000", "linear-gradient(...)")
     * @param result Output parsed color value
     * @return true if parsing succeeds, false otherwise
     */
    static bool parse(const std::string& cssValue, ColorValue& result);

private:
    static std::string trim(const std::string& str);
    static bool parseFloat(const std::string& str, float& value);

    // Gradient dispatch: determines gradient type and delegates
    static bool parseGradient(const std::string& cssGradient, GradientInfo& result);

    // Internal parsing helpers
    static bool parseLinearGradient(const std::string& content, GradientInfo& result);
    static bool parseRadialGradient(const std::string& content, GradientInfo& result);
    static bool parseConicGradient(const std::string& content, GradientInfo& result);

    // Angle parsing: supports deg, turn, grad, rad units
    static bool parseAngle(const std::string& str, float& angleDeg);

    // Direction keyword parsing: "to top", "to bottom right", etc.
    static bool parseDirectionKeyword(const std::string& str, float& angleDeg);

    // Color parsing: #hex, rgb(), rgba(), hsl(), hsla(), hwb(), named colors
    static bool parseColor(const std::string& str, uint32_t& color);

    // Color stop parsing: "red 50%", "#ff0000", "rgba(255,0,0,0.5) 30%"
    static bool parseColorStop(const std::string& str, ColorStop& stop);

    // Position parsing for radial/conic: "at center", "at 50% 50%", "at 100px 200px"
    // xIsPx/yIsPx: true if the value is in px, false if normalized (keyword or %)
    static bool parsePosition(const std::string& str, float& x, float& y,
                              bool& xIsPx, bool& yIsPx);

    // Utility: convert string to lowercase
    static std::string toLower(const std::string& str);

    // Utility: split by top-level commas (respecting parentheses nesting)
    static std::vector<std::string> splitTopLevelCommas(const std::string& str);

    // Utility: parse an integer from string
    static bool parseInt(const std::string& str, int& value);

    // Named color lookup
    static bool lookupNamedColor(const std::string& name, uint32_t& color);

    // HSL/HSLA color value parsing
    static bool parseHslValues(const std::string& inner, uint32_t& color);

    // HWB color value parsing
    static bool parseHwbValues(const std::string& inner, uint32_t& color);

    // HSL to RGB conversion
    static void hslToRgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b);

    // HWB to RGB conversion
    static void hwbToRgb(float h, float w, float bk, uint8_t& r, uint8_t& g, uint8_t& b);

    // Hue-to-RGB helper for HSL conversion
    static float hueToRgbHelper(float p, float q, float t);
};

}  // namespace agenui
