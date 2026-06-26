#include "napi_internal.h"
#include "a2ui/bridge/image_loader_bridge.h"

napi_value RegisterImageLoader(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("RegisterImageLoader: Expected 1 argument, got %zu", argc);
        napi_value result;
        napi_get_undefined(env, &result);
        return result;
    }

    a2ui::ImageLoaderBridge::getInstance().registerLoader(env, args[0]);
    HM_LOGI("RegisterImageLoader: IImageLoader registered successfully");
    NAPI_RETURN_UNDEFINED(env);
}

napi_value SetImagePixelMap(napi_env env, napi_callback_info info) {
    size_t argc = 6;
    napi_value args[6];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 6) {
        HM_LOGE("SetImagePixelMap: Expected 6 arguments, got %zu", argc);
        napi_value result;
        napi_get_undefined(env, &result);
        return result;
    }

    std::string requestId = napiGetString(env, args[0]);

    bool isArrayBuffer = false;
    napi_is_arraybuffer(env, args[1], &isArrayBuffer);
    if (!isArrayBuffer) {
        HM_LOGE("SetImagePixelMap: args[1] is not ArrayBuffer, requestId=%s", requestId.c_str());
        a2ui::ImageLoaderBridge::getInstance().onFailed(requestId, false);
        napi_value ret;
        napi_get_undefined(env, &ret);
        return ret;
    }
    void* rawData = nullptr;
    size_t dataLen = 0;
    napi_get_arraybuffer_info(env, args[1], &rawData, &dataLen);

    double width = 0.0, height = 0.0, pixelFormat = 0.0, alphaType = 0.0;
    napi_get_value_double(env, args[2], &width);
    napi_get_value_double(env, args[3], &height);
    napi_get_value_double(env, args[4], &pixelFormat);
    napi_get_value_double(env, args[5], &alphaType);

    HM_LOGI("SetImagePixelMap: requestId=%s dataLen=%zu w=%d h=%d fmt=%d alpha=%d",
        requestId.c_str(), dataLen,
        static_cast<int>(width), static_cast<int>(height),
        static_cast<int>(pixelFormat), static_cast<int>(alphaType));

    if (rawData == nullptr || dataLen == 0 || width <= 0 || height <= 0) {
        HM_LOGE("SetImagePixelMap: invalid data, requestId=%s", requestId.c_str());
        a2ui::ImageLoaderBridge::getInstance().onFailed(requestId, false);
        napi_value ret;
        napi_get_undefined(env, &ret);
        return ret;
    }

    a2ui::ImageLoaderBridge::getInstance().setImagePixelMapFromBytes({
        requestId,
        static_cast<uint8_t*>(rawData),
        dataLen,
        static_cast<int32_t>(width),
        static_cast<int32_t>(height),
        static_cast<int32_t>(pixelFormat),
        static_cast<int32_t>(alphaType)
    });

    napi_value ret;
    napi_get_undefined(env, &ret);
    return ret;
}

napi_value OnImageLoadFailed(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("OnImageLoadFailed: Expected at least 1 argument, got %zu", argc);
        napi_value result;
        napi_get_undefined(env, &result);
        return result;
    }

    std::string requestId = napiGetString(env, args[0]);

    bool isCancelled = false;
    if (argc >= 2) {
        napi_get_value_bool(env, args[1], &isCancelled);
    }

    HM_LOGI("OnImageLoadFailed: requestId=%s cancelled=%d", requestId.c_str(), isCancelled);

    a2ui::ImageLoaderBridge::getInstance().onFailed(requestId, isCancelled);

    napi_value ret;
    napi_get_undefined(env, &ret);
    return ret;
}
