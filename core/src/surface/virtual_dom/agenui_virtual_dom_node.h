#pragma once

#include "agenui_component_snapshot.h"
#include "agenui_virtual_dom_observer.h"
#include "agenui_measurement.h"
#include <memory>
#include <vector>
#include <string>
#include <limits>

// Forward declaration
namespace agenui {
class IMeasurementManager;
class ILayoutDelegate;
}  // namespace agenui

#include <yoga/Yoga.h>
#include "surface/yoga_node/agenui_yoga_node.h"
#include "surface/yoga_node/agenui_yoga_node_manager.h"

namespace agenui {

/**
 * @brief Interface for fetching orphan component snapshots
 * @remark Used to retrieve snapshots of components whose parent node was not found
 */
class IOrphanSnapshotFetcher {
public:
    virtual ~IOrphanSnapshotFetcher() = default;

    /**
     * @brief Fetch and remove the orphan snapshot for the specified ID
     * @param id Component ID
     * @param outSnapshot Output parameter; returns the found component snapshot
     * @return true if found, false otherwise
     * @remark The snapshot is automatically removed from the orphan collection after a successful fetch
     */
    virtual bool takeOrphanSnapshot(const std::string& id, ComponentSnapshot& outSnapshot) = 0;
};

/**
 * @brief Virtual DOM node
 * @remark Used to build a tree structure of components
 */
class VirtualDOMNode {
public:
    /**
     * @brief Constructor
     * @param id Node ID
     * @param observer Virtual DOM observer
     * @param orphanFetcher Orphan snapshot fetcher
     */
    VirtualDOMNode(const std::string& id,
                   IVirtualDOMObserver* observer,
                   IOrphanSnapshotFetcher* orphanFetcher,
                   ::agenui::IMeasurementManager* measurementManager = nullptr
                   , ILayoutDelegate* layoutDelegate = nullptr
                   );

    /**
     * @brief Destructor
     */
    ~VirtualDOMNode();


    /**
     * @brief Get the node ID
     * @return Node ID
     */
    const std::string& getId() const;

    /**
     * @brief Check whether the node has a snapshot
     * @return true if a snapshot exists, false otherwise
     */
    bool hasSnapshot() const;

    /**
     * @brief Get the component snapshot
     * @return Snapshot pointer; nullptr if no snapshot exists
     */
    const ComponentSnapshot* getSnapshot() const { return _snapshot.get(); }

    /**
     * @brief Get the snapshot as a shared_ptr (for callers that need to share
     *        ownership, e.g. constructing a ComponentSnapshotWrapper).
     * @return Shared pointer to the snapshot; may be empty if no snapshot exists.
     */
    const std::shared_ptr<ComponentSnapshot>& getSnapshotShared() const { return _snapshot; }

    /**
     * @brief Get the parent node pointer (non-owning)
     */
    const VirtualDOMNode* getParent() const { return _parent; }

    /**
     * @brief Get the snapshot with layout info (mutable)
     * @return Snapshot pointer; nullptr if layout has not been notified yet
     */
    ComponentSnapshot* getSnapshotWithLayout() { return _snapshotWithLayout.get(); }

    /**
     * @brief Set the component snapshot
     * @param snapshot Component snapshot
     * @param parentId Parent node ID
     */
    void setSnapshot(const ComponentSnapshot& snapshot, const std::string& parentId);

    /**
     * @brief Get all child nodes
     * @return Child node array
     */
    const std::vector<std::shared_ptr<VirtualDOMNode>>& getChildren() const;

    /**
     * @brief Get the Yoga layout node (YGNodeRef, backward-compatible)
     * @return Yoga node reference
     */
    YGNodeRef getYogaNode() const { return _yogaNode ? _yogaNode->get() : nullptr; }

    /**
     * @brief Get the YogaNode wrapper object
     */
    YogaNode* getYogaNodeObj() const { return _yogaNode; }

    /**
     * @brief Set the Yoga node dimensions
     * @param width Width
     * @param height Height
     * @remark Used to dynamically update component dimensions, e.g., after Markdown rendering completes
     */
    void setYogaNodeSize(float width, float height);

    /**
     * @brief Check and notify layout changes
     * @remark Recursively checks the current node and its children for layout changes and notifies the observer
     */
    void checkAndNotifyLayoutChanges();

    /**
     * @brief Refresh child nodes level by level
     * @remark Updates the node's own children first, then recursively refreshes child nodes
     */
    void refreshChildrenRecursively();

    /**
     * @brief Find a child node by ID
     * @param id Node ID
     * @return Shared pointer to the child node; nullptr if not found
     */
    std::shared_ptr<VirtualDOMNode> findChild(const std::string& id) const;

    /**
     * @brief Reset platform-supplied size lock, restore Yoga style to original values
     * @remark Called when Surface size changes; restores width/height to applySnapshot values
     */
    void resetPlatformSize();

private:
    /**
     * @brief Log Yoga layout results and applied style properties for the current node
     * @param snapshotWithLayout Snapshot copy populated with layout info
     */
    void logYogaLayoutInfo(const ComponentSnapshot& snapshotWithLayout) const;
    /**
     * @brief Notify the observer of a component update
     * @param newSnapshot New component snapshot
     */
    void notifyComponentUpdate(const ComponentSnapshot& newSnapshot);

    /**
     * @brief Notify the observer that a component was added
     * @param parentId Parent node ID
     */
    void notifyComponentAdded();

    /**
     * @brief Notify the observer that a component was removed
     */
    void notifyComponentRemoved();

    /**
     * @brief Update the child node list to match _snapshot->children
     * @remark Synchronizes both _children and the Yoga child nodes
     */
    void updateChildren();

    /**
     * @brief Set up a measure function for components that need intrinsic sizing
     * @remark Called before convertYogaStyles; determines whether intrinsic measurement is needed
     */
    void setupMeasureFunctionIfNeeded();
    
    /**
     * @brief Save width/height from snapshot.styles into layout.styleInfo before Yoga conversion
     * @remark Only applies to Image components; must be called before convertToYoga with clearAfterConvert=true
     */
    void saveImageStyleInfo();

    /**
     * @brief Route measurement to the corresponding component implementation via IMeasurementManager
     * @remark Constructs paramJson and MeasureModes, then calls _measurementManager->measure()
     */
    YGSize routeMeasure(const ComponentSnapshot& snapshot,
                        float width, YGMeasureMode widthMode,
                        float height, YGMeasureMode heightMode);

    /**
     * @brief Serialize ComponentSnapshot to paramJson (for use by IMeasurement)
     */
    std::string buildParamJson(const ComponentSnapshot& snapshot) const;

    /**
     * @brief Compare two component snapshots for changes (ignoring the children field)
     * @param snapshot1 First component snapshot
     * @param snapshot2 Second component snapshot
     * @param compareLayout Whether to compare the layout field
     * @param diff Output parameter; returns diff info (optional, default nullptr)
     * @return true if changed, false otherwise
     */
    static bool checkSnapshotChanged(const ComponentSnapshot& snapshot1, const ComponentSnapshot& snapshot2, bool compareLayout, std::string* diff = nullptr);

private:
    std::string _id;                                                                        // Node ID
    std::string _yogaKey;                                                                   // Unique key for YogaNodeManager (id#seq)
    std::string _parentId;                                                                  // Parent node ID
    VirtualDOMNode* _parent = nullptr;                                                      // Parent node pointer (non-owning)
    std::shared_ptr<ComponentSnapshot> _snapshot;                                           // Raw component snapshot (before Yoga layout)
    std::shared_ptr<ComponentSnapshot> _snapshotWithLayout;                                 // Component snapshot (with Yoga layout info filled in)
    std::vector<std::shared_ptr<VirtualDOMNode>> _children;                                 // Child node list
    IVirtualDOMObserver* _observer;                                                         // Virtual DOM observer
    IOrphanSnapshotFetcher* _orphanFetcher;                                                 // Orphan snapshot fetcher
    ::agenui::IMeasurementManager* _measurementManager = nullptr;                          // Component measurement manager (non-owning)
    YogaNode* _yogaNode = nullptr;                                                          // Yoga layout node (owned by YogaNodeManager)
    ILayoutDelegate* _layoutDelegate = nullptr;                                             // Layout delegate (non-owning)
    bool _hasPlatformSize = false;                                                          // True after platform reports a concrete size via notifyRenderFinish;
                                                                                            // suppresses measureFunc re-registration until the snapshot actually changes

    bool _sizeFromReport = false;                                                           // True when the next layout notification carries a platform-reported
                                                                                            // size (set by setYogaNodeSize, consumed by checkAndNotifyLayoutChanges)

    // De-duplication state for logYogaLayoutInfo: skip output when layout (x/y/w/h) is unchanged.
    // Keeps original AGENUI_LOG macro but prevents flooding the same numbers on every recalc.
    mutable float _lastLogX = std::numeric_limits<float>::quiet_NaN();
    mutable float _lastLogY = std::numeric_limits<float>::quiet_NaN();
    mutable float _lastLogW = std::numeric_limits<float>::quiet_NaN();
    mutable float _lastLogH = std::numeric_limits<float>::quiet_NaN();
};

}  // namespace agenui
