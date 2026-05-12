#pragma once

#include "agenui_yoga_node.h"
#include <memory>
#include <map>
#include <string>

namespace agenui {

class VirtualDOMNode;  // Forward declaration for calculateLayoutWithAdjust

/**
 * @brief Unified lifecycle manager for Yoga nodes
 *
 * Held by VirtualDOM, responsible for:
 *  1. Creating/destroying YogaNode for each VirtualDOMNode (by nodeId)
 *  2. Providing insertChild/removeChild wrappers; nodes cannot directly operate YogaNode
 *  3. Triggering global layout calculation via calculateLayout (using _nodes["root"] as root)
 *
 * Threading model: same as VirtualDOMNode, operated on message thread, no additional lock protection.
 */
class YogaNodeManager {
public:
    YogaNodeManager();
    ~YogaNodeManager();
    // On destruction, calls clearAll() internally to avoid UAF from uncertain _nodes map batch destruction order.

    // Non-copyable and non-movable
    YogaNodeManager(const YogaNodeManager&)            = delete;
    YogaNodeManager& operator=(const YogaNodeManager&) = delete;

    /**
     * @brief Create and return a YogaNode bound to nodeId (returns existing one if already present)
     * @param nodeId VirtualDOMNode id
     * @return Raw pointer to YogaNode, lifetime managed by YogaNodeManager
     */
    YogaNode* createNode(const std::string& nodeId);

    /**
     * @brief Get an existing YogaNode
     * @return Raw pointer if found, nullptr if not found
     */
    YogaNode* getNode(const std::string& nodeId) const;

    /**
     * @brief Destroy a specific node (should be called after removing from parent)
     */
    void removeNode(const std::string& nodeId);

    /**
     * @brief Get the layout root node (YogaNode corresponding to _nodes["root"])
     */
    YogaNode* getRootNode() const { return getNode("root"); }

    /**
     * @brief Destroy all managed nodes
     *
     * Typically called during Surface destruction or reset to safely release all nodes.
     */
    void clearAll();

    /**
     * @brief Insert the YogaNode for childId into the YogaNode for parentId at the specified position
     * @param parentId  Parent node id
     * @param childId   Child node id
     * @param index     Insert position
     */
    void insertChild(const std::string& parentId, const std::string& childId, uint32_t index);

    /**
     * @brief Remove the YogaNode for childId from the YogaNode for parentId
     * @param parentId  Parent node id
     * @param childId   Child node id
     */
    void removeChild(const std::string& parentId, const std::string& childId);

    /**
     * @brief Trigger global layout calculation
     *
     * Centralizes platform differences:
     *  - USE_YOGA: calls YGNodeCalculateLayout
     *  - USE_ARKUI_CAPI: system auto-triggers, this is a no-op
     *
     * @param rootWidth  Layout width (screen width or surface width)
     * @param rootHeight Layout height (usually YGUndefined)
     */
    void calculateLayout(float rootWidth, float rootHeight = 0.0f);

    /**
     * @brief Global layout calculation with Tabs secondary layout
     *
     * Encapsulates the complete Tabs height correction flow:
     *  1. First calculateLayout: compute child content height
     *  2. TabsYogaHelper::updateMinHeightRecursive: update Tabs minHeight using child height
     *  3. If updated, second calculateLayout: recalculate full tree so Tabs height is correct
     *
     * @param root         VirtualDOM root node (for recursively traversing all Tabs nodes)
     * @param surfaceWidth Surface width
     * @return true if a secondary layout was performed
     */
    bool calculateLayoutWithAdjust(
        std::shared_ptr<VirtualDOMNode> root,
        float surfaceWidth);

    /**
     * @brief Update the selected index for a Tabs component
     *
     * Stores tabsId -> selectedIndex mapping internally; used automatically on next
     * calculateLayoutWithTabsAdjust call without requiring additional parameters.
     *
     * @param tabsId        Tabs component id
     * @param selectedIndex Selected tab index
     */
    void updateTabsSelectedIndex(const std::string& tabsId, int selectedIndex);

private:
    std::map<std::string, std::unique_ptr<YogaNode>> _nodes;
    std::map<std::string, int> _tabsSelectedIndices;  // tabsId -> selectedIndex
};

}  // namespace agenui
