#include "column_component.h"
#include "log/a2ui_capi_log.h"


namespace a2ui {

ColumnComponent::ColumnComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, "Column") {
    
    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_COLUMN);
    
    // Merge initial properties.
    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
    }

    HM_LOGI( "ColumnComponent - Created: id=%s, handle=%s", id.c_str(), m_nodeHandle ? "valid" : "null");
}

ColumnComponent::~ColumnComponent() {
    HM_LOGI( "ColumnComponent - Destroyed: id=%s", m_id.c_str());
}

// ---- Property Updates ----

void ColumnComponent::onUpdateProperties(const nlohmann::json& properties) {
    if (!m_nodeHandle) {
        HM_LOGE( "handle or nodeApi is null, id=%s",m_id.c_str());
        return;
    }

    applyJustify(properties);
    applyAlign(properties);
    applyStyles(properties);

    HM_LOGI( "Applied properties, id=%s", m_id.c_str());
}

// ---- Justify ----

void ColumnComponent::applyJustify(const nlohmann::json& properties) {
    if (properties.find("justify") == properties.end() || !properties["justify"].is_string()) {
        return;
    }

    ArkUI_FlexAlignment justifyValue = (ArkUI_FlexAlignment)mapJustifyContent(properties["justify"].get<std::string>());
    A2UIColumnNode node(m_nodeHandle);
    node.setJustifyContent(justifyValue);
}

// ---- Align ----

void ColumnComponent::applyAlign(const nlohmann::json& properties) {
    if (properties.find("align") == properties.end() || !properties["align"].is_string()) {
        return;
    }

    ArkUI_ItemAlignment alignValue = (ArkUI_ItemAlignment)mapAlignItems(properties["align"].get<std::string>());
    A2UIColumnNode node(m_nodeHandle);
    node.setAlignItems(alignValue);
}

// ---- Custom Styles ----

void ColumnComponent::applyStyles(const nlohmann::json& properties) {
    if (properties.find("styles") == properties.end() || !properties["styles"].is_object()) {
        return;
    }

    // Delegate background and border handling to the base class
    applyBackgroundColor(properties);
    applyBorderStyles(properties);
}

// ---- Enum Mappings ----

int32_t ColumnComponent::mapJustifyContent(const std::string& justify) {
    if (justify == "center") {
        return ARKUI_FLEX_ALIGNMENT_CENTER;
    } else if (justify == "end") {
        return ARKUI_FLEX_ALIGNMENT_END;
    } else if (justify == "spaceBetween") {
        return ARKUI_FLEX_ALIGNMENT_SPACE_BETWEEN;
    } else if (justify == "spaceAround") {
        return ARKUI_FLEX_ALIGNMENT_SPACE_AROUND;
    } else if (justify == "spaceEvenly") {
        return ARKUI_FLEX_ALIGNMENT_SPACE_EVENLY;
    }
    // Default to START.
    return ARKUI_FLEX_ALIGNMENT_START;
}

int32_t ColumnComponent::mapAlignItems(const std::string& align) {
    if (align == "center") {
        return ARKUI_ITEM_ALIGNMENT_CENTER;
    } else if (align == "end") {
        return ARKUI_ITEM_ALIGNMENT_END;
    } else if (align == "stretch") {
        return ARKUI_ITEM_ALIGNMENT_STRETCH;
    }
    // Default to START.
    return ARKUI_ITEM_ALIGNMENT_START;
}

} // namespace a2ui
