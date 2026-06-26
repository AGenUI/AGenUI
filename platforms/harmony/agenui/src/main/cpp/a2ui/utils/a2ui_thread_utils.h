#pragma once

#include <functional>

#include <napi/native_api.h>

#include "a2ui_napi_utils.h"
#include "log/a2ui_capi_log.h"

namespace a2ui {

inline void postToMainThread(std::function<void()> task) {
    napi_threadsafe_function tsfn = a2ui_get_main_tsfn();
    if (!tsfn) {
        HM_LOGE("[postToMainThread] g_mainTsfn is null, dropping UI task");
        return;
    }
    using Task = std::function<void(napi_env)>;
    auto* wrapper = new Task([t = std::move(task)](napi_env) { t(); });
    napi_status status = napi_call_threadsafe_function(tsfn, wrapper, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        HM_LOGE("[postToMainThread] napi_call_threadsafe_function failed, status=%d",
                 static_cast<int>(status));
        delete wrapper;
    }
}

} // namespace a2ui
