#include "card_component.h"
#include "../a2ui_node.h"
#include "a2ui/utils/a2ui_color_palette.h"
#include "a2ui/utils/a2ui_parse_utils.h"
#include "a2ui/utils/a2ui_shadow_utils.h"
#include "log/a2ui_capi_log.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <cctype>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "A2UI_CardComponent"

namespace a2ui {

using colors::kColorWhite;
using colors::kColorBorderGray;
using colors::kColorTransparent;
using colors::kColorShadow20;

CardComponent::CardComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, "Card") {

    // Use a COLUMN node as the card container.
    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_COLUMN);

    // Apply the default card chrome.
    {
        A2UINode node(m_nodeHandle);
        node.setBackgroundColor(kColorWhite);
        node.setBorderRadius(16.0f);
        node.setBorderWidth(1.0f, 1.0f, 1.0f, 1.0f);
        node.setBorderColor(kColorBorderGray);
    }

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

    // W3C properties live under styles; legacy ones stay at the top level.
    const nlohmann::json& styles = properties.contains("styles") && properties["styles"].is_object()
                                   ? properties["styles"]
                                   : properties;

    applyRadius(styles);
    applyBackgroundColor(properties);
    applyBorderWidth(styles);
    applyBorderColor(styles);
    applyFilter(styles);
    applyElevation(properties);  // Legacy elevation remains a top-level property.

    HM_LOGI("Applied properties, id=%s", m_id.c_str());
}

// ---- CSS Length Parsing ----

float CardComponent::parseCssLength(const nlohmann::json& val, float fallback) {
    return a2ui::parseCssLength(val, fallback);
}

// ---- Radius ----

void CardComponent::applyRadius(const nlohmann::json& properties) {
    // Prefer border-radius, with radius kept for legacy input.
    if (properties.contains("border-radius")) {
        float r = parseCssLength(properties["border-radius"], -1.0f);
        if (r >= 0.0f) A2UINode(m_nodeHandle).setBorderRadius(r);
    } else if (properties.contains("radius") && properties["radius"].is_number()) {
        float r = properties["radius"].get<float>();
        A2UINode(m_nodeHandle).setBorderRadius(r);
    }
}

// ---- Border Width ----

void CardComponent::applyBorderWidth(const nlohmann::json& properties) {
    float bw = -1.0f;
    if (properties.contains("border-width")) {
        bw = parseCssLength(properties["border-width"], -1.0f);
    } else if (properties.contains("borderWidth")) {
        bw = parseCssLength(properties["borderWidth"], -1.0f);
    }
    if (bw >= 0.0f) {
        A2UINode node(m_nodeHandle);
        node.setBorderWidth(bw, bw, bw, bw);
        node.setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
    }
}

// ---- Border Color ----

void CardComponent::applyBorderColor(const nlohmann::json& properties) {
    std::string colorStr;
    if (properties.contains("border-color") && properties["border-color"].is_string()) {
        colorStr = properties["border-color"].get<std::string>();
    } else if (properties.contains("borderColor") && properties["borderColor"].is_string()) {
        colorStr = properties["borderColor"].get<std::string>();
    }
    if (!colorStr.empty()) {
        A2UINode(m_nodeHandle).setBorderColor(parseColor(colorStr));
    }
}

// ---- Filter: drop-shadow ----

void CardComponent::applyFilter(const nlohmann::json& properties) {
    if (!properties.contains("filter") || !properties["filter"].is_string()) return;

    std::string filterVal = properties["filter"].get<std::string>();

    auto params = parseDropShadow(filterVal);
    if (!params.valid) return;

    // Parse the shadow color.
    uint32_t color = parseColor(params.colorStr);
    if (color == kColorTransparent && params.colorStr != "#00000000" && params.colorStr != "rgba(0, 0, 0, 0)" &&
        params.colorStr != "rgba(0,0,0,0)" && params.colorStr != "rgb(0, 0, 0)") {
        return;
    }

    // Apply the shadow.
    A2UINode(m_nodeHandle).setCustomShadow(params.blurRadius, params.offsetX, params.offsetY, color);
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
