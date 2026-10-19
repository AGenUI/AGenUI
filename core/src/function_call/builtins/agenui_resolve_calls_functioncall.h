#pragma once

#include "function_call/agenui_ifunctioncall.h"
#include "function_call/agenui_functioncall_resolution.h"
#include "function_call/agenui_functioncall_config.h"
#include <optional>
#include <string>

namespace agenui {

class FunctionCallManager;

/**
 * @brief resolveCalls functionCall — resolves every functionCall nested inside its `json` argument
 * @remark Walks the given JSON and replaces each `{"call": ..., "args": ...}` node with that call's
 *         value, so a payload delivered as one JSON literal can still carry functionCalls.
 *         A call that fails is dropped rather than left as written, the way StructuredDataValue
 *         drops an unresolved value; the drop is logged with the node's JSONPath location.
 *         Data bindings (`{"path": ...}`) are deliberately left as written: a builtin functionCall
 *         is handed no IDataValueContext, so there is no data model to read them against.
 */
class ResolveCallsFunctionCall : public IFunctionCall {
public:
    /**
     * @brief Constructor
     * @param manager FunctionCall manager used to resolve nested calls; not owned, and it
     *        outlives this object because it is the one holding it
     */
    explicit ResolveCallsFunctionCall(FunctionCallManager* manager);

    /**
     * @brief Required by the base class, which declares it pure virtual
     * @remark FunctionCallManager always calls the context-taking overload, so this one is only
     *         reached by a caller holding a ResolveCallsFunctionCall directly.
     */
    FunctionCallResolution execute(const nlohmann::json& args) override;

    FunctionCallResolution execute(const nlohmann::json& args,
                                   const FunctionCallContext& context) override;

    FunctionCallConfig getConfig() const override;

private:
    /**
     * @brief Walk one node, resolving it if it is a functionCall and descending otherwise
     * @param path JSONPath location of @p node, used only by the failure log
     * @return nullopt when the node is a call that could not be resolved, so the caller can drop it
     *         the way StructuredDataValue drops an unresolved field; a JSON null written in the
     *         payload comes back as a value, not as nullopt
     * @remark Scalars are returned untouched, so a binding object stays a binding object.
     */
    std::optional<nlohmann::json> resolveNode(const nlohmann::json& node,
                                              const FunctionCallContext& context,
                                              const std::string& path) const;

    /**
     * @brief Resolve one `{"call": ..., "args": ...}` node
     * @param path JSONPath location of @p node, used only by the failure log
     * @return The call's value, or nullopt when it failed
     * @remark A nested resolveCalls is resolved here rather than routed through the manager, so its
     *         payload is walked once instead of twice.
     */
    std::optional<nlohmann::json> resolveCall(const nlohmann::json& node,
                                              const FunctionCallContext& context,
                                              const std::string& path) const;

    FunctionCallManager* _manager;
};

} // namespace agenui
