#include "napi_internal.h"
#include "a2ui/render/a2ui_component_state.h"
#include "a2ui/measure/a2ui_platform_layout_bridge.h"
#include "agenui_render_info_types.h"
#include <cinttypes>

static uint64_t g_messageThreadFactoryPtr = 0;

napi_value SetPathConfig(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("SetPathConfig: Expected 1 argument, got %zu", argc);
        return napiBoolean(env, false);
    }

    std::string configJson = napiGetString(env, args[0]);

    HM_LOGI("SetPathConfig: configJson length=%zu", configJson.size());

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("SetPathConfig: Engine not initialized");
        return napiBoolean(env, false);
    }

    bool success = engine->setPathConfig(configJson);
    return napiBoolean(env, success);
}

napi_value GetVersion(napi_env env, napi_callback_info info) {
    napi_value result;
    napi_create_string_utf8(env, agenui::getAGenUIVersion(), NAPI_AUTO_LENGTH, &result);
    return result;
}

napi_value SetDeviceInfo(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 3) {
        HM_LOGE("SetDeviceInfo: Expected 3 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    double width = 0.0;
    double height = 0.0;
    double density = 0.0;
    napi_get_value_double(env, args[0], &width);
    napi_get_value_double(env, args[1], &height);
    napi_get_value_double(env, args[2], &density);

    a2ui::setDeviceInfo(static_cast<int>(width), static_cast<int>(height), static_cast<float>(density));

    HM_LOGI("SetDeviceInfo: width=%d, height=%d, density=%f", static_cast<int>(width), static_cast<int>(height), static_cast<float>(density));

    NAPI_RETURN_UNDEFINED(env);
}

napi_value HybridFactory_getAttribute(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("HybridFactory_getAttribute: Expected 2 arguments, got %zu", argc);
        napi_value result;
        napi_create_string_utf8(env, "", NAPI_AUTO_LENGTH, &result);
        return result;
    }

    uint64_t ptrValue;
    bool lossless;
    napi_get_value_bigint_uint64(env, args[0], &ptrValue, &lossless);
    void* ptr = reinterpret_cast<void*>(ptrValue);

    std::string key = napiGetString(env, args[1]);

    a2ui::ComponentState* componentState = reinterpret_cast<a2ui::ComponentState*>(ptr);
    std::string value;
    if (componentState) {
        value = componentState->getProperty(key);
    }

    HM_LOGI("HybridFactory_getAttribute: ptr=%p, key=%s, value=%s", ptr, key.c_str(), value.c_str());

    napi_value result;
    napi_create_string_utf8(env, value.c_str(), NAPI_AUTO_LENGTH, &result);
    return result;
}

napi_value HybridFactory_getPropertiesJson(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    napi_value result;
    if (argc < 1) {
        napi_create_string_utf8(env, "{}", NAPI_AUTO_LENGTH, &result);
        return result;
    }

    uint64_t ptrValue = 0;
    bool lossless = true;
    napi_get_value_bigint_uint64(env, args[0], &ptrValue, &lossless);
    a2ui::ComponentState* state = reinterpret_cast<a2ui::ComponentState*>(ptrValue);
    std::string propertiesJson = "{}";
    if (state != nullptr) {
        propertiesJson = state->getProperties().dump();
    }

    napi_create_string_utf8(env, propertiesJson.c_str(), propertiesJson.size(), &result);
    return result;
}

napi_value SetMessageThreadFactory(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("SetMessageThreadFactory: Invalid argument count");
        NAPI_RETURN_UNDEFINED(env);
    }

    uint64_t ptrValue = 0;
    bool lossless = true;
    napi_get_value_bigint_uint64(env, args[0], &ptrValue, &lossless);

    g_messageThreadFactoryPtr = ptrValue;
    HM_LOGI("SetMessageThreadFactory: cached ptrValue=%" PRIu64, ptrValue);

    NAPI_RETURN_UNDEFINED(env);
}

napi_value SubmitUIAction(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 4) {
        HM_LOGE("SubmitUIAction: Expected 4 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string surfaceId         = napiGetString(env, args[1]);
    std::string sourceComponentId = napiGetString(env, args[2]);
    std::string contextJson       = napiGetString(env, args[3]);

    HM_LOGI("SubmitUIAction: instanceId=%d, surfaceId=%s, sourceComponentId=%s", instanceId, surfaceId.c_str(), sourceComponentId.c_str());

    auto* sm = findSurfaceManagerByInstanceId(instanceId);
    if (sm) {
        agenui::ActionMessage msg;
        msg.surfaceId = std::move(surfaceId);
        msg.sourceComponentId = std::move(sourceComponentId);
        msg.contextJson = std::move(contextJson);
        sm->submitUIAction(msg);
    }

    NAPI_RETURN_UNDEFINED(env);
}

napi_value SubmitUIDataModel(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 4) {
        HM_LOGE("SubmitUIDataModel: Expected 4 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string surfaceId   = napiGetString(env, args[1]);
    std::string componentId = napiGetString(env, args[2]);
    std::string change      = napiGetString(env, args[3]);

    HM_LOGI("SubmitUIDataModel: instanceId=%d, surfaceId=%s, componentId=%s", instanceId, surfaceId.c_str(), componentId.c_str());

    auto* sm = findSurfaceManagerByInstanceId(instanceId);
    if (sm) {
        agenui::SyncUIToDataMessage msg;
        msg.surfaceId = std::move(surfaceId);
        msg.componentId = std::move(componentId);
        msg.change = std::move(change);
        sm->submitUIDataModel(msg);
    }

    NAPI_RETURN_UNDEFINED(env);
}
