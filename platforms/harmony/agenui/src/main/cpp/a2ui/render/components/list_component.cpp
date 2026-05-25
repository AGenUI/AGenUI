#include "list_component.h"
#include "../a2ui_node.h"
#include "log/a2ui_capi_log.h"

namespace a2ui {

// ---- Construction / Destruction ----

ListComponent::ListComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, "List") {

    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_LIST);

    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
        if (properties.contains("direction") && properties["direction"].is_string()) {
            m_horizontal = (properties["direction"].get<std::string>() == "horizontal");
        }
        if (properties.contains("scrollable") && properties["scrollable"].is_boolean()) {
            m_scrollable = properties["scrollable"].get<bool>();
        }
        if (properties.contains("align") && properties["align"].is_string()) {
            m_align = properties["align"].get<std::string>();
        }
    }

    A2UIListNode node(m_nodeHandle);
    node.setScrollBarDisplayOff();
    if (m_horizontal) {
        node.setScrollDirectionHorizontal();
    } else {
        node.setScrollDirectionVertical();
    }
    if (m_scrollable) {
        node.setEdgeEffectSpring();
    } else {
        node.setEdgeEffectNone();
    }

    // Item space always 0 (gap not supported on Harmony).
    node.setItemSpace(0.0f);

    // Only horizontal lists allow gesture scrolling.
    node.setScrollInteraction(m_horizontal);

    // Set width/height early so Yoga layout uses the correct constraint
    // from the very first calculation. updateLayoutProperties will override
    // with any user-explicit width/height later.
    if (!m_horizontal) {
        node.setPercentWidth(1.0f);
    }

    HM_LOGI("ListComponent - Created: id=%s, handle=%s, scrollable=%d, horizontal=%d",
             id.c_str(), m_nodeHandle ? "valid" : "null", m_scrollable, m_horizontal);
}

ListComponent::~ListComponent() {
    for (auto& pair : m_listItemWrappers) {
        if (pair.second) {
            g_nodeAPI->disposeNode(pair.second);
        }
    }
    m_listItemWrappers.clear();
    HM_LOGI("ListComponent - Destroyed: id=%s", m_id.c_str());
}

// ---- Child Management ----

void ListComponent::addChild(A2UIComponent* child) {
    if (!child) {
        return;
    }
    A2UIComponent::addChild(child);

    if (!m_nodeHandle || !child->getNodeHandle()) {
        return;
    }

    ArkUI_NodeHandle listItemHandle = createListItemWrapper(child->getNodeHandle());
    g_nodeAPI->addChild(m_nodeHandle, listItemHandle);

    HM_LOGI("id=%s wrapped child=%s in ListItem",
             m_id.c_str(), child->getId().c_str());
}

void ListComponent::removeChild(A2UIComponent* child) {
    if (!child) {
        return;
    }
    if (m_nodeHandle && child->getNodeHandle()) {
        ArkUI_NodeHandle listItemHandle = findListItemWrapper(child->getNodeHandle());
        if (listItemHandle) {
            g_nodeAPI->removeChild(m_nodeHandle, listItemHandle);
            g_nodeAPI->disposeNode(listItemHandle);
            removeListItemWrapper(child->getNodeHandle());
        }
    }
    A2UIComponent::removeChild(child);

    HM_LOGI("id=%s removed child=%s",
             m_id.c_str(), child->getId().c_str());
}

// ---- Property Updates ----

void ListComponent::onUpdateProperties(const nlohmann::json& properties) {
    if (!m_nodeHandle) {
        HM_LOGE("handle is null, id=%s", m_id.c_str());
        return;
    }

    applyDirection(properties);
    applyScrollable(properties);
    applyAlign(properties);
    applyStyles(properties);

    // Sync ListItem cross-axis size to match Yoga layout.
    // Vertical list: ListItem width = 100% (children fill the list width).
    // Horizontal list: ListItem height is governed by Yoga (no percent override)
    // so that items remain vertically centered per the Yoga-computed position.
    bool userSetWidth = false;
    const std::string& styleInfo = getStyleInfo();
    if (!styleInfo.empty()) {
        try {
            auto styleInfoJson = nlohmann::json::parse(styleInfo);
            userSetWidth = styleInfoJson.contains("width");
        } catch (const nlohmann::json::exception& e) {
            HM_LOGW("Failed to parse styleInfo for %s: %s", m_id.c_str(), e.what());
        }
    }

    // Vertical list: ListItem width = 100% so children fill the list width.
    if (!userSetWidth && !m_horizontal) {
        for (auto& pair : m_listItemWrappers) {
            A2UINode(pair.second).setPercentWidth(1.0f);
        }
    }

    // Apply Yoga-computed spacing between items as ListItem margins.
    // Horizontal lists also need this to honor margin-left/right between items,
    // because ArkUI ListItem stacks children tightly and ignores Yoga main-axis margin.
    updateListItemMargins();

    HM_LOGI("id=%s, properties=%s", m_id.c_str(), properties.dump().c_str());
}

// ---- direction ----

void ListComponent::applyDirection(const nlohmann::json& properties) {
    if (!properties.contains("direction") || !properties["direction"].is_string()) {
        return;
    }

    const std::string& dir = properties["direction"].get<std::string>();
    bool nowHorizontal = (dir == "horizontal");
    if (nowHorizontal == m_horizontal) {
        return;
    }
    m_horizontal = nowHorizontal;

    A2UIListNode node(m_nodeHandle);
    if (m_horizontal) {
        node.setScrollDirectionHorizontal();
    } else {
        node.setScrollDirectionVertical();
    }

    for (auto& pair : m_listItemWrappers) {
        if (!m_horizontal) {
            // Vertical list: ListItem fills list width.
            A2UINode(pair.second).setPercentWidth(1.0f);
        }
        // Horizontal list: ListItem height governed by Yoga, no percent override.
    }

    // Only horizontal lists allow gesture scrolling.
    node.setScrollInteraction(m_horizontal);
}

// ---- scrollable ----

void ListComponent::applyScrollable(const nlohmann::json& properties) {
    if (!properties.contains("scrollable") || !properties["scrollable"].is_boolean()) {
        return;
    }

    bool newScrollable = properties["scrollable"].get<bool>();
    if (newScrollable == m_scrollable) {
        return;
    }
    m_scrollable = newScrollable;

    A2UIListNode node(m_nodeHandle);
    if (m_scrollable) {
        node.setEdgeEffectSpring();
    } else {
        node.setEdgeEffectNone();
    }
}

// ---- align ----

void ListComponent::applyAlign(const nlohmann::json& properties) {
    if (!properties.contains("align") || !properties["align"].is_string()) {
        return;
    }
    m_align = properties["align"].get<std::string>();
    // Yoga re-layout will push updated x/y via onApplyChildPosition on the next frame.
}

// ---- onApplyChildPosition ----

void ListComponent::onApplyChildPosition(A2UIComponent* child, float x, float y) {
    if (!child) return;

    if (m_horizontal) {
        child->getNode().setPosition(0.0f, y);
    } else {
        child->getNode().setPosition(x, 0.0f);
    }

    // Yoga has finished computing this child's main-axis offset; recompute
    // ListItem margins so the gap between items reflects margin-left/right.
    updateListItemMargins();
}

void ListComponent::onChildLayoutSizeChanged(A2UIComponent* child) {
    if (!child) return;

    ArkUI_NodeHandle listItemHandle = findListItemWrapper(child->getNodeHandle());
    if (!listItemHandle) return;

    A2UINode itemNode(listItemHandle);
    if (m_horizontal) {
        // Horizontal list: sync ListItem width to child's Yoga-computed width.
        if (child->getWidth() > 0.0f) {
            itemNode.setWidth(child->getWidth());
        }
        // Height is governed by Yoga cross-axis layout.
        if (child->getHeight() > 0.0f) {
            itemNode.setHeight(child->getHeight());
        }
    } else {
        // Vertical list: ListItem width is always 100%, only sync height.
        if (child->getHeight() > 0.0f) {
            itemNode.setHeight(child->getHeight());
        }
    }

    // Width change can shift sibling positions; recompute item margins.
    updateListItemMargins();
}

// ---- CSS Length Parser ----

float ListComponent::parseCssLength(const nlohmann::json& val, float fallback) {
    if (val.is_number()) {
        return val.get<float>();
    }
    if (val.is_string()) {
        std::string s = val.get<std::string>();
        if (s.size() > 2 && s.substr(s.size() - 2) == "px") {
            s = s.substr(0, s.size() - 2);
        }
        float f = static_cast<float>(std::atof(s.c_str()));
        return f >= 0.0f ? f : fallback;
    }
    return fallback;
}

// ---- Styles (border, background) ----

void ListComponent::applyStyles(const nlohmann::json& properties) {
    if (!properties.contains("styles") || !properties["styles"].is_object()) {
        return;
    }

    const auto& styles = properties["styles"];
    A2UINode node(m_nodeHandle);

    // border-width
    {
        float bw = -1.0f;
        if (styles.contains("border-width")) {
            bw = parseCssLength(styles["border-width"], -1.0f);
        } else if (styles.contains("borderWidth")) {
            bw = parseCssLength(styles["borderWidth"], -1.0f);
        }
        if (bw >= 0.0f) {
            node.setBorderWidth(bw, bw, bw, bw);
            node.setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
        }
    }

    // border-color
    {
        std::string bc;
        if (styles.contains("border-color") && styles["border-color"].is_string()) {
            bc = styles["border-color"].get<std::string>();
        } else if (styles.contains("borderColor") && styles["borderColor"].is_string()) {
            bc = styles["borderColor"].get<std::string>();
        }
        if (!bc.empty()) {
            node.setBorderColor(parseColor(bc));
        }
    }

    // border-radius
    {
        float br = -1.0f;
        if (styles.contains("border-radius")) {
            br = parseCssLength(styles["border-radius"], -1.0f);
        } else if (styles.contains("borderRadius")) {
            br = parseCssLength(styles["borderRadius"], -1.0f);
        }
        if (br >= 0.0f) {
            node.setBorderRadius(br);
        }
    }

    // background-color
    {
        std::string bgStr;
        if (styles.contains("background-color") && styles["background-color"].is_string()) {
            bgStr = styles["background-color"].get<std::string>();
        } else if (styles.contains("backgroundColor") && styles["backgroundColor"].is_string()) {
            bgStr = styles["backgroundColor"].get<std::string>();
        }
        if (!bgStr.empty()) {
            node.setBackgroundColor(parseColor(bgStr));
        }
    }

    HM_LOGI("ListComponent::applyStyles applied, id=%s", m_id.c_str());
}

// ---- ListItem Margin Compensation ----
//
// ArkUI ListItem stacks children automatically, ignoring Yoga's main-axis
// margin/padding. We convert the Yoga-computed spacing into ListItem margins
// so the visual gap between items matches Android.

void ListComponent::updateListItemMargins() {
    if (m_children.empty()) return;

    for (size_t i = 0; i < m_children.size(); ++i) {
        auto* child = m_children[i];
        if (!child) continue;

        ArkUI_NodeHandle listItemHandle = findListItemWrapper(child->getNodeHandle());
        if (!listItemHandle) continue;

        float marginTop = 0.0f;
        float marginLeft = 0.0f;
        if (i == 0) {
            // First item: margin = Yoga main-axis offset (includes list padding).
            if (m_horizontal) {
                marginLeft = child->getX();
            } else {
                marginTop = child->getY();
            }
        } else {
            auto* prev = m_children[i - 1];
            if (prev) {
                if (m_horizontal) {
                    // Gap = current x - (prev x + prev width)
                    marginLeft = child->getX() - (prev->getX() + prev->getWidth());
                } else {
                    // Gap = current y - (prev y + prev height)
                    marginTop = child->getY() - (prev->getY() + prev->getHeight());
                }
            }
        }
        if (marginTop < 0.0f) marginTop = 0.0f;
        if (marginLeft < 0.0f) marginLeft = 0.0f;

        A2UINode(listItemHandle).setMargin(marginTop, 0.0f, 0.0f, marginLeft);
    }
}

// ---- Helper Methods ----

bool ListComponent::isHorizontal() const {
    return m_horizontal;
}

ArkUI_NodeHandle ListComponent::createListItemWrapper(ArkUI_NodeHandle childHandle) {
    ArkUI_NodeHandle listItemHandle = g_nodeAPI->createNode(ARKUI_NODE_LIST_ITEM);
    g_nodeAPI->addChild(listItemHandle, childHandle);
    if (!m_horizontal) {
        // Vertical list: ListItem fills the list width so children stretch horizontally.
        A2UINode(listItemHandle).setPercentWidth(1.0f);
    } else {
        // Horizontal list: let ListItem width wrap its child so the ArkUI List
        // can sum up the correct content width for scrolling.
        A2UINode(listItemHandle).setWidth(-2.0f);  // wrap_content
    }
    m_listItemWrappers.push_back({childHandle, listItemHandle});
    return listItemHandle;
}

ArkUI_NodeHandle ListComponent::findListItemWrapper(ArkUI_NodeHandle childHandle) const {
    for (const auto& pair : m_listItemWrappers) {
        if (pair.first == childHandle) {
            return pair.second;
        }
    }
    return nullptr;
}

void ListComponent::removeListItemWrapper(ArkUI_NodeHandle childHandle) {
    for (auto it = m_listItemWrappers.begin(); it != m_listItemWrappers.end(); ++it) {
        if (it->first == childHandle) {
            m_listItemWrappers.erase(it);
            return;
        }
    }
}

} // namespace a2ui
