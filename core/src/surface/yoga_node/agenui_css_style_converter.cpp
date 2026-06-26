#include "agenui_css_style_converter.h"
#include "agenui_component_snapshot_wrapper.h"
#include "agenui_yoga_internal_parse.h"
#include <cctype>
#include <sstream>
#include <vector>

namespace agenui {

namespace {

std::string trimWhitespace(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

bool parseAspectRatioValue(const std::string& rawValue, float& aspectRatio) {
    const std::string value = trimWhitespace(rawValue);
    if (value.empty()) {
        return false;
    }

    const size_t slashPos = value.find('/');
    if (slashPos == std::string::npos) {
        bool ok = false;
        aspectRatio = yoga_internal::parseCssFloat(value, &ok);
        return ok && aspectRatio > 0.0f;
    }

    const std::string numeratorStr = trimWhitespace(value.substr(0, slashPos));
    const std::string denominatorStr = trimWhitespace(value.substr(slashPos + 1));
    if (numeratorStr.empty() || denominatorStr.empty()) {
        return false;
    }

    bool okN = false, okD = false;
    const float numerator   = yoga_internal::parseCssFloat(numeratorStr,   &okN);
    const float denominator = yoga_internal::parseCssFloat(denominatorStr, &okD);
    if (!okN || !okD || numerator <= 0.0f || denominator <= 0.0f) {
        return false;
    }

    aspectRatio = numerator / denominator;
    return true;
}

// Forward to the noexcept yoga_internal parser. Kept as a thin alias so
// existing call sites in this file continue to compile unchanged.
inline float parseCssFloat(const std::string& token, bool* ok = nullptr) noexcept {
    return yoga_internal::parseCssFloat(token, ok);
}

}  // namespace

bool CSSStyleConverter::isRichText(const std::string& text) {
    // Simple check: whether the text contains HTML tags
    // Using find instead of regex for better performance and C++11 compatibility
    if (text.find('<') != std::string::npos && text.find('>') != std::string::npos) {
        return true;
    }

    // Optional: check Markdown syntax
    // Check bold: **text** or __text__
    if (text.find("**") != std::string::npos || text.find("__") != std::string::npos) {
        return true;
    }
    
    // Check italic: *text* or _text_ (needs more precise matching to avoid false positives)
    size_t asteriskPos = text.find('*');
    if (asteriskPos != std::string::npos && asteriskPos + 1 < text.size()) {
        // Check for paired *
        size_t nextAsterisk = text.find('*', asteriskPos + 1);
        if (nextAsterisk != std::string::npos) {
            return true;
        }
    }
    
    size_t underscorePos = text.find('_');
    if (underscorePos != std::string::npos && underscorePos + 1 < text.size()) {
        // Check for paired _
        size_t nextUnderscore = text.find('_', underscorePos + 1);
        if (nextUnderscore != std::string::npos) {
            return true;
        }
    }

    return false;
}

void CSSStyleConverter::convertToYoga(ILayoutDataWrapper& wrapper, YGNodeRef yogaNode, bool clearAfterConvert) {
    if (!yogaNode) {
        return;
    }
    
    bool hasMeasureFunc = YGNodeHasMeasureFunc(yogaNode);
    
    // Process width attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kWidth);
        if (value.isValid()) {
            applyWidth(yogaNode, value, hasMeasureFunc);
        } 
        else if (hasMeasureFunc) {
            // No explicit width and node has measure function -> set to auto
            // Let measure function determine width, avoid Yoga treating as Undefined
            YGNodeStyleSetWidthAuto(yogaNode);
        }
    }
    
    // Process height attribute (mirror the width path so axes are symmetric)
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kHeight);
        if (value.isValid()) {
            applyHeight(yogaNode, value, hasMeasureFunc);
        }
    }
    
    // Process flex-direction attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kFlexDirection);
        if (value.isValid()) {
            applyFlexDirection(yogaNode, value);
        }
    }
    
    // Process justify-content attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kJustifyContent);
        if (value.isValid()) {
            applyJustifyContent(yogaNode, value);
        }
    }
    
    // Process align-items attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kAlignItems);
        if (value.isValid()) {
            applyAlignItems(yogaNode, value);
        }
    }
    
    // Process align-self attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kAlignSelf);
        if (value.isValid()) {
            applyAlignSelf(yogaNode, value);
        }
    }

    // Process flex-grow attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kFlexGrow);
        if (value.isValid()) {
            applyFlexGrow(yogaNode, value);
        }
    }
    
    // Process flex-shrink attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kFlexShrink);
        if (value.isValid()) {
            applyFlexShrink(yogaNode, value);
        }
    }
    
    // Process padding attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPadding);
        if (value.isValid()) {
            applyPadding(yogaNode, value);
        }
    }
    
    // Process padding-left/right/top/bottom attributes (override padding shorthand)
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingLeft);
        if (value.isValid()) {
            applyPaddingLeft(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingRight);
        if (value.isValid()) {
            applyPaddingRight(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingTop);
        if (value.isValid()) {
            applyPaddingTop(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingBottom);
        if (value.isValid()) {
            applyPaddingBottom(yogaNode, value);
        }
    }
    
    // Process margin attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMargin);
        if (value.isValid()) {
            applyMargin(yogaNode, value);
        }
    }
    
    // Process margin-left/right/top/bottom attributes (override margin shorthand)
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginLeft);
        if (value.isValid()) {
            applyMarginLeft(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginRight);
        if (value.isValid()) {
            applyMarginRight(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginTop);
        if (value.isValid()) {
            applyMarginTop(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginBottom);
        if (value.isValid()) {
            applyMarginBottom(yogaNode, value);
        }
    }
    
    
    // Process min-width attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMinWidth);
        if (value.isValid()) {
            applyMinWidth(yogaNode, value);
        }
    }
    
    // Process max-width attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMaxWidth);
        if (value.isValid()) {
            applyMaxWidth(yogaNode, value);
        }
    }
    
    // Process min-height attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMinHeight);
        if (value.isValid()) {
            applyMinHeight(yogaNode, value);
        }
    }
    
    // Process max-height attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMaxHeight);
        if (value.isValid()) {
            applyMaxHeight(yogaNode, value);
        }
    }
    
    // Process flex-wrap attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kFlexWrap);
        if (value.isValid()) {
            applyFlexWrap(yogaNode, value);
        }
    }
    
    // Process align-content attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kAlignContent);
        if (value.isValid()) {
            applyAlignContent(yogaNode, value);
        }
    }
    
    // Process flex-basis attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kFlexBasis);
        if (value.isValid()) {
            applyFlexBasis(yogaNode, value);
        }
    }
    
    // Process border attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kBorder);
        if (value.isValid()) {
            applyBorder(yogaNode, value);
        }
    }
    
    // Process border-width attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kBorderWidth);
        if (value.isValid()) {
            applyBorderWidth(yogaNode, value);
        }
    }

    // Process gap attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kGap);
        if (value.isValid()) {
            applyGap(yogaNode, value);
        }
    }

    // Process position attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPosition);
        if (value.isValid()) {
            applyPosition(yogaNode, value);
        }
    }
    
    // Process top attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kTop);
        if (value.isValid()) {
            applyTop(yogaNode, value);
        }
    }
    
    // Process right attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kRight);
        if (value.isValid()) {
            applyRight(yogaNode, value);
        }
    }
    
    // Process bottom attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kBottom);
        if (value.isValid()) {
            applyBottom(yogaNode, value);
        }
    }
    
    // Process left attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kLeft);
        if (value.isValid()) {
            applyLeft(yogaNode, value);
        }
    }
    
    // Process display attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kDisplay);
        if (value.isValid()) {
            applyDisplay(yogaNode, value);
        }
    }
    
    // Process overflow attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kOverflow);
        if (value.isValid()) {
            applyOverflow(yogaNode, value);
        }
    }
    
    // Process direction attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kDirection);
        if (value.isValid()) {
            applyDirection(yogaNode, value);
        }
    }
    
    // Process aspect-ratio attribute
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kAspectRatio);
        if (value.isValid()) {
            applyAspectRatio(yogaNode, value);
        }
    }
    
    // Process CSS Logical Properties
    // Inset logical properties
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kInsetInlineStart);
        if (value.isValid()) {
            applyInsetInlineStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kInsetInlineEnd);
        if (value.isValid()) {
            applyInsetInlineEnd(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kInsetBlockStart);
        if (value.isValid()) {
            applyInsetBlockStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kInsetBlockEnd);
        if (value.isValid()) {
            applyInsetBlockEnd(yogaNode, value);
        }
    }
    
    // Margin logical properties
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginInlineStart);
        if (value.isValid()) {
            applyMarginInlineStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginInlineEnd);
        if (value.isValid()) {
            applyMarginInlineEnd(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginBlockStart);
        if (value.isValid()) {
            applyMarginBlockStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginBlockEnd);
        if (value.isValid()) {
            applyMarginBlockEnd(yogaNode, value);
        }
    }
    
    // Padding logical properties
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingInlineStart);
        if (value.isValid()) {
            applyPaddingInlineStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingInlineEnd);
        if (value.isValid()) {
            applyPaddingInlineEnd(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingBlockStart);
        if (value.isValid()) {
            applyPaddingBlockStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingBlockEnd);
        if (value.isValid()) {
            applyPaddingBlockEnd(yogaNode, value);
        }
    }
    

    // camelCase alias processing (equivalent to kebab-case)
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kJustifyContentCC);
        if (value.isValid()) {
            applyJustifyContent(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kAlignItemsCC);
        if (value.isValid()) {
            applyAlignItems(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kFlexWrapCC);
        if (value.isValid()) {
            applyFlexWrap(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kAlignContentCC);
        if (value.isValid()) {
            applyAlignContent(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kAlignSelfCC);
        if (value.isValid()) {
            applyAlignSelf(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kFlexGrowCC);
        if (value.isValid()) {
            applyFlexGrow(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kFlexShrinkCC);
        if (value.isValid()) {
            applyFlexShrink(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kFlexBasisCC);
        if (value.isValid()) {
            applyFlexBasis(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kBorderWidthCC);
        if (value.isValid()) {
            applyBorderWidth(yogaNode, value);
        }
    }
    // Inset logical properties camelCase
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kInsetInlineStartCC);
        if (value.isValid()) {
            applyInsetInlineStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kInsetInlineEndCC);
        if (value.isValid()) {
            applyInsetInlineEnd(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kInsetBlockStartCC);
        if (value.isValid()) {
            applyInsetBlockStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kInsetBlockEndCC);
        if (value.isValid()) {
            applyInsetBlockEnd(yogaNode, value);
        }
    }
    // Margin logical properties camelCase
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginInlineStartCC);
        if (value.isValid()) {
            applyMarginInlineStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginInlineEndCC);
        if (value.isValid()) {
            applyMarginInlineEnd(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginBlockStartCC);
        if (value.isValid()) {
            applyMarginBlockStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kMarginBlockEndCC);
        if (value.isValid()) {
            applyMarginBlockEnd(yogaNode, value);
        }
    }
    // Padding logical properties camelCase
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingInlineStartCC);
        if (value.isValid()) {
            applyPaddingInlineStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingInlineEndCC);
        if (value.isValid()) {
            applyPaddingInlineEnd(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingBlockStartCC);
        if (value.isValid()) {
            applyPaddingBlockStart(yogaNode, value);
        }
    }
    {
        YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kPaddingBlockEndCC);
        if (value.isValid()) {
            applyPaddingBlockEnd(yogaNode, value);
        }
    }

    // Clear converted CSS properties if requested
    if (clearAfterConvert) {
        wrapper.clearStyle(CSSPropertyNames::kWidth);
        wrapper.clearStyle(CSSPropertyNames::kHeight);
        wrapper.clearStyle(CSSPropertyNames::kMinWidth);
        wrapper.clearStyle(CSSPropertyNames::kMaxWidth);
        wrapper.clearStyle(CSSPropertyNames::kMinHeight);
        wrapper.clearStyle(CSSPropertyNames::kMaxHeight);
        wrapper.clearStyle(CSSPropertyNames::kFlexDirection);
        wrapper.clearStyle(CSSPropertyNames::kFlexWrap);
        wrapper.clearStyle(CSSPropertyNames::kJustifyContent);
        wrapper.clearStyle(CSSPropertyNames::kAlignItems);
        wrapper.clearStyle(CSSPropertyNames::kAlignSelf);
        wrapper.clearStyle(CSSPropertyNames::kAlignContent);
        wrapper.clearStyle(CSSPropertyNames::kFlexGrow);
        wrapper.clearStyle(CSSPropertyNames::kFlexShrink);
        wrapper.clearStyle(CSSPropertyNames::kFlexBasis);

        wrapper.clearStyle(CSSPropertyNames::kMargin);
        wrapper.clearStyle(CSSPropertyNames::kBorder);
        wrapper.clearStyle(CSSPropertyNames::kGap);
        wrapper.clearStyle(CSSPropertyNames::kPosition);
        wrapper.clearStyle(CSSPropertyNames::kTop);
        wrapper.clearStyle(CSSPropertyNames::kRight);
        wrapper.clearStyle(CSSPropertyNames::kBottom);
        wrapper.clearStyle(CSSPropertyNames::kLeft);
        wrapper.clearStyle(CSSPropertyNames::kDisplay);
        wrapper.clearStyle(CSSPropertyNames::kDirection);
        wrapper.clearStyle(CSSPropertyNames::kAspectRatio);
        
        // Clear CSS logical properties
        wrapper.clearStyle(CSSPropertyNames::kInsetInlineStart);
        wrapper.clearStyle(CSSPropertyNames::kInsetInlineEnd);
        wrapper.clearStyle(CSSPropertyNames::kInsetBlockStart);
        wrapper.clearStyle(CSSPropertyNames::kInsetBlockEnd);
        wrapper.clearStyle(CSSPropertyNames::kMarginInlineStart);
        wrapper.clearStyle(CSSPropertyNames::kMarginInlineEnd);
        wrapper.clearStyle(CSSPropertyNames::kMarginBlockStart);
        wrapper.clearStyle(CSSPropertyNames::kMarginBlockEnd);

        // camelCase aliases
        wrapper.clearStyle(CSSPropertyNames::kJustifyContentCC);
        wrapper.clearStyle(CSSPropertyNames::kAlignItemsCC);
        wrapper.clearStyle(CSSPropertyNames::kFlexWrapCC);
        wrapper.clearStyle(CSSPropertyNames::kAlignContentCC);
        wrapper.clearStyle(CSSPropertyNames::kAlignSelfCC);
        wrapper.clearStyle(CSSPropertyNames::kFlexGrowCC);
        wrapper.clearStyle(CSSPropertyNames::kFlexShrinkCC);
        wrapper.clearStyle(CSSPropertyNames::kFlexBasisCC);
        wrapper.clearStyle(CSSPropertyNames::kBorderWidthCC);
        wrapper.clearStyle(CSSPropertyNames::kInsetInlineStartCC);
        wrapper.clearStyle(CSSPropertyNames::kInsetInlineEndCC);
        wrapper.clearStyle(CSSPropertyNames::kInsetBlockStartCC);
        wrapper.clearStyle(CSSPropertyNames::kInsetBlockEndCC);
        wrapper.clearStyle(CSSPropertyNames::kMarginInlineStartCC);
        wrapper.clearStyle(CSSPropertyNames::kMarginInlineEndCC);
        wrapper.clearStyle(CSSPropertyNames::kMarginBlockStartCC);
        wrapper.clearStyle(CSSPropertyNames::kMarginBlockEndCC);

    }
}

void CSSStyleConverter::applyWidth(YGNodeRef yogaNode, YogaValue value, bool hasMeasureFunc) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetWidth(yogaNode, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "auto") {
        YGNodeStyleSetWidthAuto(yogaNode);
    } else if (!actualValue.empty() && actualValue.back() == '%') {
        float percent = yoga_internal::parseCssFloat(actualValue.substr(0, actualValue.size() - 1));
        YGNodeStyleSetWidthPercent(yogaNode, percent);
    } else if (!actualValue.empty()) {
        std::string numericPart = actualValue;
        size_t unitPos = actualValue.find_first_not_of("0123456789.-");
        if (unitPos != std::string::npos) {
            numericPart = actualValue.substr(0, unitPos);
        }
        float width = yoga_internal::parseCssFloat(numericPart);
        YGNodeStyleSetWidth(yogaNode, width);
    } else if (hasMeasureFunc) {
        YGNodeStyleSetWidthAuto(yogaNode);
    }
}

void CSSStyleConverter::applyHeight(YGNodeRef yogaNode, YogaValue value, bool hasMeasureFunc) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetHeight(yogaNode, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "auto") {
        YGNodeStyleSetHeightAuto(yogaNode);
    } else if (!actualValue.empty() && actualValue.back() == '%') {
        float percent = yoga_internal::parseCssFloat(actualValue.substr(0, actualValue.size() - 1));
        YGNodeStyleSetHeightPercent(yogaNode, percent);
    } else if (!actualValue.empty()) {
        std::string numericPart = actualValue;
        size_t unitPos = actualValue.find_first_not_of("0123456789.-");
        if (unitPos != std::string::npos) {
            numericPart = actualValue.substr(0, unitPos);
        }
        float height = yoga_internal::parseCssFloat(numericPart);
        YGNodeStyleSetHeight(yogaNode, height);
    } else if (hasMeasureFunc) {
        YGNodeStyleSetHeightAuto(yogaNode);
    }
}

void CSSStyleConverter::applyFlexDirection(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "row") {
        YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionRow);
    } else if (actualValue == "row-reverse") {
        YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionRowReverse);
    } else if (actualValue == "column") {
        YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionColumn);
    } else if (actualValue == "column-reverse") {
        YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionColumnReverse);
    }
}

void CSSStyleConverter::applyJustifyContent(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "flex-start") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifyFlexStart);
    } else if (actualValue == "center") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifyCenter);
    } else if (actualValue == "flex-end") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifyFlexEnd);
    } else if (actualValue == "space-between") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifySpaceBetween);
    } else if (actualValue == "space-around") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifySpaceAround);
    } else if (actualValue == "space-evenly") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifySpaceEvenly);
    }
}

void CSSStyleConverter::applyAlignItems(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "flex-start") {
        YGNodeStyleSetAlignItems(yogaNode, YGAlignFlexStart);
    } else if (actualValue == "center") {
        YGNodeStyleSetAlignItems(yogaNode, YGAlignCenter);
    } else if (actualValue == "flex-end") {
        YGNodeStyleSetAlignItems(yogaNode, YGAlignFlexEnd);
    } else if (actualValue == "stretch") {
        YGNodeStyleSetAlignItems(yogaNode, YGAlignStretch);
    } else if (actualValue == "baseline") {
        YGNodeStyleSetAlignItems(yogaNode, YGAlignBaseline);
    }
}

void CSSStyleConverter::applyAlignSelf(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "auto") {
        YGNodeStyleSetAlignSelf(yogaNode, YGAlignAuto);
    } else if (actualValue == "flex-start") {
        YGNodeStyleSetAlignSelf(yogaNode, YGAlignFlexStart);
    } else if (actualValue == "center") {
        YGNodeStyleSetAlignSelf(yogaNode, YGAlignCenter);
    } else if (actualValue == "flex-end") {
        YGNodeStyleSetAlignSelf(yogaNode, YGAlignFlexEnd);
    } else if (actualValue == "stretch") {
        YGNodeStyleSetAlignSelf(yogaNode, YGAlignStretch);
    } else if (actualValue == "baseline") {
        YGNodeStyleSetAlignSelf(yogaNode, YGAlignBaseline);
    }
}

void CSSStyleConverter::applyFlex(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetFlex(yogaNode, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (!actualValue.empty()) {
        float flex = parseCssFloat(actualValue);
        YGNodeStyleSetFlex(yogaNode, flex);
    }
}

void CSSStyleConverter::applyFlexGrow(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetFlexGrow(yogaNode, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (!actualValue.empty()) {
        float flexGrow = parseCssFloat(actualValue);
        YGNodeStyleSetFlexGrow(yogaNode, flexGrow);
    }
}

void CSSStyleConverter::applyFlexShrink(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetFlexShrink(yogaNode, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (!actualValue.empty()) {
        float flexShrink = parseCssFloat(actualValue);
        YGNodeStyleSetFlexShrink(yogaNode, flexShrink);
    }
}

void CSSStyleConverter::applyPadding(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        float val = value.asFloat();
        YGNodeStyleSetPadding(yogaNode, YGEdgeAll, val);
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (actualValue.empty()) {
        return;
    }
    
    std::vector<float> values;
    std::istringstream iss(actualValue);
    std::string token;
    while (iss >> token) {
        values.push_back(parseCssFloat(token));
    }
    
    if (values.size() == 1) {
        YGNodeStyleSetPadding(yogaNode, YGEdgeAll, values[0]);
    } else if (values.size() == 2) {
        YGNodeStyleSetPadding(yogaNode, YGEdgeVertical, values[0]);
        YGNodeStyleSetPadding(yogaNode, YGEdgeHorizontal, values[1]);
    } else if (values.size() == 3) {
        // CSS 3-value shorthand: top | left-right | bottom
        YGNodeStyleSetPadding(yogaNode, YGEdgeTop, values[0]);
        YGNodeStyleSetPadding(yogaNode, YGEdgeHorizontal, values[1]);
        YGNodeStyleSetPadding(yogaNode, YGEdgeBottom, values[2]);
    } else if (values.size() >= 4) {
        YGNodeStyleSetPadding(yogaNode, YGEdgeTop, values[0]);
        YGNodeStyleSetPadding(yogaNode, YGEdgeRight, values[1]);
        YGNodeStyleSetPadding(yogaNode, YGEdgeBottom, values[2]);
        YGNodeStyleSetPadding(yogaNode, YGEdgeLeft, values[3]);
    }
}

void CSSStyleConverter::applyMargin(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        float val = value.asFloat();
        YGNodeStyleSetMargin(yogaNode, YGEdgeAll, val);
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (actualValue.empty()) {
        return;
    }
    
    std::vector<float> values;
    std::istringstream iss(actualValue);
    std::string token;
    while (iss >> token) {
        values.push_back(parseCssFloat(token));
    }
    
    if (values.size() == 1) {
        YGNodeStyleSetMargin(yogaNode, YGEdgeAll, values[0]);
    } else if (values.size() == 2) {
        YGNodeStyleSetMargin(yogaNode, YGEdgeVertical, values[0]);
        YGNodeStyleSetMargin(yogaNode, YGEdgeHorizontal, values[1]);
    } else if (values.size() == 3) {
        // CSS 3-value shorthand: top | left-right | bottom
        YGNodeStyleSetMargin(yogaNode, YGEdgeTop, values[0]);
        YGNodeStyleSetMargin(yogaNode, YGEdgeHorizontal, values[1]);
        YGNodeStyleSetMargin(yogaNode, YGEdgeBottom, values[2]);
    } else if (values.size() >= 4) {
        YGNodeStyleSetMargin(yogaNode, YGEdgeTop, values[0]);
        YGNodeStyleSetMargin(yogaNode, YGEdgeRight, values[1]);
        YGNodeStyleSetMargin(yogaNode, YGEdgeBottom, values[2]);
        YGNodeStyleSetMargin(yogaNode, YGEdgeLeft, values[3]);
    }
}

void CSSStyleConverter::applyGap(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetGap(yogaNode, YGGutterAll, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (!actualValue.empty()) {
        float gap = parseCssFloat(actualValue);
        YGNodeStyleSetGap(yogaNode, YGGutterAll, gap);
    }
}

void CSSStyleConverter::applyPosition(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "relative") {
        YGNodeStyleSetPositionType(yogaNode, YGPositionTypeRelative);
    } else if (actualValue == "absolute") {
        YGNodeStyleSetPositionType(yogaNode, YGPositionTypeAbsolute);
    }
}

void CSSStyleConverter::applyMinWidth(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetMinWidth(yogaNode, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (actualValue.empty()) {
        return;
    }
    
    if (actualValue.back() == '%') {
        float percent = yoga_internal::parseCssFloat(actualValue.substr(0, actualValue.size() - 1));
        YGNodeStyleSetMinWidthPercent(yogaNode, percent);
    } else {
        std::string numericPart = actualValue;
        size_t unitPos = actualValue.find_first_not_of("0123456789.-");
        if (unitPos != std::string::npos) {
            numericPart = actualValue.substr(0, unitPos);
        }
        float minWidth = yoga_internal::parseCssFloat(numericPart);
        YGNodeStyleSetMinWidth(yogaNode, minWidth);
    }
}

void CSSStyleConverter::applyMaxWidth(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetMaxWidth(yogaNode, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (actualValue.empty()) {
        return;
    }
    
    if (actualValue.back() == '%') {
        float percent = yoga_internal::parseCssFloat(actualValue.substr(0, actualValue.size() - 1));
        YGNodeStyleSetMaxWidthPercent(yogaNode, percent);
    } else {
        std::string numericPart = actualValue;
        size_t unitPos = actualValue.find_first_not_of("0123456789.-");
        if (unitPos != std::string::npos) {
            numericPart = actualValue.substr(0, unitPos);
        }
        float maxWidth = yoga_internal::parseCssFloat(numericPart);
        YGNodeStyleSetMaxWidth(yogaNode, maxWidth);
    }
}

void CSSStyleConverter::applyMinHeight(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetMinHeight(yogaNode, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (actualValue.empty()) {
        return;
    }
    
    if (actualValue.back() == '%') {
        float percent = yoga_internal::parseCssFloat(actualValue.substr(0, actualValue.size() - 1));
        YGNodeStyleSetMinHeightPercent(yogaNode, percent);
    } else {
        std::string numericPart = actualValue;
        size_t unitPos = actualValue.find_first_not_of("0123456789.-");
        if (unitPos != std::string::npos) {
            numericPart = actualValue.substr(0, unitPos);
        }
        float minHeight = yoga_internal::parseCssFloat(numericPart);
        YGNodeStyleSetMinHeight(yogaNode, minHeight);
    }
}

void CSSStyleConverter::applyMaxHeight(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetMaxHeight(yogaNode, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (actualValue.empty()) {
        return;
    }
    
    if (actualValue.back() == '%') {
        float percent = yoga_internal::parseCssFloat(actualValue.substr(0, actualValue.size() - 1));
        YGNodeStyleSetMaxHeightPercent(yogaNode, percent);
    } else {
        std::string numericPart = actualValue;
        size_t unitPos = actualValue.find_first_not_of("0123456789.-");
        if (unitPos != std::string::npos) {
            numericPart = actualValue.substr(0, unitPos);
        }
        float maxHeight = yoga_internal::parseCssFloat(numericPart);
        YGNodeStyleSetMaxHeight(yogaNode, maxHeight);
    }
}

void CSSStyleConverter::applyFlexWrap(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "nowrap") {
        YGNodeStyleSetFlexWrap(yogaNode, YGWrapNoWrap);
    } else if (actualValue == "wrap") {
        YGNodeStyleSetFlexWrap(yogaNode, YGWrapWrap);
    } else if (actualValue == "wrap-reverse") {
        YGNodeStyleSetFlexWrap(yogaNode, YGWrapWrapReverse);
    }
}

void CSSStyleConverter::applyAlignContent(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "flex-start") {
        YGNodeStyleSetAlignContent(yogaNode, YGAlignFlexStart);
    } else if (actualValue == "center") {
        YGNodeStyleSetAlignContent(yogaNode, YGAlignCenter);
    } else if (actualValue == "flex-end") {
        YGNodeStyleSetAlignContent(yogaNode, YGAlignFlexEnd);
    } else if (actualValue == "stretch") {
        YGNodeStyleSetAlignContent(yogaNode, YGAlignStretch);
    } else if (actualValue == "space-between") {
        YGNodeStyleSetAlignContent(yogaNode, YGAlignSpaceBetween);
    } else if (actualValue == "space-around") {
        YGNodeStyleSetAlignContent(yogaNode, YGAlignSpaceAround);
    }
}

void CSSStyleConverter::applyFlexBasis(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetFlexBasis(yogaNode, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "auto") {
        YGNodeStyleSetFlexBasisAuto(yogaNode);
    } else if (!actualValue.empty() && actualValue.back() == '%') {
        float percent = yoga_internal::parseCssFloat(actualValue.substr(0, actualValue.size() - 1));
        YGNodeStyleSetFlexBasisPercent(yogaNode, percent);
    } else if (!actualValue.empty()) {
        std::string numericPart = actualValue;
        size_t unitPos = actualValue.find_first_not_of("0123456789.-");
        if (unitPos != std::string::npos) {
            numericPart = actualValue.substr(0, unitPos);
        }
        float flexBasis = yoga_internal::parseCssFloat(numericPart);
        YGNodeStyleSetFlexBasis(yogaNode, flexBasis);
    }
}

void CSSStyleConverter::applyBorder(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        float val = value.asFloat();
        YGNodeStyleSetBorder(yogaNode, YGEdgeAll, val);
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (actualValue.empty()) {
        return;
    }
    
    std::vector<float> values;
    std::istringstream iss(actualValue);
    std::string token;
    while (iss >> token) {
        // Only process tokens starting with a digit or dot (e.g. "1px", "1.5"), skip "solid", "#fff", etc.
        if (!token.empty() && (std::isdigit(static_cast<unsigned char>(token[0])) || token[0] == '.')) {
            bool ok = false;
            float v = yoga_internal::parseCssFloat(token, &ok);
            if (ok) {
                values.push_back(v);
            }
            // unparseable tokens silently ignored (CSS spec tolerant)
        }
    }
    
    if (values.size() == 1) {
        YGNodeStyleSetBorder(yogaNode, YGEdgeAll, values[0]);
    } else if (values.size() == 2) {
        YGNodeStyleSetBorder(yogaNode, YGEdgeVertical, values[0]);
        YGNodeStyleSetBorder(yogaNode, YGEdgeHorizontal, values[1]);
    } else if (values.size() == 4) {
        YGNodeStyleSetBorder(yogaNode, YGEdgeTop, values[0]);
        YGNodeStyleSetBorder(yogaNode, YGEdgeRight, values[1]);
        YGNodeStyleSetBorder(yogaNode, YGEdgeBottom, values[2]);
        YGNodeStyleSetBorder(yogaNode, YGEdgeLeft, values[3]);
    }
}

void CSSStyleConverter::applyTop(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetPosition(yogaNode, YGEdgeTop, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (actualValue.empty()) {
        return;
    }
    
    if (actualValue.back() == '%') {
        float percent = yoga_internal::parseCssFloat(actualValue.substr(0, actualValue.size() - 1));
        YGNodeStyleSetPositionPercent(yogaNode, YGEdgeTop, percent);
    } else {
        std::string numericPart = actualValue;
        size_t unitPos = actualValue.find_first_not_of("0123456789.-");
        if (unitPos != std::string::npos) {
            numericPart = actualValue.substr(0, unitPos);
        }
        float top = yoga_internal::parseCssFloat(numericPart);
        YGNodeStyleSetPosition(yogaNode, YGEdgeTop, top);
    }
}

void CSSStyleConverter::applyRight(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetPosition(yogaNode, YGEdgeRight, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (actualValue.empty()) {
        return;
    }
    
    if (actualValue.back() == '%') {
        float percent = yoga_internal::parseCssFloat(actualValue.substr(0, actualValue.size() - 1));
        YGNodeStyleSetPositionPercent(yogaNode, YGEdgeRight, percent);
    } else {
        std::string numericPart = actualValue;
        size_t unitPos = actualValue.find_first_not_of("0123456789.-");
        if (unitPos != std::string::npos) {
            numericPart = actualValue.substr(0, unitPos);
        }
        float right = yoga_internal::parseCssFloat(numericPart);
        YGNodeStyleSetPosition(yogaNode, YGEdgeRight, right);
    }
}

void CSSStyleConverter::applyBottom(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetPosition(yogaNode, YGEdgeBottom, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (actualValue.empty()) {
        return;
    }
    
    if (actualValue.back() == '%') {
        float percent = yoga_internal::parseCssFloat(actualValue.substr(0, actualValue.size() - 1));
        YGNodeStyleSetPositionPercent(yogaNode, YGEdgeBottom, percent);
    } else {
        std::string numericPart = actualValue;
        size_t unitPos = actualValue.find_first_not_of("0123456789.-");
        if (unitPos != std::string::npos) {
            numericPart = actualValue.substr(0, unitPos);
        }
        float bottom = yoga_internal::parseCssFloat(numericPart);
        YGNodeStyleSetPosition(yogaNode, YGEdgeBottom, bottom);
    }
}

void CSSStyleConverter::applyLeft(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetPosition(yogaNode, YGEdgeLeft, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (actualValue.empty()) {
        return;
    }
    
    if (actualValue.back() == '%') {
        float percent = yoga_internal::parseCssFloat(actualValue.substr(0, actualValue.size() - 1));
        YGNodeStyleSetPositionPercent(yogaNode, YGEdgeLeft, percent);
    } else {
        std::string numericPart = actualValue;
        size_t unitPos = actualValue.find_first_not_of("0123456789.-");
        if (unitPos != std::string::npos) {
            numericPart = actualValue.substr(0, unitPos);
        }
        float left = yoga_internal::parseCssFloat(numericPart);
        YGNodeStyleSetPosition(yogaNode, YGEdgeLeft, left);
    }
}

void CSSStyleConverter::applyDisplay(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "flex") {
        YGNodeStyleSetDisplay(yogaNode, YGDisplayFlex);
    } else if (actualValue == "none") {
        YGNodeStyleSetDisplay(yogaNode, YGDisplayNone);
    }
}

void CSSStyleConverter::applyOverflow(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "visible") {
        YGNodeStyleSetOverflow(yogaNode, YGOverflowVisible);
    } else if (actualValue == "hidden") {
        YGNodeStyleSetOverflow(yogaNode, YGOverflowHidden);
    } else if (actualValue == "scroll") {
        YGNodeStyleSetOverflow(yogaNode, YGOverflowScroll);
    }
}

void CSSStyleConverter::applyDirection(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    if (actualValue == "ltr") {
        YGNodeStyleSetDirection(yogaNode, YGDirectionLTR);
    } else if (actualValue == "rtl") {
        YGNodeStyleSetDirection(yogaNode, YGDirectionRTL);
    } else if (actualValue == "inherit") {
        YGNodeStyleSetDirection(yogaNode, YGDirectionInherit);
    }
}

void CSSStyleConverter::applyAspectRatio(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetAspectRatio(yogaNode, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    if (!actualValue.empty()) {
        float aspectRatio = 0.0f;
        if (parseAspectRatioValue(actualValue, aspectRatio)) {
            YGNodeStyleSetAspectRatio(yogaNode, aspectRatio);
        }
    }
}

static float parseLengthValue(const std::string& val) {
    if (val.empty()) return 0.0f;
    std::string numericPart = val;
    size_t unitPos = val.find_first_not_of("0123456789.-");
    if (unitPos != std::string::npos) {
        numericPart = val.substr(0, unitPos);
    }
    return yoga_internal::parseCssFloat(numericPart);
}

void CSSStyleConverter::applyMarginLeft(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetMargin(yogaNode, YGEdgeLeft, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& v = value.asString();
    if (v == "auto") { YGNodeStyleSetMarginAuto(yogaNode, YGEdgeLeft); }
    else if (!v.empty() && v.back() == '%') { YGNodeStyleSetMarginPercent(yogaNode, YGEdgeLeft, yoga_internal::parseCssFloat(v.substr(0, v.size()-1))); }
    else if (!v.empty()) { YGNodeStyleSetMargin(yogaNode, YGEdgeLeft, parseLengthValue(v)); }
}

void CSSStyleConverter::applyMarginRight(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetMargin(yogaNode, YGEdgeRight, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& v = value.asString();
    if (v == "auto") { YGNodeStyleSetMarginAuto(yogaNode, YGEdgeRight); }
    else if (!v.empty() && v.back() == '%') { YGNodeStyleSetMarginPercent(yogaNode, YGEdgeRight, yoga_internal::parseCssFloat(v.substr(0, v.size()-1))); }
    else if (!v.empty()) { YGNodeStyleSetMargin(yogaNode, YGEdgeRight, parseLengthValue(v)); }
}

void CSSStyleConverter::applyMarginTop(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetMargin(yogaNode, YGEdgeTop, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& v = value.asString();
    if (v == "auto") { YGNodeStyleSetMarginAuto(yogaNode, YGEdgeTop); }
    else if (!v.empty() && v.back() == '%') { YGNodeStyleSetMarginPercent(yogaNode, YGEdgeTop, yoga_internal::parseCssFloat(v.substr(0, v.size()-1))); }
    else if (!v.empty()) { YGNodeStyleSetMargin(yogaNode, YGEdgeTop, parseLengthValue(v)); }
}

void CSSStyleConverter::applyMarginBottom(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetMargin(yogaNode, YGEdgeBottom, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& v = value.asString();
    if (v == "auto") { YGNodeStyleSetMarginAuto(yogaNode, YGEdgeBottom); }
    else if (!v.empty() && v.back() == '%') { YGNodeStyleSetMarginPercent(yogaNode, YGEdgeBottom, yoga_internal::parseCssFloat(v.substr(0, v.size()-1))); }
    else if (!v.empty()) { YGNodeStyleSetMargin(yogaNode, YGEdgeBottom, parseLengthValue(v)); }
}

void CSSStyleConverter::applyPaddingLeft(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetPadding(yogaNode, YGEdgeLeft, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& v = value.asString();
    if (!v.empty() && v.back() == '%') { YGNodeStyleSetPaddingPercent(yogaNode, YGEdgeLeft, yoga_internal::parseCssFloat(v.substr(0, v.size()-1))); }
    else if (!v.empty()) { YGNodeStyleSetPadding(yogaNode, YGEdgeLeft, parseLengthValue(v)); }
}

void CSSStyleConverter::applyPaddingRight(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetPadding(yogaNode, YGEdgeRight, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& v = value.asString();
    if (!v.empty() && v.back() == '%') { YGNodeStyleSetPaddingPercent(yogaNode, YGEdgeRight, yoga_internal::parseCssFloat(v.substr(0, v.size()-1))); }
    else if (!v.empty()) { YGNodeStyleSetPadding(yogaNode, YGEdgeRight, parseLengthValue(v)); }
}

void CSSStyleConverter::applyPaddingTop(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetPadding(yogaNode, YGEdgeTop, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& v = value.asString();
    if (!v.empty() && v.back() == '%') { YGNodeStyleSetPaddingPercent(yogaNode, YGEdgeTop, yoga_internal::parseCssFloat(v.substr(0, v.size()-1))); }
    else if (!v.empty()) { YGNodeStyleSetPadding(yogaNode, YGEdgeTop, parseLengthValue(v)); }
}

void CSSStyleConverter::applyPaddingBottom(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetPadding(yogaNode, YGEdgeBottom, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& v = value.asString();
    if (!v.empty() && v.back() == '%') { YGNodeStyleSetPaddingPercent(yogaNode, YGEdgeBottom, yoga_internal::parseCssFloat(v.substr(0, v.size()-1))); }
    else if (!v.empty()) { YGNodeStyleSetPadding(yogaNode, YGEdgeBottom, parseLengthValue(v)); }
}

void CSSStyleConverter::applyBorderWidth(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() == YogaValue::kFloat) {
        YGNodeStyleSetBorder(yogaNode, YGEdgeAll, value.asFloat());
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& v = value.asString();
    if (!v.empty()) {
        YGNodeStyleSetBorder(yogaNode, YGEdgeAll, parseLengthValue(v));
    }
}

void CSSStyleConverter::applyCellPadding(YGNodeRef yogaNode, ILayoutDataWrapper& wrapper) {
    // Same logic as applyPadding: parse single or multi-value CSS padding format
    YogaValue value = wrapper.getStyleValue(CSSPropertyNames::kCellPadding);
    if (value.isValid()) {
        applyPadding(yogaNode, value);
    }
}

void CSSStyleConverter::applyInsetInlineStart(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    YGDirection direction = YGNodeStyleGetDirection(yogaNode);
    if (direction == YGDirectionRTL) {
        applyRight(yogaNode, value);
    } else {
        applyLeft(yogaNode, value);
    }
}

void CSSStyleConverter::applyInsetInlineEnd(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    YGDirection direction = YGNodeStyleGetDirection(yogaNode);
    if (direction == YGDirectionRTL) {
        applyLeft(yogaNode, value);
    } else {
        applyRight(yogaNode, value);
    }
}

void CSSStyleConverter::applyInsetBlockStart(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    applyTop(yogaNode, value);
}

void CSSStyleConverter::applyInsetBlockEnd(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    applyBottom(yogaNode, value);
}

// Margin logical properties implementation
void CSSStyleConverter::applyMarginInlineStart(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    YGDirection direction = YGNodeStyleGetDirection(yogaNode);
    if (direction == YGDirectionRTL) {
        applyMarginRight(yogaNode, value);
    } else {
        applyMarginLeft(yogaNode, value);
    }
}

void CSSStyleConverter::applyMarginInlineEnd(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    YGDirection direction = YGNodeStyleGetDirection(yogaNode);
    if (direction == YGDirectionRTL) {
        applyMarginLeft(yogaNode, value);
    } else {
        applyMarginRight(yogaNode, value);
    }
}

void CSSStyleConverter::applyMarginBlockStart(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    applyMarginTop(yogaNode, value);
}

void CSSStyleConverter::applyMarginBlockEnd(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    applyMarginBottom(yogaNode, value);
}

// Padding logical properties implementation
void CSSStyleConverter::applyPaddingInlineStart(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    YGDirection direction = YGNodeStyleGetDirection(yogaNode);
    if (direction == YGDirectionRTL) {
        applyPaddingRight(yogaNode, value);
    } else {
        applyPaddingLeft(yogaNode, value);
    }
}

void CSSStyleConverter::applyPaddingInlineEnd(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    YGDirection direction = YGNodeStyleGetDirection(yogaNode);
    if (direction == YGDirectionRTL) {
        applyPaddingLeft(yogaNode, value);
    } else {
        applyPaddingRight(yogaNode, value);
    }
}

void CSSStyleConverter::applyPaddingBlockStart(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    applyPaddingTop(yogaNode, value);
}

void CSSStyleConverter::applyPaddingBlockEnd(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    applyPaddingBottom(yogaNode, value);
}

float CSSStyleConverter::parseStyleDimension(const nlohmann::json& styleConfig, const char* key, float fallbackValue) {
    if (!styleConfig.is_object() || !styleConfig.contains(key)) {
        return fallbackValue;
    }

    const nlohmann::json& value = styleConfig[key];
    if (value.is_number()) {
        return value.get<float>();
    }
    if (value.is_string()) {
        bool ok = false;
        float result = yoga_internal::parseCssFloat(value.get<std::string>(), &ok);
        if (ok) return result;
    }
    return fallbackValue;
}

}  // namespace agenui
