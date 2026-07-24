#include "napi_internal.h"
#include "a2ui/font/a2ui_font_registry.h"
#include "a2ui/bridge/harmony_platform_function.h"
#include "a2ui/render/a2ui_component_types.h"
#include "a2ui/render/a2ui_surface.h"
#include "a2ui/render/a2ui_component.h"
#include "a2ui/measure/a2ui_platform_layout_bridge.h"
#include "agenui_measurement.h"
#include "a2ui/measure/image_component_measurement.h"
#include "a2ui/measure/slider_component_measurement.h"
#include "a2ui/measure/text_component_measurement.h"
#include "a2ui/measure/checkbox_component_measurement.h"
#include "a2ui/measure/choice_picker_component_measurement.h"
#include "a2ui/measure/table_component_measurement.h"
#include "a2ui/measure/tabs_component_measurement.h"
#include "a2ui/measure/datetimeinput_component_measurement.h"
#include "a2ui/measure/divider_component_measurement.h"
#include "a2ui/measure/audioplayer_component_measurement.h"
#include "a2ui_api.h"
#include "agenui_logger_interface.h"
#include "agenui_logger_internal.h"

#include <atomic>
#include <cstdio>
#include <cstdarg>
#include <pthread.h>
#include <vector>
#include <native_drawing/drawing_register_font.h>
#include <native_drawing/drawing_font_collection.h>

// ---------------------------------------------------------------------------
// Global thread-safe function for dispatching worker-thread callbacks onto
// the main thread.  Declared before namespace a2ui so RuntimeLoggerImpl can
// access it.
// ---------------------------------------------------------------------------
napi_threadsafe_function g_mainTsfn = nullptr;

namespace a2ui {

static std::map<std::string, EtsFunction>& getEtsFunctionsTable() {
    static auto* p = new std::map<std::string, EtsFunction>();
    return *p;
}
static std::mutex g_ets_functions_mutex;

class HarmonyNAPI : public IHarmonyNAPI {
public:
	ArkTSObject ref(const std::string& name) override {
        std::lock_guard<std::mutex> lock(g_ets_functions_mutex);
        auto& table = getEtsFunctionsTable();
        auto i = table.find(name);
        if (i != table.end()) {
            return ArkTSObject { i->second.env, i->second.ref };
        }
        return ArkTSObject { nullptr, nullptr };
    }
};

static HarmonyNAPI g_harmony_napi;

IHarmonyNAPI* implHarmonyNAPI() {
    return &g_harmony_napi;
}

// MARK: - C++ Wrapper Class for HarmonyOS
class RuntimeLoggerImpl : public agenui::IRuntimeLogger {
public:
    RuntimeLoggerImpl(napi_env env, napi_ref arktsLoggerRef)
        : mEnv(env), mArktsLoggerRef(arktsLoggerRef), mMainThreadId(pthread_self()),mMinLevel(agenui::LOG_LEVEL_DEBUG) {}

    ~RuntimeLoggerImpl() {
        if (mEnv && mArktsLoggerRef) {
            napi_delete_reference(mEnv, mArktsLoggerRef);
            mArktsLoggerRef = nullptr;
        }
    }

    agenui::LogLevel getMinLevel() const override {
        return mMinLevel;
    }

    void setMinLevel(agenui::LogLevel level) {
        mMinLevel = level;
    }

    void log(agenui::LogLevel level, const char* tag, const char* func, int line, const char* format, ...) override {
        va_list args;
        va_start(args, format);
        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        if (pthread_self() != mMainThreadId) {
            LogLevel hilogLevel;
            switch (level) {
                case agenui::LOG_LEVEL_DEBUG:   hilogLevel = LOG_DEBUG; break;
                case agenui::LOG_LEVEL_INFO:    hilogLevel = LOG_INFO; break;
                case agenui::LOG_LEVEL_WARN:    hilogLevel = LOG_WARN; break;
                case agenui::LOG_LEVEL_ERROR:   hilogLevel = LOG_ERROR; break;
                case agenui::LOG_LEVEL_FATAL:   hilogLevel = LOG_FATAL; break;
                default:                        hilogLevel = LOG_INFO; break;
            }
            OH_LOG_Print(LOG_APP, hilogLevel, LOG_DOMAIN, tag ? tag : "AGenUI", "[%{public}s:%{public}d] %{public}s", func ? func : "", line, buffer);

            if (g_mainTsfn && mArktsLoggerRef) {
                int32_t capturedLevel = static_cast<int32_t>(level);
                std::string capturedTag = tag ? tag : "";
                std::string capturedFunc = func ? func : "";
                int capturedLine = line;
                std::string capturedMessage = buffer;
                napi_ref loggerRef = mArktsLoggerRef;

                auto* task = new agenui::MainThreadTask(
                    [loggerRef, capturedLevel, capturedTag = std::move(capturedTag),
                     capturedFunc = std::move(capturedFunc), capturedLine,
                     capturedMessage = std::move(capturedMessage)](napi_env env) {
                        napi_value loggerValue;
                        napi_status st = napi_get_reference_value(env, loggerRef, &loggerValue);
                        if (st != napi_ok || loggerValue == nullptr) return;

                        napi_value method;
                        st = napi_get_named_property(env, loggerValue, "onLogFromNative", &method);
                        if (st != napi_ok) return;

                        napi_value argv[5];
                        napi_create_int32(env, capturedLevel, &argv[0]);
                        napi_create_string_utf8(env, capturedTag.c_str(), NAPI_AUTO_LENGTH, &argv[1]);
                        napi_create_string_utf8(env, capturedFunc.c_str(), NAPI_AUTO_LENGTH, &argv[2]);
                        napi_create_int32(env, capturedLine, &argv[3]);
                        napi_create_string_utf8(env, capturedMessage.c_str(), NAPI_AUTO_LENGTH, &argv[4]);

                        napi_value result;
                        napi_call_function(env, loggerValue, method, 5, argv, &result);
                    });
                napi_status tsfnStatus = napi_call_threadsafe_function(g_mainTsfn, task, napi_tsfn_nonblocking);
                if (tsfnStatus != napi_ok) {
                    delete task;
                }
            }
            return;
        }

        if (!mEnv || !mArktsLoggerRef) {
            return;
        }

        napi_value loggerValue;
        napi_status status = napi_get_reference_value(mEnv, mArktsLoggerRef, &loggerValue);
        if (status != napi_ok || loggerValue == nullptr) {
            return;
        }

        napi_value onLogFromNativeMethod;
        status = napi_get_named_property(mEnv, loggerValue, "onLogFromNative", &onLogFromNativeMethod);
        if (status != napi_ok) {
            return;
        }

        napi_value args_array[5];
        napi_create_int32(mEnv, static_cast<int32_t>(level), &args_array[0]);
        napi_create_string_utf8(mEnv, tag ? tag : "", NAPI_AUTO_LENGTH, &args_array[1]);
        napi_create_string_utf8(mEnv, func ? func : "", NAPI_AUTO_LENGTH, &args_array[2]);
        napi_create_int32(mEnv, line, &args_array[3]);
        napi_create_string_utf8(mEnv, buffer, NAPI_AUTO_LENGTH, &args_array[4]);

        napi_value result;
        napi_call_function(mEnv, loggerValue, onLogFromNativeMethod, 5, args_array, &result);
    }

private:
    napi_env mEnv;
    napi_ref mArktsLoggerRef;
    pthread_t mMainThreadId;
    std::atomic<agenui::LogLevel> mMinLevel;
};

static std::unique_ptr<RuntimeLoggerImpl> gRuntimeLoggerImpl;

void registerEtsFunction(const std::string& name, napi_env env, napi_value value) {
    std::lock_guard<std::mutex> lock(g_ets_functions_mutex);
    auto& table = getEtsFunctionsTable();
    auto existing = table.find(name);
    if (existing != table.end()) {
        napi_delete_reference(existing->second.env, existing->second.ref);
        table.erase(existing);
    }

    napi_ref ref;
    napi_create_reference(env, value, 1, &ref);
    table[name] = { name, env, ref };
}

}

a2ui::IHarmonyNAPI* implHarmonyNAPI() {
    return a2ui::implHarmonyNAPI();
}

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static napi_env g_napiEnv = nullptr;
pthread_t g_mainThreadId = 0;
napi_env a2ui_get_napi_env() { return g_napiEnv; }

napi_threadsafe_function a2ui_get_main_tsfn() { return g_mainTsfn; }

static std::string& getFilesDir() {
    static auto* p = new std::string();
    return *p;
}
const std::string& a2ui_get_files_dir() { return getFilesDir(); }

// Platform Function Management
std::mutex g_platformFunctionsMutex;
std::map<std::string, std::unique_ptr<agenui::HarmonyPlatformFunction>>& getPlatformFunctions() {
    static auto* p = new std::map<std::string, std::unique_ptr<agenui::HarmonyPlatformFunction>>();
    return *p;
}

// Multi-instance Management
std::mutex g_messageListenersMutex;
std::map<int, std::shared_ptr<agenui::A2UIMessageListener>>& getMessageListeners() {
    static auto* p = new std::map<int, std::shared_ptr<agenui::A2UIMessageListener>>();
    return *p;
}

agenui::A2UIMessageListener* findMessageListenerByInstanceId(int instanceId) {
    std::lock_guard<std::mutex> lock(g_messageListenersMutex);
    auto it = getMessageListeners().find(instanceId);
    if (it != getMessageListeners().end()) {
        return it->second.get();
    }
    HM_LOGE("A2UIMessageListener not found for instanceId=%d", instanceId);
    return nullptr;
}

agenui::ISurfaceManager* findSurfaceManagerByInstanceId(int instanceId) {
    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("AGenUI Engine not initialized");
        return nullptr;
    }
    auto* sm = engine->findSurfaceManager(instanceId);
    if (!sm) {
        HM_LOGE("ISurfaceManager not found for instanceId=%d", instanceId);
    }
    return sm;
}

// ---------------------------------------------------------------------------
// Engine lifecycle (Start / Stop / SetMinLogLevel)
// ---------------------------------------------------------------------------

static napi_value Start(napi_env env, napi_callback_info info) {
    HM_LOGI("AGenUI Start called - initializing IAGenUIEngine");
    agenui::IAGenUIEngine* engine = agenui::initAGenUIEngine();
    if (engine == nullptr) {
        HM_LOGE("Failed to initialize AGenUI Engine");
        NAPI_RETURN_UNDEFINED(env);
    }

    if (!a2ui::gRuntimeLoggerImpl) {
        size_t argc = 1;
        napi_value argv[1];
        napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

        if (argc > 0 && argv[0] != nullptr) {
            napi_ref loggerRef;
            napi_create_reference(env, argv[0], 1, &loggerRef);

            a2ui::gRuntimeLoggerImpl = std::make_unique<a2ui::RuntimeLoggerImpl>(env, loggerRef);
            engine->setRuntimeLogger(a2ui::gRuntimeLoggerImpl.get());
            HM_LOGI("AGenUI RuntimeLogger initialized successfully");
        } else {
            HM_LOGW("AGenUI Start: No logger provided, using default logger");
        }
    }

    auto* mm = engine->getMeasurementManager();
    if (mm) {
        mm->registerMeasurement("Image",        std::make_shared<a2ui::ImageComponentMeasurement>());
        mm->registerMeasurement("Icon",         std::make_shared<a2ui::ImageComponentMeasurement>());
        mm->registerMeasurement("Slider",       std::make_shared<a2ui::SliderComponentMeasurement>());
        mm->registerMeasurement("Text",         std::make_shared<a2ui::TextComponentMeasurement>());
        mm->registerMeasurement("RichText",     std::make_shared<a2ui::TextComponentMeasurement>());
        mm->registerMeasurement("CheckBox",     std::make_shared<a2ui::CheckBoxComponentMeasurement>());
        mm->registerMeasurement("ChoicePicker", std::make_shared<a2ui::ChoicePickerComponentMeasurement>());
        mm->registerMeasurement("Table",        std::make_shared<a2ui::TableComponentMeasurement>());
        mm->registerMeasurement("Tabs",          std::make_shared<a2ui::TabsComponentMeasurement>());
        mm->registerMeasurement("DateTimeInput", std::make_shared<a2ui::DateTimeInputComponentMeasurement>());
        mm->registerMeasurement("Divider",       std::make_shared<a2ui::DividerComponentMeasurement>());
        mm->registerMeasurement("AudioPlayer",   std::make_shared<a2ui::AudioPlayerComponentMeasurement>());
    }

    HM_LOGI("AGenUI Engine initialized successfully");
    NAPI_RETURN_UNDEFINED(env);
}

static napi_value Stop(napi_env env, napi_callback_info info) {
    HM_LOGI("AGenUI Stop called - destroying IAGenUIEngine");

    auto* engine = agenui::getAGenUIEngine();
    if (engine) {
        if (a2ui::gRuntimeLoggerImpl) {
            a2ui::gRuntimeLoggerImpl.reset();
            HM_LOGI("Cleaned up RuntimeLogger");
        }

        {
            std::lock_guard<std::mutex> lock(g_platformFunctionsMutex);
            for (auto it = getPlatformFunctions().begin(); it != getPlatformFunctions().end(); ) {
                engine->unregisterFunction(it->first);
                it = getPlatformFunctions().erase(it);
            }
            HM_LOGI("Cleaned up all PlatformFunctions");
        }

        {
            std::lock_guard<std::mutex> lock(g_messageListenersMutex);
            for (auto it = getMessageListeners().begin(); it != getMessageListeners().end(); ) {
                int instanceId = it->first;
                auto* sm = engine->findSurfaceManager(instanceId);
                if (sm) {
                    sm->removeSurfaceEventListener(it->second.get());
                    engine->destroySurfaceManager(sm);
                }
                it = getMessageListeners().erase(it);
                HM_LOGI("Cleaned up listener for instanceId=%d", instanceId);
            }
        }
    }

    agenui::destroyAGenUIEngine();
    HM_LOGI("AGenUI Engine destroyed");

    if (g_mainTsfn) {
        napi_unref_threadsafe_function(env, g_mainTsfn);
        napi_release_threadsafe_function(g_mainTsfn, napi_tsfn_release);
        g_mainTsfn = nullptr;
        HM_LOGI("g_mainTsfn released");
    }

    NAPI_RETURN_UNDEFINED(env);
}

static napi_value SetMinLogLevel(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value argv[1];
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc < 1) {
        NAPI_RETURN_UNDEFINED(env);
    }
    int32_t level = 0;
    napi_get_value_int32(env, argv[0], &level);
    if (level < agenui::LOG_LEVEL_DEBUG || level > agenui::LOG_LEVEL_PERFORMANCE) {
        level = agenui::LOG_LEVEL_DEBUG;
    }
    auto lv = static_cast<agenui::LogLevel>(level);
    if (a2ui::gRuntimeLoggerImpl) {
        a2ui::gRuntimeLoggerImpl->setMinLevel(lv);
    }
    agenui::setDefaultLogMinLevel(lv);
    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Re-evaluate host-backed function call values.
 * @param args[0] instanceId (number)
 */
static napi_value InvalidateFunctionCallValues(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("InvalidateFunctionCallValues: Expected 1 argument, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    auto* sm = findSurfaceManagerByInstanceId(instanceId);
    if (!sm) {
        HM_LOGE("InvalidateFunctionCallValues: SurfaceManager not found for instanceId=%d", instanceId);
        NAPI_RETURN_UNDEFINED(env);
    }

    sm->invalidateFunctionCallValues();
    HM_LOGI("InvalidateFunctionCallValues: instanceId=%d success", instanceId);
    NAPI_RETURN_UNDEFINED(env);
}

// ---------------------------------------------------------------------------
// Font registration
// ---------------------------------------------------------------------------

// Register font data into the OH_Drawing global FontCollection so that CAPI
// nodes (which look up OH_Drawing_GetFontCollectionGlobalInstance) can find
// the custom font. The global instance is managed by the system and must NOT
// be destroyed. Also registers in a2ui::FontRegistry for CSS font-family
// resolution.
static bool RegisterFontData(const std::string& familyName, uint8_t* data, size_t dataLen) {
    OH_Drawing_FontCollection* fontCollection = OH_Drawing_GetFontCollectionGlobalInstance();
    if (!fontCollection) {
        HM_LOGE("RegisterFontData: failed to get global font collection");
        return false;
    }

    uint32_t drawingErr = OH_Drawing_RegisterFontBuffer(
            fontCollection, familyName.c_str(), data, dataLen);
    if (drawingErr != 0) {
        HM_LOGE("RegisterFontData: OH_Drawing_RegisterFontBuffer failed (err=%u) for '%s' (%zu bytes)",
                drawingErr, familyName.c_str(), dataLen);
        return false;
    }

    a2ui::FontRegistry::instance().registerFont(familyName, familyName);
    HM_LOGI("RegisterFontData: OK '%s' (%zu bytes)", familyName.c_str(), dataLen);
    return true;
}

static napi_value RegisterFontNative(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value argv[2];
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("RegisterFont: expected 2 arguments (familyName, filePath)");
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }

    char familyBuf[256] = {};
    size_t familyLen = 0;
    napi_get_value_string_utf8(env, argv[0], familyBuf, sizeof(familyBuf), &familyLen);

    char pathBuf[1024] = {};
    size_t pathLen = 0;
    napi_get_value_string_utf8(env, argv[1], pathBuf, sizeof(pathBuf), &pathLen);

    std::string familyName(familyBuf, familyLen);
    std::string filePath(pathBuf, pathLen);

    if (familyName.empty() || filePath.empty()) {
        HM_LOGE("RegisterFont: familyName or filePath is empty");
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }

    OH_Drawing_FontCollection* fontCollection = OH_Drawing_GetFontCollectionGlobalInstance();
    if (!fontCollection) {
        HM_LOGE("RegisterFont: failed to get global font collection");
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }

    uint32_t drawingErr = OH_Drawing_RegisterFont(fontCollection, familyName.c_str(), filePath.c_str());
    if (drawingErr != 0) {
        HM_LOGE("RegisterFont: OH_Drawing_RegisterFont failed (err=%u) for '%s' path='%s'",
                drawingErr, familyName.c_str(), filePath.c_str());
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }

    a2ui::FontRegistry::instance().registerFont(familyName, familyName);
    HM_LOGI("RegisterFont: OK '%s' path='%s'", familyName.c_str(), filePath.c_str());
    napi_value result;
    napi_get_boolean(env, true, &result);
    return result;
}

static napi_value RegisterFontFromBufferNative(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value argv[2];
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("RegisterFontFromBuffer: expected 2 arguments (familyName, arrayBuffer)");
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }

    char familyBuf[256] = {};
    size_t familyLen = 0;
    napi_get_value_string_utf8(env, argv[0], familyBuf, sizeof(familyBuf), &familyLen);
    std::string familyName(familyBuf, familyLen);

    if (familyName.empty()) {
        HM_LOGE("RegisterFontFromBuffer: familyName is empty");
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }

    bool isArrayBuffer = false;
    napi_is_arraybuffer(env, argv[1], &isArrayBuffer);
    if (!isArrayBuffer) {
        HM_LOGE("RegisterFontFromBuffer: second argument is not an ArrayBuffer");
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }

    void* data = nullptr;
    size_t dataLen = 0;
    napi_get_arraybuffer_info(env, argv[1], &data, &dataLen);

    if (!data || dataLen == 0) {
        HM_LOGE("RegisterFontFromBuffer: buffer is empty");
        napi_value result;
        napi_get_boolean(env, false, &result);
        return result;
    }

    bool ok = RegisterFontData(familyName, static_cast<uint8_t*>(data), dataLen);
    napi_value result;
    napi_get_boolean(env, ok, &result);
    return result;
}

// ---------------------------------------------------------------------------
// Module init and registration
// ---------------------------------------------------------------------------

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    pthread_t currentThreadId = pthread_self();

    if (!pthread_equal(currentThreadId, g_mainThreadId)) {
        HM_LOGW("Init: called from non-main thread! mainThreadId=%lu, currentThreadId=%lu, env=%p - skipping",
                (unsigned long)g_mainThreadId, (unsigned long)currentThreadId, env);
        return exports;
    }

    HM_LOGI("Init: main thread init, threadId=%lu, env=%p", (unsigned long)currentThreadId, env);

    g_napiEnv = env;

    if (!g_mainTsfn) {
        napi_value asyncResourceName;
        napi_create_string_utf8(env, "A2UIMainThreadDispatcher", NAPI_AUTO_LENGTH, &asyncResourceName);
        napi_status status = napi_create_threadsafe_function(
            env,
            nullptr,
            nullptr,
            asyncResourceName,
            0,
            1,
            nullptr,
            nullptr,
            nullptr,
            [](napi_env env, napi_value /*js_func*/, void* /*context*/, void* data) {
                if (!data) return;
                auto* task = static_cast<agenui::MainThreadTask*>(data);
                (*task)(env);
                delete task;
            },
            &g_mainTsfn
        );
        if (status != napi_ok) {
            HM_LOGE("Init: failed to create g_mainTsfn, status=%d", status);
            g_mainTsfn = nullptr;
        } else {
            napi_ref_threadsafe_function(env, g_mainTsfn);
            HM_LOGI("Init: g_mainTsfn created and ref'd successfully");
        }
    }

    napi_property_descriptor desc[] = {
        { "start", nullptr, Start, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "stop", nullptr, Stop, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setMinLogLevel", nullptr, SetMinLogLevel, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "createSurfaceManager", nullptr, CreateSurfaceManager, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "destroySurfaceManager", nullptr, DestroySurfaceManager, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setPathConfig", nullptr, SetPathConfig, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getVersion", nullptr, GetVersion, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "requestSurface", nullptr, RequestSurface, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerA2UISurfaceListener", nullptr, RegisterA2UISurfaceListener, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "unregisterA2UISurfaceListener", nullptr, UnregisterA2UISurfaceListener, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "bindSurface", nullptr, BindSurface, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "unbindSurface", nullptr, UnbindSurface, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "clearA2UiContainer", nullptr, ClearA2UiContainer, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerOpenUrlCallback", nullptr, RegisterOpenUrlCallback, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerSkillInvokerCallback", nullptr, RegisterSkillInvokerCallback, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerEtsFunction", nullptr, RegisterEtsFunction, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setDeviceInfo", nullptr, SetDeviceInfo, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "hybridFactoryGetAttribute", nullptr, HybridFactory_getAttribute, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "hybridFactoryGetPropertiesJson", nullptr, HybridFactory_getPropertiesJson, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "reportComponentRenderSize", nullptr, ReportComponentRenderSize, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerMeasurement", nullptr, RegisterMeasurement, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "unregisterMeasurement", nullptr, UnregisterMeasurement, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "onSurfaceSizeChanged", nullptr, Surface_onSizeChanged, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setThemeConfig", nullptr, SetThemeConfig, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setDesignTokenConfig", nullptr, SetDesignTokenConfig, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setMessageThreadFactory", nullptr, SetMessageThreadFactory, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerFunction", nullptr, RegisterFunction, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "unregisterFunction", nullptr, UnregisterFunction, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "submitUIAction", nullptr, SubmitUIAction, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "submitUIDataModel", nullptr, SubmitUIDataModel, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "destroySurface", nullptr, DestroySurface, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "receiveTextChunk", nullptr, ReceiveTextChunk, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerDefaultTheme", nullptr, RegisterDefaultTheme, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setDayNightMode", nullptr, SetDayNightMode, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "beginTextStream", nullptr, BeginTextStream, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "endTextStream", nullptr, EndTextStream, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "invalidateFunctionCallValues", nullptr, InvalidateFunctionCallValues, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerComponent", nullptr, RegisterComponent, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerImageLoader", nullptr, RegisterImageLoader, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setImagePixelMap", nullptr, SetImagePixelMap, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setImagePixelMapNative", nullptr, SetImagePixelMapNative, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "onImageLoadFailed", nullptr, OnImageLoadFailed, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "surfaceStartBlankCheck", nullptr, Surface_startBlankCheck, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "surfaceCancelBlankCheck", nullptr, Surface_cancelBlankCheck, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerFont", nullptr, RegisterFontNative, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerFontFromBuffer", nullptr, RegisterFontFromBufferNative, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "a2ui-capi",
    .nm_priv = nullptr,
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    g_mainThreadId = pthread_self();
    napi_module_register(&demoModule);
}
