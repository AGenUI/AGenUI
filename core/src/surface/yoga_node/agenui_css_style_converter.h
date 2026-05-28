#pragma once

#include "surface/yoga_node/agenui_yoga_value.h"
#include "surface/virtual_dom/agenui_component_snapshot.h"
#include <string>

#include <yoga/Yoga.h>
#include "nlohmann/json.hpp"

namespace agenui {

class ComponentSnapshotWrapper;
class ILayoutDataWrapper;

// CSS property name static mapping (W3C standard naming)
namespace CSSPropertyNames {
    // Size properties
    inline constexpr const char* kWidth = "width";
    inline constexpr const char* kHeight = "height";
    inline constexpr const char* kMinWidth = "min-width";
    inline constexpr const char* kMaxWidth = "max-width";
    inline constexpr const char* kMinHeight = "min-height";
    inline constexpr const char* kMaxHeight = "max-height";
    
    // Flexbox layout properties
    inline constexpr const char* kFlexDirection = "flex-direction";
    inline constexpr const char* kFlexWrap = "flex-wrap";
    inline constexpr const char* kJustifyContent = "justify-content";
    inline constexpr const char* kAlignItems = "align-items";
    inline constexpr const char* kAlignSelf = "align-self";
    inline constexpr const char* kAlignContent = "align-content";
    inline constexpr const char* kFlexGrow = "flex-grow";
    inline constexpr const char* kFlexShrink = "flex-shrink";
    inline constexpr const char* kFlexBasis = "flex-basis";
    
    // Spacing properties
    inline constexpr const char* kPadding = "padding";
    inline constexpr const char* kPaddingLeft = "padding-left";
    inline constexpr const char* kPaddingRight = "padding-right";
    inline constexpr const char* kPaddingTop = "padding-top";
    inline constexpr const char* kPaddingBottom = "padding-bottom";
    inline constexpr const char* kMargin = "margin";
    inline constexpr const char* kMarginLeft = "margin-left";
    inline constexpr const char* kMarginRight = "margin-right";
    inline constexpr const char* kMarginTop = "margin-top";
    inline constexpr const char* kMarginBottom = "margin-bottom";
    inline constexpr const char* kBorder = "border";
    inline constexpr const char* kBorderWidth = "border-width";
    inline constexpr const char* kGap = "gap";
    
    // Positioning properties
    inline constexpr const char* kPosition = "position";
    inline constexpr const char* kTop = "top";
    inline constexpr const char* kRight = "right";
    inline constexpr const char* kBottom = "bottom";
    inline constexpr const char* kLeft = "left";
    
    // Display and overflow properties
    inline constexpr const char* kDisplay = "display";
    inline constexpr const char* kOverflow = "overflow";
    inline constexpr const char* kDirection = "direction";
    
    // Other properties
    inline constexpr const char* kAspectRatio = "aspect-ratio";
    
    // Divider specific properties
    inline constexpr const char* kThickness = "thickness";
    
    // Table specific properties
    inline constexpr const char* kCellPadding = "cell-padding";
    inline constexpr const char* kColumnWeights = "column-weights";

        // ChoicePicker specific properties
    inline constexpr const char* kOrientation = "orientation";   // ChoicePicker layout direction (horizontal/vertical)

    // CSS Logical Properties
    // Inset logical properties
    inline constexpr const char* kInsetInlineStart = "inset-inline-start";
    inline constexpr const char* kInsetInlineEnd = "inset-inline-end";
    inline constexpr const char* kInsetBlockStart = "inset-block-start";
    inline constexpr const char* kInsetBlockEnd = "inset-block-end";
    
    // Margin logical properties
    inline constexpr const char* kMarginInlineStart = "margin-inline-start";
    inline constexpr const char* kMarginInlineEnd = "margin-inline-end";
    inline constexpr const char* kMarginBlockStart = "margin-block-start";
    inline constexpr const char* kMarginBlockEnd = "margin-block-end";
    
    // Padding logical properties
    inline constexpr const char* kPaddingInlineStart = "padding-inline-start";
    inline constexpr const char* kPaddingInlineEnd = "padding-inline-end";
    inline constexpr const char* kPaddingBlockStart = "padding-block-start";
    inline constexpr const char* kPaddingBlockEnd = "padding-block-end";

    // camelCase aliases (equivalent to standard kebab-case properties)
    inline constexpr const char* kJustifyContentCC     = "justifyContent";
    inline constexpr const char* kAlignItemsCC         = "alignItems";
    inline constexpr const char* kFlexWrapCC           = "flexWrap";
    inline constexpr const char* kAlignContentCC       = "alignContent";
    inline constexpr const char* kAlignSelfCC          = "alignSelf";
    inline constexpr const char* kFlexGrowCC           = "flexGrow";
    inline constexpr const char* kFlexShrinkCC         = "flexShrink";
    inline constexpr const char* kFlexBasisCC          = "flexBasis";
    inline constexpr const char* kBorderWidthCC        = "borderWidth";
    // Inset logical properties camelCase aliases
    inline constexpr const char* kInsetInlineStartCC   = "insetInlineStart";
    inline constexpr const char* kInsetInlineEndCC     = "insetInlineEnd";
    inline constexpr const char* kInsetBlockStartCC    = "insetBlockStart";
    inline constexpr const char* kInsetBlockEndCC      = "insetBlockEnd";
    // Margin logical properties camelCase aliases
    inline constexpr const char* kMarginInlineStartCC  = "marginInlineStart";
    inline constexpr const char* kMarginInlineEndCC    = "marginInlineEnd";
    inline constexpr const char* kMarginBlockStartCC   = "marginBlockStart";
    inline constexpr const char* kMarginBlockEndCC     = "marginBlockEnd";
    // Padding logical properties camelCase aliases
    inline constexpr const char* kPaddingInlineStartCC = "paddingInlineStart";
    inline constexpr const char* kPaddingInlineEndCC   = "paddingInlineEnd";
    inline constexpr const char* kPaddingBlockStartCC  = "paddingBlockStart";
    inline constexpr const char* kPaddingBlockEndCC    = "paddingBlockEnd";
}

/**
 * @brief CSS style converter
 * @remark Converts CSS style properties from ComponentSnapshot to Yoga layout properties
 */
class CSSStyleConverter {
public:
    /**
     * @brief Convert CSS styles to Yoga layout properties
     * @param snapshot Component snapshot (input source)
     * @param yogaNode Yoga layout node (output target)
     * @param clearAfterConvert Whether to clear CSS styles from snapshot after conversion, default true
     * @remark Processes CSS style properties in the styles field
     */
    static void convertToYoga(ILayoutDataWrapper& wrapper, YGNodeRef yogaNode, bool clearAfterConvert = true);
    
    static bool isRichText(const std::string& text);

public:    
    static void applyCellPadding(YGNodeRef yogaNode, ILayoutDataWrapper& wrapper);
    
    /**
     * @brief Parse style dimension value
     * @param styleConfig Style config JSON object
     * @param key Key name to parse
     * @param fallbackValue Default value
     * @return Parsed float value
     */
    static float parseStyleDimension(const nlohmann::json& styleConfig, const char* key, float fallbackValue);
    
public:
    // CSS-specific apply functions (handle CSS standard value formats)

    // Size properties
    static void applyWidth(YGNodeRef yogaNode, YogaValue value, bool hasMeasureFunc);
    static void applyHeight(YGNodeRef yogaNode, YogaValue value, bool hasMeasureFunc);
    static void applyMinWidth(YGNodeRef yogaNode, YogaValue value);
    static void applyMaxWidth(YGNodeRef yogaNode, YogaValue value);
    static void applyMinHeight(YGNodeRef yogaNode, YogaValue value);
    static void applyMaxHeight(YGNodeRef yogaNode, YogaValue value);
    
    // Flexbox layout properties
    static void applyFlexDirection(YGNodeRef yogaNode, YogaValue value);
    static void applyFlexWrap(YGNodeRef yogaNode, YogaValue value);
    static void applyJustifyContent(YGNodeRef yogaNode, YogaValue value);
    static void applyAlignItems(YGNodeRef yogaNode, YogaValue value);
    static void applyAlignSelf(YGNodeRef yogaNode, YogaValue value);
    static void applyAlignContent(YGNodeRef yogaNode, YogaValue value);
    static void applyFlex(YGNodeRef yogaNode, YogaValue value);
    static void applyFlexGrow(YGNodeRef yogaNode, YogaValue value);
    static void applyFlexShrink(YGNodeRef yogaNode, YogaValue value);
    static void applyFlexBasis(YGNodeRef yogaNode, YogaValue value);
    
    // Spacing properties
    static void applyPadding(YGNodeRef yogaNode, YogaValue value);
    static void applyPaddingLeft(YGNodeRef yogaNode, YogaValue value);
    static void applyPaddingRight(YGNodeRef yogaNode, YogaValue value);
    static void applyPaddingTop(YGNodeRef yogaNode, YogaValue value);
    static void applyPaddingBottom(YGNodeRef yogaNode, YogaValue value);
    static void applyMargin(YGNodeRef yogaNode, YogaValue value);
    static void applyMarginLeft(YGNodeRef yogaNode, YogaValue value);
    static void applyMarginRight(YGNodeRef yogaNode, YogaValue value);
    static void applyMarginTop(YGNodeRef yogaNode, YogaValue value);
    static void applyMarginBottom(YGNodeRef yogaNode, YogaValue value);
    static void applyBorder(YGNodeRef yogaNode, YogaValue value);
    static void applyBorderWidth(YGNodeRef yogaNode, YogaValue value);
    static void applyGap(YGNodeRef yogaNode, YogaValue value);
    
    // Positioning properties
    static void applyPosition(YGNodeRef yogaNode, YogaValue value);
    static void applyTop(YGNodeRef yogaNode, YogaValue value);
    static void applyRight(YGNodeRef yogaNode, YogaValue value);
    static void applyBottom(YGNodeRef yogaNode, YogaValue value);
    static void applyLeft(YGNodeRef yogaNode, YogaValue value);
    
    // Display and overflow properties
    static void applyDisplay(YGNodeRef yogaNode, YogaValue value);
    static void applyOverflow(YGNodeRef yogaNode, YogaValue value);
    static void applyDirection(YGNodeRef yogaNode, YogaValue value);
    
    // Other properties
    static void applyAspectRatio(YGNodeRef yogaNode, YogaValue value);
    
    // CSS Logical Properties
    // Inset logical properties
    static void applyInsetInlineStart(YGNodeRef yogaNode, YogaValue value);
    static void applyInsetInlineEnd(YGNodeRef yogaNode, YogaValue value);
    static void applyInsetBlockStart(YGNodeRef yogaNode, YogaValue value);
    static void applyInsetBlockEnd(YGNodeRef yogaNode, YogaValue value);
    
    // Margin logical properties
    static void applyMarginInlineStart(YGNodeRef yogaNode, YogaValue value);
    static void applyMarginInlineEnd(YGNodeRef yogaNode, YogaValue value);
    static void applyMarginBlockStart(YGNodeRef yogaNode, YogaValue value);
    static void applyMarginBlockEnd(YGNodeRef yogaNode, YogaValue value);
    
    // Padding logical properties
    static void applyPaddingInlineStart(YGNodeRef yogaNode, YogaValue value);
    static void applyPaddingInlineEnd(YGNodeRef yogaNode, YogaValue value);
    static void applyPaddingBlockStart(YGNodeRef yogaNode, YogaValue value);
    static void applyPaddingBlockEnd(YGNodeRef yogaNode, YogaValue value);
};

}  // namespace agenui
