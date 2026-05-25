#include "agenui_virtual_dom_node.h"
#include <cstdio>
#include "surface/yoga_node/agenui_css_style_converter.h"
#include "surface/yoga_node/agenui_a2ui_attribute_converter.h"
#include "agenui_ivirtual_define.h"
#include "agenui_platform_layout_bridge.h"
#include "surface/style_defaults/agenui_style_defaults.h"
#include "agenui_logger_internal.h"
#include "surface/agenui_serializable_data_impl.h"
#include <climits>
#include <functional>
#include "nlohmann/json.hpp"
#include <yoga/Yoga.h>
#include "surface/virtual_dom/agenui_ivirtual_define.h"
#include "surface/yoga_node/agenui_yoga_node_manager.h"
#include "surface/yoga_node/agenui_tabs_yoga_helper.h"
#include "surface/yoga_node/agenui_measurement_manager.h"
#include "surface/yoga_node/agenui_layout_delegate.h"

namespace agenui {

namespace {

uint64_t nextYogaKey() {
    static uint64_t sKey = 0;
    return ++sKey;
}

std::string parseSnapshotValue(const SerializableData& rawValue) {
    if (!rawValue.isValid()) {
        return "";
    }
    
    if (rawValue.isString()) {
        return rawValue.asString();
    }
    if (rawValue.isNumber()) {
        return std::to_string(rawValue.asDouble());
    }
    if (rawValue.isBool()) {
        return rawValue.asBool() ? "true" : "false";
    }

    return rawValue.dump();
}

}  // namespace

VirtualDOMNode::VirtualDOMNode(const std::string& id,
                               IVirtualDOMObserver* observer,
                               IOrphanSnapshotFetcher* orphanFetcher,
                               ::agenui::IMeasurementManager* measurementManager
                               , ILayoutDelegate* layoutDelegate
                               )
    : _id(id)
    , _yogaKey(id + "#" + std::to_string(nextYogaKey()))
    , _observer(observer)
    , _orphanFetcher(orphanFetcher)
    , _measurementManager(measurementManager)
    , _layoutDelegate(layoutDelegate)
{
    if (_layoutDelegate) {
        _yogaNode = _layoutDelegate->createNode(_yogaKey);
        if (_id == "root") {
            _layoutDelegate->setRootNode(_yogaNode);
            // Root node is created; layout engine will handle setRootNode internally
        }
    }
}

VirtualDOMNode::~VirtualDOMNode() {
    notifyComponentRemoved();

    // Clear measure callback before yoga node is freed (captures raw `this`)
    if (_yogaNode) {
        _yogaNode->clearMeasureFunc();
    }

    // Destroy children first so their destructors can detach from a still-live parent YGNode
    _children.clear();

    if (_layoutDelegate && !_yogaKey.empty()) {
        _layoutDelegate->removeNode(_yogaKey);
    }

    _yogaNode = nullptr;
    _layoutDelegate = nullptr;
}

const std::string& VirtualDOMNode::getId() const {
    return _id;
}

bool VirtualDOMNode::hasSnapshot() const {
    return _snapshot != nullptr;
}

void VirtualDOMNode::setSnapshot(const ComponentSnapshot& snapshot, const std::string& parentId) {
    if (snapshot.id != _id) {
        return;
    }

    _parentId = parentId;

    if (!_snapshot) {
        // First snapshot set
        _snapshot = std::make_shared<ComponentSnapshot>(snapshot);
        if (_yogaNode) {
            _yogaNode->setHasNewLayout(true);
        }
    } else if (VirtualDOMNode::checkSnapshotChanged(*_snapshot, snapshot, false)) {
        // Snapshot changed (layout field not compared).
        // Reset _hasPlatformSize only when business content (attributes / component type / children)
        // actually changes — NOT when only styles change, because styles contain layout output
        // values (width/height numbers written back by Yoga) that change every layout pass and
        // must not invalidate the platform-supplied size lock.
        if (_snapshot->attributes != snapshot.attributes
            || _snapshot->component  != snapshot.component
            || _snapshot->children   != snapshot.children) {
            _hasPlatformSize = false;
        }
        *_snapshot = snapshot;
        if (_yogaNode) {
            _yogaNode->setHasNewLayout(true);
        }
    }

    // Clear measureFunc before updating children to prevent Yoga assertion failure.
    // Yoga rule: a node with measureFunc cannot have children (YGNodeInsertChild asserts
    // !YGNodeHasMeasureFunc). This can happen when a previous setSnapshot registered
    // measureFunc (children were empty at that time), then template expansion adds children.
    // setupMeasureFunctionIfNeeded() below will re-evaluate and re-register if still appropriate.
    if (_yogaNode && _yogaNode->get() && YGNodeHasMeasureFunc(_yogaNode->get())) {
        _yogaNode->clearMeasureFunc();
    }

    // Update child node list to match snapshot.children
    updateChildren();

    // Save Image component width/height from styles into layout.styleInfo before convertToYoga clears styles
    saveImageStyleInfo();

    // Set up measure function for components needing intrinsic sizing (must be before convertToYoga)
    setupMeasureFunctionIfNeeded();

    // Inject flex-grow:1 for Tabs if not explicitly set in styles.
    // Must be called before StyleDefaults loop to avoid being overwritten.
    TabsYogaHelper::injectFlexGrowIfNeeded(*_snapshot);

    // Apply StyleDefaults before Yoga conversion to ensure complete default layout styles are available
    const auto& styleDefaults = StyleDefaults::getDefaults();
    for (const auto& pair : styleDefaults) {
        if (_snapshot->styles.find(pair.first) == _snapshot->styles.end()) {
            _snapshot->styles[pair.first] = SerializableData(SerializableData::Impl::parse(pair.second));
        }
    }
    // Apply Yoga layout conversion with Tabs-specific hints (flex-grow injection + absolute layout),
    // clearAfterConvert=true clears already-converted properties from _snapshot
    if (_yogaNode) {
        _yogaNode->applySnapshotWithTabsHints(
            *_snapshot,
            _parent ? _parent->getSnapshot() : nullptr,
            _id,
            true);
    }
}

void VirtualDOMNode::logYogaLayoutInfo(const ComponentSnapshot& snapshotWithLayout) const {
    if (!_yogaNode) return;

    float lx = snapshotWithLayout.layout.x;
    float ly = snapshotWithLayout.layout.y;
    float lw = snapshotWithLayout.layout.width;
    float lh = snapshotWithLayout.layout.height;

    // De-duplicate: skip output when layout numbers are unchanged since last log.
    // Reduces high-frequency repeated dumps triggered by every layout recalc.
    if (lx == _lastLogX && ly == _lastLogY && lw == _lastLogW && lh == _lastLogH) {
        return;
    }
    _lastLogX = lx;
    _lastLogY = ly;
    _lastLogW = lw;
    _lastLogH = lh;

    const std::string& comp   = _snapshot ? _snapshot->component : "?";
    const std::string& nodeId = _id;
    const std::string& pid    = _parentId;

    float flexGrow   = YGNodeStyleGetFlexGrow(_yogaNode->get());
    float flexShrink = YGNodeStyleGetFlexShrink(_yogaNode->get());
    uint32_t cc      = YGNodeGetChildCount(_yogaNode->get());
    float aspectRatio = YGNodeStyleGetAspectRatio(_yogaNode->get());
    bool hasMeasureFunc = YGNodeHasMeasureFunc(_yogaNode->get());

    // Merge previous two-line dump into a single line to halve log volume.
    AGENUI_LOG("[layout] id=%s type=%s parent=%s xywh=(%.1f,%.1f,%.1f,%.1f) flex=(%.1f,%.1f) child=%u aspect=%.4f measure=%d",
               nodeId.c_str(), comp.c_str(), pid.c_str(),
               lx, ly, lw, lh, flexGrow, flexShrink, cc, aspectRatio, (int)hasMeasureFunc);
}

void VirtualDOMNode::saveImageStyleInfo() {
    if (!_snapshot || _snapshot->component != "Image") {
        return;
    }
    nlohmann::json styleJson = nlohmann::json::object();
    auto widthIt = _snapshot->styles.find("width");
    if (widthIt != _snapshot->styles.end() && widthIt->second.isValid()) {
        styleJson["width"] = widthIt->second.isString() ? widthIt->second.asString() : widthIt->second.dump();
    }
    auto heightIt = _snapshot->styles.find("height");
    if (heightIt != _snapshot->styles.end() && heightIt->second.isValid()) {
        styleJson["height"] = heightIt->second.isString() ? heightIt->second.asString() : heightIt->second.dump();
    }
    if (!styleJson.empty()) {
        _snapshot->layout.styleInfo = styleJson.dump();
    }
}

void VirtualDOMNode::setupMeasureFunctionIfNeeded() {
    // measureFunc trigger decision.
    // Policy owner: MeasurementManagerImpl::shouldUseMeasureFunc
    // (agenui_measurement_manager.cpp). This function only:
    //   1) assembles a MeasureDecisionContext
    //   2) applies the returned MeasureDecision (Register / Clear / Skip).
    if (!_snapshot || !_yogaNode || !_measurementManager) {
        return;
    }

    auto getStyleStr = [&](const std::string& key) -> std::string {
        auto it = _snapshot->styles.find(key);
        if (it == _snapshot->styles.end() || !it->second.isValid()) return "";
        return it->second.isString() ? it->second.asString() : it->second.dump();
    };

    MeasureDecisionContext ctx;
    ctx.hasChildren        = _yogaNode->childCount() > 0;
    ctx.platformSizeLocked = _hasPlatformSize;
    ctx.widthStyle         = getStyleStr("width");
    ctx.heightStyle        = getStyleStr("height");
    ctx.aspectRatioStyle   = getStyleStr("aspect-ratio");

    const std::string& comp = _snapshot->component;
    // Downcast to concrete impl: the decision API is internal and not exposed
    // on the public IMeasurementManager interface.
    auto* mgrImpl = static_cast<MeasurementManagerImpl*>(_measurementManager);
    MeasureDecision decision = mgrImpl->shouldUseMeasureFunc(comp, ctx);

    if (decision == MeasureDecision::Register) {
        AGENUI_LOG("[setupMeasureFunc] id=%s comp=%s decision=Register",
                   _id.c_str(), comp.c_str());
    }

    switch (decision) {
        case MeasureDecision::Clear:
            _yogaNode->clearMeasureFunc();
            return;
        case MeasureDecision::Skip:
            return;
        case MeasureDecision::Register: {
            VirtualDOMNode* self = this;
            _yogaNode->setMeasureFunc(
                [self](float w, YGMeasureMode wMode, float h, YGMeasureMode hMode) -> YGSize {
                    if (!self->_snapshot) return {0.0f, 0.0f};
                    return self->routeMeasure(*self->_snapshot, w, wMode, h, hMode);
                });
            return;
        }
    }
}

YGSize VirtualDOMNode::routeMeasure(const ComponentSnapshot& snapshot,
                                    float width, YGMeasureMode widthMode,
                                    float height, YGMeasureMode heightMode) {
    if (!_measurementManager) {
        return {0.0f, 0.0f};
    }

    // Build paramJson
    std::string paramJson = buildParamJson(snapshot);

    // Build MeasureModes
    ::agenui::MeasureModes modes;
    modes.width.maxValue  = width;
    modes.width.mode      = static_cast<int>(widthMode);
    modes.height.maxValue = height;
    modes.height.mode     = static_cast<int>(heightMode);

    // Call unified measurement entry point
    ::agenui::MeasureResult result = _measurementManager->measure(
        snapshot.component, paramJson, modes);

    if (result.calcType == ::agenui::CalcType::Async) {
        // Async component: return {0, 0} first, wait for markDirty to trigger second calculation
        return {0.0f, 0.0f};
    }

    // Write back line count (Text-type components)
    if (_snapshot && result.countOfLines > 0) {
        _snapshot->layout.lines = result.countOfLines;
    }

    return {result.width, result.height};
}

std::string VirtualDOMNode::buildParamJson(const ComponentSnapshot& snapshot) const {
    // Serialize using snapshot.stringify(), including component/attributes/styles/layout etc.
    // ImageComponentMeasurement reads width/height from the styleInfo field saved earlier
    return snapshot.stringify();
}

void VirtualDOMNode::setYogaNodeSize(float width, float height) {
    if (_yogaNode) {
        _yogaNode->setSize(width, height);
        // Once the engine receives a concrete size from the platform (via notifyRenderFinish),
        // the measureFunc is no longer needed. Clearing it prevents Yoga from calling measure
        // again on the next calculateLayout, which would otherwise cause oscillation loops
        // (e.g. Image width 720 vs 718 due to border-radius difference).
        _yogaNode->clearMeasureFunc();
        // Mark that this node has a platform-supplied size so setupMeasureFunctionIfNeeded
        // will not re-register the measureFunc on subsequent setSnapshot calls.
        _hasPlatformSize = true;
    }
}

void VirtualDOMNode::resetPlatformSize() {
    if (!_yogaNode || !_hasPlatformSize) return;
    _hasPlatformSize = false;
    _yogaNode->resetToStyleSize();
    _yogaNode->markDirty();
    setupMeasureFunctionIfNeeded();
}

const std::vector<std::shared_ptr<VirtualDOMNode> >& VirtualDOMNode::getChildren() const {
    return _children;
}

std::shared_ptr<VirtualDOMNode> VirtualDOMNode::findChild(const std::string& id) const {
    for (const auto& child : _children) {
        if (child && child->getId() == id) {
            return child;
        }
    }
    return nullptr;
}

void VirtualDOMNode::notifyComponentUpdate(const ComponentSnapshot& newSnapshot) {
    if (!_observer || !_snapshotWithLayout) {
        return;
    }
    
    std::string diff;
    if (VirtualDOMNode::checkSnapshotChanged(*_snapshotWithLayout, newSnapshot, true, &diff)) {
        _observer->onNodeUpdate(_id, diff);
    }
}

void VirtualDOMNode::notifyComponentAdded() {
    if (!_observer || !_snapshotWithLayout) {
        return;
    }
    _observer->onNodeAdded(_parentId, _snapshotWithLayout->stringify());
}

void VirtualDOMNode::notifyComponentRemoved() {
    if (!_observer || !_snapshotWithLayout) {
        return;
    }
    _observer->onNodeRemoved(_parentId, _id);
}

bool VirtualDOMNode::checkSnapshotChanged(const ComponentSnapshot& desc1, const ComponentSnapshot& desc2, bool compareLayout, std::string* diff) {
    using json = nlohmann::json;
    bool changed = desc1.id != desc2.id
        || desc1.component != desc2.component
        || desc1.attributes != desc2.attributes
        || desc1.children != desc2.children
        || desc1.styles != desc2.styles
        || (compareLayout && desc1.layout != desc2.layout);
    
    if (diff != nullptr && changed) {
        json json1 = json::parse(desc1.stringify(), nullptr, false);
        json json2 = json::parse(desc2.stringify(), nullptr, false);

        json diffJson;
        if (!json2.is_discarded()) {
            diffJson["id"]        = json2.value("id", desc2.id);
            diffJson["component"] = json2.value("component", desc2.component);
        }

        if (!json1.is_discarded() && !json2.is_discarded()) {
            // Include only changed or new fields
            for (auto it = json2.begin(); it != json2.end(); ++it) {
                const std::string& key = it.key();
                if (key == "id" || key == "rawId") continue;
                if (json1.find(key) == json1.end() || json1[key] != it.value()) {
                    diffJson[key] = it.value();
                }
            }

            // Fields present in json1 but absent in json2 are set to null (deleted)
            for (auto it = json1.begin(); it != json1.end(); ++it) {
                const std::string& key = it.key();
                if (key == "id" || key == "rawId") continue;
                if (json2.find(key) == json2.end()) {
                    diffJson[key] = nullptr;
                }
            }
        }

        *diff = diffJson.dump();
    }
    
    return changed;
}

void VirtualDOMNode::updateChildren() {
    if (!_snapshot) {
        return;
    }

    const auto& targetChildrenIds = _snapshot->children;
    std::map<std::string, ComponentSnapshot> removedSnapshots;
    size_t i = 0;
    const size_t targetSize = targetChildrenIds.size();

    for (; i < targetSize; i++) {
        const std::string& targetId = targetChildrenIds[i];

        // Remove mismatched nodes at current position until targetId is found or end
        while (i < _children.size() && targetId != _children[i]->getId()) {
            auto& currentChild = _children[i];
            if (currentChild && _layoutDelegate) {
                _layoutDelegate->removeChild(_yogaKey, currentChild->_yogaKey);
            }
            if (currentChild && currentChild->hasSnapshot()) {
                removedSnapshots[currentChild->getId()] = *(currentChild->_snapshot);
            }
            _children.erase(_children.begin() + i);
        }

        if (i >= _children.size()) {
            // Not found: insert a new node at the end
            auto newChild = std::make_shared<VirtualDOMNode>(targetId, _observer, _orphanFetcher,
                                                              _measurementManager
                                                              , _layoutDelegate
                                                              );
            newChild->_parent = this;
            _children.emplace_back(newChild);

            if (_layoutDelegate && newChild->_yogaNode && !newChild->_yogaNode->isAttached()) {
                const uint32_t insertIdx = _yogaNode ? _yogaNode->childCount() : 0u;
                _layoutDelegate->insertChild(_yogaKey, newChild->_yogaKey, insertIdx);
            }
        }

        if (i < _children.size() && _children[i]) {
            auto child = _children[i];
            if (!child->hasSnapshot()) {
                // Restore from stash if available
                auto it = removedSnapshots.find(targetId);
                if (it != removedSnapshots.end()) {
                    child->setSnapshot(it->second, _id);
                    removedSnapshots.erase(it);
                } else if (_orphanFetcher != nullptr) {
                    // Try to fetch from orphan snapshot pool
                    ComponentSnapshot orphanSnapshot;
                    if (_orphanFetcher->takeOrphanSnapshot(targetId, orphanSnapshot)) {
                        child->setSnapshot(orphanSnapshot, _id);
                    }
                }
                // Mark dirty if child has a measure function
                if (child->_yogaNode && YGNodeHasMeasureFunc(child->_yogaNode->get())) {
                    child->_yogaNode->markDirty();
                }
            }
        }
    }

    // Remove trailing nodes not in target list
    while (i < _children.size()) {
        if (_children[i] && _layoutDelegate) {
            _layoutDelegate->removeChild(_yogaKey, _children[i]->_yogaKey);
        }
        _children.erase(_children.begin() + i);
    }
}

void VirtualDOMNode::refreshChildrenRecursively() {
    updateChildren();
    for (const auto& child : _children) {
        if (child) {
            child->refreshChildrenRecursively();
        }
    }
}


void VirtualDOMNode::checkAndNotifyLayoutChanges() {
    if (!_snapshot) {
        return;
    }

    // Fast path: if no new layout and already notified, only recurse into children
    if (_yogaNode && !_yogaNode->hasNewLayout() && _snapshotWithLayout) {
        for (const auto& child : _children) {
            if (child) {
                child->checkAndNotifyLayoutChanges();
            }
        }
        return;
    }

    // Create a copy of the raw snapshot to fill in layout info
    ComponentSnapshot snapshotWithLayout = *_snapshot;

    if (!_yogaNode) {
        return;
    }

    if (_yogaNode->hasNewLayout()) {
        snapshotWithLayout.layout.x      = _yogaNode->getLayoutLeft();
        snapshotWithLayout.layout.y      = _yogaNode->getLayoutTop();
        snapshotWithLayout.layout.width  = _yogaNode->getLayoutWidth();
        snapshotWithLayout.layout.height = _yogaNode->getLayoutHeight();

        // Propagate styleInfo saved earlier in setSnapshot (e.g. Image width/height)
        if (!_snapshot->layout.styleInfo.empty()) {
            snapshotWithLayout.layout.styleInfo = _snapshot->layout.styleInfo;
        }

        logYogaLayoutInfo(snapshotWithLayout);

        // Clear new-layout flag
        _yogaNode->clearNewLayout();
    } else {
        if (_snapshotWithLayout) {
            snapshotWithLayout = *_snapshotWithLayout;
        }
    }

    // Fill in missing style default values
    const auto& styleDefaults = StyleDefaults::getDefaults();
    for (const auto& pair : styleDefaults) {
        if (snapshotWithLayout.styles.find(pair.first) == snapshotWithLayout.styles.end()) {
            snapshotWithLayout.styles[pair.first] = SerializableData(SerializableData::Impl::parse(pair.second));
        }
    }

    if (!_snapshotWithLayout) {
        // First notification: create layout-filled snapshot and notify addition
        _snapshotWithLayout = std::make_shared<ComponentSnapshot>(snapshotWithLayout);
        notifyComponentAdded();
    } else {
        notifyComponentUpdate(snapshotWithLayout);
        *_snapshotWithLayout = snapshotWithLayout;
    }

    // Recursively check all children for layout changes
    for (const auto& child : _children) {
        if (child) {
            child->checkAndNotifyLayoutChanges();
        }
    }
    }
}  // namespace agenui
