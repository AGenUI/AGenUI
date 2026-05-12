#include "surface/virtual_dom/agenui_virtual_dom.h"
#include "agenui_component_render_observable.h"
#include "agenui_surface_layout_observable.h"
#include "agenui_logger_internal.h"
#include <functional>
#include <yoga/Yoga.h>
#include "surface/virtual_dom/agenui_ivirtual_define.h"
#include "surface/yoga_node/agenui_yoga_node_manager.h"

namespace agenui {

VirtualDOM::VirtualDOM(IVirtualDOMObserver* observer,
                       ::agenui::IMeasurementManager* measurementManager)
    : _root(nullptr)
    , _observer(observer)
    , _measurementManager(measurementManager) {
    _yogaNodeManager = std::make_unique<YogaNodeManager>();
    YGSize screenSize = AGenUIVirtualDefine::getDeviceScreenSize();
    _surfaceWidth  = screenSize.width;
    _surfaceHeight = screenSize.height;
    _root = std::make_shared<VirtualDOMNode>("root", observer, this, measurementManager
        , _yogaNodeManager.get()
    );
}

VirtualDOM::~VirtualDOM() {
    // _yogaNodeManager is released automatically by unique_ptr
}

void VirtualDOM::updateNode(const ComponentSnapshot& snapshot) {
    if (snapshot.id == "root") {
        _root->setSnapshot(snapshot, "");
        if (_yogaNodeManager) {
            _yogaNodeManager->calculateLayoutWithAdjust(_root, _surfaceWidth);
            checkAndNotifyLayoutChanges();
        }
        return;
    }

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

    if (_yogaNodeManager) {
        _yogaNodeManager->calculateLayoutWithAdjust(_root, _surfaceWidth);
        checkAndNotifyLayoutChanges();
    }
}

void VirtualDOM::clear() {
    // Correct clear order:
    // 1. First release old tree: _root.reset() recursively destructs from leaves to root.
    //    Each VirtualDOMNode destructor only nulls out, and YogaNode::~YogaNode handles _hasOwner detach.
    // 2. Then clearAll: detaches all YG parent-child relationships in _nodes, then frees in batch.
    _directOrphanSnapshots.clear();
    _dataDependentOrphanSnapshots.clear();
    _root.reset();
    if (_yogaNodeManager) {
        _yogaNodeManager->clearAll();
    }
    _root = std::make_shared<VirtualDOMNode>("root", _observer, this, _measurementManager
        , _yogaNodeManager.get()
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

void VirtualDOM::updateSurfaceSize(const SurfaceLayoutInfo& info) {
    AGENUI_LOG("updateSurfaceSize: surfaceId=%s, width=%.1f, height=%.1f",
               info.surfaceId.c_str(), info.width, info.height);
    if (info.width > 0.0f)  _surfaceWidth  = info.width;
    if (info.height > 0.0f) _surfaceHeight = info.height;
    if (_yogaNodeManager) {
        _yogaNodeManager->calculateLayoutWithAdjust(_root, _surfaceWidth);
    }
    checkAndNotifyLayoutChanges();
}


void VirtualDOM::updateComponentSize(const ComponentRenderInfo& info) {
    auto node = findNodeByComponentIdAndTypeRecursive(_root, info.componentId, info.type);
    if (!node) {
        return;
    }

    node->setYogaNodeSize(info.width, info.height);
    if (_yogaNodeManager) {
        _yogaNodeManager->calculateLayoutWithAdjust(_root, _surfaceWidth);
        checkAndNotifyLayoutChanges();
    }
}

void VirtualDOM::updateTabsSelectedIndex(const std::string& tabsId, int selectedIndex) {
    AGENUI_LOG("[Tabs] updateTabsSelectedIndex id=%s index=%d", tabsId.c_str(), selectedIndex);
    if (!_yogaNodeManager) return;
    _yogaNodeManager->updateTabsSelectedIndex(tabsId, selectedIndex);
    _yogaNodeManager->calculateLayoutWithAdjust(_root, _surfaceWidth);
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
