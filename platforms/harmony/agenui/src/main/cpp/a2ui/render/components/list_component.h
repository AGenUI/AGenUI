#pragma once

#include "../a2ui_component.h"
#include <unordered_map>

namespace a2ui {

/**
 * List component (list layout with scrolling support)
 * Implemented with HarmonyOS ArkUI C-API ARKUI_NODE_LIST. Each child node is
 * wrapped automatically in ARKUI_NODE_LIST_ITEM.
 *
 * Cross-axis alignment is implemented by applying the Yoga-computed x offset
 * (for vertical lists) or y offset (for horizontal lists) directly to the
 * child ArkUI node via NODE_POSITION.
 *
 * Supported properties:
 *   - direction:  Layout direction - vertical (default), horizontal
 *   - align:      Cross-axis alignment - start (default), center, end, stretch
 *   - scrollable: Scrollable - true (default), false
 */
class ListComponent final : public A2UIComponent {
public:
    ListComponent(const std::string& id, const nlohmann::json& properties);
    ~ListComponent() override;

    void addChild(A2UIComponent* child) override;
    void removeChild(A2UIComponent* child) override;
    bool shouldAutoAddChildView() const override { return false; }
    bool shouldCreateChildView() const override { return !m_horizontal; }

    /**
     * The framework must not apply absolute (List-relative) y coordinates to
     * child nodes. ListItem stacking is managed by ArkUI List. We apply only
     * the cross-axis offset via onApplyChildPosition, which sets position(x,0)
     * for vertical lists.
     */
    bool shouldApplyChildLayoutPosition(const A2UIComponent* child) const override {
        (void)child;
        return false;
    }

    /**
     * Apply only the Yoga cross-axis offset to the child node.
     * Vertical list:   x = Yoga align-items offset; y is always 0 (ListItem stacks).
     * Horizontal list: y = Yoga align-items offset; x is always 0.
     */
    void onApplyChildPosition(A2UIComponent* child, float x, float y) override;
    void onChildLayoutSizeChanged(A2UIComponent* child) override;

protected:
    void onUpdateProperties(const nlohmann::json& properties) override;

    /**
     * Tear down the NodeAdapter and dispose ListItem wrappers while
     * m_nodeHandle and all child handles are still valid.
     *
     * This MUST run in onDestroy() (called at the very beginning of
     * A2UIComponent::destroy()) rather than in ~ListComponent(), because by
     * the time the destructor runs, destroy() has already disposed
     * m_nodeHandle and all child node handles — using them would be UAF.
     */
    void onDestroy() override;

private:
    void applyScrollable(const nlohmann::json& properties);
    void applyDirection(const nlohmann::json& properties);
    void applyAlign(const nlohmann::json& properties);
    void applyStyles(const nlohmann::json& properties);
    void updateListItemMargins();
    static float parseCssLength(const nlohmann::json& val, float fallback);
    bool isHorizontal() const;

    ArkUI_NodeHandle createListItemWrapper(ArkUI_NodeHandle childHandle);
    ArkUI_NodeHandle findListItemWrapper(ArkUI_NodeHandle childHandle) const;
    void removeListItemWrapper(ArkUI_NodeHandle childHandle);

    // Maps child handle -> ListItem handle
    std::unordered_map<ArkUI_NodeHandle, ArkUI_NodeHandle> m_listItemWrappers;
    bool m_scrollable = true;
    bool m_horizontal = false;
    std::string m_align = "stretch";
    float m_paddingRight = 0.0f;
    float m_paddingBottom = 0.0f;

    void parseContainerPadding(const nlohmann::json& properties);

    // ---- NodeAdapter (horizontal list lazy-load / recycling) ----
    // When m_adapterActive is true, the List uses OH_ArkUI_NodeAdapter to
    // create ListItem wrappers on-demand (only for visible items) and
    // recycle them when they scroll out of the viewport.  Vertical lists
    // keep the eager addChild path and are completely unaffected.
    ArkUI_NodeAdapterHandle m_adapter = nullptr;
    bool m_adapterActive = false;

    void setupNodeAdapter();
    void teardownNodeAdapter();
    void migrateToAdapterMode();
    void migrateToEagerMode();

    // Static callback registered with OH_ArkUI_NodeAdapter_RegisterEventReceiver.
    static void onAdapterEvent(ArkUI_NodeAdapterEvent* event);

    // Helpers invoked from onAdapterEvent (instance methods).
    void handleAdapterAddNode(uint32_t index, ArkUI_NodeAdapterEvent* event);
    void handleAdapterRemoveNode(ArkUI_NodeHandle listItemHandle);
};

} // namespace a2ui
