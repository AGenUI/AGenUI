#include "card_component.h"
#include "../a2ui_node.h"
#include "a2ui/utils/a2ui_color_palette.h"
#include "log/a2ui_capi_log.h"
#include <string>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "A2UI_CardComponent"

namespace a2ui {

using colors::kColorShadow20;

CardComponent::CardComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, "Card") {

    // Use a COLUMN node as the card container.
    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_COLUMN);

    // No default chrome is applied here on purpose. Card's defaults live in core's
    // per-component spec (kBaseComponentSpecConfig, "Card" -> border-radius: 16px)
    // and reach this node through onUpdateProperties, which the creation path always
    // runs before a frame can render (addChild -> createView ->
    // updateProperties(m_properties), all in one call stack). Anything set here would
    // be overwritten a moment later anyway: core's baseline injects
    // background-color: transparent and border-width: 0px into every snapshot, which
    // is exactly how the white background and 1px border this constructor used to set
    // ended up invisible on device. Add component defaults to the core spec, not here.

    // Merge initial properties.
    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
    }

    HM_LOGI("CardComponent - Created: id=%s, handle=%s", id.c_str(), m_nodeHandle ? "valid" : "null");
}

CardComponent::~CardComponent() {
    HM_LOGI("CardComponent - Destroyed: id=%s", m_id.c_str());
}

// ---- Property Updates ----

void CardComponent::onUpdateProperties(const nlohmann::json& properties) {
    if (!m_nodeHandle) {
        HM_LOGE("handle is null, id=%s", m_id.c_str());
        return;
    }

    // Every snapshot reaching the platform layer carries a non-empty styles object:
    // core merges kStyleDefaultsConfig into it unconditionally, so border-radius /
    // border-width / border-color / filter are always present. That is why there is
    // no "no styles, read the top level instead" fallback here -- it would be dead
    // code, and it used to make Card fork its own radius parser that set
    // NODE_BORDER_RADIUS without the matching NODE_CLIP, rendering the injected
    // default border-radius: 16px as square corners while Android and iOS rounded.
    applyBorderStyles(properties);
    applyBackgroundColor(properties);
    applyFilter(properties);
    applyElevation(properties);  // Legacy elevation is read from the top level.

    HM_LOGI("Applied properties, id=%s", m_id.c_str());
}

// ---- Elevation ----

void CardComponent::applyElevation(const nlohmann::json& properties) {
    if (!properties.contains("elevation") || !properties["elevation"].is_number()) return;
    float elev = properties["elevation"].get<float>();
    if (elev <= 0.0f) return;
    // Map elevation to a vertical shadow.
    A2UINode(m_nodeHandle).setCustomShadow(elev * 2.0f, 0.0f, elev, kColorShadow20);
}

} // namespace a2ui
