#include "button_component.h"
#include "../a2ui_node.h"
#include "../../utils/a2ui_color_palette.h"
#include "log/a2ui_capi_log.h"
#include <cstdlib>
#include <cstdio>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "A2UI_ButtonComponent"

namespace a2ui {

ButtonComponent::ButtonComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, "Button")
    , m_disabled(false) {

    // Use a STACK container to center a single child.
    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_STACK);

    // Center the content.
    {
        ArkUI_NumberValue alignVal[] = {{.i32 = ARKUI_ALIGNMENT_CENTER}};
        ArkUI_AttributeItem alignItem = {alignVal, 1, nullptr, nullptr};
        g_nodeAPI->setAttribute(m_nodeHandle, NODE_STACK_ALIGN_CONTENT, &alignItem);
    }

    // setupClickListener() owns click registration through the action property.

    // Keep the default background transparent for tap feedback.
    A2UINode(m_nodeHandle).setBackgroundColor(colors::kColorTransparent);

    // Merge initial properties.
    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
    }

    HM_LOGI("ButtonComponent - Created: id=%s, handle=%s", id.c_str(), m_nodeHandle ? "valid" : "null");
}

ButtonComponent::~ButtonComponent() {
    HM_LOGI("ButtonComponent - Destroyed: id=%s", m_id.c_str());
}

bool ButtonComponent::isClickDisabled() const {
    return m_disabled;
}

bool ButtonComponent::shouldApplyChildLayoutPosition(const A2UIComponent* child) const {
    (void)child;
    // Let the native Stack center the child without Yoga x/y offsets.
    return false;
}

void ButtonComponent::onApplyChildPosition(A2UIComponent* child, float x, float y) {
    (void)child; (void)x; (void)y;
    // No-op: suppress base-class setPosition so that Stack ALIGNMENT_CENTER takes effect.
}

float ButtonComponent::resolveAppearTargetOpacity(const nlohmann::json& properties) const {
    if (properties.contains("disable") && properties["disable"].is_boolean() &&
        properties["disable"].get<bool>()) {
        return 0.5f;
    }
    return A2UIComponent::resolveAppearTargetOpacity(properties);
}

// ---- Property Updates ----

void ButtonComponent::applyChild(const nlohmann::json& properties) {
    if (!properties.contains("child") || !properties["child"].is_string()) {
        return;
    }

    std::string newChildId = properties["child"].get<std::string>();

    // Skip redundant child updates.
    if (!m_children.empty()) {
        A2UIComponent* currentChild = m_children[0];
        if (currentChild && currentChild->getId() == newChildId) {
            HM_LOGI("Child unchanged: id=%s, child=%s", m_id.c_str(), newChildId.c_str());
            return;
        }

        // Detach the current child without destroying it.
        HM_LOGI("Removing old child from native tree: id=%s, oldChild=%s", m_id.c_str(), currentChild->getId().c_str());
        removeChild(currentChild);
    }

    HM_LOGI("Child changed: id=%s, newChild=%s", m_id.c_str(), newChildId.c_str());
}

void ButtonComponent::onUpdateProperties(const nlohmann::json& properties) {
    if (!m_nodeHandle) {
        HM_LOGE("handle is null, id=%s", m_id.c_str());
        return;
    }

    applyChild(properties);
    applyVariant(properties);
    applyDisable(properties);
    applyStyles(properties);
    applyChecks(properties);

    HM_LOGI("Applied properties, id=%s", m_id.c_str());
}

// ---- Variant ----

void ButtonComponent::applyVariant(const nlohmann::json& properties) {
    if (!properties.contains("variant") || !properties["variant"].is_string()) {
        return;
    }

    std::string variant = properties["variant"].get<std::string>();

    if (variant == "borderless") {
        // Transparent background without a border.
        A2UINode(m_nodeHandle).setBackgroundColor(colors::kColorTransparent);

    } else if (variant == "primary") {
        // Apply the default primary button styling.
        A2UINode node(m_nodeHandle);
        node.setBackgroundColor(0xFF1976D2);
        node.setBorderRadius(8.0f);
    }
}

// ---- Disabled State ----

void ButtonComponent::applyDisable(const nlohmann::json& properties) {
    if (!properties.contains("disable")) {
        return;
    }

    bool disabled = false;
    if (properties["disable"].is_boolean()) {
        disabled = properties["disable"].get<bool>();
    }

    m_disabled = disabled;

    // Dim disabled buttons.
    A2UINode(m_nodeHandle).setOpacity(disabled ? 0.5f : 1.0f);
}

// ---- Styles ----

void ButtonComponent::applyStyles(const nlohmann::json& properties) {
    if (!properties.contains("styles") || !properties["styles"].is_object()) {
        return;
    }

    const auto& styles = properties["styles"];
    A2UINode node(m_nodeHandle);

    // Disabled buttons prefer the dedicated disabled background keys.
    std::string bgColorKey = m_disabled ? "background-color-disabled" : "background-color";
    std::string bgCamelKey = m_disabled ? "backgroundColorDisabled" : "backgroundColor";
    std::string bgColorStr;
    if (styles.contains(bgCamelKey) && styles[bgCamelKey].is_string()) {
        bgColorStr = styles[bgCamelKey].get<std::string>();
    } else if (styles.contains(bgColorKey) && styles[bgColorKey].is_string()) {
        bgColorStr = styles[bgColorKey].get<std::string>();
    }
    if (!bgColorStr.empty()) {
        node.setBackgroundColor(parseColor(bgColorStr));
    } else {
        // Default to a transparent background.
        node.setBackgroundColor(colors::kColorTransparent);
    }

    // Border radius.
    if (styles.contains("border-radius") && styles["border-radius"].is_string()) {
        float radius = static_cast<float>(std::atof(styles["border-radius"].get<std::string>().c_str()));
        node.setBorderRadius(radius);
    }

    // Uniform border width.
    if (styles.contains("border-width") && styles["border-width"].is_string()) {
        float bw = static_cast<float>(std::atof(styles["border-width"].get<std::string>().c_str()));
        node.setBorderWidth(bw, bw, bw, bw);
        // Enable a solid border when width is non-zero.
        if (bw > 0.0f) {
            node.setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
        }
    }

    // Border color.
    if (styles.contains("border-color") && styles["border-color"].is_string()) {
        node.setBorderColor(parseColor(styles["border-color"].get<std::string>()));
    }

}

// ---- Checks ----

void ButtonComponent::applyChecks(const nlohmann::json& properties) {
    if (!properties.contains("checks") || !properties["checks"].is_object()) {
        return;
    }

    const auto& checks = properties["checks"];
    if (checks.contains("result") && checks["result"].is_boolean()) {
        bool result = checks["result"].get<bool>();
        m_disabled = !result;

        // Keep opacity in sync with checks.result.
        A2UINode(m_nodeHandle).setOpacity(m_disabled ? 0.5f : 1.0f);
    }
}

} // namespace a2ui
