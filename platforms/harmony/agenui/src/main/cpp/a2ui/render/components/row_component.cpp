#include "row_component.h"
#include "a2ui/utils/a2ui_enum_utils.h"
#include "log/a2ui_capi_log.h"


namespace a2ui {

RowComponent::RowComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, "Row") {

    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_ROW);

    // Merge initial properties.
    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
    }

    HM_LOGI( "RowComponent - Created: id=%s, handle=%s",
                id.c_str(), m_nodeHandle ? "valid" : "null");
}

RowComponent::~RowComponent() {
    HM_LOGI( "RowComponent - Destroyed: id=%s", m_id.c_str());
}

// ---- Property Updates ----

void RowComponent::onUpdateProperties(const nlohmann::json& properties) {
    if (!m_nodeHandle) {
        HM_LOGE( "handle is null, id=%s", m_id.c_str());
        return;
    }

    applyStyles(properties);
    applyJustify(properties);
    applyAlign(properties);

    HM_LOGI( "Applied properties, id=%s", m_id.c_str());
}

// ---- Custom Styles ----

void RowComponent::applyStyles(const nlohmann::json& properties) {
    if (properties.find("styles") == properties.end() || !properties["styles"].is_object()) {
        return;
    }

    // Delegate background and border handling to the base class
    applyBackgroundColor(properties);
    applyBorderStyles(properties);
}

// ---- Justify ----

void RowComponent::applyJustify(const nlohmann::json& properties) {
    if (properties.find("justify") == properties.end() || !properties["justify"].is_string()) {
        return;
    }

    auto justifyValue = static_cast<ArkUI_FlexAlignment>(mapFlexJustifyContent(properties["justify"].get<std::string>()));
    A2UIRowNode node(m_nodeHandle);
    node.setJustifyContent(justifyValue);
}

// ---- Align ----

void RowComponent::applyAlign(const nlohmann::json& properties) {
    if (properties.find("align") == properties.end() || !properties["align"].is_string()) {
        return;
    }

    auto alignValue = static_cast<ArkUI_VerticalAlignment>(mapFlexAlignItems(properties["align"].get<std::string>()));
    A2UIRowNode node(m_nodeHandle);
    node.setAlignItems(alignValue);
}

} // namespace a2ui
