#include "surface/virtual_dom/agenui_virtual_dom.h"
#include "agenui_render_info_types.h"
#include "agenui_logger_internal.h"
#include "surface/agenui_isurface_context.h"
#include <functional>
#include <yoga/Yoga.h>
#include "surface/yoga_node/agenui_yoga_node_manager.h"

namespace agenui {

VirtualDOM::VirtualDOM(IVirtualDOMObserver* observer,
                       ISurfaceContext* surfaceContext,
                       ::agenui::IMeasurementManager* measurementManager)
    : _root(nullptr)
    , _observer(observer)
    , _surfaceContext(surfaceContext)
    , _measurementManager(measurementManager)
    , _batchGuard([this] {
          // Layout against whatever width the surface context currently
          // reports. The context owns the validity policy (cache + one-shot
          // pull bootstrap + push channel overwrite); VirtualDOM trusts it
          // unconditionally and does not second-guess the value.
          if (_layoutEngine) {
              const float surfaceWidth = _surfaceContext ? _surfaceContext->getSurfaceWidth() : 0.0f;
              _layoutEngine->calculateLayoutWithAdjust(_root, surfaceWidth);
              checkAndNotifyLayoutChanges();
          }
      }) {
    _layoutEngine = std::make_unique<YogaLayoutEngine>(measurementManager);
    _root = std::make_shared<VirtualDOMNode>("root", observer, this, measurementManager
        , _layoutEngine.get()
    );
}

VirtualDOM::~VirtualDOM() {
    // CRITICAL: _root must be torn down BEFORE _layoutEngine.
    // Each VirtualDOMNode dtor touches its YogaNode (clearMeasureFunc) and
    // YogaNodeManager (removeNode); if _layoutEngine (which owns the
    // YogaNodeManager) destructs first, those raw pointers become dangling.
    // Default member-dtor order is reverse-of-declaration, which would
    // otherwise tear down _layoutEngine before _root — so we release _root
    // explicitly here before the implicit destruction of remaining members.
    _root.reset();
    // _layoutEngine, orphan maps, etc. are released automatically afterwards.
}

void VirtualDOM::updateNode(const ComponentSnapshot& snapshot) {
    if (snapshot.id == "root") {
        _root->setSnapshot(snapshot, "");
    } else {
        std::string parentId;
        auto node = findNodeByIdRecursive(_root, snapshot.id, parentId);
        if (node && checkCanDisplay(snapshot)) {
            node->setSnapshot(snapshot, parentId);
        } else {
            // Node not found: save as an orphan snapshot
            if (snapshot.displayRule == DisplayRule::Always) {
                _directOrphanSnapshots[snapshot.id] = snapshot;
            } else {
                _dataDependentOrphanSnapshots[snapshot.id] = snapshot;
            }
            // After adding a new orphan, check whether any are ready to attach
            tryAttachReadyOrphans();
        }
    }

    // Request a layout+notify pass. If inside a batch window, the pass is
    // deferred to the outermost endBatch() so N updateNode calls in the
    // same window collapse into 1 layout + 1 notify. Otherwise it fires
    // immediately.
    _batchGuard.requestFlush();
}

void VirtualDOM::clear() {
    // Correct clear order:
    // 1. First release old tree: _root.reset() recursively destructs from leaves to root.
    //    Each VirtualDOMNode destructor only nulls out, and YogaNode::~YogaNode handles _hasOwner detach.
    // 2. Then clearAll: detaches all YG parent-child relationships in _nodes, then frees in batch.
    _directOrphanSnapshots.clear();
    _dataDependentOrphanSnapshots.clear();
    _root.reset();
    if (_layoutEngine) {
        _layoutEngine->clearAll();
    }
    _root = std::make_shared<VirtualDOMNode>("root", _observer, this, _measurementManager
        , _layoutEngine.get()
    );
}

bool VirtualDOM::takeOrphanSnapshot(const std::string& id, ComponentSnapshot& outSnapshot) {
    auto directIt = _directOrphanSnapshots.find(id);
    if (directIt != _directOrphanSnapshots.end() && checkCanDisplay(directIt->second)) {
        outSnapshot = directIt->second;
        _directOrphanSnapshots.erase(directIt);
        return true;
    }

    auto it = _dataDependentOrphanSnapshots.find(id);
    if (it != _dataDependentOrphanSnapshots.end() && checkCanDisplay(it->second)) {
        outSnapshot = it->second;
        _dataDependentOrphanSnapshots.erase(it);
        return true;
    }
    return false;
}

bool VirtualDOM::checkCanDisplay(const ComponentSnapshot& snapshot) const {
    // true if status is PartiallyReady or FullyReady
    auto isAnyDataReady = [](DataBindingStatus status) -> bool {
        return status == DataBindingStatus::PartiallyReady || status == DataBindingStatus::FullyReady;
    };

    // true if status is FullyReady or NotDependent
    auto isFullyReadyOrNotDependent = [](DataBindingStatus status) -> bool {
        return status == DataBindingStatus::FullyReady || status == DataBindingStatus::NotDependent;
    };

    // Search both orphan maps for a child snapshot by ID
    auto findChildSnapshot = [this](const std::string& childId) -> const ComponentSnapshot* {
        auto it = _dataDependentOrphanSnapshots.find(childId);
        if (it != _dataDependentOrphanSnapshots.end()) {
            return &it->second;
        }
        auto directIt = _directOrphanSnapshots.find(childId);
        if (directIt != _directOrphanSnapshots.end()) {
            return &directIt->second;
        }
        return nullptr;
    };

    // Recursively check if any descendant has data ready
    std::function<bool(const ComponentSnapshot&)> hasAnyDescendantDataReady =
        [&](const ComponentSnapshot& s) -> bool {
        for (const auto& childId : s.children) {
            const ComponentSnapshot* child = findChildSnapshot(childId);
            if (child == nullptr) continue;
            if (isAnyDataReady(child->dataBindingStatus)) return true;
            if (hasAnyDescendantDataReady(*child)) return true;
        }
        return false;
    };

    // Recursively check if all descendants are in orphan and are NotDependent
    std::function<bool(const ComponentSnapshot&)> allDescendantsNotDependent =
        [&](const ComponentSnapshot& s) -> bool {
        for (const auto& childId : s.children) {
            const ComponentSnapshot* child = findChildSnapshot(childId);
            if (child == nullptr || child->dataBindingStatus != DataBindingStatus::NotDependent) return false;
            if (!allDescendantsNotDependent(*child)) return false;
        }
        return true;
    };

    // Recursively check if all descendants are in orphan and are FullyReady or NotDependent
    std::function<bool(const ComponentSnapshot&)> allDescendantsFullyReadyOrNotDependent =
        [&](const ComponentSnapshot& s) -> bool {
        for (const auto& childId : s.children) {
            const ComponentSnapshot* child = findChildSnapshot(childId);
            if (child == nullptr || !isFullyReadyOrNotDependent(child->dataBindingStatus)) return false;
            if (!allDescendantsFullyReadyOrNotDependent(*child)) return false;
        }
        return true;
    };

    // Top-level rule: the snapshot's own dataBindingStatus must be NotDependent or FullyReady
    if (!isFullyReadyOrNotDependent(snapshot.dataBindingStatus)) {
        return false;
    }

    switch (snapshot.displayRule) {
        case DisplayRule::Always: {
            return true;
        }

        case DisplayRule::AnyDataReady: {
            // Condition A: own data is ready
            if (isAnyDataReady(snapshot.dataBindingStatus)) return true;

            // Condition B: any descendant's data is ready
            if (hasAnyDescendantDataReady(snapshot)) return true;

            // Condition C: self and all descendants are orphans and all are NotDependent
            if (snapshot.dataBindingStatus == DataBindingStatus::NotDependent) {
                return allDescendantsNotDependent(snapshot);
            }
            return false;
        }

        case DisplayRule::AllDataReady: {
            // All descendants must be in orphan and be FullyReady or NotDependent
            if (!isFullyReadyOrNotDependent(snapshot.dataBindingStatus)) return false;
            return allDescendantsFullyReadyOrNotDependent(snapshot);
        }
    }
    return false;
}

void VirtualDOM::tryAttachReadyOrphans() {
    auto it = _dataDependentOrphanSnapshots.begin();
    while (it != _dataDependentOrphanSnapshots.end()) {
        if (!checkCanDisplay(it->second)) {
            ++it;
            continue;
        }
        
        std::string parentId;
        auto node = findNodeByIdRecursive(_root, it->first, parentId);
        if (node) {
            node->setSnapshot(it->second, parentId);
            it = _dataDependentOrphanSnapshots.erase(it);
        } else {
            ++it;
        }
    }
}

std::shared_ptr<VirtualDOMNode> VirtualDOM::findNodeByIdRecursive(std::shared_ptr<VirtualDOMNode> parent, const std::string& id, std::string& outParentId) {
    if (!parent) {
        return nullptr;
    }
    
    const auto& children = parent->getChildren();
    for (const auto& child : children) {
        if (!child) {
            continue;
        }
        
        if (child->getId() == id) {
            outParentId = parent->getId();
            return child;
        }
        
        auto found = findNodeByIdRecursive(child, id, outParentId);
        if (found) {
            return found;
        }
    }
    
    return nullptr;
}

void VirtualDOM::checkAndNotifyLayoutChanges() {
    if (_root) {
        _root->checkAndNotifyLayoutChanges();
    }
}


void VirtualDOM::notifySurfaceSizeChanged() {
    AGENUI_LOG("notifySurfaceSizeChanged");
    // Surface has already refreshed its cached size before invoking this
    // notification, so _surfaceContext->getSurfaceWidth() now observes the
    // freshly pushed value. This method's only job is to invalidate stale
    // platform-measured sizes on the tree and trigger a re-layout against
    // the surface context's current width.
    resetPlatformSizeRecursive(_root);

    // Single-shot external event: bypass the batch path and fire
    // immediately. (Batching is exclusive to updateNode bursts driven by
    // ComponentManager flush.)
    if (_layoutEngine) {
        const float surfaceWidth = _surfaceContext ? _surfaceContext->getSurfaceWidth() : 0.0f;
        _layoutEngine->calculateLayoutWithAdjust(_root, surfaceWidth);
    }
    checkAndNotifyLayoutChanges();
}

void VirtualDOM::resetPlatformSizeRecursive(std::shared_ptr<VirtualDOMNode>& node) {
    if (!node) return;
    node->resetPlatformSize();
    const auto& children = node->getChildren();
    for (const auto& child : children) {
        auto mutableChild = child;
        resetPlatformSizeRecursive(mutableChild);
    }
}

void VirtualDOM::updateComponentSize(const ComponentRenderInfo& info) {
    AGENUI_LOG("updateComponentSize: id=%s, type=%s, width=%.1f, height=%.1f",
               info.componentId.c_str(), info.type.c_str(), info.width, info.height);
    auto node = findNodeByComponentIdAndTypeRecursive(_root, info.componentId, info.type);
    if (!node) return;
    // Record the measured intrinsic size on the Yoga node; it is independent
    // of the surface width and will be consumed by the next layout pass.
    node->setYogaNodeSize(info.width, info.height);
    // Single-shot renderer callback: fire layout pass immediately against the
    // surface context's current width.
    if (_layoutEngine) {
        const float surfaceWidth = _surfaceContext ? _surfaceContext->getSurfaceWidth() : 0.0f;
        _layoutEngine->calculateLayoutWithAdjust(_root, surfaceWidth);
        checkAndNotifyLayoutChanges();
    }
}

void VirtualDOM::updateTabsSelectedIndex(const std::string& tabsId, int selectedIndex) {
    AGENUI_LOG("[Tabs] updateTabsSelectedIndex id=%s index=%d", tabsId.c_str(), selectedIndex);
    if (!_layoutEngine) return;
    // Record the new selected index unconditionally and replay layout
    // immediately against the surface context's current width.
    _layoutEngine->updateTabsSelectedIndex(tabsId, selectedIndex);
    const float surfaceWidth = _surfaceContext ? _surfaceContext->getSurfaceWidth() : 0.0f;
    _layoutEngine->calculateLayoutWithAdjust(_root, surfaceWidth);
    checkAndNotifyLayoutChanges();
}

std::shared_ptr<VirtualDOMNode> VirtualDOM::findNodeByComponentIdAndTypeRecursive(
    std::shared_ptr<VirtualDOMNode> parent, 
    const std::string& componentId, 
    const std::string& type) {
    
    if (!parent) {
        return nullptr;
    }

    if (parent->hasSnapshot()) {
        const ComponentSnapshot* snapshot = parent->getSnapshot();
        if (snapshot && snapshot->id == componentId && snapshot->component == type) {
            return parent;
        }
    }

    const auto& children = parent->getChildren();
    for (const auto& child : children) {
        if (!child) continue;
        auto found = findNodeByComponentIdAndTypeRecursive(child, componentId, type);
        if (found) return found;
    }
    
    return nullptr;
}

}  // namespace agenui
