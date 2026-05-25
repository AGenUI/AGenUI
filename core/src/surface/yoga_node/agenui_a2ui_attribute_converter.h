#pragma once

#include "surface/virtual_dom/agenui_component_snapshot.h"
#include <string>

#include <yoga/Yoga.h>

namespace agenui {

// A2UI property name static mapping (A2UI spec naming)
namespace A2UIPropertyNames {
    // Layout container common properties
    inline constexpr const char* kJustify = "justify";           // Main-axis alignment for Row/Column/List
    inline constexpr const char* kAlign = "align";               // Cross-axis alignment for Row/Column/List
    inline constexpr const char* kDirection = "direction";       // List layout direction
    inline constexpr const char* kWeight = "weight";             // Relative weight in Row/Column (like flex-grow)

    // Table specific properties
    inline constexpr const char* kColumns = "columns";           // Table column definition array (count = columns.size())
    inline constexpr const char* kRows = "rows";                 // Table data row array (count = rows.size())
    inline constexpr const char* kAxis = "axis";                 // Divider horizontal or vertical
    inline constexpr const char* kOptions = "options";           // ChoicePicker option array
    inline constexpr const char* kTabs = "tabs";                 // Tabs label array

    // A2UI justify value enum
    namespace JustifyValues {
        inline constexpr const char* kStart = "start";
        inline constexpr const char* kCenter = "center";
        inline constexpr const char* kEnd = "end";
        inline constexpr const char* kSpaceBetween = "spaceBetween";
        inline constexpr const char* kSpaceAround = "spaceAround";
        inline constexpr const char* kSpaceEvenly = "spaceEvenly";
        inline constexpr const char* kStretch = "stretch";
    }
    
    // A2UI align value enum
    namespace AlignValues {
        inline constexpr const char* kStart = "start";
        inline constexpr const char* kCenter = "center";
        inline constexpr const char* kEnd = "end";
        inline constexpr const char* kStretch = "stretch";
    }
    
    // A2UI direction value enum
    namespace DirectionValues {
        inline constexpr const char* kVertical = "vertical";
        inline constexpr const char* kHorizontal = "horizontal";
    }
}

/**
 * @brief A2UI attribute converter
 * @remark Converts A2UI attributes from ComponentSnapshot to Yoga layout properties
 */
class A2UIAttributeConverter {
public:
    /**
     * @brief Convert A2UI attributes to Yoga layout properties
     * @param snapshot Component snapshot (input source)
     * @param yogaNode Yoga layout node (output target)
     * @param clearAfterConvert Whether to clear A2UI attributes from snapshot after conversion, default false
     * @remark Processes A2UI-specific attributes in the attributes field (direction, justify, align, weight)
     */
    static void convertToYoga(ComponentSnapshot& snapshot, YGNodeRef yogaNode, bool clearAfterConvert = false);

private:
    // A2UI-specific apply functions (handle A2UI value format + component type checks)
    static void applyFlexDirection(YGNodeRef yogaNode, const SerializableData& value, ComponentSnapshot& snapshot);
    static void applyJustifyContent(YGNodeRef yogaNode, const SerializableData& value);
    static void applyAlignItems(YGNodeRef yogaNode, const SerializableData& value);
    static void applyFlexGrow(YGNodeRef yogaNode, const SerializableData& value);
};

}  // namespace agenui
