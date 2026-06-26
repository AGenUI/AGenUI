#include "agenui_functioncall_manager.h"
#include "agenui_platform_function.h"
#include "agenui_logger_internal.h"
#include "agenui_type_define.h"
#include <fstream>
#include <sstream>

namespace agenui {

FunctionCallManager::FunctionCallManager() {
}

FunctionCallManager::~FunctionCallManager() {
    // NOTE: IPlatformFunction* pointers stored in _functionCalls are owned by the caller (platform/JNI layer).
    // FunctionCallManager doe∂s not release them. The caller must ensure all registered functions
    // are either unregistered or still alive before FunctionCallManager is destroyed,
    // otherwise executeFunctionCallSync may access a dangling pointer.
    _functionCalls.clear();
    _cppFunctionCalls.clear();
}

bool FunctionCallManager::registerFunctionCall(const FunctionCallConfig& config, IPlatformFunction* function) {
    if (!config.isValid()) {
        AGENUI_LOG("invalid config");
        return false;
    }
    if (!function) {
        AGENUI_LOG("function is null");
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    FunctionCallEntry entry;
    entry.config = config;
    entry.function = function;

    // Register by short name so lookups use "toast" rather than "agenui.platform::toast"
    const std::string& name = config.getName();
    std::string fullName = config.getFullName();
    _functionCalls[name] = entry;

    return true;
}

bool FunctionCallManager::unregisterFunctionCall(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    auto it = _functionCalls.find(name);
    if (it == _functionCalls.end()) {
        AGENUI_LOG("name:%s not found", name.c_str());
        return false;
    }
    _functionCalls.erase(it);
    return true;
}

bool FunctionCallManager::registerFunctionCall(FunctionCallPtr functionCall) {
    if (!functionCall) {
        AGENUI_LOG("functionCall is null");
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(_mutex);

    std::string name = functionCall->getName();
    _cppFunctionCalls[name] = functionCall;

    return true;
}

FunctionCallResolution FunctionCallManager::executeFunctionCallSync(const std::string& name, const FunctionCallContext& context, const nlohmann::json& args) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    // 1. Look up C++ functionCall first (shared_ptr, inherently safe)
    FunctionCallPtr cppFunctionCall = findCppFunctionCall(name);
    if (cppFunctionCall) {
        return cppFunctionCall->execute(args);
    }

    // 2. Fall back to platform functionCall
    FunctionCallEntry* entry = findFunctionCall(name);
    if (!entry || !entry->function) {
        AGENUI_LOG("Platform functionCall not found: %s", name.c_str());
        return FunctionCallResolution::createError("FunctionCall not found: " + name);
    }

    // 3. Execute platform functionCall synchronously while holding the lock.
    //    This ensures the pointer remains valid — unregisterFunctionCall (main thread)
    //    cannot erase the entry until callSync returns.
    FunctionCallResult callResult = entry->function->callSync(context, args.dump());
    FunctionCallResolution resolution = FunctionCallResolution::fromPlatformResult(callResult);
    if (resolution.getStatus() == FunctionCallStatus::Pending) {
        AGENUI_LOG("callSync unexpectedly returned Pending for functionCall: %s", name.c_str());
        return FunctionCallResolution::createError("callSync unexpectedly returned Pending: " + name);
    }
    if (resolution.getStatus() == FunctionCallStatus::Error) {
        AGENUI_LOG("callSync failed for functionCall: %s, error: %s", name.c_str(), resolution.getError().c_str());
    }
    return resolution;
}


std::vector<FunctionCallConfig> FunctionCallManager::getAllFunctionCalls() const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    std::vector<FunctionCallConfig> configs;
    
    // Add C++ functionCalls
    for (const auto& pair : _cppFunctionCalls) {
        configs.emplace_back(pair.second->getConfig());
    }

    // Add platform functionCalls
    for (const auto& pair : _functionCalls) {
        configs.emplace_back(pair.second.config);
    }

    return configs;
}

nlohmann::json FunctionCallManager::exportCatalog() const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    nlohmann::json catalog;
    nlohmann::json functions = nlohmann::json::array();

    // Add C++ functionCalls
    for (const auto& pair : _cppFunctionCalls) {
        functions.emplace_back(pair.second->getConfig().toJson());
    }

    // Add platform functionCalls
    for (const auto& pair : _functionCalls) {
        functions.emplace_back(pair.second.config.toJson());
    }
    
    catalog["functions"] = functions;
    return catalog;
}


FunctionCallEntry* FunctionCallManager::findFunctionCall(const std::string& name) {
    auto it = _functionCalls.find(name);
    if (it != _functionCalls.end()) {
        return &(it->second);
    }
    return nullptr;
}

FunctionCallPtr FunctionCallManager::findCppFunctionCall(const std::string& name) {
    auto it = _cppFunctionCalls.find(name);
    if (it != _cppFunctionCalls.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace agenui
