#pragma once

#include <map>
#include <memory>
#include <string>

#include "agenui_yoga_node.h"
#include "surface/virtual_dom/agenui_component_snapshot.h"

namespace agenui {

// Forward declaration -- avoid mutual include with VirtualDOMNode
class VirtualDOMNode;

/**
 * @brief Yoga layout helper for Tabs component
 *
 * Encapsulates all Tabs-related Yoga layout operations to avoid scattering
 * Tabs-specific logic across VirtualDOMNode / VirtualDOM.
 *
 * Design principles:
 *  - All methods are static, no instance state
 *  - Does not depend on VirtualDOMNode/VirtualDOM private members; operates via public interfaces
 *  - Called by VirtualDOMNode::setSnapshot and VirtualDOM
 *
 * Constants:
 *   kTabBarHeight = 96.0f  -- fixed tab bar height (consistent with tabs_component.cpp)
 */
class TabsYogaHelper {
public:
    /** Fixed tab bar height (vp), consistent with rendering layer tabs_component.cpp */
    static constexpr float kTabBarHeight = 96.0f;

    /**
     * @brief For Tabs own snapshot: inject flex-grow:1 if styles doesn't explicitly set it
     *
     * Must be called before applySnapshot / convertToYoga to ensure Tabs fills its parent.
     * If snapshot's component is not "Tabs", this is a no-op.
     *
     * @param snapshot Tabs component snapshot (modifies styles in-place)
     */
    static void injectFlexGrowIfNeeded(ComponentSnapshot& snapshot);

    /**
     * @brief For Tabs direct children: set Yoga node to absolute layout
     *
     * Child is removed from flex flow; Yoga independently lays out its subtree
     * without affecting Tabs own flex size calculation.
     *   - positionType = Absolute
     *   - top          = kTabBarHeight (content area starts right below tab bar)
     *   - left         = 0
     *   - width        = 100%
     *
     * @param yogaNode Child's YogaNode (directly operates underlying YGNodeRef)
     * @param childId  For logging, child node id
     */
    static void setChildAbsoluteLayout(YogaNode& yogaNode,
                                       const std::string& childId);

    /**
     * @brief Recursively traverse VirtualDOM tree, updating Yoga minHeight for all Tabs nodes
     *
     * Two-phase strategy:
     *  1. Find each Tabs node
     *  2. For each direct child, execute YGNodeCalculateLayout independently (pass surfaceWidth as width)
     *     so that inner leaf nodes (Text/Image etc.) are correctly measured first
     *  3. Read the selected child's Yoga layoutHeight
     *  4. If kTabBarHeight + contentH > current layoutHeight, write minHeight and markDirty
     *
     * Caller must perform a global secondary calculateLayout after this method returns true.
     *
     * @param node            Current traversal node (caller passes root node)
     * @param selectedIndices tabsId -> selectedIndex mapping (held by VirtualDOM)
     * @param surfaceWidth    Surface width, used for independent child layout calculation
     * @return Whether any Tabs node was updated (true means secondary calculateLayout needed)
     */
    static bool updateMinHeightRecursive(
        std::shared_ptr<VirtualDOMNode> node,
        const std::map<std::string, int>& selectedIndices,
        float surfaceWidth);
};

}  // namespace agenui
