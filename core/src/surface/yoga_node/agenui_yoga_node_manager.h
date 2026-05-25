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
 *  2. Providing insertChild/removeChild wrappers
 *  3. Triggering global layout calculation via calculateLayout (using _nodes["root"] as root)
 *
 * Threading model: operated on message thread, no additional lock protection.
 */
class YogaNodeManager {
public:
    YogaNodeManager();
    /**
     * @brief Destructor; delegates to clearAll() to detach all YGNode parent-
     *        child relationships before the unique_ptr<YogaNode> map is torn
     *        down (avoids UAF from undefined map destruction order).
     */
    ~YogaNodeManager();

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
     * @brief Destroy the YogaNode bound to nodeId.
     *
     * @pre Caller must have already destroyed all child YogaNodes belonging
     *      to nodeId; this method does NOT recurse. The current sole caller
     *      (`VirtualDOMNode::~VirtualDOMNode`) destroys children first via
     *      `_children.clear()`, which triggers child VirtualDOMNode dtors
     *      that in turn call removeNode for each child. Direct callers
     *      that bypass VirtualDOMNode must replicate this discipline.
     *
     * @param nodeId VirtualDOMNode id whose YogaNode entry should be erased.
     */
    void removeNode(const std::string& nodeId);

    void setRootNode(YogaNode* node) { _rootNode = node; }

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
     * @brief Two-pass layout with Tabs height correction
     *
     * 1. First calculateLayout: compute child content height
     * 2. TabsYogaHelper::updateMinHeightRecursive: update Tabs minHeight
     * 3. If updated, second calculateLayout for correct Tabs height
     *
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
    YogaNode* _rootNode = nullptr;
    std::map<std::string, int> _tabsSelectedIndices;  // tabsId -> selectedIndex
};

}  // namespace agenui
