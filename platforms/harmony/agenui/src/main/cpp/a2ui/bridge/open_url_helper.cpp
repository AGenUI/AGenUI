#include "open_url_helper.h"
#include "log/a2ui_capi_log.h"

namespace a2ui {

OpenUrlHelper& OpenUrlHelper::getInstance() {
    static OpenUrlHelper instance;
    return instance;
}

OpenUrlHelper::~OpenUrlHelper() {
    clear();
}

void OpenUrlHelper::registerCallback(napi_env env, napi_value callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Clear the previous callback reference
    if (callback_ref_ != nullptr && env_ != nullptr) {
        napi_delete_reference(env_, callback_ref_);
        callback_ref_ = nullptr;
    }

    // Create a new callback reference
    napi_status status = napi_create_reference(env, callback, 1, &callback_ref_);
    if (status != napi_ok) {
        HM_LOGE("OpenUrlHelper: Failed to create callback reference");
        return;
    }

    env_ = env;
    HM_LOGI("OpenUrlHelper: Callback registered successfully");
}

void OpenUrlHelper::openUrl(const std::string& url) {
    napi_env env;
    napi_ref callbackRef;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (env_ == nullptr || callback_ref_ == nullptr) {
            HM_LOGE("OpenUrlHelper: No callback registered, cannot open URL: %s", url.c_str());
            return;
        }
        env = env_;
        callbackRef = callback_ref_;
    }

    napi_value callback;
    napi_status status = napi_get_reference_value(env, callbackRef, &callback);
    if (status != napi_ok) {
        HM_LOGE("OpenUrlHelper: Failed to get callback reference value");
        return;
    }

    napi_value urlArg;
    status = napi_create_string_utf8(env, url.c_str(), NAPI_AUTO_LENGTH, &urlArg);
    if (status != napi_ok) {
        HM_LOGE("OpenUrlHelper: Failed to create URL string");
        return;
    }

    napi_value undefined;
    napi_get_undefined(env, &undefined);

    napi_value result;
    napi_value args[] = {urlArg};
    status = napi_call_function(env, undefined, callback, 1, args, &result);
    if (status != napi_ok) {
        HM_LOGE("OpenUrlHelper: Failed to call callback for URL: %s", url.c_str());
        return;
    }

    HM_LOGI("OpenUrlHelper: Successfully called callback for URL: %s", url.c_str());
}

void OpenUrlHelper::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (callback_ref_ != nullptr && env_ != nullptr) {
        napi_delete_reference(env_, callback_ref_);
        callback_ref_ = nullptr;
    }
    env_ = nullptr;

    HM_LOGI("OpenUrlHelper: Cleared");
}

} // namespace a2ui
