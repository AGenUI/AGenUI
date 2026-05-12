#include "agenui_tabs_yoga_helper.h"
#include "surface/virtual_dom/agenui_virtual_dom_node.h"
#include "surface/agenui_serializable_data_impl.h"
#include "agenui_logger_internal.h"
#include <yoga/Yoga.h>
#include <cmath>

namespace agenui {

void TabsYogaHelper::injectFlexGrowIfNeeded(ComponentSnapshot& snapshot) {
    if (snapshot.component != "Tabs") {
        return;
    }
    // If styles does not explicitly set flex-grow, inject flex-grow:1 so Tabs fills its parent.
    // Must be called before convertToYoga / applySnapshot,
    // so that StyleDefaults' flex-grow:0 does not override the injected value.
    if (snapshot.styles.find("flex-grow") == snapshot.styles.end()) {
        snapshot.styles["flex-grow"] =
            SerializableData(SerializableData::Impl::parse("1"));
        AGENUI_LOG("[TabsYogaHelper] injected flex-grow:1 for Tabs id=%s",
                   snapshot.id.c_str());
    }
}

void TabsYogaHelper::setChildAbsoluteLayout(YogaNode& yogaNode,
                                            const std::string& childId) {
    // Tabs direct child: set to Yoga absolute layout, removing from flex flow.
    // top  = kTabBarHeight (content area starts below tabBar)
    // left = 0, width = 100%
    // Note: child Yoga coordinates are not applied to ArkUI nodes (shouldApplyChildLayoutPosition/Size both return false).
    // Yoga-layer absolute layout is only used by updateMinHeightRecursive to read contentH;
    // ArkUI-layer child width/height is determined by contentContainer's COLUMN flex naturally.
    YGNodeStyleSetPositionType(yogaNode.get(), YGPositionTypeAbsolute);
    YGNodeStyleSetPosition(yogaNode.get(), YGEdgeTop, kTabBarHeight);
    YGNodeStyleSetPosition(yogaNode.get(), YGEdgeLeft, 0.0f);
    YGNodeStyleSetWidthPercent(yogaNode.get(), 100.0f);
    AGENUI_LOG("[TabsYogaHelper] child id=%s set absolute top=%.0f left=0 width=100%%",
               childId.c_str(), kTabBarHeight);
}

bool TabsYogaHelper::updateMinHeightRecursive(
    std::shared_ptr<VirtualDOMNode> node,
    const std::map<std::string, int>& selectedIndices,
    float surfaceWidth) {

    (void)surfaceWidth;  // After switching child to flex layout, no need for independent child layout

    if (!node) return false;

    bool anyUpdated = false;
    const ComponentSnapshot* snap = node->getSnapshot();

    if (snap && snap->component == "Tabs") {
        YogaNode* tabsYoga = node->getYogaNodeObj();
        if (tabsYoga) {
            const auto& children = node->getChildren();

            // Read the actual height of the selected child after global layout
            int selectedIndex = 0;
            auto it = selectedIndices.find(snap->id);
            if (it != selectedIndices.end()) {
                selectedIndex = it->second;
            }

            float contentH = 0.0f;
            if (selectedIndex >= 0 &&
                selectedIndex < static_cast<int>(children.size())) {
                YogaNode* childYoga = children[selectedIndex]->getYogaNodeObj();
                if (childYoga) {
                    contentH = childYoga->getLayoutHeight();
                }
            }

            float newMinH = kTabBarHeight + contentH;
            float curH    = tabsYoga->getLayoutHeight();

            AGENUI_LOG("[TabsYogaHelper] id=%s selectedIndex=%d "
                       "contentH=%.1f curH=%.1f newMinH=%.1f",
                       snap->id.c_str(), selectedIndex,
                       contentH, curH, newMinH);
            
            // Whether height increases or decreases, update minHeight and maxHeight to match
            // the currently selected tab's content height.
            // Also set height=auto (clear fixed height) so Yoga uses minHeight as the baseline for shrinking.
            // (Tab switching may increase or decrease content height)
            if (std::fabs(newMinH - curH) > 0.5f) {
                YGNodeStyleSetMinHeight(tabsYoga->get(), newMinH);
                YGNodeStyleSetMaxHeight(tabsYoga->get(), newMinH);
                anyUpdated = true;
            }
        }
    }

    // Recursively process children
    for (const auto& child : node->getChildren()) {
        if (updateMinHeightRecursive(child, selectedIndices, surfaceWidth)) {
            anyUpdated = true;
        }
    }
    return anyUpdated;
}

}  // namespace agenui
