#include "list_component.h"
#include "../a2ui_node.h"
#include "a2ui/utils/a2ui_parse_utils.h"
#include "style_parser/agenui_edge_insets_parser.h"
#include "log/a2ui_capi_log.h"
#include <algorithm>

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

    // Horizontal lists use NodeAdapter for lazy node creation / recycling.
    // Vertical lists keep the eager addChild path (unchanged).
    if (m_horizontal) {
        setupNodeAdapter();
    }

    HM_LOGI("ListComponent - Created: id=%s, handle=%s, scrollable=%d, horizontal=%d, adapter=%d",
             id.c_str(), m_nodeHandle ? "valid" : "null", m_scrollable, m_horizontal, m_adapterActive);
}

ListComponent::~ListComponent() {
    // Teardown adapter first (unregisters callbacks, disposes adapter,
    // and cleans up any adapter-managed ListItem wrappers).
    if (m_adapter) {
        teardownNodeAdapter();
    }
    // Clean up any remaining wrappers (eager-mode vertical lists).
    for (auto& [childHandle, listItemHandle] : m_listItemWrappers) {
        if (listItemHandle) {
            g_nodeAPI->disposeNode(listItemHandle);
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

    if (m_adapterActive) {
        // Adapter mode (horizontal list): do NOT eagerly create/mount the
        // ListItem wrapper.  Just inform the adapter that the total item
        // count increased — it will request the node via ON_ADD_NODE when
        // the item scrolls into the viewport.
        OH_ArkUI_NodeAdapter_SetTotalNodeCount(
            m_adapter, static_cast<uint32_t>(m_children.size()));
        HM_LOGI("id=%s adapter addChild, total=%zu (lazy)",
                m_id.c_str(), m_children.size());
        return;
    }

    // Eager mode (vertical list) — unchanged path.
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

    if (m_adapterActive) {
        // Adapter mode: find the child's index, remove from m_children,
        // then tell the adapter to remove that position.
        auto it = std::find(m_children.begin(), m_children.end(), child);
        if (it != m_children.end()) {
            uint32_t index = static_cast<uint32_t>(std::distance(m_children.begin(), it));
            A2UIComponent::removeChild(child);
            OH_ArkUI_NodeAdapter_RemoveItem(m_adapter, index, 1);
            OH_ArkUI_NodeAdapter_SetTotalNodeCount(
                m_adapter, static_cast<uint32_t>(m_children.size()));
            HM_LOGI("id=%s adapter removeChild at index=%u, total=%zu",
                    m_id.c_str(), index, m_children.size());
        }
        return;
    }

    // Eager mode (vertical list) — unchanged path.
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
    parseContainerPadding(properties);

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
        for (auto& [childHandle, listItemHandle] : m_listItemWrappers) {
            A2UINode(listItemHandle).setPercentWidth(1.0f);
        }
    }

    // Apply Yoga-computed spacing between items as ListItem margins.
    // Horizontal lists also need this to honor margin-left/right between items,
    // because ArkUI ListItem stacks children tightly and ignores Yoga main-axis margin.
    updateListItemMargins();

    HM_LOGD("id=%s, properties=%s", m_id.c_str(), properties.dump().c_str());
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

    // Handle adapter ↔ eager migration on direction change.
    if (nowHorizontal && !m_adapterActive) {
        // vertical → horizontal: switch to adapter (lazy) mode.
        m_horizontal = true;
        migrateToAdapterMode();
    } else if (!nowHorizontal && m_adapterActive) {
        // horizontal → vertical: switch to eager mode.
        m_horizontal = false;
        migrateToEagerMode();
    } else {
        m_horizontal = nowHorizontal;
    }

    A2UIListNode node(m_nodeHandle);
    if (m_horizontal) {
        node.setScrollDirectionHorizontal();
    } else {
        node.setScrollDirectionVertical();
    }

    for (auto& [childHandle, listItemHandle] : m_listItemWrappers) {
        if (!m_horizontal) {
            // Vertical list: ListItem fills list width.
            A2UINode(listItemHandle).setPercentWidth(1.0f);
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
    return a2ui::parseCssLength(val, fallback);
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

    bool isLast = false;
    for (size_t i = 0; i < m_children.size(); ++i) {
        auto* child = m_children[i];
        if (!child) continue;

        ArkUI_NodeHandle listItemHandle = findListItemWrapper(child->getNodeHandle());
        if (!listItemHandle) continue;

        isLast = (i == m_children.size() - 1);

        float marginTop = 0.0f;
        float marginRight = 0.0f;
        float marginBottom = 0.0f;
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

        // Last item: append the container's padding-right/bottom so
        // scrolling to the end reveals the trailing padding gutter.
        if (isLast) {
            marginRight = m_paddingRight;
            marginBottom = m_paddingBottom;
        }

        if (marginTop < 0.0f) marginTop = 0.0f;
        if (marginLeft < 0.0f) marginLeft = 0.0f;

        A2UINode(listItemHandle).setMargin(marginTop, marginRight, marginBottom, marginLeft);
    }
}

// ---- Container Padding ----
// Yoga bakes padding-left/top into child positions, but padding-right/bottom
// must be explicitly added as trailing margins on the last ListItem so the
// scroll gutter is visible when scrolling to the end.

void ListComponent::parseContainerPadding(const nlohmann::json& properties) {
    if (!properties.contains("styles") || !properties["styles"].is_object()) {
        return;
    }
    const auto& styles = properties["styles"];

    float paddingRight = 0.0f;
    float paddingBottom = 0.0f;

    if (styles.contains("padding") && styles["padding"].is_string()) {
        agenui::EdgeInsets insets{};
        if (agenui::EdgeInsetsParser::parse(styles["padding"].get<std::string>(), insets)) {
            if (insets.right.unit == agenui::EdgeInsetUnit::Px) {
                paddingRight = insets.right.value;
            }
            if (insets.bottom.unit == agenui::EdgeInsetUnit::Px) {
                paddingBottom = insets.bottom.value;
            }
        }
    }
    if (styles.contains("padding-right")) {
        paddingRight = parseCssLength(styles["padding-right"], paddingRight);
    }
    if (styles.contains("paddingRight")) {
        paddingRight = parseCssLength(styles["paddingRight"], paddingRight);
    }
    if (styles.contains("padding-bottom")) {
        paddingBottom = parseCssLength(styles["padding-bottom"], paddingBottom);
    }
    if (styles.contains("paddingBottom")) {
        paddingBottom = parseCssLength(styles["paddingBottom"], paddingBottom);
    }

    m_paddingRight = paddingRight;
    m_paddingBottom = paddingBottom;
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
    m_listItemWrappers[childHandle] = listItemHandle;
    return listItemHandle;
}

ArkUI_NodeHandle ListComponent::findListItemWrapper(ArkUI_NodeHandle childHandle) const {
    auto it = m_listItemWrappers.find(childHandle);
    return it != m_listItemWrappers.end() ? it->second : nullptr;
}

void ListComponent::removeListItemWrapper(ArkUI_NodeHandle childHandle) {
    m_listItemWrappers.erase(childHandle);
}

// ---- NodeAdapter Lifecycle ----

void ListComponent::setupNodeAdapter() {
    if (m_adapter) {
        return;  // already active
    }
    m_adapter = OH_ArkUI_NodeAdapter_Create();
    if (!m_adapter) {
        HM_LOGE("ListComponent::setupNodeAdapter - OH_ArkUI_NodeAdapter_Create failed, id=%s "
                "(falling back to eager mode)", m_id.c_str());
        return;
    }

    OH_ArkUI_NodeAdapter_RegisterEventReceiver(m_adapter, this, &ListComponent::onAdapterEvent);

    // Disable prefetch — only create nodes for items actually entering the
    // viewport (matches Android setItemPrefetchEnabled(false)).
    A2UIListNode node(m_nodeHandle);
    node.setCachedCount(0);
    node.setNodeAdapter(m_adapter);

    m_adapterActive = true;
    HM_LOGI("ListComponent::setupNodeAdapter - adapter bound, id=%s", m_id.c_str());
}

void ListComponent::teardownNodeAdapter() {
    if (!m_adapter) {
        return;
    }
    m_adapterActive = false;

    // Unregister callbacks BEFORE disposing so no stray callbacks fire.
    OH_ArkUI_NodeAdapter_UnregisterEventReceiver(m_adapter);

    // Unbind adapter from the List node.
    A2UIListNode node(m_nodeHandle);
    node.resetNodeAdapter();

    // Clean up any ListItem wrappers that the adapter is currently managing.
    // The child content nodes are NOT disposed here — they are owned by
    // A2UIComponent and merely detached from their wrappers.
    for (auto& [childHandle, listItemHandle] : m_listItemWrappers) {
        if (listItemHandle && childHandle) {
            g_nodeAPI->removeChild(listItemHandle, childHandle);
        }
        if (listItemHandle) {
            g_nodeAPI->disposeNode(listItemHandle);
        }
    }
    m_listItemWrappers.clear();

    OH_ArkUI_NodeAdapter_Dispose(m_adapter);
    m_adapter = nullptr;

    HM_LOGI("ListComponent::teardownNodeAdapter - adapter disposed, id=%s", m_id.c_str());
}

void ListComponent::migrateToAdapterMode() {
    // Remove all eager ListItem wrappers from the List node and detach
    // child content nodes (children persist, only wrappers are destroyed).
    for (auto& [childHandle, listItemHandle] : m_listItemWrappers) {
        if (listItemHandle) {
            g_nodeAPI->removeChild(m_nodeHandle, listItemHandle);
            if (childHandle) {
                g_nodeAPI->removeChild(listItemHandle, childHandle);
            }
            g_nodeAPI->disposeNode(listItemHandle);
        }
    }
    m_listItemWrappers.clear();

    // Activate adapter and set total count so it starts requesting nodes.
    setupNodeAdapter();
    if (m_adapter) {
        OH_ArkUI_NodeAdapter_SetTotalNodeCount(
            m_adapter, static_cast<uint32_t>(m_children.size()));
    }
    HM_LOGI("ListComponent::migrateToAdapterMode - migrated to adapter, id=%s, children=%zu",
            m_id.c_str(), m_children.size());
}

void ListComponent::migrateToEagerMode() {
    teardownNodeAdapter();

    // Re-add every child eagerly (same as the vertical constructor path).
    for (auto* child : m_children) {
        if (child && child->getNodeHandle()) {
            ArkUI_NodeHandle listItemHandle = createListItemWrapper(child->getNodeHandle());
            g_nodeAPI->addChild(m_nodeHandle, listItemHandle);
        }
    }
    HM_LOGI("ListComponent::migrateToEagerMode - migrated to eager, id=%s, children=%zu",
            m_id.c_str(), m_children.size());
}

// ---- NodeAdapter Event Callback (static) ----

void ListComponent::onAdapterEvent(ArkUI_NodeAdapterEvent* event) {
    void* userData = OH_ArkUI_NodeAdapterEvent_GetUserData(event);
    auto* self = static_cast<ListComponent*>(userData);
    if (!self || !self->m_adapterActive) {
        return;
    }

    ArkUI_NodeAdapterEventType type = OH_ArkUI_NodeAdapterEvent_GetType(event);
    switch (type) {
        case NODE_ADAPTER_EVENT_ON_GET_NODE_ID: {
            uint32_t index = OH_ArkUI_NodeAdapterEvent_GetItemIndex(event);
            OH_ArkUI_NodeAdapterEvent_SetNodeId(event, static_cast<int32_t>(index));
            break;
        }
        case NODE_ADAPTER_EVENT_ON_ADD_NODE_TO_ADAPTER: {
            uint32_t index = OH_ArkUI_NodeAdapterEvent_GetItemIndex(event);
            self->handleAdapterAddNode(index, event);
            break;
        }
        case NODE_ADAPTER_EVENT_ON_REMOVE_NODE_FROM_ADAPTER: {
            ArkUI_NodeHandle removedNode = OH_ArkUI_NodeAdapterEvent_GetRemovedNode(event);
            self->handleAdapterRemoveNode(removedNode);
            break;
        }
        default:
            break;
    }
}

void ListComponent::handleAdapterAddNode(uint32_t index, ArkUI_NodeAdapterEvent* event) {
    if (index >= m_children.size()) {
        HM_LOGW("ListComponent::handleAdapterAddNode - index %u out of range (%zu), id=%s",
                index, m_children.size(), m_id.c_str());
        return;
    }

    A2UIComponent* child = m_children[index];
    if (!child || !child->getNodeHandle()) {
        HM_LOGW("ListComponent::handleAdapterAddNode - null child/handle at index %u, id=%s",
                index, m_id.c_str());
        return;
    }

    // createListItemWrapper creates the ListItem, adds the child node to it,
    // configures width (wrap_content for horizontal), and tracks the mapping.
    ArkUI_NodeHandle listItemHandle = createListItemWrapper(child->getNodeHandle());

    // Apply the child's current Yoga-computed size to the wrapper so the
    // List can measure scrollable content correctly from the first frame.
    A2UINode itemNode(listItemHandle);
    if (m_horizontal) {
        if (child->getWidth() > 0.0f) {
            itemNode.setWidth(child->getWidth());
        }
        if (child->getHeight() > 0.0f) {
            itemNode.setHeight(child->getHeight());
        }
    }

    // Apply cross-axis offset (horizontal list: y only; x is managed by List).
    onApplyChildPosition(child, child->getX(), child->getY());

    // Recompute margins for all visible items (cheap — only visible wrappers
    // exist in m_listItemWrappers at any time).
    updateListItemMargins();

    // Hand the ListItem wrapper to the adapter.
    OH_ArkUI_NodeAdapterEvent_SetItem(event, listItemHandle);

    HM_LOGI("ListComponent::handleAdapterAddNode - index=%u child=%s wrapped, id=%s",
            index, child->getId().c_str(), m_id.c_str());

    // Fire exposure event: child is now bound to a viewport slot
    if (!child->getId().empty() && !child->getProperties().empty()) {
        this->notifyAppeared("List", child->getProperties());
    }
}

void ListComponent::handleAdapterRemoveNode(ArkUI_NodeHandle listItemHandle) {
    if (!listItemHandle) {
        return;
    }

    // Reverse lookup: find which child content was hosted in this wrapper.
    ArkUI_NodeHandle childHandle = nullptr;
    for (auto& [ch, listItem] : m_listItemWrappers) {
        if (listItem == listItemHandle) {
            childHandle = ch;
            break;
        }
    }

    // Detach the child content node from the wrapper (child persists).
    if (childHandle) {
        g_nodeAPI->removeChild(listItemHandle, childHandle);
        removeListItemWrapper(childHandle);
    }

    // Dispose the ListItem wrapper — this is the actual recycling: the
    // lightweight shell node is destroyed, freeing native resources.
    g_nodeAPI->disposeNode(listItemHandle);

    HM_LOGI("ListComponent::handleAdapterRemoveNode - wrapper recycled, id=%s", m_id.c_str());
}

} // namespace a2ui
