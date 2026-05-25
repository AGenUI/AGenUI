#pragma once

#include "agenui_icomponent_manager.h"
#include "agenui_component_model.h"
#include "surface/virtual_dom/agenui_ivirtual_dom.h"
#include "surface/agenui_isurface_context.h"
#include "agenui_batch_guard.h"
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <unordered_set>
#include "nlohmann/json.hpp"

namespace agenui {

// Forward declaration
class IDataModel;

/**
 * @brief Component manager
 * @remark Manages all components for a surface, handling component creation, update, and deletion
 */
class ComponentManager : public IComponentManager, public IComponentChangedObserver, public ITemplateComponentGenerator {
public:
    /**
     * @brief Constructor
     * @param surfaceContext Surface context pointer
     * @param virtualDom Virtual DOM pointer
     * @param theme Theme identifier
     */
    ComponentManager(ISurfaceContext* surfaceContext, IVirtualDOM* virtualDom, const std::string& theme);

    ~ComponentManager() override;

    /**
     * @brief Update components
     * @param components Array of component JSON strings
     */
    void updateComponents(const std::vector<std::string>& components) override;

    /**
     * @brief Synchronize a binding value
     * @param id Component ID
     * @param attributeName Attribute name
     * @param value New value
     */
    void syncBindingValue(const std::string& id, const std::string& attributeName, const std::string& value) override;

    /**
     * @brief Get the parent component ID
     * @param componentId Component ID
     * @return Parent component ID, or empty string if none
     */
    std::string getParentId(const std::string& componentId) override;

    /**
     * @brief Re-evaluate every component's attributes and styles
     * @remark Iterates all components and triggers a full snapshot rebuild
     *         (refreshing both attributes and styles). Unchanged values are
     *         filtered out by VirtualDom's two-layer diff.
     */
    void invalidateFunctionCallValues() override;

    /**
     * @brief Batch-set display rules for components
     * @param displayRules Map from componentId to DisplayRule
     */
    void setComponentsDisplayRule(const std::map<std::string, DisplayRule>& displayRules) override;

    /**
     * @brief Execute a component action
     * @param componentId Component ID
     * @param surfaceId Surface ID
     * @param dispatcher Event dispatcher pointer
     */
    void executeComponentAction(const std::string& componentId, const std::string& surfaceId, void* dispatcher) override;

    /**
     * @brief Called when a component is deleted
     * @param componentId ID of the deleted component
     */
    void onComponentDeleted(const std::string& componentId) override;

    /**
     * @brief Called when a component has changed. Enqueues the component
     *        id into the pending dirty queue and requests a flush via the
     *        BatchGuard. If a batch window is open, the flush is deferred;
     *        otherwise it fires immediately.
     */
    void onComponentChanged(const std::string& componentId) override;

    /**
     * @brief Access the batch guard for this component manager.
     */
    BatchGuard* batchGuard() override { return &_batchGuard; }

    /**
     * @brief Generate list child components
     * @param templateId Child component template ID
     * @param data Component data
     * @return Array of generated component model smart pointers
     */
    std::vector<std::shared_ptr<ComponentModel>> generateListChildren(const std::string& templateId, std::shared_ptr<DataValue> data) override;

    /**
     * @brief Generate a component from a template
     * @param templateId Template ID
     * @param data Component data (must be DataBindingDataValue so that bindingPath can be extracted)
     * @return Newly created component model, or nullptr on failure
     */
    std::shared_ptr<ComponentModel> generateComponentWithTemplate(const std::string& templateId, std::shared_ptr<DataValue> data) override;

private:
    /**
     * @brief Parse a component from a JSON string
     * @param componentJson Component JSON string
     * @return Component model smart pointer, or nullptr on failure
     */
    std::shared_ptr<ComponentModel> parseComponent(const std::string& componentJson);

    /**
     * @brief Parse child components from JSON
     * @param json JSON object
     * @param componentType Component type string
     * @param entity Parent component model
     */
    void parseChildren(const nlohmann::json& json, const std::string& componentType, std::shared_ptr<ComponentModel> entity);

    /**
     * @brief Register a newly created component and enqueue it for the
     *        next flush, atomically.
     *
     * Wraps the two operations that must always happen together for a
     * brand-new component:
     *   1. insert it into the _components lookup table
     *   2. enqueue its id into the dirty queue
     *
     * The component is constructed full-dirty (see ComponentModel default
     * state), so this method only needs to enqueue the id without touching
     * the model's dirty flags.
     *
     * Safe to call inside a batch window; the flush is deferred to batch close.
     *
     * @param component Component to register; ignored when null.
     */
    void addComponent(std::shared_ptr<ComponentModel> component);

    /**
     * @brief Flush a single dirty component through the virtual DOM.
     *
     * Reads the merged snapshot from ComponentModel::flushDirty() and
     * forwards it to IVirtualDOM::updateNode. Safe to call when the
     * component has nothing pending: the call becomes a no-op.
     */
    void flushDirtyComponent(const std::shared_ptr<ComponentModel>& component);

    /**
     * @brief Drain the pending dirty-component queue.
     *
     * Called by the BatchGuard flush callback. Iterates the queue in arrival
     * order, deduplicating on the fly so each component is flushed exactly
     * once even if it was marked dirty multiple times during the batch.
     * The queue is cleared on entry to allow re-entrant marking (e.g. a
     * flush that itself triggers further data binding callbacks).
     */
    void flushDirtyComponents();

    /**
     * @brief Attempt to update a template
     * @param componentId Component ID
     */
    void tryUpdateTemplate(const std::string& componentId);

    ISurfaceContext* _surfaceContext;                                     // Surface context pointer
    IVirtualDOM* _virtualDom;                                            // Virtual DOM pointer
    std::string _theme;                                                  // Theme identifier
    std::map<std::string, std::shared_ptr<ComponentModel>> _components;  // Component map (key = id)
    std::map<std::string, DisplayRule> _displayRules;                    // Display rule map (key = componentId)

    // ---- Batched dirty-component bookkeeping ----
    // _batchGuard: manages batch depth and triggers flushDirtyComponents()
    //              when the outermost batch window closes (or immediately
    //              if no batch is active when requestFlush() is called).
    // _dirtyOrder: arrival-ordered list of component ids marked dirty in
    //              the current batch; iteration order determines flush order.
    // _dirtyIndex: membership set used to deduplicate _dirtyOrder so the
    //              same component is only flushed once per batch even when
    //              several of its attributes change.
    BatchGuard _batchGuard;
    std::vector<std::string> _dirtyOrder;
    std::unordered_set<std::string> _dirtyIndex;
};

}  // namespace agenui
