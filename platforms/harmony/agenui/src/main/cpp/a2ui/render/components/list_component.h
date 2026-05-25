#pragma once

#include "../a2ui_component.h"
#include <vector>

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
class ListComponent : public A2UIComponent {
public:
    ListComponent(const std::string& id, const nlohmann::json& properties);
    ~ListComponent() override;

    void addChild(A2UIComponent* child) override;
    void removeChild(A2UIComponent* child) override;
    bool shouldAutoAddChildView() const override { return false; }

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
    virtual void onChildLayoutSizeChanged(A2UIComponent* child) override;

protected:
    void onUpdateProperties(const nlohmann::json& properties) override;

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
    std::vector<std::pair<ArkUI_NodeHandle, ArkUI_NodeHandle>> m_listItemWrappers;
    bool m_scrollable = true;
    bool m_horizontal = false;
    std::string m_align = "stretch";
};

} // namespace a2ui
