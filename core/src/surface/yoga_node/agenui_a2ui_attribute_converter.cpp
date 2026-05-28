#include "agenui_a2ui_attribute_converter.h"
#include <yoga/Yoga.h>
#include "agenui_component_snapshot_wrapper.h"
#include "agenui_yoga_internal_parse.h"

namespace agenui {

void A2UIAttributeConverter::convertToYoga(ILayoutDataWrapper& wrapper, YGNodeRef yogaNode, bool clearAfterConvert) {
    if (!yogaNode) {
        return;
    }

    // Process direction attribute (maps to flex-direction).
    // Note: applyFlexDirection enforces component-type defaults (Row/Column take
    // precedence over the explicit value) regardless of whether `direction` is
    // present, so we always invoke it — the implicit component-type default is
    // intentional and lives inside applyFlexDirection, not in a `valid` branch here.
    {
        applyFlexDirection(yogaNode,
                           wrapper.getAttributeValue(A2UIPropertyNames::kDirection),
                           wrapper);
    }
    
    // Process justify attribute (maps to justify-content)
    {
        YogaValue value = wrapper.getAttributeValue(A2UIPropertyNames::kJustify);
        if (value.isValid()) {
            applyJustifyContent(yogaNode, value);
        }
    }
    
    // Process align attribute (maps to align-items)
    {
        YogaValue value = wrapper.getAttributeValue(A2UIPropertyNames::kAlign);
        if (value.isValid()) {
            applyAlignItems(yogaNode, value);
        }
    }
    
    // Process weight attribute (maps to flex-grow)
    {
        YogaValue value = wrapper.getAttributeValue(A2UIPropertyNames::kWeight);
        if (value.isValid()) {
            applyFlexGrow(yogaNode, value);
        }
    }
    
    // Clear A2UI-related attributes if requested.
    if (clearAfterConvert) {
        wrapper.clearAttribute(A2UIPropertyNames::kJustify);
        wrapper.clearAttribute(A2UIPropertyNames::kWeight);
    }
}

void A2UIAttributeConverter::applyFlexDirection(YGNodeRef yogaNode, YogaValue value, ILayoutDataWrapper& wrapper) {
    if (!yogaNode) {
        return;
    }
    
    const std::string& component = wrapper.componentType();
    if (component == "Row") {
        YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionRow);
    } else if (component == "Column" || component == "column") {
        YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionColumn);
    } else if (value.type() == YogaValue::kString && !value.asString().empty()) {
        // Use explicitly set value for other component types
        const std::string& actualValue = value.asString();
        // Handle A2UI value format
        if (actualValue == "horizontal") {
            YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionRow);
        } else if (actualValue == "vertical") {
            YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionColumn);
        }
    }
}

void A2UIAttributeConverter::applyJustifyContent(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    // Handle A2UI value format
    if (actualValue == "start") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifyFlexStart);
    } else if (actualValue == "center") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifyCenter);
    } else if (actualValue == "end") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifyFlexEnd);
    } else if (actualValue == "spaceBetween") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifySpaceBetween);
    } else if (actualValue == "spaceAround") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifySpaceAround);
    } else if (actualValue == "spaceEvenly") {
        YGNodeStyleSetJustifyContent(yogaNode, YGJustifySpaceEvenly);
    }
}

void A2UIAttributeConverter::applyAlignItems(YGNodeRef yogaNode, YogaValue value) {
    if (!yogaNode) {
        return;
    }
    if (value.type() != YogaValue::kString) {
        return;
    }
    const std::string& actualValue = value.asString();
    
    // Handle A2UI value format
    if (actualValue == "start") {
        YGNodeStyleSetAlignItems(yogaNode, YGAlignFlexStart);
    } else if (actualValue == "center") {
        YGNodeStyleSetAlignItems(yogaNode, YGAlignCenter);
    } else if (actualValue == "end") {
        YGNodeStyleSetAlignItems(yogaNode, YGAlignFlexEnd);
    } else if (actualValue == "stretch") {
        YGNodeStyleSetAlignItems(yogaNode, YGAlignStretch);
    }
}

void A2UIAttributeConverter::applyFlexGrow(YGNodeRef yogaNode, YogaValue value) {
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
        bool ok = false;
        float flexGrow = yoga_internal::parseCssFloat(actualValue, &ok);
        if (ok) {
            YGNodeStyleSetFlexGrow(yogaNode, flexGrow);
        }
    }
}

}  // namespace agenui
