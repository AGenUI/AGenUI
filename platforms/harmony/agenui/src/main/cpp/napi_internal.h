#pragma once

#include "napi/native_api.h"
#include "a2ui/a2ui_message_listener.h"
#include "a2ui/utils/a2ui_napi_utils.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
#include "log/a2ui_capi_log.h"

#include <map>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <string>

namespace agenui { class HarmonyPlatformFunction; }

using a2ui::napiGetString;
using a2ui::napiBoolean;
using a2ui::napiCheckArgIsFunction;

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "AGenUI_NAPI"

#define NAPI_RETURN_UNDEFINED(env) \
    do { napi_value _r; napi_get_undefined(env, &_r); return _r; } while(0)

#define NAPI_GET_ARGS(env, info, count, args) \
    do { size_t _argc = count; napi_get_cb_info(env, info, &_argc, args, nullptr, nullptr); \
         if (_argc < count) { HM_LOGE("%s: Expected %d args, got %zu", __func__, count, _argc); NAPI_RETURN_UNDEFINED(env); } } while(0)

// ---------------------------------------------------------------------------
// Shared state (defined in napi_init.cpp)
// ---------------------------------------------------------------------------
extern napi_threadsafe_function g_mainTsfn;
extern pthread_t g_mainThreadId;

extern std::mutex g_platformFunctionsMutex;
std::map<std::string, std::unique_ptr<agenui::HarmonyPlatformFunction>>& getPlatformFunctions();

extern std::mutex g_messageListenersMutex;
std::map<int, std::shared_ptr<agenui::A2UIMessageListener>>& getMessageListeners();

agenui::A2UIMessageListener* findMessageListenerByInstanceId(int instanceId);
agenui::ISurfaceManager* findSurfaceManagerByInstanceId(int instanceId);

namespace a2ui {
void registerEtsFunction(const std::string& name, napi_env env, napi_value value);
}

// ---------------------------------------------------------------------------
// NAPI function declarations (defined in split modules)
// ---------------------------------------------------------------------------

// napi_surface.cpp
napi_value CreateSurfaceManager(napi_env env, napi_callback_info info);
napi_value DestroySurfaceManager(napi_env env, napi_callback_info info);
napi_value RequestSurface(napi_env env, napi_callback_info info);
napi_value RegisterA2UISurfaceListener(napi_env env, napi_callback_info info);
napi_value UnregisterA2UISurfaceListener(napi_env env, napi_callback_info info);
napi_value ClearA2UiContainer(napi_env env, napi_callback_info info);
napi_value BindSurface(napi_env env, napi_callback_info info);
napi_value UnbindSurface(napi_env env, napi_callback_info info);
napi_value Surface_onSizeChanged(napi_env env, napi_callback_info info);
napi_value DestroySurface(napi_env env, napi_callback_info info);
napi_value FindInstanceIdBySurfaceId(napi_env env, napi_callback_info info);

// napi_measurement.cpp
napi_value ReportComponentRenderSize(napi_env env, napi_callback_info info);
napi_value RegisterMeasurement(napi_env env, napi_callback_info info);
napi_value UnregisterMeasurement(napi_env env, napi_callback_info info);

// napi_image.cpp
napi_value RegisterImageLoader(napi_env env, napi_callback_info info);
napi_value SetImagePixelMap(napi_env env, napi_callback_info info);
napi_value OnImageLoadFailed(napi_env env, napi_callback_info info);

// napi_theme.cpp
napi_value SetThemeConfig(napi_env env, napi_callback_info info);
napi_value SetDesignTokenConfig(napi_env env, napi_callback_info info);
napi_value SetDayNightMode(napi_env env, napi_callback_info info);
napi_value RegisterDefaultTheme(napi_env env, napi_callback_info info);

// napi_function.cpp
napi_value RegisterOpenUrlCallback(napi_env env, napi_callback_info info);
napi_value RegisterSkillInvokerCallback(napi_env env, napi_callback_info info);
napi_value RegisterEtsFunction(napi_env env, napi_callback_info info);
napi_value RegisterFunction(napi_env env, napi_callback_info info);
napi_value UnregisterFunction(napi_env env, napi_callback_info info);
napi_value RegisterComponent(napi_env env, napi_callback_info info);

// napi_text_stream.cpp
napi_value BeginTextStream(napi_env env, napi_callback_info info);
napi_value EndTextStream(napi_env env, napi_callback_info info);
napi_value ReceiveTextChunk(napi_env env, napi_callback_info info);

// napi_misc.cpp
napi_value SetPathConfig(napi_env env, napi_callback_info info);
napi_value GetVersion(napi_env env, napi_callback_info info);
napi_value SetDeviceInfo(napi_env env, napi_callback_info info);
napi_value HybridFactory_getAttribute(napi_env env, napi_callback_info info);
napi_value HybridFactory_getPropertiesJson(napi_env env, napi_callback_info info);
napi_value SetMessageThreadFactory(napi_env env, napi_callback_info info);
napi_value SubmitUIAction(napi_env env, napi_callback_info info);
napi_value SubmitUIDataModel(napi_env env, napi_callback_info info);
