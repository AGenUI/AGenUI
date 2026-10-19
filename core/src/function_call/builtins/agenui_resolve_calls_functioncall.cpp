#include "agenui_resolve_calls_functioncall.h"

#include "function_call/agenui_functioncall_manager.h"
#include "agenui_logger_internal.h"

namespace agenui {

namespace {

constexpr const char* kSelfName = "resolveCalls";
constexpr const char* kJsonArgName = "json";
constexpr const char* kCallKey = "call";
constexpr const char* kArgsKey = "args";

// Failure logs locate a node by JSONPath, rooted at the payload passed as the `json` argument.
constexpr const char* kRootPath = "$";

bool isFunctionCallNode(const nlohmann::json& node) {
    return node.is_object() && node.contains(kCallKey) && node[kCallKey].is_string();
}

std::string fieldPath(const std::string& parent, const std::string& key) {
    return parent + "." + key;
}

std::string indexPath(const std::string& parent, size_t index) {
    return parent + "[" + std::to_string(index) + "]";
}

// json declares a templated operator ValueType(), which competes with optional's converting
// constructor, so a json cannot be returned implicitly from a function typed optional<json>.
using ResolvedNode = std::optional<nlohmann::json>;

}  // namespace

ResolveCallsFunctionCall::ResolveCallsFunctionCall(FunctionCallManager* manager) : _manager(manager) {
}

FunctionCallResolution ResolveCallsFunctionCall::execute(const nlohmann::json& args) {
    return execute(args, FunctionCallContext{0, ""});
}

FunctionCallResolution ResolveCallsFunctionCall::execute(const nlohmann::json& args,
                                                         const FunctionCallContext& context) {
    if (_manager == nullptr) {
        return FunctionCallResolution::createError("resolveCalls: FunctionCallManager is not available");
    }

    if (!args.contains(kJsonArgName)) {
        return FunctionCallResolution::createError("Missing required parameter: json");
    }

    const ResolvedNode resolved = resolveNode(args[kJsonArgName], context, kRootPath);
    // A payload that is a single failing call resolves to nothing. Returning a JSON null rather
    // than an error matches FunctionCallDataValue, which turns either into an invalid
    // SerializableData that ComponentSnapshot::stringify then drops.
    return FunctionCallResolution::createSuccess(resolved.value_or(nlohmann::json(nullptr)));
}

ResolvedNode ResolveCallsFunctionCall::resolveNode(const nlohmann::json& node,
                                                   const FunctionCallContext& context,
                                                   const std::string& path) const {
    if (isFunctionCallNode(node)) {
        return resolveCall(node, context, path);
    }

    if (node.is_object()) {
        nlohmann::json resolved = nlohmann::json::object();
        for (auto it = node.begin(); it != node.end(); ++it) {
            ResolvedNode value = resolveNode(it.value(), context, fieldPath(path, it.key()));
            // StructuredDataValue omits an unresolved object field rather than emitting null, so
            // the key disappears the same way it would have if the call had been written inline.
            if (value.has_value()) {
                resolved[it.key()] = *value;
            }
        }
        return ResolvedNode(std::move(resolved));
    }

    if (node.is_array()) {
        nlohmann::json resolved = nlohmann::json::array();
        for (size_t index = 0; index < node.size(); ++index) {
            // StructuredDataValue appends null for an unresolved element because dropping it would
            // shift the index of every element after it.
            ResolvedNode value = resolveNode(node[index], context, indexPath(path, index));
            resolved.emplace_back(value.value_or(nlohmann::json(nullptr)));
        }
        return ResolvedNode(std::move(resolved));
    }

    return ResolvedNode(node);
}

ResolvedNode ResolveCallsFunctionCall::resolveCall(const nlohmann::json& node,
                                                   const FunctionCallContext& context,
                                                   const std::string& path) const {
    const std::string name = node[kCallKey].get<std::string>();

    // A nested resolveCalls would be handed pre-resolved args and then walk that subtree a second
    // time, so its payload is resolved here instead. The walk only ever descends into a strict
    // sub-node of its input, so there is no route back for it to recurse on itself.
    if (name == kSelfName) {
        if (node.contains(kArgsKey) && node[kArgsKey].is_object()
            && node[kArgsKey].contains(kJsonArgName)) {
            const std::string payloadPath = fieldPath(fieldPath(path, kArgsKey), kJsonArgName);
            return resolveNode(node[kArgsKey][kJsonArgName], context, payloadPath);
        }
        AGENUI_LOG("resolveCalls error: path=%s, name=%s, error=no json argument",
                   path.c_str(), name.c_str());
        return std::nullopt;
    }

    // Resolving args before the call matches what FunctionCallDataValue does at parse time, so a
    // nested call sees the same argument values it would have seen written inline. An arg whose
    // own resolution failed is left out, as FunctionCallDataValue leaves out an invalid arg.
    nlohmann::json resolvedArgs = nlohmann::json::object();
    if (node.contains(kArgsKey) && node[kArgsKey].is_object()) {
        const std::string argsPath = fieldPath(path, kArgsKey);
        for (auto it = node[kArgsKey].begin(); it != node[kArgsKey].end(); ++it) {
            ResolvedNode value = resolveNode(it.value(), context, fieldPath(argsPath, it.key()));
            if (value.has_value()) {
                resolvedArgs[it.key()] = *value;
            }
        }
    }

    // Forwarding the context is what makes a nested platform functionCall work: those read
    // instanceId to reach per-instance platform services (theme, colorMode). Nested builtins
    // ignore it unless they override the context-taking execute.
    FunctionCallResolution resolution =
        _manager->executeFunctionCallSync(name, context, resolvedArgs);

    if (resolution.getStatus() != FunctionCallStatus::Success) {
        AGENUI_LOG("resolveCalls error: path=%s, name=%s, error=%s",
                   path.c_str(), name.c_str(), resolution.getError().c_str());
        return std::nullopt;
    }

    return ResolvedNode(resolution.getValue());
}

FunctionCallConfig ResolveCallsFunctionCall::getConfig() const {
    FunctionCallConfig config;
    config.setName(kSelfName);
    config.setDescription("Resolves every functionCall nested inside the given JSON, replacing each "
                          "call node with its value. A call that fails is dropped: omitted where it "
                          "is an object field, written as null where it is an array element. Data "
                          "bindings are left as written.");
    config.setReturnType("any");
    return config;
}

} // namespace agenui
