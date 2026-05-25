#include "agenui_color_parser.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace agenui {

// ============================================================================
// Forward declarations of internal helpers
// ============================================================================

static uint32_t hexCharToInt(char c);
static std::string trimStr(const std::string& s);
static std::vector<std::string> splitByWhitespace(const std::string& s);
static bool parseFloatValue(const std::string& str, float& value);
static bool isCalcExpression(const std::string& str);
static bool extractCalcContent(const std::string& str, std::string& expr);
static bool parseColorComponent(const std::string& str, int& value);
static bool parseAlphaValue(const std::string& str, float& alpha);
static bool parseHueValue(const std::string& str, float& degrees);
static bool parseRgbRgbaValues(const std::string& inner, uint32_t& color);
static bool isValidColorSpace(const std::string& s);

// ============================================================================
// Public API
// ============================================================================

bool ColorParser::parse(const std::string& cssValue, ColorValue& result) {
    std::string input = trim(cssValue);
    if (input.empty()) {
        return false;
    }

    std::string lower = toLower(input);

    // Check if the input is a gradient function
    bool isGradient = (lower.find("linear-gradient(") == 0 ||
                       lower.find("radial-gradient(") == 0 ||
                       lower.find("conic-gradient(") == 0 ||
                       lower.find("repeating-linear-gradient(") == 0 ||
                       lower.find("repeating-radial-gradient(") == 0 ||
                       lower.find("repeating-conic-gradient(") == 0);

    if (isGradient) {
        result.type = ColorValueType::Gradient;
        result.solidColor = 0;
        result.isCurrentColor = false;
        return parseGradient(input, result.gradient);
    }

    // Try parsing as solid color
    result.type = ColorValueType::Solid;
    result.isCurrentColor = (lower == "currentcolor");
    return parseColor(input, result.solidColor);
}

bool ColorParser::parseGradient(const std::string& cssGradient, GradientInfo& result) {
    std::string input = trim(cssGradient);
    if (input.empty()) {
        return false;
    }

    // Reset output to avoid stale data from previous calls
    result.colorStops.clear();
    result.isRepeating = false;
    result.colorInterpolationMethod = "";
    result.linear.angle = 180.0f;
    result.linear.angleIsCalc = false;
    result.linear.angleCalcExpr = "";
    result.radial.shape = RadialShape::Ellipse;
    result.radial.size = RadialSize::FarthestCorner;
    result.radial.centerX = 0.5f;
    result.radial.centerY = 0.5f;
    result.radial.radiusX = 0.0f;
    result.radial.radiusY = 0.0f;
    result.radial.hasExplicitSize = false;
    result.radial.radiusXIsPercent = false;
    result.radial.radiusYIsPercent = false;
    result.radial.centerXIsPx = false;
    result.radial.centerYIsPx = false;
    result.radial.radiusXIsCalc = false;
    result.radial.radiusYIsCalc = false;
    result.radial.radiusXCalcExpr = "";
    result.radial.radiusYCalcExpr = "";
    result.conic.startAngle = 0.0f;
    result.conic.centerX = 0.5f;
    result.conic.centerY = 0.5f;
    result.conic.centerXIsPx = false;
    result.conic.centerYIsPx = false;
    result.conic.startAngleIsCalc = false;
    result.conic.startAngleCalcExpr = "";

    std::string lower = toLower(input);

    // Determine gradient type by prefix (check repeating variants first)
    std::string content;

    if (lower.find("repeating-linear-gradient(") == 0 && input.back() == ')') {
        result.type = GradientType::Linear;
        result.isRepeating = true;
        content = input.substr(26, input.size() - 27);
        return parseLinearGradient(content, result);
    } else if (lower.find("repeating-radial-gradient(") == 0 && input.back() == ')') {
        result.type = GradientType::Radial;
        result.isRepeating = true;
        content = input.substr(26, input.size() - 27);
        return parseRadialGradient(content, result);
    } else if (lower.find("repeating-conic-gradient(") == 0 && input.back() == ')') {
        result.type = GradientType::Conic;
        result.isRepeating = true;
        content = input.substr(25, input.size() - 26);
        return parseConicGradient(content, result);
    } else if (lower.find("linear-gradient(") == 0 && input.back() == ')') {
        result.type = GradientType::Linear;
        content = input.substr(16, input.size() - 17);
        return parseLinearGradient(content, result);
    } else if (lower.find("radial-gradient(") == 0 && input.back() == ')') {
        result.type = GradientType::Radial;
        content = input.substr(16, input.size() - 17);
        return parseRadialGradient(content, result);
    } else if (lower.find("conic-gradient(") == 0 && input.back() == ')') {
        result.type = GradientType::Conic;
        content = input.substr(15, input.size() - 16);
        return parseConicGradient(content, result);
    }

    return false;
}

// ============================================================================
// Linear Gradient Parsing
// ============================================================================

bool ColorParser::parseLinearGradient(const std::string& content, GradientInfo& result) {
    std::vector<std::string> parts = splitTopLevelCommas(content);
    if (parts.empty()) {
        return false;
    }

    size_t colorStartIndex = 0;
    result.linear.angle = 180.0f; // Default: "to bottom"

    // Try to parse first part as angle or direction
    std::string firstPart = trim(parts[0]);
    std::string firstPartLower = toLower(firstPart);

    // Extract color-interpolation-method from the first part (can be combined with direction)
    // Look for " in " that indicates color interpolation
    size_t inPos = firstPartLower.rfind(" in ");
    if (inPos != std::string::npos) {
        std::string possibleSpace = trimStr(firstPartLower.substr(inPos + 4));
        if (isValidColorSpace(possibleSpace)) {
            result.colorInterpolationMethod = possibleSpace;
            firstPart = trim(firstPart.substr(0, inPos));
            firstPartLower = toLower(firstPart);
        }
    }

    float angle = 0.0f;
    if (parseAngle(firstPartLower, angle)) {
        result.linear.angle = angle;
        if (isCalcExpression(firstPart)) {
            result.linear.angleIsCalc = true;
            std::string expr;
            extractCalcContent(firstPart, expr);
            result.linear.angleCalcExpr = expr;
        }
        colorStartIndex = 1;
    } else if (firstPartLower.find("to ") == 0) {
        if (parseDirectionKeyword(firstPartLower, angle)) {
            result.linear.angle = angle;
            colorStartIndex = 1;
        } else {
            return false;
        }
    } else if (!firstPart.empty() && result.colorInterpolationMethod.empty()) {
        // firstPart is not a direction/angle, treat as color stop (colorStartIndex stays 0)
    } else if (!result.colorInterpolationMethod.empty() && firstPart.empty()) {
        // Only "in <space>" was in firstPart, direction defaults applied
        colorStartIndex = 1;
    } else if (!result.colorInterpolationMethod.empty()) {
        // Had direction + "in <space>" combined
        colorStartIndex = 1;
    }

    // Check for color-interpolation-method as separate segment (fallback)
    if (result.colorInterpolationMethod.empty() && colorStartIndex < parts.size()) {
        std::string nextPart = toLower(trim(parts[colorStartIndex]));
        if (nextPart.rfind("in ", 0) == 0) {
            std::string space = trimStr(nextPart.substr(3));
            if (isValidColorSpace(space)) {
                result.colorInterpolationMethod = space;
                colorStartIndex++;
            }
        }
    }

    // Parse color stops
    for (size_t i = colorStartIndex; i < parts.size(); ++i) {
        ColorStop stop;
        if (!parseColorStop(trim(parts[i]), stop)) {
            return false;
        }
        result.colorStops.push_back(stop);
    }

    return result.colorStops.size() >= 2;
}

// ============================================================================
// Radial Gradient Parsing
// ============================================================================

bool ColorParser::parseRadialGradient(const std::string& content, GradientInfo& result) {
    std::vector<std::string> parts = splitTopLevelCommas(content);
    if (parts.empty()) {
        return false;
    }

    // Set defaults
    result.radial.shape = RadialShape::Ellipse;
    result.radial.size = RadialSize::FarthestCorner;
    result.radial.centerX = 0.5f;
    result.radial.centerY = 0.5f;
    result.radial.radiusX = 0.0f;
    result.radial.radiusY = 0.0f;
    result.radial.hasExplicitSize = false;
    result.radial.radiusXIsPercent = false;
    result.radial.radiusYIsPercent = false;

    size_t colorStartIndex = 0;

    // Try to parse the first part as shape/size/position descriptor
    std::string firstPart = trim(parts[0]);
    std::string firstPartLower = toLower(firstPart);

    // Extract color-interpolation-method from the first part (can be combined with descriptor)
    size_t inPos = firstPartLower.rfind(" in ");
    if (inPos != std::string::npos) {
        std::string possibleSpace = trimStr(firstPartLower.substr(inPos + 4));
        if (isValidColorSpace(possibleSpace)) {
            result.colorInterpolationMethod = possibleSpace;
            firstPart = trim(firstPart.substr(0, inPos));
            firstPartLower = toLower(firstPart);
        }
    }

    // Check if first part is a color stop or a descriptor
    // Descriptors contain keywords like "circle", "ellipse", "at", size keywords,
    // or start with a numeric value (explicit size), or calc() expression
    bool isDescriptor = false;
    if (firstPartLower.find("circle") != std::string::npos ||
        firstPartLower.find("ellipse") != std::string::npos ||
        firstPartLower.find("at ") != std::string::npos ||
        firstPartLower.find("closest-") != std::string::npos ||
        firstPartLower.find("farthest-") != std::string::npos ||
        firstPartLower.find("calc(") != std::string::npos) {
        isDescriptor = true;
    }

    // Also check if it starts with a numeric value followed by px/% (explicit size)
    if (!isDescriptor && !firstPartLower.empty()) {
        // Check for pattern: number + "px" or number + "%" possibly followed by more
        size_t idx = 0;
        if (firstPartLower[idx] == '-' || firstPartLower[idx] == '+' ||
            firstPartLower[idx] == '.' ||
            (firstPartLower[idx] >= '0' && firstPartLower[idx] <= '9')) {
            // Might be an explicit size like "50px" or "50px 30px" or "50%"
            // Check if there's "px" or "%" nearby
            if (firstPartLower.find("px") != std::string::npos ||
                (firstPartLower.find('%') != std::string::npos &&
                 firstPartLower.find("at ") != std::string::npos)) {
                isDescriptor = true;
            }
            // Also match "50px at center" or standalone "50px"
            if (!isDescriptor && firstPartLower.find("px") != std::string::npos) {
                isDescriptor = true;
            }
            // Match percentage size like "50% 30% at center"
            if (!isDescriptor && firstPartLower.find('%') != std::string::npos) {
                // Could be a color stop percentage, so only treat as descriptor
                // if there's "at" keyword or two percentage values
                size_t firstPercent = firstPartLower.find('%');
                size_t secondPercent = firstPartLower.find('%', firstPercent + 1);
                if (secondPercent != std::string::npos ||
                    firstPartLower.find(" at ") != std::string::npos) {
                    isDescriptor = true;
                }
            }
        }
    }

    if (isDescriptor) {
        colorStartIndex = 1;

        // Split by "at" to separate shape/size from position
        size_t atPos = firstPartLower.find(" at ");
        std::string shapeSize;
        std::string positionStr;

        if (atPos != std::string::npos) {
            shapeSize = trim(firstPart.substr(0, atPos));
            positionStr = trim(firstPart.substr(atPos + 4));
        } else if (firstPartLower.find("at ") == 0) {
            positionStr = trim(firstPart.substr(3));
        } else {
            shapeSize = firstPart;
        }

        // Parse shape and size from shapeSize string
        if (!shapeSize.empty()) {
            std::string shapeSizeLower = toLower(shapeSize);

            // Detect shape keywords
            bool hasCircleKeyword = (shapeSizeLower.find("circle") != std::string::npos);
            bool hasEllipseKeyword = (shapeSizeLower.find("ellipse") != std::string::npos);

            if (hasCircleKeyword) {
                result.radial.shape = RadialShape::Circle;
            } else if (hasEllipseKeyword) {
                result.radial.shape = RadialShape::Ellipse;
            }

            // Check size keywords
            if (shapeSizeLower.find("closest-side") != std::string::npos) {
                result.radial.size = RadialSize::ClosestSide;
            } else if (shapeSizeLower.find("closest-corner") != std::string::npos) {
                result.radial.size = RadialSize::ClosestCorner;
            } else if (shapeSizeLower.find("farthest-side") != std::string::npos) {
                result.radial.size = RadialSize::FarthestSide;
            } else if (shapeSizeLower.find("farthest-corner") != std::string::npos) {
                result.radial.size = RadialSize::FarthestCorner;
            } else {
                // Try to parse explicit size values (px, %, or calc())
                // Tokenize the shapeSize, skipping shape keywords (paren-aware)
                std::vector<std::string> sizeTokens;
                std::string token;
                int parenDepth = 0;
                for (size_t i = 0; i < shapeSize.size(); ++i) {
                    if (shapeSize[i] == '(') {
                        parenDepth++;
                        token += shapeSize[i];
                    } else if (shapeSize[i] == ')') {
                        parenDepth--;
                        token += shapeSize[i];
                    } else if (shapeSize[i] == ' ' && parenDepth == 0) {
                        if (!token.empty()) {
                            sizeTokens.push_back(token);
                            token.clear();
                        }
                    } else {
                        token += shapeSize[i];
                    }
                }
                if (!token.empty()) {
                    sizeTokens.push_back(token);
                }

                // Filter out shape keywords from tokens
                std::vector<std::string> numericTokens;
                for (const auto& t : sizeTokens) {
                    std::string tLower = toLower(t);
                    if (tLower != "circle" && tLower != "ellipse") {
                        numericTokens.push_back(t);
                    }
                }

                // Parse numeric size tokens (supports px, %, and calc())
                auto parseSizeValue = [](const std::string& tok, float& val, bool& isPercent, bool& isCalc, std::string& calcExpr) -> bool {
                    std::string t = tok;
                    isPercent = false;
                    isCalc = false;
                    calcExpr = "";

                    // Support calc() expressions
                    if (isCalcExpression(t)) {
                        val = 0.0f;
                        isCalc = true;
                        std::string expr;
                        extractCalcContent(t, expr);
                        calcExpr = expr;
                        return true;
                    }

                    if (t.size() > 2 && t.substr(t.size() - 2) == "px") {
                        t = t.substr(0, t.size() - 2);
                    } else if (t.size() > 1 && t.back() == '%') {
                        t = t.substr(0, t.size() - 1);
                        isPercent = true;
                    } else {
                        return false;
                    }
                    // Parse the numeric part
                    bool negative = false;
                    size_t idx = 0;
                    if (idx < t.size() && (t[idx] == '-' || t[idx] == '+')) {
                        negative = (t[idx] == '-');
                        idx++;
                    }
                    bool hasDigit = false;
                    float result = 0.0f;
                    while (idx < t.size() && t[idx] >= '0' && t[idx] <= '9') {
                        result = result * 10.0f + (t[idx] - '0');
                        idx++;
                        hasDigit = true;
                    }
                    if (idx < t.size() && t[idx] == '.') {
                        idx++;
                        float frac = 0.1f;
                        while (idx < t.size() && t[idx] >= '0' && t[idx] <= '9') {
                            result += (t[idx] - '0') * frac;
                            frac *= 0.1f;
                            idx++;
                            hasDigit = true;
                        }
                    }
                    if (!hasDigit || idx != t.size()) return false;
                    val = negative ? -result : result;
                    if (isPercent) val = val / 100.0f;
                    return true;
                };

                if (numericTokens.size() == 1) {
                    // Single value: implies circle
                    float val = 0.0f;
                    bool isPercent = false;
                    bool isCalc = false;
                    std::string calcExpr;
                    if (parseSizeValue(numericTokens[0], val, isPercent, isCalc, calcExpr)) {
                        result.radial.hasExplicitSize = true;
                        result.radial.radiusXIsPercent = isPercent;
                        result.radial.radiusYIsPercent = isPercent;
                        result.radial.radiusX = val;
                        result.radial.radiusY = val;
                        result.radial.radiusXIsCalc = isCalc;
                        result.radial.radiusYIsCalc = isCalc;
                        result.radial.radiusXCalcExpr = calcExpr;
                        result.radial.radiusYCalcExpr = calcExpr;
                        if (!hasCircleKeyword && !hasEllipseKeyword) {
                            result.radial.shape = RadialShape::Circle;
                        }
                    }
                } else if (numericTokens.size() >= 2) {
                    if (hasCircleKeyword) {
                        return false;  // circle does not allow two radii per W3C spec
                    }
                    // Two values: implies ellipse
                    float valX = 0.0f, valY = 0.0f;
                    bool isPercentX = false, isPercentY = false;
                    bool isCalcX = false, isCalcY = false;
                    std::string calcExprX, calcExprY;
                    if (parseSizeValue(numericTokens[0], valX, isPercentX, isCalcX, calcExprX) &&
                        parseSizeValue(numericTokens[1], valY, isPercentY, isCalcY, calcExprY)) {
                        result.radial.hasExplicitSize = true;
                        result.radial.radiusXIsPercent = isPercentX;
                        result.radial.radiusYIsPercent = isPercentY;
                        result.radial.radiusX = valX;
                        result.radial.radiusY = valY;
                        result.radial.radiusXIsCalc = isCalcX;
                        result.radial.radiusYIsCalc = isCalcY;
                        result.radial.radiusXCalcExpr = calcExprX;
                        result.radial.radiusYCalcExpr = calcExprY;
                        if (!hasCircleKeyword && !hasEllipseKeyword) {
                            result.radial.shape = RadialShape::Ellipse;
                        }
                    }
                }
            }
        }

        // Parse position
        if (!positionStr.empty()) {
            if (!parsePosition(positionStr, result.radial.centerX, result.radial.centerY,
                               result.radial.centerXIsPx, result.radial.centerYIsPx)) {
                return false;
            }
        }
    }

    // Check for color-interpolation-method as separate segment (fallback for radial)
    if (result.colorInterpolationMethod.empty() && colorStartIndex < parts.size()) {
        std::string nextPart = toLower(trim(parts[colorStartIndex]));
        if (nextPart.rfind("in ", 0) == 0) {
            std::string space = trimStr(nextPart.substr(3));
            if (isValidColorSpace(space)) {
                result.colorInterpolationMethod = space;
                colorStartIndex++;
            }
        }
    }

    // Parse color stops
    for (size_t i = colorStartIndex; i < parts.size(); ++i) {
        ColorStop stop;
        if (!parseColorStop(trim(parts[i]), stop)) {
            return false;
        }
        result.colorStops.push_back(stop);
    }

    return result.colorStops.size() >= 2;
}

// ============================================================================
// Conic Gradient Parsing
// ============================================================================

bool ColorParser::parseConicGradient(const std::string& content, GradientInfo& result) {
    std::vector<std::string> parts = splitTopLevelCommas(content);
    if (parts.empty()) {
        return false;
    }

    // Set defaults
    result.conic.startAngle = 0.0f;
    result.conic.centerX = 0.5f;
    result.conic.centerY = 0.5f;

    size_t colorStartIndex = 0;

    // Check if first part is a descriptor (contains "from" or "at")
    std::string firstPart = trim(parts[0]);
    std::string firstPartLower = toLower(firstPart);

    // Extract color-interpolation-method from the first part (can be combined with descriptor)
    size_t inPos = firstPartLower.rfind(" in ");
    if (inPos != std::string::npos) {
        std::string possibleSpace = trimStr(firstPartLower.substr(inPos + 4));
        if (isValidColorSpace(possibleSpace)) {
            result.colorInterpolationMethod = possibleSpace;
            firstPart = trim(firstPart.substr(0, inPos));
            firstPartLower = toLower(firstPart);
        }
    }

    bool isDescriptor = (firstPartLower.find("from ") != std::string::npos ||
                         firstPartLower.find("at ") != std::string::npos);

    if (isDescriptor) {
        colorStartIndex = 1;

        // Parse "from <angle>" portion and locate "at <position>" in one pass
        size_t fromPos = firstPartLower.find("from ");
        size_t resolvedAtPos = std::string::npos;

        if (fromPos != std::string::npos) {
            // Find " at " after the angle value to separate from-clause and at-clause
            size_t atAfterFrom = firstPartLower.find(" at ", fromPos + 5);
            std::string angleStr;
            if (atAfterFrom != std::string::npos) {
                angleStr = trim(firstPart.substr(fromPos + 5, atAfterFrom - (fromPos + 5)));
                resolvedAtPos = atAfterFrom + 1; // point to "at " within the string
            } else {
                angleStr = trim(firstPart.substr(fromPos + 5));
            }
            float angle = 0.0f;
            if (!parseAngle(toLower(angleStr), angle)) {
                return false; // "from" keyword present but angle is invalid
            }
            result.conic.startAngle = angle;
            if (isCalcExpression(angleStr)) {
                result.conic.startAngleIsCalc = true;
                std::string expr;
                extractCalcContent(angleStr, expr);
                result.conic.startAngleCalcExpr = expr;
            }
        }

        // Parse "at <position>" portion — use position found during from-parsing,
        // or search from the beginning if there was no "from" clause
        if (resolvedAtPos == std::string::npos) {
            size_t atPos = firstPartLower.find("at ");
            if (atPos != std::string::npos) {
                resolvedAtPos = atPos;
            }
        }
        if (resolvedAtPos != std::string::npos) {
            std::string posStr = trim(firstPart.substr(resolvedAtPos + 3));
            if (!parsePosition(posStr, result.conic.centerX, result.conic.centerY,
                               result.conic.centerXIsPx, result.conic.centerYIsPx)) {
                return false;
            }
        }
    }

    // Also handle case where firstPart was only "in <space>" (no from/at descriptor)
    if (!isDescriptor && !result.colorInterpolationMethod.empty() && firstPart.empty()) {
        colorStartIndex = 1;
    }

    // Check for color-interpolation-method as separate segment (fallback for conic)
    if (result.colorInterpolationMethod.empty() && colorStartIndex < parts.size()) {
        std::string nextPart = toLower(trim(parts[colorStartIndex]));
        if (nextPart.rfind("in ", 0) == 0) {
            std::string space = trimStr(nextPart.substr(3));
            if (isValidColorSpace(space)) {
                result.colorInterpolationMethod = space;
                colorStartIndex++;
            }
        }
    }

    // Parse color stops
    for (size_t i = colorStartIndex; i < parts.size(); ++i) {
        ColorStop stop;
        if (!parseColorStop(trim(parts[i]), stop)) {
            return false;
        }
        result.colorStops.push_back(stop);
    }

    return result.colorStops.size() >= 2;
}

// ============================================================================
// Angle Parsing
// ============================================================================

bool ColorParser::parseAngle(const std::string& str, float& angleDeg) {
    std::string s = trim(str);
    if (s.empty()) {
        return false;
    }

    // Support calc() expressions: treat as 0 and return true (deferred to rendering layer)
    if (isCalcExpression(s)) {
        angleDeg = 0.0f;
        return true;
    }

    float value = 0.0f;

    // Check for "deg" suffix
    if (s.size() > 3 && s.substr(s.size() - 3) == "deg") {
        std::string numStr = s.substr(0, s.size() - 3);
        if (!parseFloat(numStr, value)) {
            return false;
        }
        angleDeg = std::fmod(value, 360.0f);
        if (angleDeg < 0) angleDeg += 360.0f;
        return true;
    }

    // Check for "turn" suffix
    if (s.size() > 4 && s.substr(s.size() - 4) == "turn") {
        std::string numStr = s.substr(0, s.size() - 4);
        if (!parseFloat(numStr, value)) {
            return false;
        }
        angleDeg = std::fmod(value * 360.0f, 360.0f);
        if (angleDeg < 0) angleDeg += 360.0f;
        return true;
    }

    // IMPORTANT: "grad" must be checked before "rad" because "grad" ends with "rad".
    // Check for "grad" suffix
    if (s.size() > 4 && s.substr(s.size() - 4) == "grad") {
        std::string numStr = s.substr(0, s.size() - 4);
        if (!parseFloat(numStr, value)) {
            return false;
        }
        angleDeg = std::fmod(value * 0.9f, 360.0f); // 400 grad = 360 deg
        if (angleDeg < 0) angleDeg += 360.0f;
        return true;
    }

    // Check for "rad" suffix
    if (s.size() > 3 && s.substr(s.size() - 3) == "rad") {
        std::string numStr = s.substr(0, s.size() - 3);
        if (!parseFloat(numStr, value)) {
            return false;
        }
        angleDeg = std::fmod(value * (180.0f / 3.14159265358979323846f), 360.0f);
        if (angleDeg < 0) angleDeg += 360.0f;
        return true;
    }

    // W3C allows unitless zero as a valid angle (equivalent to 0deg)
    if (parseFloat(s, value) && value == 0.0f) {
        angleDeg = 0.0f;
        return true;
    }

    return false;
}

// ============================================================================
// Direction Keyword Parsing
// ============================================================================

bool ColorParser::parseDirectionKeyword(const std::string& str, float& angleDeg) {
    std::string s = trim(toLower(str));

    // Normalize multiple spaces to single space
    std::string normalized;
    normalized.reserve(s.size());
    bool prevSpace = false;
    for (char c : s) {
        if (c == ' ' || c == '\t') {
            if (!prevSpace) {
                normalized += ' ';
                prevSpace = true;
            }
        } else {
            normalized += c;
            prevSpace = false;
        }
    }
    s = normalized;

    // Must start with "to "
    if (s.find("to ") != 0) {
        return false;
    }
    std::string direction = trim(s.substr(3));

    // Single direction keywords
    if (direction == "top") { angleDeg = 0.0f; return true; }
    if (direction == "right") { angleDeg = 90.0f; return true; }
    if (direction == "bottom") { angleDeg = 180.0f; return true; }
    if (direction == "left") { angleDeg = 270.0f; return true; }

    // Two-keyword combinations
    if (direction == "top right" || direction == "right top") { angleDeg = 45.0f; return true; }
    if (direction == "bottom right" || direction == "right bottom") { angleDeg = 135.0f; return true; }
    if (direction == "bottom left" || direction == "left bottom") { angleDeg = 225.0f; return true; }
    if (direction == "top left" || direction == "left top") { angleDeg = 315.0f; return true; }

    return false;
}

// ============================================================================
// Color Parsing
// ============================================================================

bool ColorParser::parseColor(const std::string& str, uint32_t& color) {
    std::string s = trim(str);
    if (s.empty()) {
        return false;
    }

    // Support "currentcolor" keyword (CSS Color 3/4)
    std::string lower = toLower(s);
    if (lower == "currentcolor") {
        color = 0xFF000000; // default black placeholder, actual value resolved by rendering layer
        return true;
    }

    // Hex color: #RGB, #RGBA, #RRGGBB, #RRGGBBAA
    if (s[0] == '#') {
        std::string hex = s.substr(1);
        if (hex.size() == 3) {
            // #RGB -> #RRGGBB (fully opaque)
            uint32_t r = 0, g = 0, b = 0;
            r = hexCharToInt(hex[0]);
            g = hexCharToInt(hex[1]);
            b = hexCharToInt(hex[2]);
            if (r > 15 || g > 15 || b > 15) return false;
            r = r * 17; g = g * 17; b = b * 17;
            color = 0xFF000000 | (r << 16) | (g << 8) | b;
            return true;
        } else if (hex.size() == 4) {
            // #RGBA -> #RRGGBBAA
            uint32_t r = 0, g = 0, b = 0, a = 0;
            r = hexCharToInt(hex[0]);
            g = hexCharToInt(hex[1]);
            b = hexCharToInt(hex[2]);
            a = hexCharToInt(hex[3]);
            if (r > 15 || g > 15 || b > 15 || a > 15) return false;
            r = r * 17; g = g * 17; b = b * 17; a = a * 17;
            color = (a << 24) | (r << 16) | (g << 8) | b;
            return true;
        } else if (hex.size() == 6) {
            // #RRGGBB
            uint32_t val = 0;
            for (char c : hex) {
                uint32_t digit = hexCharToInt(c);
                if (digit > 15) return false;
                val = (val << 4) | digit;
            }
            color = 0xFF000000 | val;
            return true;
        } else if (hex.size() == 8) {
            // #RRGGBBAA
            uint32_t val = 0;
            for (char c : hex) {
                uint32_t digit = hexCharToInt(c);
                if (digit > 15) return false;
                val = (val << 4) | digit;
            }
            uint32_t r = (val >> 24) & 0xFF;
            uint32_t g = (val >> 16) & 0xFF;
            uint32_t b = (val >> 8) & 0xFF;
            uint32_t a = val & 0xFF;
            color = (a << 24) | (r << 16) | (g << 8) | b;
            return true;
        }
        return false;
    }

    // rgb() / rgba() - CSS Color Level 4: both accept identical syntax
    if (lower.find("rgba(") == 0 && s.back() == ')') {
        std::string inner = s.substr(5, s.size() - 6);
        return parseRgbRgbaValues(inner, color);
    }
    if (lower.find("rgb(") == 0 && s.back() == ')') {
        std::string inner = s.substr(4, s.size() - 5);
        return parseRgbRgbaValues(inner, color);
    }

    // hsl() / hsla()
    if (lower.find("hsla(") == 0 && s.back() == ')') {
        std::string inner = s.substr(5, s.size() - 6);
        return parseHslValues(inner, color);
    }
    if (lower.find("hsl(") == 0 && s.back() == ')') {
        std::string inner = s.substr(4, s.size() - 5);
        return parseHslValues(inner, color);
    }

    // hwb()
    if (lower.find("hwb(") == 0 && s.back() == ')') {
        std::string inner = s.substr(4, s.size() - 5);
        return parseHwbValues(inner, color);
    }

    // Named color
    return lookupNamedColor(lower, color);
}

// ============================================================================
// Color Stop Parsing
// ============================================================================

bool ColorParser::parseColorStop(const std::string& str, ColorStop& stop) {
    std::string s = trim(str);
    if (s.empty()) {
        return false;
    }

    stop.hasPosition = false;
    stop.position = 0.0f;
    stop.positionEnd = 0.0f;
    stop.hasPositionEnd = false;
    stop.isHint = false;
    stop.isCurrentColor = false;
    stop.positionIsCalc = false;
    stop.positionCalcExpr = "";
    stop.unit = StopUnit::Percent;
    stop.unitEnd = StopUnit::Percent;

    // Helper lambda: try to parse a token as a position with unit
    auto tryParsePosition = [](const std::string& tok, float& val, StopUnit& unit) -> bool {
        if (tok.empty()) return false;
        // Support calc() expressions
        if (isCalcExpression(tok)) {
            val = 0.0f;
            unit = StopUnit::Percent;
            return true;
        }
        // Percentage
        if (tok.back() == '%') {
            std::string numStr = tok.substr(0, tok.size() - 1);
            float v = 0.0f;
            if (parseFloatValue(numStr, v)) {
                val = v / 100.0f;
                unit = StopUnit::Percent;
                return true;
            }
            return false;
        }
        // Pixels
        if (tok.size() > 2 && tok.substr(tok.size() - 2) == "px") {
            std::string numStr = tok.substr(0, tok.size() - 2);
            float v = 0.0f;
            if (parseFloatValue(numStr, v)) {
                val = v;
                unit = StopUnit::Px;
                return true;
            }
            return false;
        }
        // Degrees
        if (tok.size() > 3 && tok.substr(tok.size() - 3) == "deg") {
            std::string numStr = tok.substr(0, tok.size() - 3);
            float v = 0.0f;
            if (parseFloatValue(numStr, v)) {
                val = v;
                unit = StopUnit::Deg;
                return true;
            }
            return false;
        }
        // IMPORTANT: "grad" must be checked before "rad" because "grad" ends with "rad".
        // Gradians
        if (tok.size() > 4 && tok.substr(tok.size() - 4) == "grad") {
            std::string numStr = tok.substr(0, tok.size() - 4);
            float v = 0.0f;
            if (parseFloatValue(numStr, v)) {
                val = v;
                unit = StopUnit::Grad;
                return true;
            }
            return false;
        }
        // Radians
        if (tok.size() > 3 && tok.substr(tok.size() - 3) == "rad") {
            std::string numStr = tok.substr(0, tok.size() - 3);
            float v = 0.0f;
            if (parseFloatValue(numStr, v)) {
                val = v;
                unit = StopUnit::Rad;
                return true;
            }
            return false;
        }
        // Turns
        if (tok.size() > 4 && tok.substr(tok.size() - 4) == "turn") {
            std::string numStr = tok.substr(0, tok.size() - 4);
            float v = 0.0f;
            if (parseFloatValue(numStr, v)) {
                val = v;
                unit = StopUnit::Turn;
                return true;
            }
            return false;
        }
        return false;
    };

    // Check if this is a color hint (position-only, no color)
    {
        float hintVal = 0.0f;
        StopUnit hintUnit = StopUnit::Percent;
        std::string sLower = toLower(s);
        if (tryParsePosition(sLower, hintVal, hintUnit)) {
            stop.isHint = true;
            stop.color = 0;
            stop.hasPosition = true;
            stop.position = hintVal;
            stop.unit = hintUnit;
            if (isCalcExpression(s)) {
                stop.positionIsCalc = true;
                std::string expr;
                extractCalcContent(s, expr);
                stop.positionCalcExpr = expr;
            }
            return true;
        }
    }

    // Tokenize by spaces, respecting parentheses nesting
    std::vector<std::string> tokens;
    int parenDepth = 0;
    size_t tokenStart = 0;
    bool inToken = false;

    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(') {
            parenDepth++;
            if (!inToken) { tokenStart = i; inToken = true; }
        } else if (s[i] == ')') {
            parenDepth--;
            if (!inToken) { tokenStart = i; inToken = true; }
        } else if (s[i] == ' ' && parenDepth == 0) {
            if (inToken) {
                tokens.push_back(s.substr(tokenStart, i - tokenStart));
                inToken = false;
            }
        } else {
            if (!inToken) { tokenStart = i; inToken = true; }
        }
    }
    if (inToken) {
        tokens.push_back(s.substr(tokenStart));
    }

    if (tokens.empty()) {
        return false;
    }

    // Try to identify color portion vs position portions from the end
    // Possible layouts:
    //   [color]                        -> no position
    //   [color] [pos1]                 -> single position
    //   [color] [pos1] [pos2]          -> double position

    // Check last two tokens for position pattern (double position)
    if (tokens.size() >= 3) {
        float pos1 = 0.0f, pos2 = 0.0f;
        StopUnit unit1 = StopUnit::Percent, unit2 = StopUnit::Percent;
        if (tryParsePosition(toLower(tokens[tokens.size() - 2]), pos1, unit1) &&
            tryParsePosition(toLower(tokens[tokens.size() - 1]), pos2, unit2)) {
            // Last two tokens are positions
            std::string colorPart;
            for (size_t i = 0; i < tokens.size() - 2; ++i) {
                if (i > 0) colorPart += ' ';
                colorPart += tokens[i];
            }
            if (parseColor(colorPart, stop.color)) {
                stop.isCurrentColor = (toLower(trim(colorPart)) == "currentcolor");
                stop.hasPosition = true;
                stop.position = pos1;
                stop.unit = unit1;
                if (isCalcExpression(tokens[tokens.size() - 2])) {
                    stop.positionIsCalc = true;
                    std::string expr;
                    extractCalcContent(tokens[tokens.size() - 2], expr);
                    stop.positionCalcExpr = expr;
                }
                stop.hasPositionEnd = true;
                stop.positionEnd = pos2;
                stop.unitEnd = unit2;
                return true;
            }
        }
    }

    // Check last token for position pattern (single position)
    if (tokens.size() >= 2) {
        float pos1 = 0.0f;
        StopUnit unit1 = StopUnit::Percent;
        if (tryParsePosition(toLower(tokens[tokens.size() - 1]), pos1, unit1)) {
            std::string colorPart;
            for (size_t i = 0; i < tokens.size() - 1; ++i) {
                if (i > 0) colorPart += ' ';
                colorPart += tokens[i];
            }
            if (parseColor(colorPart, stop.color)) {
                stop.isCurrentColor = (toLower(trim(colorPart)) == "currentcolor");
                stop.hasPosition = true;
                stop.position = pos1;
                stop.unit = unit1;
                if (isCalcExpression(tokens[tokens.size() - 1])) {
                    stop.positionIsCalc = true;
                    std::string expr;
                    extractCalcContent(tokens[tokens.size() - 1], expr);
                    stop.positionCalcExpr = expr;
                }
                return true;
            }
        }
    }

    // No position suffix - try entire string as color
    if (parseColor(s, stop.color)) {
        stop.isCurrentColor = (toLower(trim(s)) == "currentcolor");
        return true;
    }

    return false;
}

// ============================================================================
// Position Parsing
// ============================================================================

bool ColorParser::parsePosition(const std::string& str, float& x, float& y,
                                      bool& xIsPx, bool& yIsPx) {
    std::string s = trim(toLower(str));
    if (s.empty()) {
        return false;
    }

    // Defaults: center, normalized
    x = 0.5f;
    y = 0.5f;
    xIsPx = false;
    yIsPx = false;

    if (s == "center") {
        return true;
    }

    // Split by space
    std::vector<std::string> tokens;
    std::string token;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == ' ') {
            if (!token.empty()) { tokens.push_back(token); token.clear(); }
        } else {
            token += s[i];
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }

    if (tokens.empty()) {
        return false;
    }

    auto isHorizontalKeyword = [](const std::string& tok) -> bool {
        return tok == "left" || tok == "right";
    };
    auto isVerticalKeyword = [](const std::string& tok) -> bool {
        return tok == "top" || tok == "bottom";
    };

    // Parse a length-percentage, also reporting whether it was px
    auto parseLengthPercentage = [](const std::string& tok, float& val, bool& isPx) -> bool {
        if (tok.empty()) return false;
        // Support calc() expressions
        if (isCalcExpression(tok)) {
            val = 0.0f;
            isPx = false;
            return true;
        }
        if (tok.back() == '%') {
            std::string numStr = tok.substr(0, tok.size() - 1);
            float numVal = 0.0f;
            if (!parseFloatValue(numStr, numVal)) return false;
            val = numVal / 100.0f;
            isPx = false;
            return true;
        }
        if (tok.size() > 2 && tok.substr(tok.size() - 2) == "px") {
            std::string numStr = tok.substr(0, tok.size() - 2);
            float numVal = 0.0f;
            if (!parseFloatValue(numStr, numVal)) return false;
            val = numVal;
            isPx = true;
            return true;
        }
        return false;
    };

    // Parse a single position component: keyword, percentage, or px
    auto parseOnePosition = [&](const std::string& tok, float& val, bool& isPx) -> bool {
        isPx = false;
        if (tok == "left")   { val = 0.0f; return true; }
        if (tok == "center") { val = 0.5f; return true; }
        if (tok == "right")  { val = 1.0f; return true; }
        if (tok == "top")    { val = 0.0f; return true; }
        if (tok == "bottom") { val = 1.0f; return true; }
        return parseLengthPercentage(tok, val, isPx);
    };

    // Apply a keyword + offset pair: e.g. "right 10%" -> 1.0 - 0.1 = 0.9
    // For px offsets: "right 10px" stores raw 10px with isPx=true (caller handles inversion)
    auto applyKeywordOffset = [&](const std::string& keyword, const std::string& offsetTok,
                                  float& val, bool& isPx) -> bool {
        float offset = 0.0f;
        bool offsetIsPx = false;
        if (!parseLengthPercentage(offsetTok, offset, offsetIsPx)) return false;
        isPx = offsetIsPx;
        if (offsetIsPx) {
            // For px: store raw value; the rendering layer handles keyword + px offset
            val = offset;
            // Negate for right/bottom to indicate offset from opposite edge
            if (keyword == "right" || keyword == "bottom") {
                val = -offset; // negative means "from opposite edge"
            }
        } else {
            if (keyword == "left" || keyword == "top") {
                val = offset;
            } else if (keyword == "right" || keyword == "bottom") {
                val = 1.0f - offset;
            } else {
                return false;
            }
        }
        return true;
    };

    // 1-value syntax
    if (tokens.size() == 1) {
        if (tokens[0] == "top")    { x = 0.5f; y = 0.0f; return true; }
        if (tokens[0] == "bottom") { x = 0.5f; y = 1.0f; return true; }
        if (tokens[0] == "left")   { x = 0.0f; y = 0.5f; return true; }
        if (tokens[0] == "right")  { x = 1.0f; y = 0.5f; return true; }
        if (tokens[0] == "center") { x = 0.5f; y = 0.5f; return true; }
        float val = 0.0f;
        bool isPx = false;
        if (parseOnePosition(tokens[0], val, isPx)) {
            x = val; xIsPx = isPx;
            y = 0.5f; yIsPx = false;
            return true;
        }
        return false;
    }

    // 4-value syntax: e.g. "right 10% top 20%"
    if (tokens.size() == 4) {
        bool firstIsH = isHorizontalKeyword(tokens[0]);
        bool firstIsV = isVerticalKeyword(tokens[0]);
        bool thirdIsH = isHorizontalKeyword(tokens[2]);
        bool thirdIsV = isVerticalKeyword(tokens[2]);

        if (firstIsH && thirdIsV) {
            return applyKeywordOffset(tokens[0], tokens[1], x, xIsPx) &&
                   applyKeywordOffset(tokens[2], tokens[3], y, yIsPx);
        }
        if (firstIsV && thirdIsH) {
            return applyKeywordOffset(tokens[0], tokens[1], y, yIsPx) &&
                   applyKeywordOffset(tokens[2], tokens[3], x, xIsPx);
        }
        return false;
    }

    // 3-value syntax: e.g. "left 50% center", "center top 20%"
    if (tokens.size() == 3) {
        bool firstIsH = isHorizontalKeyword(tokens[0]);
        bool firstIsV = isVerticalKeyword(tokens[0]);
        float tmpVal = 0.0f;
        bool tmpIsPx = false;

        if (firstIsH && parseLengthPercentage(tokens[1], tmpVal, tmpIsPx)) {
            applyKeywordOffset(tokens[0], tokens[1], x, xIsPx);
            return parseOnePosition(tokens[2], y, yIsPx);
        }
        if (firstIsV && parseLengthPercentage(tokens[1], tmpVal, tmpIsPx)) {
            applyKeywordOffset(tokens[0], tokens[1], y, yIsPx);
            return parseOnePosition(tokens[2], x, xIsPx);
        }
        if (tokens[0] == "center") {
            bool secondIsH = isHorizontalKeyword(tokens[1]);
            bool secondIsV = isVerticalKeyword(tokens[1]);
            if (secondIsV) {
                x = 0.5f; xIsPx = false;
                return applyKeywordOffset(tokens[1], tokens[2], y, yIsPx);
            }
            if (secondIsH) {
                y = 0.5f; yIsPx = false;
                return applyKeywordOffset(tokens[1], tokens[2], x, xIsPx);
            }
        }
        return false;
    }

    // 2-value syntax
    if (tokens.size() == 2) {
        bool firstIsV = isVerticalKeyword(tokens[0]);
        if (firstIsV) {
            if (!parseOnePosition(tokens[0], y, yIsPx) || !parseOnePosition(tokens[1], x, xIsPx)) {
                return false;
            }
        } else {
            if (!parseOnePosition(tokens[0], x, xIsPx) || !parseOnePosition(tokens[1], y, yIsPx)) {
                return false;
            }
        }
        return true;
    }

    return false;
}

// ============================================================================
// Utility Methods
// ============================================================================

std::string ColorParser::trim(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && (str[start] == ' ' || str[start] == '\t' ||
           str[start] == '\n' || str[start] == '\r')) {
        start++;
    }
    size_t end = str.size();
    while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t' ||
           str[end - 1] == '\n' || str[end - 1] == '\r')) {
        end--;
    }
    return str.substr(start, end - start);
}

std::string ColorParser::toLower(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        if (c >= 'A' && c <= 'Z') {
            c = c + ('a' - 'A');
        }
    }
    return result;
}

std::vector<std::string> ColorParser::splitTopLevelCommas(const std::string& str) {
    std::vector<std::string> parts;
    int parenDepth = 0;
    size_t start = 0;

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '(') {
            parenDepth++;
        } else if (str[i] == ')') {
            parenDepth--;
        } else if (str[i] == ',' && parenDepth == 0) {
            parts.push_back(str.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.push_back(str.substr(start));
    return parts;
}

bool ColorParser::parseFloat(const std::string& str, float& value) {
    std::string s = trim(str);
    if (s.empty()) {
        return false;
    }

    // Validate that the string contains only valid float characters
    size_t idx = 0;
    if (s[idx] == '-' || s[idx] == '+') {
        idx++;
    }
    bool hasDigit = false;
    while (idx < s.size() && s[idx] >= '0' && s[idx] <= '9') {
        idx++;
        hasDigit = true;
    }
    if (idx < s.size() && s[idx] == '.') {
        idx++;
        while (idx < s.size() && s[idx] >= '0' && s[idx] <= '9') {
            idx++;
            hasDigit = true;
        }
    }
    if (!hasDigit || idx != s.size()) {
        return false;
    }

    // Use strtof for better floating-point precision
    char* endPtr = nullptr;
    value = std::strtof(s.c_str(), &endPtr);
    return endPtr == s.c_str() + s.size();
}

bool ColorParser::parseInt(const std::string& str, int& value) {
    std::string s = trim(str);
    if (s.empty()) {
        return false;
    }

    bool negative = false;
    size_t idx = 0;

    if (s[idx] == '-' || s[idx] == '+') {
        negative = (s[idx] == '-');
        idx++;
    }

    bool hasDigit = false;
    long long result = 0;

    while (idx < s.size() && s[idx] >= '0' && s[idx] <= '9') {
        result = result * 10 + (s[idx] - '0');
        if (result > INT_MAX) {
            return false;
        }
        idx++;
        hasDigit = true;
    }

    if (!hasDigit || idx != s.size()) {
        return false;
    }

    long long signedResult = negative ? -result : result;
    if (signedResult < INT_MIN || signedResult > INT_MAX) {
        return false;
    }
    value = static_cast<int>(signedResult);
    return true;
}

// ============================================================================
// Named Color Lookup
// ============================================================================

bool ColorParser::lookupNamedColor(const std::string& name, uint32_t& color) {
    // Full W3C CSS Color Module Level 4: 148 named colors (ARGB format)
    // Sorted alphabetically for binary search
    struct NamedColor {
        const char* name;
        uint32_t color;
    };

    static const NamedColor namedColors[] = {
        {"aliceblue",           0xFFF0F8FF},
        {"antiquewhite",        0xFFFAEBD7},
        {"aqua",                0xFF00FFFF},
        {"aquamarine",          0xFF7FFFD4},
        {"azure",               0xFFF0FFFF},
        {"beige",               0xFFF5F5DC},
        {"bisque",              0xFFFFE4C4},
        {"black",               0xFF000000},
        {"blanchedalmond",      0xFFFFEBCD},
        {"blue",                0xFF0000FF},
        {"blueviolet",          0xFF8A2BE2},
        {"brown",               0xFFA52A2A},
        {"burlywood",           0xFFDEB887},
        {"cadetblue",           0xFF5F9EA0},
        {"chartreuse",          0xFF7FFF00},
        {"chocolate",           0xFFD2691E},
        {"coral",               0xFFFF7F50},
        {"cornflowerblue",      0xFF6495ED},
        {"cornsilk",            0xFFFFF8DC},
        {"crimson",             0xFFDC143C},
        {"cyan",                0xFF00FFFF},
        {"darkblue",            0xFF00008B},
        {"darkcyan",            0xFF008B8B},
        {"darkgoldenrod",       0xFFB8860B},
        {"darkgray",            0xFFA9A9A9},
        {"darkgreen",           0xFF006400},
        {"darkgrey",            0xFFA9A9A9},
        {"darkkhaki",           0xFFBDB76B},
        {"darkmagenta",         0xFF8B008B},
        {"darkolivegreen",      0xFF556B2F},
        {"darkorange",          0xFFFF8C00},
        {"darkorchid",          0xFF9932CC},
        {"darkred",             0xFF8B0000},
        {"darksalmon",          0xFFE9967A},
        {"darkseagreen",        0xFF8FBC8F},
        {"darkslateblue",       0xFF483D8B},
        {"darkslategray",       0xFF2F4F4F},
        {"darkslategrey",       0xFF2F4F4F},
        {"darkturquoise",       0xFF00CED1},
        {"darkviolet",          0xFF9400D3},
        {"deeppink",            0xFFFF1493},
        {"deepskyblue",         0xFF00BFFF},
        {"dimgray",             0xFF696969},
        {"dimgrey",             0xFF696969},
        {"dodgerblue",          0xFF1E90FF},
        {"firebrick",           0xFFB22222},
        {"floralwhite",         0xFFFFFAF0},
        {"forestgreen",         0xFF228B22},
        {"fuchsia",             0xFFFF00FF},
        {"gainsboro",           0xFFDCDCDC},
        {"ghostwhite",          0xFFF8F8FF},
        {"gold",                0xFFFFD700},
        {"goldenrod",           0xFFDAA520},
        {"gray",                0xFF808080},
        {"green",               0xFF008000},
        {"greenyellow",         0xFFADFF2F},
        {"grey",                0xFF808080},
        {"honeydew",            0xFFF0FFF0},
        {"hotpink",             0xFFFF69B4},
        {"indianred",           0xFFCD5C5C},
        {"indigo",              0xFF4B0082},
        {"ivory",               0xFFFFFFF0},
        {"khaki",               0xFFF0E68C},
        {"lavender",            0xFFE6E6FA},
        {"lavenderblush",       0xFFFFF0F5},
        {"lawngreen",           0xFF7CFC00},
        {"lemonchiffon",        0xFFFFFACD},
        {"lightblue",           0xFFADD8E6},
        {"lightcoral",          0xFFF08080},
        {"lightcyan",           0xFFE0FFFF},
        {"lightgoldenrodyellow",0xFFFAFAD2},
        {"lightgray",           0xFFD3D3D3},
        {"lightgreen",          0xFF90EE90},
        {"lightgrey",           0xFFD3D3D3},
        {"lightpink",           0xFFFFB6C1},
        {"lightsalmon",         0xFFFFA07A},
        {"lightseagreen",       0xFF20B2AA},
        {"lightskyblue",        0xFF87CEFA},
        {"lightslategray",      0xFF778899},
        {"lightslategrey",      0xFF778899},
        {"lightsteelblue",      0xFFB0C4DE},
        {"lightyellow",         0xFFFFFFE0},
        {"lime",                0xFF00FF00},
        {"limegreen",           0xFF32CD32},
        {"linen",               0xFFFAF0E6},
        {"magenta",             0xFFFF00FF},
        {"maroon",              0xFF800000},
        {"mediumaquamarine",    0xFF66CDAA},
        {"mediumblue",          0xFF0000CD},
        {"mediumorchid",        0xFFBA55D3},
        {"mediumpurple",        0xFF9370DB},
        {"mediumseagreen",      0xFF3CB371},
        {"mediumslateblue",     0xFF7B68EE},
        {"mediumspringgreen",   0xFF00FA9A},
        {"mediumturquoise",     0xFF48D1CC},
        {"mediumvioletred",     0xFFC71585},
        {"midnightblue",        0xFF191970},
        {"mintcream",           0xFFF5FFFA},
        {"mistyrose",           0xFFFFE4E1},
        {"moccasin",            0xFFFFE4B5},
        {"navajowhite",         0xFFFFDEAD},
        {"navy",                0xFF000080},
        {"oldlace",             0xFFFDF5E6},
        {"olive",               0xFF808000},
        {"olivedrab",           0xFF6B8E23},
        {"orange",              0xFFFFA500},
        {"orangered",           0xFFFF4500},
        {"orchid",              0xFFDA70D6},
        {"palegoldenrod",       0xFFEEE8AA},
        {"palegreen",           0xFF98FB98},
        {"paleturquoise",       0xFFAFEEEE},
        {"palevioletred",       0xFFDB7093},
        {"papayawhip",          0xFFFFEFD5},
        {"peachpuff",           0xFFFFDAB9},
        {"peru",                0xFFCD853F},
        {"pink",                0xFFFFC0CB},
        {"plum",                0xFFDDA0DD},
        {"powderblue",          0xFFB0E0E6},
        {"purple",              0xFF800080},
        {"rebeccapurple",       0xFF663399},
        {"red",                 0xFFFF0000},
        {"rosybrown",           0xFFBC8F8F},
        {"royalblue",           0xFF4169E1},
        {"saddlebrown",         0xFF8B4513},
        {"salmon",              0xFFFA8072},
        {"sandybrown",          0xFFF4A460},
        {"seagreen",            0xFF2E8B57},
        {"seashell",            0xFFFFF5EE},
        {"sienna",              0xFFA0522D},
        {"silver",              0xFFC0C0C0},
        {"skyblue",             0xFF87CEEB},
        {"slateblue",           0xFF6A5ACD},
        {"slategray",           0xFF708090},
        {"slategrey",           0xFF708090},
        {"snow",                0xFFFFFAFA},
        {"springgreen",         0xFF00FF7F},
        {"steelblue",           0xFF4682B4},
        {"tan",                 0xFFD2B48C},
        {"teal",                0xFF008080},
        {"thistle",             0xFFD8BFD8},
        {"tomato",              0xFFFF6347},
        {"transparent",         0x00000000},
        {"turquoise",           0xFF40E0D0},
        {"violet",              0xFFEE82EE},
        {"wheat",               0xFFF5DEB3},
        {"white",               0xFFFFFFFF},
        {"whitesmoke",          0xFFF5F5F5},
        {"yellow",              0xFFFFFF00},
        {"yellowgreen",         0xFF9ACD32},
    };

    static const size_t colorCount = sizeof(namedColors) / sizeof(namedColors[0]);

    // Binary search on the alphabetically sorted color table
    size_t low = 0;
    size_t high = colorCount;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        int cmp = name.compare(namedColors[mid].name);
        if (cmp == 0) {
            color = namedColors[mid].color;
            return true;
        } else if (cmp < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }

    return false;
}

// ============================================================================
// Internal Helpers
// ============================================================================

// Helper: convert hex character to integer value (0-15), returns 16 on error
static uint32_t hexCharToInt(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 16; // invalid
}

// Helper: trim whitespace
static std::string trimStr(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' ||
           s[start] == '\n' || s[start] == '\r')) {
        start++;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' ||
           s[end - 1] == '\n' || s[end - 1] == '\r')) {
        end--;
    }
    return s.substr(start, end - start);
}

// Helper: split string by whitespace into tokens
static std::vector<std::string> splitByWhitespace(const std::string& s) {
    std::vector<std::string> tokens;
    std::string token;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == ' ' || s[i] == '\t') {
            if (!token.empty()) { tokens.push_back(token); token.clear(); }
        } else {
            token += s[i];
        }
    }
    if (!token.empty()) tokens.push_back(token);
    return tokens;
}

// Helper: parse a float value from string
static bool parseFloatValue(const std::string& str, float& value) {
    std::string s = trimStr(str);
    if (s.empty()) return false;

    size_t idx = 0;
    if (s[idx] == '-' || s[idx] == '+') idx++;
    bool hasDigit = false;
    while (idx < s.size() && s[idx] >= '0' && s[idx] <= '9') { idx++; hasDigit = true; }
    if (idx < s.size() && s[idx] == '.') {
        idx++;
        while (idx < s.size() && s[idx] >= '0' && s[idx] <= '9') { idx++; hasDigit = true; }
    }
    if (!hasDigit || idx != s.size()) return false;

    char* endPtr = nullptr;
    value = std::strtof(s.c_str(), &endPtr);
    return endPtr == s.c_str() + s.size();
}

// Helper: check if a string is a calc() expression
static bool isCalcExpression(const std::string& str) {
    std::string s = trimStr(str);
    if (s.size() < 6) return false; // minimum: "calc()"
    std::string lower = s;
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') c = c + ('a' - 'A');
    }
    return lower.find("calc(") == 0 && s.back() == ')';
}

// Helper: check if a string is a valid CSS color interpolation space
static bool isValidColorSpace(const std::string& s) {
    static const char* validSpaces[] = {
        "srgb", "srgb-linear", "display-p3", "a98-rgb", "prophoto-rgb",
        "rec2020", "lab", "oklab", "xyz", "xyz-d50", "xyz-d65",
        "hsl", "hwb", "lch", "oklch"
    };
    static const size_t count = sizeof(validSpaces) / sizeof(validSpaces[0]);

    // Tokenize by whitespace to handle multiple spaces between words
    std::vector<std::string> tokens;
    std::string token;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == ' ') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += s[i];
        }
    }
    if (!token.empty()) tokens.push_back(token);

    if (tokens.empty()) return false;

    // First token must be a valid color space
    const std::string& base = tokens[0];
    bool baseValid = false;
    for (size_t i = 0; i < count; ++i) {
        if (base == validSpaces[i]) { baseValid = true; break; }
    }
    if (!baseValid) return false;

    // Base only
    if (tokens.size() == 1) return true;

    // Must be exactly 3 tokens: <space> <modifier> hue
    if (tokens.size() == 3 && tokens[2] == "hue") {
        const std::string& mod = tokens[1];
        if (mod == "shorter" || mod == "longer" ||
            mod == "increasing" || mod == "decreasing") {
            return (base == "hsl" || base == "hwb" || base == "lch" || base == "oklch");
        }
    }

    return false;
}

// Helper: extract the content inside calc(...)
static bool extractCalcContent(const std::string& str, std::string& expr) {
    std::string s = trimStr(str);
    if (!isCalcExpression(s)) return false;
    // Skip "calc(" prefix (5 chars) and ")" suffix (1 char)
    expr = s.substr(5, s.size() - 6);
    return true;
}

// Helper: parse color component - integer (0-255) or percentage (0%-100% -> 0-255)
static bool parseColorComponent(const std::string& str, int& value) {
    std::string s = trimStr(str);
    if (s.empty()) return false;
    // CSS Color Level 4: "none" means channel is absent, treated as 0
    if (s == "none") {
        value = 0;
        return true;
    }
    if (s.back() == '%') {
        float pct = 0.0f;
        if (!parseFloatValue(s.substr(0, s.size() - 1), pct)) return false;
        pct = std::max(0.0f, std::min(100.0f, pct));
        value = static_cast<int>(pct * 255.0f / 100.0f + 0.5f);
        return true;
    }
    float fval = 0.0f;
    if (!parseFloatValue(s, fval)) return false;
    value = static_cast<int>(std::max(0.0f, std::min(255.0f, fval)) + 0.5f);
    return true;
}

// Helper: parse alpha value - float (0.0-1.0) or percentage (0%-100% -> 0.0-1.0)
static bool parseAlphaValue(const std::string& str, float& alpha) {
    std::string s = trimStr(str);
    if (s.empty()) return false;
    // CSS Color Level 4: "none" in alpha channel treated as fully transparent (0)
    if (s == "none") {
        alpha = 0.0f;
        return true;
    }
    if (s.back() == '%') {
        float pct = 0.0f;
        if (!parseFloatValue(s.substr(0, s.size() - 1), pct)) return false;
        alpha = std::max(0.0f, std::min(100.0f, pct)) / 100.0f;
        return true;
    }
    if (!parseFloatValue(s, alpha)) return false;
    alpha = std::max(0.0f, std::min(1.0f, alpha));
    return true;
}

// Helper: parse hue value with optional unit (bare number = degrees)
static bool parseHueValue(const std::string& str, float& degrees) {
    std::string s = trimStr(str);
    if (s.empty()) return false;
    // CSS Color Level 4: "none" for hue treated as 0deg
    if (s == "none") {
        degrees = 0.0f;
        return true;
    }
    std::string lower = s;
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') c = c + ('a' - 'A');
    }
    float value = 0.0f;
    if (lower.size() > 3 && lower.substr(lower.size() - 3) == "deg") {
        if (!parseFloatValue(s.substr(0, s.size() - 3), value)) return false;
        degrees = value; return true;
    }
    if (lower.size() > 4 && lower.substr(lower.size() - 4) == "turn") {
        if (!parseFloatValue(s.substr(0, s.size() - 4), value)) return false;
        degrees = value * 360.0f; return true;
    }
    // IMPORTANT: "grad" must be checked before "rad" because "grad" ends with "rad".
    if (lower.size() > 4 && lower.substr(lower.size() - 4) == "grad") {
        if (!parseFloatValue(s.substr(0, s.size() - 4), value)) return false;
        degrees = value * 0.9f; return true;
    }
    if (lower.size() > 3 && lower.substr(lower.size() - 3) == "rad") {
        if (!parseFloatValue(s.substr(0, s.size() - 3), value)) return false;
        degrees = value * (180.0f / 3.14159265358979323846f); return true;
    }
    // Bare number = degrees
    if (!parseFloatValue(s, value)) return false;
    degrees = value;
    return true;
}

// Helper: parse percentage value (e.g. "50%" -> 0.5)
static bool parsePercentageValue(const std::string& str, float& normalized) {
    std::string s = trimStr(str);
    // CSS Color Level 4: "none" treated as 0
    if (s == "none") {
        normalized = 0.0f;
        return true;
    }
    if (s.empty() || s.back() != '%') return false;
    float pct = 0.0f;
    if (!parseFloatValue(s.substr(0, s.size() - 1), pct)) return false;
    normalized = std::max(0.0f, std::min(100.0f, pct)) / 100.0f;
    return true;
}

// Unified rgb()/rgba() parser supporting CSS Color Level 4 syntax
static bool parseRgbRgbaValues(const std::string& inner, uint32_t& color) {
    std::string trimmed = trimStr(inner);
    if (trimmed.empty()) return false;

    bool hasComma = (trimmed.find(',') != std::string::npos);
    int r = 0, g = 0, b = 0;
    float a = 1.0f;

    // Helper to parse 3 color components (CSS Color Level 4 allows mixed number/percentage)
    auto parseThreeComponents = [&](const std::string& c0, const std::string& c1,
                                    const std::string& c2) -> bool {
        return parseColorComponent(c0, r) &&
               parseColorComponent(c1, g) &&
               parseColorComponent(c2, b);
    };

    if (hasComma) {
        // Legacy comma-separated: rgb(r, g, b) or rgba(r, g, b, a)
        std::vector<std::string> parts;
        size_t start = 0;
        for (size_t i = 0; i < trimmed.size(); ++i) {
            if (trimmed[i] == ',') {
                parts.push_back(trimmed.substr(start, i - start));
                start = i + 1;
            }
        }
        parts.push_back(trimmed.substr(start));

        if (parts.size() == 3) {
            if (!parseThreeComponents(parts[0], parts[1], parts[2])) return false;
        } else if (parts.size() == 4) {
            if (!parseThreeComponents(parts[0], parts[1], parts[2]) ||
                !parseAlphaValue(parts[3], a)) return false;
        } else {
            return false;
        }
    } else {
        // New space-separated: rgb(r g b) or rgb(r g b / a)
        size_t slashPos = trimmed.find('/');
        std::string colorPart, alphaPart;
        if (slashPos != std::string::npos) {
            colorPart = trimStr(trimmed.substr(0, slashPos));
            alphaPart = trimStr(trimmed.substr(slashPos + 1));
        } else {
            colorPart = trimmed;
        }
        std::vector<std::string> tokens = splitByWhitespace(colorPart);
        if (tokens.size() != 3) return false;
        if (!parseThreeComponents(tokens[0], tokens[1], tokens[2])) return false;
        if (!alphaPart.empty()) {
            if (!parseAlphaValue(alphaPart, a)) return false;
        }
    }

    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));
    a = std::max(0.0f, std::min(1.0f, a));

    uint32_t alpha = static_cast<uint32_t>(a * 255.0f + 0.5f);
    color = (alpha << 24) | (static_cast<uint32_t>(r) << 16) |
            (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    return true;
}

// ============================================================================
// HSL/HWB Conversion and Parsing
// ============================================================================

float ColorParser::hueToRgbHelper(float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

void ColorParser::hslToRgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b) {
    float hNorm = std::fmod(h, 360.0f);
    if (hNorm < 0.0f) hNorm += 360.0f;
    hNorm /= 360.0f;
    s = std::max(0.0f, std::min(1.0f, s));
    l = std::max(0.0f, std::min(1.0f, l));

    float rf, gf, bf;
    if (s == 0.0f) {
        rf = gf = bf = l;
    } else {
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        rf = hueToRgbHelper(p, q, hNorm + 1.0f / 3.0f);
        gf = hueToRgbHelper(p, q, hNorm);
        bf = hueToRgbHelper(p, q, hNorm - 1.0f / 3.0f);
    }

    r = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, rf * 255.0f + 0.5f)));
    g = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, gf * 255.0f + 0.5f)));
    b = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, bf * 255.0f + 0.5f)));
}

void ColorParser::hwbToRgb(float h, float w, float bk, uint8_t& r, uint8_t& g, uint8_t& b) {
    w = std::max(0.0f, std::min(1.0f, w));
    bk = std::max(0.0f, std::min(1.0f, bk));

    if (w + bk >= 1.0f) {
        uint8_t gray = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, w / (w + bk) * 255.0f + 0.5f)));
        r = g = b = gray;
        return;
    }

    // Get the pure hue color from HSL with S=100%, L=50%
    uint8_t hr, hg, hb;
    hslToRgb(h, 1.0f, 0.5f, hr, hg, hb);

    // Apply whiteness and blackness
    float factor = 1.0f - w - bk;
    r = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, (hr / 255.0f * factor + w) * 255.0f + 0.5f)));
    g = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, (hg / 255.0f * factor + w) * 255.0f + 0.5f)));
    b = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, (hb / 255.0f * factor + w) * 255.0f + 0.5f)));
}

bool ColorParser::parseHslValues(const std::string& inner, uint32_t& color) {
    std::string trimmed = trimStr(inner);
    if (trimmed.empty()) return false;

    bool hasComma = (trimmed.find(',') != std::string::npos);
    float h = 0.0f, s = 0.0f, l = 0.0f, a = 1.0f;

    if (hasComma) {
        // Legacy comma-separated: hsl(h, s%, l%) or hsla(h, s%, l%, a)
        std::vector<std::string> parts;
        size_t start = 0;
        for (size_t i = 0; i < trimmed.size(); ++i) {
            if (trimmed[i] == ',') {
                parts.push_back(trimmed.substr(start, i - start));
                start = i + 1;
            }
        }
        parts.push_back(trimmed.substr(start));

        if (parts.size() != 3 && parts.size() != 4) return false;
        if (!parseHueValue(trimStr(parts[0]), h)) return false;
        if (!parsePercentageValue(parts[1], s)) return false;
        if (!parsePercentageValue(parts[2], l)) return false;
        if (parts.size() == 4) {
            if (!parseAlphaValue(parts[3], a)) return false;
        }
    } else {
        // New space-separated: hsl(h s% l%) or hsl(h s% l% / a)
        size_t slashPos = trimmed.find('/');
        std::string colorPart, alphaPart;
        if (slashPos != std::string::npos) {
            colorPart = trimStr(trimmed.substr(0, slashPos));
            alphaPart = trimStr(trimmed.substr(slashPos + 1));
        } else {
            colorPart = trimmed;
        }
        std::vector<std::string> tokens = splitByWhitespace(colorPart);
        if (tokens.size() != 3) return false;
        if (!parseHueValue(tokens[0], h)) return false;
        if (!parsePercentageValue(tokens[1], s)) return false;
        if (!parsePercentageValue(tokens[2], l)) return false;
        if (!alphaPart.empty()) {
            if (!parseAlphaValue(alphaPart, a)) return false;
        }
    }

    uint8_t r, g, b;
    hslToRgb(h, s, l, r, g, b);

    uint32_t alpha = static_cast<uint32_t>(a * 255.0f + 0.5f);
    color = (alpha << 24) | (static_cast<uint32_t>(r) << 16) |
            (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    return true;
}

bool ColorParser::parseHwbValues(const std::string& inner, uint32_t& color) {
    std::string trimmed = trimStr(inner);
    if (trimmed.empty()) return false;

    float h = 0.0f, w = 0.0f, bk = 0.0f, a = 1.0f;

    // HWB uses space-separated syntax: hwb(h w% b%) or hwb(h w% b% / a)
    size_t slashPos = trimmed.find('/');
    std::string colorPart, alphaPart;
    if (slashPos != std::string::npos) {
        colorPart = trimStr(trimmed.substr(0, slashPos));
        alphaPart = trimStr(trimmed.substr(slashPos + 1));
    } else {
        colorPart = trimmed;
    }

    std::vector<std::string> tokens = splitByWhitespace(colorPart);
    if (tokens.size() != 3) return false;
    if (!parseHueValue(tokens[0], h)) return false;
    if (!parsePercentageValue(tokens[1], w)) return false;
    if (!parsePercentageValue(tokens[2], bk)) return false;
    if (!alphaPart.empty()) {
        if (!parseAlphaValue(alphaPart, a)) return false;
    }

    uint8_t r, g, b;
    hwbToRgb(h, w, bk, r, g, b);

    uint32_t alpha = static_cast<uint32_t>(a * 255.0f + 0.5f);
    color = (alpha << 24) | (static_cast<uint32_t>(r) << 16) |
            (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    return true;
}

}  // namespace agenui
