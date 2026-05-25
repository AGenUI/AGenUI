#include "agenui_yoga_node.h"

#include <utility>
#include "surface/yoga_node/agenui_css_style_converter.h"
#include "surface/yoga_node/agenui_a2ui_attribute_converter.h"
#include "surface/yoga_node/agenui_tabs_yoga_helper.h"

namespace agenui {

YogaNode::YogaNode() {
    _ygNode = YGNodeNew();
}

YogaNode::~YogaNode() {
    if (_ygNode) {
        // If still attached to a parent in the Yoga tree, remove from parent first to avoid dangling pointer
        if (_hasOwner) {
            YGNodeRef owner = YGNodeGetOwner(_ygNode);
            if (owner) {
                YGNodeRemoveChild(owner, _ygNode);
            }
            _hasOwner = false;
        }
        // Clear all Yoga child node references to avoid dangling pointers after children are freed
        // (Child YGNode object lifetimes are managed by their respective YogaNode RAII wrappers)
        YGNodeRemoveAllChildren(_ygNode);
        // Clear context to prevent stale pointer
        YGNodeSetContext(_ygNode, nullptr);
        // Clear measure func before freeing
        if (YGNodeHasMeasureFunc(_ygNode)) {
            YGNodeSetMeasureFunc(_ygNode, nullptr);
        }
        YGNodeFree(_ygNode);
        _ygNode = nullptr;
    }
}

YogaNode::YogaNode(YogaNode&& other) noexcept
    : _ygNode(other._ygNode),
      _hasOwner(other._hasOwner),
      _measureCb(std::move(other._measureCb)),
      _ownedChildren(std::move(other._ownedChildren)) {
    other._ygNode   = nullptr;
    other._hasOwner = false;
    // Update context pointer (was pointing to other, should now point to this)
    if (_ygNode && YGNodeHasMeasureFunc(_ygNode)) {
        YGNodeSetContext(_ygNode, this);
    }
}

YogaNode& YogaNode::operator=(YogaNode&& other) noexcept {
    if (this != &other) {
        // Release current resources
        if (_ygNode) {
            YGNodeSetContext(_ygNode, nullptr);
            if (YGNodeHasMeasureFunc(_ygNode)) {
                YGNodeSetMeasureFunc(_ygNode, nullptr);
            }
            YGNodeFree(_ygNode);
        }
        _ygNode         = other._ygNode;
        _hasOwner       = other._hasOwner;
        _measureCb      = std::move(other._measureCb);
        _ownedChildren  = std::move(other._ownedChildren);
        other._ygNode   = nullptr;
        other._hasOwner = false;
        if (_ygNode && YGNodeHasMeasureFunc(_ygNode)) {
            YGNodeSetContext(_ygNode, this);
        }
    }
    return *this;
}

void YogaNode::setWidth(float w) {
    if (_ygNode) YGNodeStyleSetWidth(_ygNode, w);
}

void YogaNode::setHeight(float h) {
    if (_ygNode) YGNodeStyleSetHeight(_ygNode, h);
}

void YogaNode::setSize(float w, float h) {
    setWidth(w);
    setHeight(h);
}

void YogaNode::setMeasureFunc(MeasureCallback cb) {
    _measureCb = std::move(cb);
    if (_ygNode) {
        YGNodeSetContext(_ygNode, this);
        YGNodeSetMeasureFunc(_ygNode, staticMeasureFunc);
        YGNodeMarkDirty(_ygNode);
    }
}

void YogaNode::clearMeasureFunc() {
    _measureCb = nullptr;
    if (_ygNode) {
        YGNodeSetMeasureFunc(_ygNode, nullptr);
        YGNodeSetContext(_ygNode, nullptr);
    }
}

void YogaNode::markDirty() {
    // YGNodeMarkDirty requires the node to have a measureFunc to be called directly.
    // For container nodes without measureFunc (e.g. Tabs),
    // modifying styles (e.g. minHeight) already dirties the parent automatically, no extra markDirty needed.
    if (_ygNode && YGNodeHasMeasureFunc(_ygNode)) {
        YGNodeMarkDirty(_ygNode);
    }
}

void YogaNode::setHasNewLayout(bool val) {
    if (_ygNode) YGNodeSetHasNewLayout(_ygNode, val);
}

bool YogaNode::hasNewLayout() const {
    return _ygNode ? YGNodeGetHasNewLayout(_ygNode) : false;
}

void YogaNode::clearNewLayout() {
    if (_ygNode) YGNodeSetHasNewLayout(_ygNode, false);
}

void YogaNode::insertChild(YogaNode& child, uint32_t index) {
    if (child._hasOwner) return;  // Already attached, skip (scheme 4)
    if (_ygNode && child._ygNode) {
        YGNodeInsertChild(_ygNode, child._ygNode, index);
        child._hasOwner = true;
    }
}

void YogaNode::removeChild(YogaNode& child) {
    if (!_ygNode || !child._ygNode) return;
    if (!child._hasOwner) return;  // already detached; avoid Yoga assert on double-remove
    YGNodeRemoveChild(_ygNode, child._ygNode);
    child._hasOwner = false;
}

uint32_t YogaNode::childCount() const {
    return _ygNode ? YGNodeGetChildCount(_ygNode) : 0u;
}

YogaNode* YogaNode::createChild(uint32_t index) {
    auto child = std::make_unique<YogaNode>();
    if (_ygNode && child->_ygNode) {
        YGNodeInsertChild(_ygNode, child->_ygNode, index);
        child->_hasOwner = true;
    }
    auto* ptr = child.get();
    _ownedChildren.push_back(std::move(child));
    return ptr;
}

float YogaNode::getLayoutLeft()   const { auto v = _ygNode ? YGNodeLayoutGetLeft(_ygNode)   : 0.0f; return std::isnan(v) ? 0.0f : v; }
float YogaNode::getLayoutTop()    const { auto v = _ygNode ? YGNodeLayoutGetTop(_ygNode)    : 0.0f; return std::isnan(v) ? 0.0f : v; }
float YogaNode::getLayoutWidth()  const { auto v = _ygNode ? YGNodeLayoutGetWidth(_ygNode)  : 0.0f; return std::isnan(v) ? 0.0f : v; }
float YogaNode::getLayoutHeight() const { auto v = _ygNode ? YGNodeLayoutGetHeight(_ygNode) : 0.0f; return std::isnan(v) ? 0.0f : v; }

void YogaNode::applySnapshot(ComponentSnapshot& snapshot, bool clearAfterConvert) {
    if (!_ygNode) { return; }
    CSSStyleConverter::convertToYoga(snapshot, _ygNode, clearAfterConvert);
    A2UIAttributeConverter::convertToYoga(snapshot, _ygNode, clearAfterConvert);
    saveCurrentStyleSize();
}

void YogaNode::saveCurrentStyleSize() {
    if (!_ygNode) return;
    _savedWidth = YGNodeStyleGetWidth(_ygNode);
    _hasSavedWidth = true;
    _savedHeight = YGNodeStyleGetHeight(_ygNode);
    _hasSavedHeight = true;
}

void YogaNode::resetToStyleSize() {
    if (!_ygNode) return;

    if (_hasSavedWidth) {
        switch (_savedWidth.unit) {
            case YGUnitAuto:    YGNodeStyleSetWidthAuto(_ygNode);                      break;
            case YGUnitPoint:   YGNodeStyleSetWidth(_ygNode, _savedWidth.value);       break;
            case YGUnitPercent: YGNodeStyleSetWidthPercent(_ygNode, _savedWidth.value); break;
            default:            YGNodeStyleSetWidthAuto(_ygNode);                      break;
        }
    } else {
        YGNodeStyleSetWidthAuto(_ygNode);
    }

    if (_hasSavedHeight) {
        switch (_savedHeight.unit) {
            case YGUnitAuto:    YGNodeStyleSetHeightAuto(_ygNode);                       break;
            case YGUnitPoint:   YGNodeStyleSetHeight(_ygNode, _savedHeight.value);       break;
            case YGUnitPercent: YGNodeStyleSetHeightPercent(_ygNode, _savedHeight.value); break;
            default:            YGNodeStyleSetHeightAuto(_ygNode);                       break;
        }
    } else {
        YGNodeStyleSetHeightAuto(_ygNode);
    }
}

void YogaNode::applySnapshotWithTabsHints(ComponentSnapshot& snapshot,
                                          const ComponentSnapshot* parentSnapshot,
                                          const std::string& nodeId,
                                          bool clearAfterConvert) {
    // Apply snapshot attributes to Yoga node
    // Note: injectFlexGrowIfNeeded must be called by the caller before StyleDefaults; not repeated here.
    applySnapshot(snapshot, clearAfterConvert);

    // Rule 2: parent is Tabs -- set this node to Yoga absolute layout (for height measurement)
    if (parentSnapshot && parentSnapshot->component == "Tabs") {
        TabsYogaHelper::setChildAbsoluteLayout(*this, nodeId);
    }
}

YGSize YogaNode::staticMeasureFunc(
        YGNodeRef node,
        float w, YGMeasureMode wMode,
        float h, YGMeasureMode hMode) {
    auto* self = static_cast<YogaNode*>(YGNodeGetContext(node));
    if (self && self->_measureCb) {
        return self->_measureCb(w, wMode, h, hMode);
    }
    // Empty cb: return Undefined so Yoga falls back to style-based sizing
    // instead of treating the node as 0x0 and collapsing it.
    return {YGUndefined, YGUndefined};
}

}  // namespace agenui
