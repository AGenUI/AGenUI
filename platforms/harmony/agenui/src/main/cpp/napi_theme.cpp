#include "napi_internal.h"

napi_value SetThemeConfig(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("SetThemeConfig: Expected at least 1 argument, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    std::string config = napiGetString(env, args[0]);

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("SetThemeConfig: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }

    std::string errorResult;
    bool success = engine->loadThemeConfig(config, errorResult);
    if (!success) {
        HM_LOGE("SetThemeConfig: failed, error=%s", errorResult.c_str());
    } else {
        HM_LOGI("SetThemeConfig: success");
    }

    NAPI_RETURN_UNDEFINED(env);
}

napi_value SetDesignTokenConfig(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("SetDesignTokenConfig: Expected at least 1 argument, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    std::string config = napiGetString(env, args[0]);

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("SetDesignTokenConfig: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }

    std::string errorResult;
    bool success = engine->loadDesignTokenConfig(config, errorResult);
    if (!success) {
        HM_LOGE("SetDesignTokenConfig: failed, error=%s", errorResult.c_str());
    } else {
        HM_LOGI("SetDesignTokenConfig: success");
    }

    NAPI_RETURN_UNDEFINED(env);
}

napi_value SetDayNightMode(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("SetDayNightMode: Expected 1 argument, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    std::string mode = napiGetString(env, args[0]);

    if (mode != "light" && mode != "dark") {
        HM_LOGE("SetDayNightMode: invalid mode '%s', expected 'light' or 'dark'", mode.c_str());
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* engine = agenui::getAGenUIEngine();
    if (engine) {
        engine->setDayNightMode(mode);
        HM_LOGI("SetDayNightMode: success, mode=%s", mode.c_str());
    }

    NAPI_RETURN_UNDEFINED(env);
}

napi_value RegisterDefaultTheme(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("RegisterDefaultTheme: Expected 2 arguments, got %zu", argc);
        return napiBoolean(env, false);
    }

    std::string theme = napiGetString(env, args[0]);
    std::string designToken = napiGetString(env, args[1]);

    HM_LOGI("RegisterDefaultTheme: theme length=%zu, designToken length=%zu", theme.size(), designToken.size());

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("RegisterDefaultTheme: Engine not initialized");
        return napiBoolean(env, false);
    }

    std::string resultStr;
    bool themeResult = engine->loadThemeConfig(theme, resultStr);
    if (!themeResult) {
        HM_LOGE("RegisterDefaultTheme: Theme registration failed: %s", resultStr.c_str());
        return napiBoolean(env, false);
    }

    bool tokenResult = engine->loadDesignTokenConfig(designToken, resultStr);
    if (!tokenResult) {
        HM_LOGE("RegisterDefaultTheme: DesignToken registration failed: %s", resultStr.c_str());
        return napiBoolean(env, false);
    }

    HM_LOGI("RegisterDefaultTheme: success");
    return napiBoolean(env, true);
}
