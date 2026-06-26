#include <arkui/native_node_napi.h>
#include <map>
#include "a2ui_hybrid_factory.h"
#include "a2ui_hybrid_view.h"
#include "a2ui/render/a2ui_component_state.h"
#include <a2ui_hm_helper.h>
#include "a2ui/utils/a2ui_napi_utils.h"
#include "../components/web_component.h"

namespace a2ui {

// Static member definition
std::string A2UIHybridFactory::s_engineWorkspace;
std::string A2UIHybridFactory::s_hapsFilesPath;

// Registered hybrid view metadata.
struct HybridViewInfo {
    napi_env env;
    napi_ref createFunctionRef;
    napi_ref updateFunctionRef;
};

// Hybrid view registry.
static std::map<std::string, HybridViewInfo> s_hybridViewRegistry;

// Attribute observer registry.
static std::map<std::string, std::pair<napi_env, napi_ref>> s_attributeObservers;

// Helper for building the update-state array.
static napi_value createUpdateStateArray(napi_env env, const std::vector<UpdateState>& updateStates) {
    napi_property_descriptor desc[] = {
        { "type", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "property", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "attribute", nullptr, nullptr, nullptr, nullptr, nullptr, napi_default, nullptr },
    };

    napi_value result = nullptr;
    napi_status status;
    status = napi_create_array(env, &result);
    if (status != napi_ok) {
        return nullptr;
    }

    for (uint32_t i = 0; i < updateStates.size(); ++i) {
        const UpdateState& state = updateStates[i];
        napi_value type;
        napi_create_int32(env, static_cast<int32_t>(state.type()), &type);
        desc[0].value = type;

        if (state.type() == UpdateType::AttributeChanged || state.type() == UpdateType::AttributeRemoved) {
            napi_value attributes;
            napi_create_string_utf8(env, state.attribute().c_str(), state.attribute().size(), &attributes);
            desc[2].value = attributes;
        } else {
            napi_value property;
            napi_create_int32(env, state.property(), &property);
            desc[1].value = property;
        }

        napi_value item = nullptr;
        status = napi_create_object_with_properties(env, &item, sizeof(desc) / sizeof(desc[0]), desc);
        if (status != napi_ok) {
            continue;
        }
        napi_set_element(env, result, i, item);
    }

    return result;
}

A2UIHybridView* A2UIHybridFactory::createHybridView(ComponentState* state) {
    ArkTSObject tsObject = HMHelper::ref("createHybridView");
    if (tsObject.ref == nullptr) {
        return nullptr;
    }
    
    const std::string& componentType = state->getType();
    std::string surfaceId = state->getSurfaceId();
    napi_value compContent = HMHelper::callArkTSFunction(tsObject, componentType, surfaceId, state->getId(), state);
    ArkUI_NodeHandle handle = nullptr;
    // Read the node handle created on the ArkTS side.
    OH_ArkUI_GetNodeHandleFromNapiValue(tsObject.env, compContent, &handle);

    if (handle) {
        napi_ref contentRef;
        napi_create_reference(tsObject.env, compContent, 1, &contentRef);
        
        // Special-case known hybrid view types.
        if (componentType == ComponentType::kWeb) {
            return new WebComponent(state, handle, {tsObject.env, contentRef});
        } else {
            A2UIHybridView* hybridView =new A2UIHybridView(state->getId(), componentType, state->getProperties(), handle, {tsObject.env, contentRef});
            hybridView->setState(state);
            return hybridView;
        }
    }
    
    return nullptr;
}

void A2UIHybridFactory::updateHybridView(A2UIHybridView* hybridView, const std::vector<UpdateState>& updateStates) {
    if (!hybridView || updateStates.empty()) {
        return;
    }
    
    // Mirror the ArkTS hybrid update entrypoint.
    ArkTSObject tsObject = HMHelper::ref("updateHybridView");
    if (tsObject.ref == nullptr) {
        return;
    }
    
    ComponentState* state = hybridView->getState();
    if (!state) {
        return;
    }
    
    // Build the update-state array.
    napi_value states = createUpdateStateArray(tsObject.env, updateStates);
    
    // Call updateHybridView(nodeId, updateStates) on the ArkTS side.
    HMHelper::callArkTSFunction(tsObject, state->getId(), states);
}

void A2UIHybridFactory::destroyHybridView(A2UIHybridView* hybridView) {
    ArkTSObject tsObject = HMHelper::ref("destroyHybridView");
    if (tsObject.ref == nullptr) {
        return;
    }
    
    HMHelper::callArkTSFunction(tsObject, hybridView->getId());
}

void A2UIHybridFactory::onInvokeHybridView(A2UIHybridView* hybridView, const std::string& key, const nlohmann::json& params) {
    ArkTSObject tsObject = HMHelper::ref("onInvokeHybridView");
    if (tsObject.ref == nullptr) {
        return;
    }
    
    ComponentState* state = hybridView->getState();
    if (!state) {
        return;
    }
    
    // Call onInvokeHybridView(nodeId, key, params) on the ArkTS side.
    HMHelper::callArkTSFunction(tsObject, state->getId(), key, params);
}

bool A2UIHybridFactory::hasCustomComponent(const std::string& componentType) {
    ArkTSObject tsObject = HMHelper::ref("hasCustomComponent");
    if (tsObject.ref == nullptr) {
        return false;
    }

    napi_value result = HMHelper::callArkTSFunction(tsObject, componentType);
    if (result == nullptr) {
        return false;
    }

    bool hasComponent = false;
    napi_status status = napi_get_value_bool(tsObject.env, result, &hasComponent);
    if (status != napi_ok) {
        return false;
    }
    return hasComponent;
}

napi_value A2UIHybridFactory::addAttributeChangeObserver(napi_env env, napi_callback_info info) {
    // Read (key, callback).
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 2) {
        return nullptr;
    }
    
    std::string key = a2ui::napiGetString(env, args[0]);

    // Create the callback reference.
    napi_ref callbackRef = nullptr;
    napi_status refStatus = napi_create_reference(env, args[1], 1, &callbackRef);
    if (refStatus != napi_ok || callbackRef == nullptr) {
        HM_LOGE("addAttributeChangeObserver: napi_create_reference failed, status=%d", refStatus);
        return nullptr;
    }

    // Store the observer.
    s_attributeObservers[key] = {env, callbackRef};
    
    return nullptr;
}

napi_value A2UIHybridFactory::removeAttributeChangeObserver(napi_env env, napi_callback_info info) {
    // Read the key.
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 1) {
        return nullptr;
    }
    
    std::string key = a2ui::napiGetString(env, args[0]);

    // Remove the observer.
    auto it = s_attributeObservers.find(key);
    if (it != s_attributeObservers.end()) {
        napi_delete_reference(it->second.first, it->second.second);
        s_attributeObservers.erase(it);
    }
    
    return nullptr;
}

napi_value A2UIHybridFactory::setAttribute(napi_env env, napi_callback_info info) {
    // Read (statePtr, key, value).
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 3) {
        return nullptr;
    }
    
    // Get the state pointer
    uint64_t ptrValue = 0;
    bool lossless = true;
    napi_get_value_bigint_uint64(env, args[0], &ptrValue, &lossless);
    ComponentState* state = reinterpret_cast<ComponentState*>(ptrValue);
    
    if (!state) {
        return nullptr;
    }
    
    std::string key = a2ui::napiGetString(env, args[1]);
    std::string value = a2ui::napiGetString(env, args[2]);

    // Update the attribute in the ComponentState.
    nlohmann::json props = state->getProperties();
    props[key] = value;
    state->updateProperties(props);
    
    return nullptr;
}

napi_value A2UIHybridFactory::getAttribute(napi_env env, napi_callback_info info) {
    // Read (statePtr, key).
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 2) {
        return nullptr;
    }
    
    // Get the state pointer
    uint64_t ptrValue = 0;
    bool lossless = true;
    napi_get_value_bigint_uint64(env, args[0], &ptrValue, &lossless);
    ComponentState* state = reinterpret_cast<ComponentState*>(ptrValue);
    
    if (!state) {
        return nullptr;
    }
    
    std::string keyStr = a2ui::napiGetString(env, args[1]);

    // Read the attribute from the ComponentState.
    const nlohmann::json& props = state->getProperties();

    if (!props.contains(keyStr)) {
        return nullptr;
    }
    
    // Convert the attribute back to a napi_value.
    napi_value result;
    const auto& value = props[keyStr];
    
    if (value.is_string()) {
        std::string strValue = value.get<std::string>();
        napi_create_string_utf8(env, strValue.c_str(), strValue.length(), &result);
    } else if (value.is_number()) {
        napi_create_double(env, value.get<double>(), &result);
    } else if (value.is_boolean()) {
        napi_get_boolean(env, value.get<bool>(), &result);
    } else {
        napi_get_null(env, &result);
    }
    
    return result;
}

} // namespace a2ui
