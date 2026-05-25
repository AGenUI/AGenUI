#pragma once

#include <string>
#include <vector>
#include <map>

namespace agenui {

class BatchGuard;
enum class DisplayRule;

/**
 * @brief Component manager interface
 * @remark Defines the basic operations for component management
 *
 * Callers should wrap bursts of operations that may fan out into many
 * attribute changes (data-model updates, function-call invalidation
 * sweeps, UI->data sync, etc.) in a BatchScope via batchGuard() so each
 * affected component is flushed to the virtual DOM at most once.
 */
class IComponentManager {
public:
    virtual ~IComponentManager() = default;

    /**
     * @brief Update components
     * @param components Array of component JSON strings
     */
    virtual void updateComponents(const std::vector<std::string>& components) = 0;

    /**
     * @brief Synchronize a binding value
     * @param id Component ID
     * @param attributeName Attribute name
     * @param value New value
     * @remark Called when a bound value of a component changes
     */
    virtual void syncBindingValue(const std::string& id, const std::string& attributeName, const std::string& value) = 0;

    /**
     * @brief Get the parent component ID
     * @param componentId Component ID
     * @return Parent component ID, or an empty string if there is no parent
     */
    virtual std::string getParentId(const std::string& componentId) = 0;

    /**
     * @brief Re-evaluate every component's attributes and styles
     * @remark Iterates all components and triggers a full snapshot rebuild,
     *         causing each DataValue to re-run getValueData() (including any
     *         registered FunctionCalls). Unchanged values are filtered out by
     *         VirtualDom's two-layer diff before reaching the native renderer.
     */
    virtual void invalidateFunctionCallValues() = 0;

    /**
     * @brief Batch-set the display rules for components
     * @param displayRules Map from componentId to DisplayRule
     * @remark Updates the display rule for each specified component, affecting orphan snapshot display logic
     */
    virtual void setComponentsDisplayRule(const std::map<std::string, DisplayRule>& displayRules) = 0;

    /**
     * @brief Execute a component action
     * @param componentId Component ID
     * @param surfaceId Surface ID
     * @param dispatcher Event dispatcher pointer
     */
    virtual void executeComponentAction(const std::string& componentId, const std::string& surfaceId, void* dispatcher) = 0;

    /**
     * @brief Access the batch guard for this component manager.
     * @return Non-owning pointer to the internal BatchGuard; never null
     *         for a live instance.
     */
    virtual BatchGuard* batchGuard() = 0;
};

}  // namespace agenui
