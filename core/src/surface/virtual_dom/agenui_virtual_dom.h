#pragma once

#include "surface/virtual_dom/agenui_virtual_dom_node.h"
#include "surface/virtual_dom/agenui_ivirtual_dom.h"
#include "agenui_batch_guard.h"

#include <memory>
#include <string>
#include <map>

#include "surface/yoga_node/agenui_yoga_node_manager.h"
#include "surface/yoga_node/agenui_tabs_yoga_helper.h"
#include "surface/yoga_node/agenui_yoga_layout_engine.h"
#include "agenui_render_info_types.h"

namespace agenui {

/**
 * @brief Virtual DOM
 * @remark Manages the component tree structure; serves as the intermediate representation before actual rendering
 */
class VirtualDOM : public IVirtualDOM, public IOrphanSnapshotFetcher {
public:
    /**
     * @brief Constructor
     * @param observer Virtual DOM observer
     */
    explicit VirtualDOM(IVirtualDOMObserver* observer,
                        ::agenui::IMeasurementManager* measurementManager = nullptr);

    /**
     * @brief Destructor
     */
    ~VirtualDOM();

    /**
     * @brief Update a node
     * @param snapshot New component snapshot
     */
    void updateNode(const ComponentSnapshot& snapshot) override;

    /**
     * @brief Clear the tree
     */
    void clear() override;

    /**
     * @brief Access the batch guard for this virtual DOM.
     */
    BatchGuard* batchGuard() override { return &_batchGuard; }

    /**
     * @brief Get the root node
     * @return Shared pointer to the root node; nullptr if the tree is empty
     */
    std::shared_ptr<VirtualDOMNode> getRoot() const { return _root; }

    /**
     * @brief Fetch and remove the orphan snapshot for the specified ID
     * @param id Component ID
     * @param outSnapshot Output parameter; returns the found component snapshot
     * @return true if found, false otherwise
     * @remark The snapshot is automatically removed from the orphan collection after a successful fetch
     */
    bool takeOrphanSnapshot(const std::string& id, ComponentSnapshot& outSnapshot) override;

    /**
     * @brief Update the Markdown component dimensions
     * @param info Markdown render info, including component ID, type, and rendered size
     * @remark Looks up the corresponding VirtualDOMNode by componentId and type,
     *         then updates the Yoga node height
     */
    void updateComponentSize(const ComponentRenderInfo& info);

    /**
     * @brief Update the Surface dimensions
     * @param info Surface info, including surfaceId and size
     * @remark Updates the root container size of the surface
     */
    void updateSurfaceSize(const SurfaceLayoutInfo& info);
    
    /**
     * @brief Update Tabs selected tab index and trigger re-layout
     * @param tabsId Tabs node ID
     * @param selectedIndex New selected tab index
     * @remark Called by the rendering layer on tab switch; updates Tabs Yoga minHeight and triggers re-layout
     */
    void updateTabsSelectedIndex(const std::string& tabsId, int selectedIndex);

private:
    /**
     * @brief Recursively search for a node by ID
     * @param parent Parent node
     * @param id Component ID
     * @param outParentId Output parameter; returns the parent node ID when found
     * @return Shared pointer to the found node; nullptr if not found
     */
    std::shared_ptr<VirtualDOMNode> findNodeByIdRecursive(std::shared_ptr<VirtualDOMNode> parent, const std::string& id, std::string& outParentId);

    /**
     * @brief Recursively search for a node by componentId and type
     * @param parent Parent node
     * @param componentId Component ID
     * @param type Component type
     * @return Shared pointer to the found node; nullptr if not found
     */
    std::shared_ptr<VirtualDOMNode> findNodeByComponentIdAndTypeRecursive(std::shared_ptr<VirtualDOMNode> parent, const std::string& componentId, const std::string& type);

    /**
     * @brief Recursively walk the tree from the root and notify the
     *        observer of any layout changes.
     */
    void checkAndNotifyLayoutChanges();

    /**
     * @brief Check whether an orphan snapshot meets the display conditions
     * @param snapshot Snapshot to evaluate
     * @return true if it can be displayed, false otherwise
     * @remark Evaluated based on DisplayRule and children's dataBindingStatus
     */
    bool checkCanDisplay(const ComponentSnapshot& snapshot) const;

    /**
     * @brief Attempt to attach ready orphan snapshots to the component tree
     * @remark Iterates _dataDependentOrphanSnapshots and attaches those that pass checkCanDisplay
     */
    void tryAttachReadyOrphans();

    /**
     * @brief Recursively reset platform-supplied size on all nodes
     * @param node Starting node for recursive traversal
     */
    void resetPlatformSizeRecursive(std::shared_ptr<VirtualDOMNode>& node);

    std::shared_ptr<VirtualDOMNode> _root;                       // Root node
    IVirtualDOMObserver* _observer;                              // Virtual DOM observer
    std::map<std::string, ComponentSnapshot> _directOrphanSnapshots;          // Orphan snapshots that display unconditionally
    std::map<std::string, ComponentSnapshot> _dataDependentOrphanSnapshots;   // Orphan snapshots that depend on data binding state
    ::agenui::IMeasurementManager* _measurementManager = nullptr;  // Component measurement manager (non-owning)
    std::unique_ptr<YogaLayoutEngine> _layoutEngine;             // Layout engine (ILayoutDelegate, owns the YogaNodeManager)
    float _surfaceWidth  = 0.0f;                                 // Current surface width (vp); initialized from getDeviceScreenSize
    float _surfaceHeight = 0.0f;                                 // Current surface height (vp); initialized from getDeviceScreenSize

    // ---- Batched updateNode bookkeeping ----
    // _batchGuard: manages batch depth and triggers the deferred layout
    //              pass (calculateLayoutWithAdjust + checkAndNotifyLayoutChanges)
    //              when the outermost batch window closes (or immediately
    //              if no batch is active when requestFlush() is called).
    BatchGuard _batchGuard;
};

}  // namespace agenui
