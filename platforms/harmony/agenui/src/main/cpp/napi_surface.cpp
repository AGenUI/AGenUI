#include "napi_internal.h"
#include "a2ui/render/a2ui_surface.h"
#include "a2ui/measure/a2ui_platform_layout_bridge.h"
#include "a2ui/utils/a2ui_unit_utils.h"
#include "agenui_render_info_types.h"
#include <nlohmann/json.hpp>

napi_value CreateSurfaceManager(napi_env env, napi_callback_info info) {
    HM_LOGI("CreateSurfaceManager called");

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("CreateSurfaceManager: Engine not initialized");
        napi_value result;
        napi_create_int32(env, 0, &result);
        return result;
    }

    auto* sm = engine->createSurfaceManager();
    if (!sm) {
        HM_LOGE("CreateSurfaceManager: Failed to create SurfaceManager");
        napi_value result;
        napi_create_int32(env, 0, &result);
        return result;
    }

    int instanceId = sm->getInstanceId();
    sm->setSurfaceSizeProvider(a2ui::getSharedSurfaceSizeProvider());

    auto listener = std::make_shared<agenui::A2UIMessageListener>(instanceId);
    listener->setTsfn(g_mainTsfn);
    sm->addSurfaceEventListener(listener.get());
    {
        std::lock_guard<std::mutex> lock(g_messageListenersMutex);
        getMessageListeners()[instanceId] = listener;
    }

    HM_LOGI("CreateSurfaceManager: instanceId=%d created successfully", instanceId);

    napi_value result;
    napi_create_int32(env, instanceId, &result);
    return result;
}

napi_value DestroySurfaceManager(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("DestroySurfaceManager: Expected 1 argument");
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    HM_LOGI("DestroySurfaceManager: instanceId=%d", instanceId);

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("DestroySurfaceManager: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }

    std::shared_ptr<agenui::A2UIMessageListener> listenerOwner;
    {
        std::lock_guard<std::mutex> lock(g_messageListenersMutex);
        auto listenerIt = getMessageListeners().find(instanceId);
        if (listenerIt != getMessageListeners().end()) {
            listenerOwner = std::move(listenerIt->second);
            getMessageListeners().erase(listenerIt);
        }
    }

    auto sm = engine->findSurfaceManagerShared(instanceId);
    if (sm) {
        if (listenerOwner) {
            sm->removeSurfaceEventListener(listenerOwner.get());
        }

        engine->destroySurfaceManager(sm.get());
    }

    HM_LOGI("DestroySurfaceManager: instanceId=%d destroyed", instanceId);
    NAPI_RETURN_UNDEFINED(env);
}

napi_value RequestSurface(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("RequestSurface: Expected 2 arguments (instanceId, data)");
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string requestContent = napiGetString(env, args[1]);

    HM_LOGI("RequestSurface: instanceId=%d, dataLen=%zu", instanceId, requestContent.size());

    auto sm = findSurfaceManagerByInstanceId(instanceId);
    if (!sm) {
        HM_LOGE("RequestSurface: SurfaceManager not found for instanceId=%d", instanceId);
        NAPI_RETURN_UNDEFINED(env);
    }

    try {
        sm->receiveTextChunk(requestContent);
        HM_LOGI("RequestSurface: Data transmitted successfully, instanceId=%d", instanceId);
    } catch (const std::exception& e) {
        HM_LOGE("RequestSurface: Exception - %s", e.what());
    }

    NAPI_RETURN_UNDEFINED(env);
}

napi_value RegisterA2UISurfaceListener(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("RegisterA2UISurfaceListener: Expected 2 arguments");
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    napi_valuetype valueType;
    napi_typeof(env, args[1], &valueType);
    if (valueType != napi_object) {
        HM_LOGE("RegisterA2UISurfaceListener: Argument[1] is not an object");
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* listener = findMessageListenerByInstanceId(instanceId);
    if (listener) {
        listener->registerListener(args[1]);
        HM_LOGI("RegisterA2UISurfaceListener: instanceId=%d, listener registered", instanceId);
    }

    NAPI_RETURN_UNDEFINED(env);
}

napi_value UnregisterA2UISurfaceListener(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("UnregisterA2UISurfaceListener: Expected 2 arguments");
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    napi_valuetype valueType;
    napi_typeof(env, args[1], &valueType);
    if (valueType != napi_object) {
        HM_LOGE("UnregisterA2UISurfaceListener: Argument[1] is not an object");
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* listener = findMessageListenerByInstanceId(instanceId);
    if (listener) {
        listener->unregisterListener(args[1]);
        HM_LOGI("UnregisterA2UISurfaceListener: instanceId=%d, listener unregistered", instanceId);
    }

    NAPI_RETURN_UNDEFINED(env);
}

napi_value ClearA2UiContainer(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("ClearA2UiContainer: Expected 1 argument");
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    HM_LOGI("ClearA2UiContainer: instanceId=%d", instanceId);

    auto* listener = findMessageListenerByInstanceId(instanceId);
    if (listener) {
        auto* surfaceManager = listener->getSurfaceManager();
        if (surfaceManager) {
            surfaceManager->unmountAllRootNodes();
        }
    }

    NAPI_RETURN_UNDEFINED(env);
}

napi_value BindSurface(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 3) {
        HM_LOGE("BindSurface: Expected 3 arguments");
        return napiBoolean(env, false);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string surfaceId = napiGetString(env, args[1]);

    napi_value nodeContent = args[2];

    HM_LOGI("BindSurface: instanceId=%d, surfaceId=%s", instanceId, surfaceId.c_str());

    auto* listener = findMessageListenerByInstanceId(instanceId);
    if (!listener) {
        return napiBoolean(env, false);
    }

    auto* surfaceManager = listener->getSurfaceManager();
    if (!surfaceManager) {
        HM_LOGE("BindSurface: SurfaceManager not found for instanceId=%d", instanceId);
        return napiBoolean(env, false);
    }

    bool success = surfaceManager->bindSurface(surfaceId, env, nodeContent);
    return napiBoolean(env, success);
}

napi_value UnbindSurface(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("UnbindSurface: Expected 2 arguments");
        return napiBoolean(env, false);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string surfaceId = napiGetString(env, args[1]);

    HM_LOGI("UnbindSurface: instanceId=%d, surfaceId=%s", instanceId, surfaceId.c_str());

    auto* listener = findMessageListenerByInstanceId(instanceId);
    if (!listener) {
        return napiBoolean(env, false);
    }

    auto* surfaceManager = listener->getSurfaceManager();
    if (!surfaceManager) {
        HM_LOGE("UnbindSurface: SurfaceManager not found for instanceId=%d", instanceId);
        return napiBoolean(env, false);
    }

    bool success = surfaceManager->unbindSurface(surfaceId);
    return napiBoolean(env, success);
}

napi_value Surface_onSizeChanged(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 4) {
        HM_LOGE("OnSurfaceSizeChanged: Expected 4 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string surfaceId = napiGetString(env, args[1]);

    double width = 0.0;
    napi_get_value_double(env, args[2], &width);

    double height = 0.0;
    napi_get_value_double(env, args[3], &height);

    HM_LOGI("OnSurfaceSizeChanged: instanceId=%d, surfaceId=%s, width=%f, height=%f", instanceId, surfaceId.c_str(), width, height);

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("OnSurfaceSizeChanged: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }
    auto sm = engine->findSurfaceManagerShared(instanceId);
    if (!sm) {
        HM_LOGE("OnSurfaceSizeChanged: SurfaceManager not found for instanceId=%d, surfaceId=%s", instanceId, surfaceId.c_str());
        NAPI_RETURN_UNDEFINED(env);
    }

    agenui::SurfaceLayoutInfo surfaceInfo;
    surfaceInfo.surfaceId = surfaceId;
    surfaceInfo.width     = a2ui::UnitConverter::vpToA2ui(width);
    surfaceInfo.height    = a2ui::UnitConverter::vpToA2ui(height);
    sm->onSurfaceSizeChanged(surfaceInfo);

    NAPI_RETURN_UNDEFINED(env);
}

napi_value DestroySurface(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("DestroySurface: Expected 2 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string surfaceId = napiGetString(env, args[1]);

    nlohmann::json requestContent = {
        {"version", "v0.9"},
        {"deleteSurface", {{"surfaceId", surfaceId}}}
    };
    HM_LOGI("DestroySurface: instanceId=%d, surfaceId=%s", instanceId, surfaceId.c_str());

    auto sm = findSurfaceManagerByInstanceId(instanceId);
    if (sm) {
        sm->receiveTextChunk(requestContent.dump());
    }

    NAPI_RETURN_UNDEFINED(env);
}

napi_value Surface_startBlankCheck(napi_env env, napi_callback_info info) {
    size_t argc = 4;
    napi_value args[4];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 4) {
        HM_LOGE("Surface_startBlankCheck: Expected 4 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string surfaceId = napiGetString(env, args[1]);

    int32_t delayMs = 0;
    napi_get_value_int32(env, args[2], &delayMs);

    int32_t minComponentCount = 0;
    napi_get_value_int32(env, args[3], &minComponentCount);

    HM_LOGI("Surface_startBlankCheck: instanceId=%d, surfaceId=%s, delayMs=%d, minCount=%d",
            instanceId, surfaceId.c_str(), delayMs, minComponentCount);

    auto* listener = findMessageListenerByInstanceId(instanceId);
    if (!listener) {
        HM_LOGE("Surface_startBlankCheck: listener not found for instanceId=%d", instanceId);
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* surfaceManager = listener->getSurfaceManager();
    if (!surfaceManager) {
        HM_LOGE("Surface_startBlankCheck: surfaceManager not found for instanceId=%d", instanceId);
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* surface = surfaceManager->getSurface(surfaceId);
    if (!surface) {
        HM_LOGE("Surface_startBlankCheck: surface not found for surfaceId=%s", surfaceId.c_str());
        NAPI_RETURN_UNDEFINED(env);
    }

    surface->startBlankCheck(delayMs, minComponentCount);

    NAPI_RETURN_UNDEFINED(env);
}

napi_value Surface_cancelBlankCheck(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("Surface_cancelBlankCheck: Expected 2 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string surfaceId = napiGetString(env, args[1]);

    HM_LOGI("Surface_cancelBlankCheck: instanceId=%d, surfaceId=%s", instanceId, surfaceId.c_str());

    auto* listener = findMessageListenerByInstanceId(instanceId);
    if (!listener) {
        HM_LOGE("Surface_cancelBlankCheck: listener not found for instanceId=%d", instanceId);
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* surfaceManager = listener->getSurfaceManager();
    if (!surfaceManager) {
        HM_LOGE("Surface_cancelBlankCheck: surfaceManager not found for instanceId=%d", instanceId);
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* surface = surfaceManager->getSurface(surfaceId);
    if (!surface) {
        HM_LOGE("Surface_cancelBlankCheck: surface not found for surfaceId=%s", surfaceId.c_str());
        NAPI_RETURN_UNDEFINED(env);
    }

    surface->cancelBlankCheck();

    NAPI_RETURN_UNDEFINED(env);
}
