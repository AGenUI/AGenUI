#include "agenui_a2ui_attribute_converter.h"
#include "agenui_yoga_internal_parse.h"

namespace agenui {

void A2UIAttributeConverter::convertToYoga(ComponentSnapshot& snapshot, YGNodeRef yogaNode, bool clearAfterConvert) {
    if (yogaNode == nullptr) {
        return;
    }
    
    // Process direction attribute (maps to flex-direction)
    {
        auto attrIt = snapshot.attributes.find(A2UIPropertyNames::kDirection);
        if (attrIt != snapshot.attributes.end()) {
            applyFlexDirection(yogaNode, attrIt->second, snapshot);
        }  else {
            // Even without explicit direction, set based on component type
            applyFlexDirection(yogaNode, SerializableData(), snapshot);
        }
    }
    
    // Process justify attribute (maps to justify-content)
    {
        auto attrIt = snapshot.attributes.find(A2UIPropertyNames::kJustify);
        if (attrIt != snapshot.attributes.end()) {
            applyJustifyContent(yogaNode, attrIt->second);
        }
    }
    
    // Process align attribute (maps to align-items)
    {
        auto attrIt = snapshot.attributes.find(A2UIPropertyNames::kAlign);
        if (attrIt != snapshot.attributes.end()) {
            applyAlignItems(yogaNode, attrIt->second);
        }
    }
    
    // Process weight attribute (maps to flex-grow)
    {
        auto attrIt = snapshot.attributes.find(A2UIPropertyNames::kWeight);
        if (attrIt != snapshot.attributes.end()) {
            applyFlexGrow(yogaNode, attrIt->second);
        }
    }
    
    // Clear A2UI-related attributes if requested.
    // NOTE: kDirection / kAlign are intentionally NOT erased — downstream
    // platform components (Row/Column/Tabs render layer) read them directly
    // from snapshot.attributes after Yoga conversion. Removing them would
    // break visual rendering. Only the keys whose semantics are fully
    // absorbed by the Yoga layout (justify, weight) are erased here.
    if (clearAfterConvert) {
        snapshot.attributes.erase(A2UIPropertyNames::kJustify);
        snapshot.attributes.erase(A2UIPropertyNames::kWeight);
    }
}

void A2UIAttributeConverter::applyFlexDirection(YGNodeRef yogaNode, const SerializableData& value, ComponentSnapshot& snapshot) {
    if (yogaNode == nullptr) {
        return;
    }
    
    // Check component type first
    if (snapshot.component == "Row" || snapshot.component == "row") {
        YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionRow);
    } else if (snapshot.component == "Column" || snapshot.component == "column")  {
        YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionColumn);
    } else if (!value.asString().empty()) {
        // Use explicitly set value for other component types
        std::string actualValue = value.asString();
        // Handle A2UI value format
        if (actualValue == "horizontal") {
            YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionRow);
        } else if (actualValue == "vertical") {
            YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionColumn);
        }
    }
}

void A2UIAttributeConverter::applyJustifyContent(YGNodeRef yogaNode, const SerializableData& value) {
    if (yogaNode == nullptr) {
        return;
    }
    
    std::string actualValue = value.asString();
    
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

void A2UIAttributeConverter::applyAlignItems(YGNodeRef yogaNode, const SerializableData& value) {
    if (yogaNode == nullptr) {
        return;
    }
    
    std::string actualValue = value.asString();
    
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

void A2UIAttributeConverter::applyFlexGrow(YGNodeRef yogaNode, const SerializableData& value) {
    if (yogaNode == nullptr) {
        return;
    }
    
    if (value.isNumber()) {
        YGNodeStyleSetFlexGrow(yogaNode, static_cast<float>(value.asDouble()));
        return;
    }
    
    std::string actualValue = value.asString();
    
    if (!actualValue.empty()) {
        bool ok = false;
        float flexGrow = yoga_internal::parseCssFloat(actualValue, &ok);
        if (ok) {
            YGNodeStyleSetFlexGrow(yogaNode, flexGrow);
        }
    }
}

}  // namespace agenui
