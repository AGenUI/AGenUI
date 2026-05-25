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
    if (it == _nodes.end()) {
        return;
    }

    YogaNode* node = it->second.get();
    if (node && node->get()) {
        YGNodeRef ygNode = node->get();

        // Detach from parent
        if (node->_hasOwner) {
            YGNodeRef owner = YGNodeGetOwner(ygNode);
            if (owner) {
                YGNodeRemoveChild(owner, ygNode);
            }
            node->_hasOwner = false;
        }

        // Detach all children BEFORE erase triggers ~YogaNode
        YGNodeRemoveAllChildren(ygNode);
    }

    _nodes.erase(it);
    _tabsSelectedIndices.erase(nodeId);
}

void YogaNodeManager::clearAll() {
    // Detach all YG parent-child relationships before batch destruction.
    // Without this, uncertain map destruction order causes UAF.
    for (auto& kv : _nodes) {
        YogaNode* node = kv.second.get();
        if (!node || !node->get()) continue;
        YGNodeRef ygNode = node->get();
        if (node->_hasOwner) {
            YGNodeRef owner = YGNodeGetOwner(ygNode);
            if (owner) {
                YGNodeRemoveChild(owner, ygNode);
            }
            node->_hasOwner = false;
        }
        YGNodeRemoveAllChildren(ygNode);
    }
    _nodes.clear();
    _rootNode = nullptr;
    _tabsSelectedIndices.clear();
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
    YogaNode* rootNode = _rootNode;
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
