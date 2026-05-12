#include "agenui_yoga_node_manager.h"

#include <yoga/Yoga.h>
#include "surface/yoga_node/agenui_tabs_yoga_helper.h"
#include "surface/virtual_dom/agenui_virtual_dom_node.h"

namespace agenui {

YogaNodeManager::YogaNodeManager() {
    // _nodes is the YogaNode pool for all VirtualDOMNodes.
    // The layout root is _nodes["root"], calculated via calculateLayout.
}

YogaNodeManager::~YogaNodeManager() {
    // Detach all YG parent-child relationships first via clearAll(), then batch free,
    // to avoid UAF from uncertain unique_ptr<YogaNode> destruction order.
    clearAll();
}

YogaNode* YogaNodeManager::createNode(const std::string& nodeId) {
    auto it = _nodes.find(nodeId);
    if (it != _nodes.end()) {
        return it->second.get();
    }
    auto node = std::make_unique<YogaNode>();
    auto* ptr = node.get();
    _nodes[nodeId] = std::move(node);
    return ptr;
}

YogaNode* YogaNodeManager::getNode(const std::string& nodeId) const {
    auto it = _nodes.find(nodeId);
    return it != _nodes.end() ? it->second.get() : nullptr;
}

void YogaNodeManager::removeNode(const std::string& nodeId) {
    auto it = _nodes.find(nodeId);
    if (it == _nodes.end()) return;

    YogaNode* node = it->second.get();
    if (node && node->get()) {
        YGNodeRef ygNode = node->get();

        // Safely remove self from parent to avoid dangling pointer.
        // YogaNode::~YogaNode will check _hasOwner again and removeChild,
        // but doing it here ensures the parent's YG child list is immediately up-to-date.
        YGNodeRef owner = YGNodeGetOwner(ygNode);
        if (owner) {
            YGNodeRemoveChild(owner, ygNode);
            // Update _hasOwner flag to avoid redundant removal on destruction
            node->_hasOwner = false;
        }
        // Note: no need to manually clear this node's YG child list.
        // Each child's YogaNode::~YogaNode handles its own detach via _hasOwner;
        // redundant operations here would cause removeChild to fail in child destructors.
    }

    _nodes.erase(it);
}

void YogaNodeManager::clearAll() {
    // Iterate _nodes, detach all YG parent-child relationships and reset _hasOwner.
    // _nodes.clear() triggers batch unique_ptr<YogaNode> destruction with uncertain order;
    // if a child has _hasOwner=true and the parent YGNodeRef is already freed, UAF occurs.
    // Detaching early prevents ~YogaNode's _hasOwner branch from accessing a dangling parent pointer.
    for (auto& kv : _nodes) {
        YogaNode* node = kv.second.get();
        if (!node || !node->get()) continue;
        YGNodeRef ygNode = node->get();
        // Detach self from parent
        if (node->_hasOwner) {
            YGNodeRef owner = YGNodeGetOwner(ygNode);
            if (owner) {
                YGNodeRemoveChild(owner, ygNode);
            }
            node->_hasOwner = false;
        }
        // Detach this node's references to its YG children
        while (YGNodeGetChildCount(ygNode) > 0) {
            YGNodeRemoveChild(ygNode, YGNodeGetChild(ygNode, 0));
        }
    }
    _nodes.clear();
}

void YogaNodeManager::insertChild(const std::string& parentId,
                                   const std::string& childId,
                                   uint32_t index) {
    YogaNode* parent = getNode(parentId);
    YogaNode* child  = getNode(childId);
    if (parent && child) {
        parent->insertChild(*child, index);
    }
}

void YogaNodeManager::removeChild(const std::string& parentId,
                                   const std::string& childId) {
    YogaNode* parent = getNode(parentId);
    YogaNode* child  = getNode(childId);
    if (parent && child) {
        parent->removeChild(*child);
    }
}

void YogaNodeManager::calculateLayout(float rootWidth, float rootHeight) {
    // Use the YogaNode corresponding to _nodes["root"] as the calculation root.
    YogaNode* rootNode = getNode("root");
    if (!rootNode || !rootNode->get()) return;

    // Yoga does NOT apply aspect-ratio on the root node (only on children).
    // If the root has aspect-ratio set and its height is not explicitly defined,
    // manually derive the height from the known root width before calling
    // YGNodeCalculateLayout so Yoga receives an explicit (EXACT) height constraint.
    {
        YGNodeRef ygRoot = rootNode->get();
        float ar = YGNodeStyleGetAspectRatio(ygRoot);
        if (!YGFloatIsUndefined(ar) && ar > 0.0f) {
            // Check that height is NOT explicitly set (unit is Auto or Undefined)
            YGValue hVal = YGNodeStyleGetHeight(ygRoot);
            bool heightIsImplicit = (hVal.unit == YGUnitAuto ||
                                     hVal.unit == YGUnitUndefined ||
                                     (hVal.unit == YGUnitPoint && YGFloatIsUndefined(hVal.value)));
            if (heightIsImplicit && !YGFloatIsUndefined(rootWidth) && rootWidth > 0.0f) {
                float computedH = rootWidth / ar;
                YGNodeStyleSetHeight(ygRoot, computedH);
            }
        }
    }

    YGNodeCalculateLayout(
        rootNode->get(),
        rootWidth,
        YGUndefined,
        YGDirectionLTR);
    (void)rootHeight;
}

bool YogaNodeManager::calculateLayoutWithAdjust(
        std::shared_ptr<VirtualDOMNode> root,
        float surfaceWidth) {
    calculateLayout(surfaceWidth);
    if (TabsYogaHelper::updateMinHeightRecursive(root, _tabsSelectedIndices, surfaceWidth)) {
        calculateLayout(surfaceWidth);
        return true;
    }
    return false;
}

void YogaNodeManager::updateTabsSelectedIndex(const std::string& tabsId, int selectedIndex) {
    _tabsSelectedIndices[tabsId] = selectedIndex;
}

}  // namespace agenui
