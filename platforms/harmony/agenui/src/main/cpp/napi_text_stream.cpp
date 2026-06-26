#include "napi_internal.h"

napi_value BeginTextStream(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("BeginTextStream: Expected 1 argument, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    HM_LOGI("BeginTextStream: instanceId=%d", instanceId);

    auto* sm = findSurfaceManagerByInstanceId(instanceId);
    if (!sm) {
        HM_LOGE("BeginTextStream: SurfaceManager not found for instanceId=%d", instanceId);
        NAPI_RETURN_UNDEFINED(env);
    }

    sm->beginTextStream();
    HM_LOGI("BeginTextStream: success");

    NAPI_RETURN_UNDEFINED(env);
}

napi_value EndTextStream(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("EndTextStream: Expected 1 argument, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    HM_LOGI("EndTextStream: instanceId=%d", instanceId);

    auto* sm = findSurfaceManagerByInstanceId(instanceId);
    if (!sm) {
        HM_LOGE("EndTextStream: SurfaceManager not found for instanceId=%d", instanceId);
        NAPI_RETURN_UNDEFINED(env);
    }

    sm->endTextStream();
    HM_LOGI("EndTextStream: success");

    NAPI_RETURN_UNDEFINED(env);
}

napi_value ReceiveTextChunk(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("ReceiveTextChunk: Expected 2 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string data = napiGetString(env, args[1]);

    HM_LOGI("ReceiveTextChunk: instanceId=%d, data length=%zu", instanceId, data.size());

    auto* sm = findSurfaceManagerByInstanceId(instanceId);
    if (sm) {
        sm->receiveTextChunk(data);
    }

    NAPI_RETURN_UNDEFINED(env);
}
