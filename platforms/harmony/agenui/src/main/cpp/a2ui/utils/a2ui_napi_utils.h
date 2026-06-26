#pragma once

#include <napi/native_api.h>
#include <string>
#include <vector>

#include "log/a2ui_capi_log.h"

napi_env a2ui_get_napi_env();
napi_threadsafe_function a2ui_get_main_tsfn();
const std::string& a2ui_get_files_dir();

namespace a2ui {

inline std::string napiGetString(napi_env env, napi_value value) {
    size_t strSize = 0;
    napi_get_value_string_utf8(env, value, nullptr, 0, &strSize);
    std::vector<char> buf(strSize + 1, '\0');
    napi_get_value_string_utf8(env, value, buf.data(), buf.size(), &strSize);
    return std::string(buf.data(), strSize);
}

inline napi_value napiBoolean(napi_env env, bool value) {
    napi_value result;
    napi_get_boolean(env, value, &result);
    return result;
}

inline bool napiCheckArgIsFunction(napi_env env, napi_value arg, const char* callerName) {
    napi_valuetype valueType;
    napi_typeof(env, arg, &valueType);
    if (valueType != napi_function) {
        HM_LOGE("%s: Argument is not a function", callerName);
        return false;
    }
    return true;
}

} // namespace a2ui
