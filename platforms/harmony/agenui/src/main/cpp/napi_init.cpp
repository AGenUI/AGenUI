#include "a2ui/render/a2ui_component_types.h"
#include "napi/native_api.h"
#include "a2ui/a2ui_message_listener.h"
#include "a2ui/bridge/open_url_helper.h"
#include "a2ui/bridge/skill_invoker_helper.h"
#include "a2ui/bridge/image_loader_bridge.h"
#include "a2ui/bridge/harmony_platform_function.h"
#include "a2ui/render/a2ui_surface.h"
#include "a2ui/render/a2ui_component.h"
#include "a2ui/measure/a2ui_platform_layout_bridge.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
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
#include "a2ui/render/a2ui_component_state.h"
#include "agenui_render_info_types.h"
#include <nlohmann/json.hpp>
#include "a2ui/utils/a2ui_unit_utils.h"
#include "log/a2ui_capi_log.h"
#include "a2ui_api.h"
#include "agenui_logger_interface.h"
#include "agenui_logger_internal.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "AGenUI_NAPI"

#include <pthread.h>
#include <map>
#include <mutex>
#include <condition_variable>
#include <cstdarg>
#include <string>

// Thread-safe function for dispatching worker-thread callbacks onto the main thread.
// Declared here (before namespace a2ui) so RuntimeLoggerImpl::log() can access it.
static napi_threadsafe_function g_mainTsfn = nullptr;

namespace a2ui {

static std::map<std::string, EtsFunction> g_ets_functions_table;
static std::mutex g_ets_functions_mutex;

class HarmonyNAPI : public IHarmonyNAPI {
public:
	virtual ArkTSObject ref(const std::string& name) override {
        std::lock_guard<std::mutex> lock(g_ets_functions_mutex);
        auto i = g_ets_functions_table.find(name);
        if (i != g_ets_functions_table.end()) {
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

    // Pushed in from ArkTS via the SetMinLogLevel NAPI function. Mirroring the value
    // in C++ avoids a JS round-trip on every log emission (which would not be safe
    // from worker threads anyway).
    void setMinLevel(agenui::LogLevel level) {
        mMinLevel = level;
    }
    
    void log(agenui::LogLevel level, const char* tag, const char* func, int line, const char* format, ...) override {
        // Format the message using variadic arguments
        va_list args;
        va_start(args, format);
        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        // NAPI calls can only be made from the main thread
        if (pthread_self() != mMainThreadId) {
            // Always output to hilog for immediate visibility
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

            // Also dispatch to the ArkTS custom logger via TSFN
            if (g_mainTsfn && mArktsLoggerRef) {
                // Capture all data needed for the main-thread callback
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
        
        // Get the ArkTS logger object from reference
        napi_value loggerValue;
        napi_status status = napi_get_reference_value(mEnv, mArktsLoggerRef, &loggerValue);
        if (status != napi_ok || loggerValue == nullptr) {
            return;
        }
        
        // Get the onLogFromNative method
        napi_value onLogFromNativeMethod;
        status = napi_get_named_property(mEnv, loggerValue, "onLogFromNative", &onLogFromNativeMethod);
        if (status != napi_ok) {
            return;
        }
        
        // Prepare arguments
        napi_value args_array[5];
        
        // level (number)
        napi_create_int32(mEnv, static_cast<int32_t>(level), &args_array[0]);
        
        // tag (string)
        napi_create_string_utf8(mEnv, tag ? tag : "", NAPI_AUTO_LENGTH, &args_array[1]);
        
        // func (string)
        napi_create_string_utf8(mEnv, func ? func : "", NAPI_AUTO_LENGTH, &args_array[2]);
        
        // line (number)
        napi_create_int32(mEnv, line, &args_array[3]);
        
        // message (string)
        napi_create_string_utf8(mEnv, buffer, NAPI_AUTO_LENGTH, &args_array[4]);
        
        // Call the method
        napi_value result;
        napi_call_function(mEnv, loggerValue, onLogFromNativeMethod, 5, args_array, &result);
    }
    
private:
    napi_env mEnv;
    napi_ref mArktsLoggerRef;
    pthread_t mMainThreadId;
    agenui::LogLevel mMinLevel;
};

// Global logger instance
static RuntimeLoggerImpl* gRuntimeLoggerImpl = nullptr;

inline void registerEtsFunction(const std::string& name, napi_env env, napi_value value) {
    std::lock_guard<std::mutex> lock(g_ets_functions_mutex);
    auto existing = g_ets_functions_table.find(name);
    if (existing != g_ets_functions_table.end()) {
        napi_delete_reference(existing->second.env, existing->second.ref);
        g_ets_functions_table.erase(existing);
    }

    napi_ref ref;
    napi_create_reference(env, value, 1, &ref);
    g_ets_functions_table[name] = { name, env, ref };
}

} 

a2ui::IHarmonyNAPI* implHarmonyNAPI() {
    return a2ui::implHarmonyNAPI();
}

// Global napi_env cache set during Init and used by C++ components such as LottieComponent when creating thread-safe functions
static napi_env g_napiEnv = nullptr;
// Main thread ID recorded in RegisterEntryModule (__attribute__((constructor)))
static pthread_t g_mainThreadId = 0;
napi_env a2ui_get_napi_env() { return g_napiEnv; }

// Thread-safe function created in Init and used to dispatch worker-thread callbacks onto the main thread
// (declared at file top, before namespace a2ui)
napi_threadsafe_function a2ui_get_main_tsfn() { return g_mainTsfn; }

// App sandbox root directory (filesDir), read by components such as IconComponent to build absolute resource paths
static std::string g_filesDir;
const std::string& a2ui_get_files_dir() { return g_filesDir; }
// Globally cached MessageThreadFactory pointer
static uint64_t g_messageThreadFactoryPtr = 0;

// ==================== Platform Function Management ====================
// Mapping from name to HarmonyPlatformFunction instance for lifecycle management
static std::mutex g_platformFunctionsMutex;
static std::map<std::string, agenui::HarmonyPlatformFunction*> g_platformFunctions;

// ==================== Multi-instance Management ====================
// Listener ownership is held as shared_ptr so worker-thread events that have
// already been posted to the main-thread TSFN queue can outlive a manual
// destroy() on the main thread (they capture weak_ptr and bail out safely).
static std::mutex g_messageListenersMutex;
static std::map<int, std::shared_ptr<agenui::A2UIMessageListener>> g_messageListeners;

/**
 * @brief Look up A2UIMessageListener by instanceId
 * @return Pointer when found, otherwise nullptr
 */
static agenui::A2UIMessageListener* findMessageListenerByInstanceId(int instanceId) {
    std::lock_guard<std::mutex> lock(g_messageListenersMutex);
    auto it = g_messageListeners.find(instanceId);
    if (it != g_messageListeners.end()) {
        return it->second.get();
    }
    HM_LOGE("A2UIMessageListener not found for instanceId=%d", instanceId);
    return nullptr;
}

/**
 * @brief Look up the engine-layer ISurfaceManager by instanceId
 * @return Pointer when found, otherwise nullptr
 */
static agenui::ISurfaceManager* findSurfaceManagerByInstanceId(int instanceId) {
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

// ==================== NAPI Helper Macros ====================
#define NAPI_RETURN_UNDEFINED(env) \
    do { napi_value _r; napi_get_undefined(env, &_r); return _r; } while(0)

#define NAPI_GET_ARGS(env, info, count, args) \
    do { size_t _argc = count; napi_get_cb_info(env, info, &_argc, args, nullptr, nullptr); \
         if (_argc < count) { HM_LOGE("%s: Expected %d args, got %zu", __func__, count, _argc); NAPI_RETURN_UNDEFINED(env); } } while(0)

// ==================== NAPI Helper Functions ====================

/**
 * @brief Read a UTF-8 string napi_value into a std::string.
 * Performs the standard two-call pattern: query size, then read into a sized buffer.
 * Uses std::vector<char> for RAII so the buffer is freed even on exceptions.
 */
static inline std::string napiGetString(napi_env env, napi_value value) {
    size_t strSize = 0;
    napi_get_value_string_utf8(env, value, nullptr, 0, &strSize);
    std::vector<char> buf(strSize + 1, '\0');
    napi_get_value_string_utf8(env, value, buf.data(), buf.size(), &strSize);
    return std::string(buf.data(), strSize);
}

/**
 * @brief Build a napi boolean value in one call.
 */
static inline napi_value napiBoolean(napi_env env, bool value) {
    napi_value result;
    napi_get_boolean(env, value, &result);
    return result;
}

/**
 * @brief Verify that the given argument is a function. On failure, log an error
 * tagged with the caller name and return false. The caller is expected to
 * return early using NAPI_RETURN_UNDEFINED on failure.
 */
static inline bool napiCheckArgIsFunction(napi_env env, napi_value arg, const char* callerName) {
    napi_valuetype valueType;
    napi_typeof(env, arg, &valueType);
    if (valueType != napi_function) {
        HM_LOGE("%s: Argument is not a function", callerName);
        return false;
    }
    return true;
}

// ==================== NAPI Function Implementations ====================

static napi_value Add(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = {nullptr};

    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    napi_valuetype valuetype0;
    napi_typeof(env, args[0], &valuetype0);

    napi_valuetype valuetype1;
    napi_typeof(env, args[1], &valuetype1);

    double value0;
    napi_get_value_double(env, args[0], &value0);

    double value1;
    napi_get_value_double(env, args[1], &value1);

    napi_value sum;
    napi_create_double(env, value0 + value1, &sum);

    return sum;
}

/**
 * @brief Initialize the AGenUI engine (replaces Start)
 * Create the global IAGenUIEngine instance and configure shared services
 */
static napi_value Start(napi_env env, napi_callback_info info) {
    HM_LOGI("AGenUI Start called - initializing IAGenUIEngine");
    agenui::IAGenUIEngine* engine = agenui::initAGenUIEngine();
    if (engine == nullptr) {
        HM_LOGE("Failed to initialize AGenUI Engine");
        NAPI_RETURN_UNDEFINED(env);
    }

    // Configure the device service (global singleton)
    engine->setPlatformLayoutBridge(new a2ui::A2UIPlatformLayoutBridge());
    
    // Initialize runtime logger
    if (a2ui::gRuntimeLoggerImpl == nullptr) {
        // Get the ArkTS logger object from the first argument
        size_t argc = 1;
        napi_value argv[1];
        napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
        
        if (argc > 0 && argv[0] != nullptr) {
            // Create a reference to the ArkTS logger object
            napi_ref loggerRef;
            napi_create_reference(env, argv[0], 1, &loggerRef);
            
            a2ui::gRuntimeLoggerImpl = new a2ui::RuntimeLoggerImpl(env, loggerRef);
            engine->setRuntimeLogger(a2ui::gRuntimeLoggerImpl);
            HM_LOGI("AGenUI RuntimeLogger initialized successfully");
        } else {
            HM_LOGW("AGenUI Start: No logger provided, using default logger");
        }
    }

    // Register all Sync component measurement classes (engine-level singleton, register once)
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

/**
 * @brief Stop the AGenUI engine (replaces Stop)
 * Destroy all SurfaceManager instances and the global engine
 */
static napi_value Stop(napi_env env, napi_callback_info info) {
    HM_LOGI("AGenUI Stop called - destroying IAGenUIEngine");

    auto* engine = agenui::getAGenUIEngine();
    if (engine) {
        // Clean up runtime logger
        if (a2ui::gRuntimeLoggerImpl) {
            delete a2ui::gRuntimeLoggerImpl;
            a2ui::gRuntimeLoggerImpl = nullptr;
            HM_LOGI("Cleaned up RuntimeLogger");
        }
        
        {
            std::lock_guard<std::mutex> lock(g_platformFunctionsMutex);
            for (auto it = g_platformFunctions.begin(); it != g_platformFunctions.end(); ) {
                engine->unregisterFunction(it->first);
                delete it->second;
                it = g_platformFunctions.erase(it);
            }
            HM_LOGI("Cleaned up all PlatformFunctions");
        }

        // Clear MessageListener instances that were not destroyed manually.
        // Same teardown order as DestroySurfaceManager: detach from engine,
        // destroy engine SM, then drop our shared_ptr (the listener is freed
        // when the last weak_ptr.lock() in any pending TSFN task releases it).
        {
            std::lock_guard<std::mutex> lock(g_messageListenersMutex);
            for (auto it = g_messageListeners.begin(); it != g_messageListeners.end(); ) {
                int instanceId = it->first;
                auto* sm = engine->findSurfaceManager(instanceId);
                if (sm) {
                    sm->removeSurfaceEventListener(it->second.get());
                    engine->destroySurfaceManager(sm);
                }
                it = g_messageListeners.erase(it);
                HM_LOGI("Cleaned up listener for instanceId=%d", instanceId);
            }
        }
    }

    agenui::destroyAGenUIEngine();
    HM_LOGI("AGenUI Engine destroyed");

    // Release the global thread-safe function
    if (g_mainTsfn) {
        // First call unref to remove the reference added in Init so the TSFN can shut down normally
        napi_unref_threadsafe_function(env, g_mainTsfn);
        // Then call release to decrement the thread count (matching initial_thread_count=1)
        napi_release_threadsafe_function(g_mainTsfn, napi_tsfn_release);
        g_mainTsfn = nullptr;
        HM_LOGI("g_mainTsfn released");
    }

    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Set the minimum log level forwarded to the C++ engine.
 *
 * Pushed down from ArkTS (AGenUILogger.setMinLogLevel) to avoid a JS round-trip
 * on every C++ log emission. Updates both the custom wrapper (if installed) and
 * the built-in default logger, so the same setter works regardless of whether a
 * custom IRuntimeLogger has been injected yet.
 *
 * @param level number — one of agenui::LogLevel (0=DEBUG ... 5=PERFORMANCE)
 */
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
 * @brief Create a SurfaceManager instance
 * @return number (instanceId)
 */
static napi_value CreateSurfaceManager(napi_env env, napi_callback_info info) {
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

    // Must allocate via make_shared so enable_shared_from_this works in the
    // listener's worker-thread callbacks. Engine still receives a raw pointer.
    auto listener = std::make_shared<agenui::A2UIMessageListener>(instanceId);
    listener->setTsfn(g_mainTsfn);
    sm->addSurfaceEventListener(listener.get());
    {
        std::lock_guard<std::mutex> lock(g_messageListenersMutex);
        g_messageListeners[instanceId] = listener;
    }

    // Wire the Harmony-internal observable layer to the cross-platform SurfaceManager.
    // C++ render-layer components (Tabs/Video/Image) publish render-finish / surface-size
    // events via observables held by A2UISurfaceManager; A2UISurfaceManager forwards them
    // to ISurfaceManager::onRenderFinish / onSurfaceSizeChanged.
    a2ui::A2UISurfaceManager* surfaceManager = listener->getSurfaceManager();
    surfaceManager->setCoreSurfaceManager(sm);

    HM_LOGI("CreateSurfaceManager: instanceId=%d created successfully", instanceId);

    napi_value result;
    napi_create_int32(env, instanceId, &result);
    return result;
}

/**
 * @brief Destroy the SurfaceManager instance
 * @param args[0] instanceId (number)
 */
static napi_value DestroySurfaceManager(napi_env env, napi_callback_info info) {
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

    // 1. Hand the listener's strong reference out of the global map.
    //    Any main-thread tasks already queued via TSFN that captured weak_ptr
    //    will safely no-op once this local shared_ptr drops the last reference.
    std::shared_ptr<agenui::A2UIMessageListener> listenerOwner;
    {
        std::lock_guard<std::mutex> lock(g_messageListenersMutex);
        auto listenerIt = g_messageListeners.find(instanceId);
        if (listenerIt != g_messageListeners.end()) {
            listenerOwner = std::move(listenerIt->second);
            g_messageListeners.erase(listenerIt);
        }
    }

    // 2. Detach from the engine first so no further onXxx events are dispatched.
    auto* sm = engine->findSurfaceManager(instanceId);
    if (sm) {
        if (listenerOwner) {
            sm->removeSurfaceEventListener(listenerOwner.get());
            // Break the back-reference from A2UISurfaceManager before the cross-platform
            // SurfaceManager is destroyed. Any further internal observable events will
            // be silently dropped instead of dereferencing a freed pointer.
            if (auto* a2uiSm = listenerOwner->getSurfaceManager()) {
                a2uiSm->setCoreSurfaceManager(nullptr);
            }
        }

        // 3. Destroy the engine-layer SurfaceManager (posts uninit to worker thread).
        engine->destroySurfaceManager(sm);
    }

    HM_LOGI("DestroySurfaceManager: instanceId=%d destroyed", instanceId);
    NAPI_RETURN_UNDEFINED(env);
}

static napi_value SendMockData(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 1) {
        HM_LOGE("SendMockData: Invalid argument count");
        NAPI_RETURN_UNDEFINED(env);
    }
    
    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Set path configuration
 * @param args[0] configJson (string) - JSON string, e.g. {"templateDir": "/path/to/templates"}
 * @return boolean - whether the operation succeeded
 */
static napi_value SetPathConfig(napi_env env, napi_callback_info info) {
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

/**
 * @brief Remove the event listener
 */
static napi_value RemoveEventListener(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 1) {
        HM_LOGE("RemoveEventListener: Invalid argument count");
        napi_value result;
        napi_get_undefined(env, &result);
        return result;
    }
    
    HM_LOGI("RemoveEventListener called");
    
    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

/**
 * @brief Get the version number
 */
static napi_value GetVersion(napi_env env, napi_callback_info info) {
    napi_value result;
    napi_create_string_utf8(env, agenui::getAGenUIVersion(), NAPI_AUTO_LENGTH, &result);
    return result;
}

/**
 * @brief Request Surface creation
 * @param args[0] instanceId (number)
 * @param args[1] data (string) A2UI protocol payload
 */
static napi_value RequestSurface(napi_env env, napi_callback_info info) {
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

    auto* sm = findSurfaceManagerByInstanceId(instanceId);
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
/**
 * @brief Register the A2UI surface listener
 * @param args[0] instanceId (number)
 * @param args[1] listener (ISurfaceListener object)
 */
static napi_value RegisterA2UISurfaceListener(napi_env env, napi_callback_info info) {
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

/**
 * @brief Unregister the A2UI surface listener
 * @param args[0] instanceId (number)
 * @param args[1] listener (ISurfaceListener object)
 */
static napi_value UnregisterA2UISurfaceListener(napi_env env, napi_callback_info info) {
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

/**
 * @brief Clear the A2UI container
 * @param args[0] instanceId (number)
 */
static napi_value ClearA2UiContainer(napi_env env, napi_callback_info info) {
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

/**
 * @brief Bind the surface
 * @param args[0] instanceId (number)
 * @param args[1] surfaceId (string)
 * @param args[2] nodeContent (object)
 */
static napi_value BindSurface(napi_env env, napi_callback_info info) {
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

/**
 * @brief Unbind the surface
 * @param args[0] instanceId (number)
 * @param args[1] surfaceId (string)
 */
static napi_value UnbindSurface(napi_env env, napi_callback_info info) {
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

/**
 * @brief Register the URL open callback
 */
static napi_value RegisterOpenUrlCallback(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("RegisterOpenUrlCallback: Invalid argument count");
        NAPI_RETURN_UNDEFINED(env);
    }

    if (!napiCheckArgIsFunction(env, args[0], "RegisterOpenUrlCallback")) {
        NAPI_RETURN_UNDEFINED(env);
    }

    a2ui::OpenUrlHelper::getInstance().registerCallback(env, args[0]);
    HM_LOGI("RegisterOpenUrlCallback: Callback registered successfully");

    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Register the skill invocation callback
 */
static napi_value RegisterSkillInvokerCallback(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("RegisterSkillInvokerCallback: Invalid argument count");
        NAPI_RETURN_UNDEFINED(env);
    }

    if (!napiCheckArgIsFunction(env, args[0], "RegisterSkillInvokerCallback")) {
        NAPI_RETURN_UNDEFINED(env);
    }

    a2ui::SkillInvokerHelper::getInstance().registerCallback(env, args[0]);
    HM_LOGI("RegisterSkillInvokerCallback: Callback registered successfully");

    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Register the ETS function in the C++ layer
 */
static napi_value RegisterEtsFunction(napi_env env, napi_callback_info info) {
    HM_LOGE("RegisterEtsFunction: invoked");
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("RegisterEtsFunction: Expected 2 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    std::string name = napiGetString(env, args[0]);

    if (!napiCheckArgIsFunction(env, args[1], "RegisterEtsFunction (second arg)")) {
        NAPI_RETURN_UNDEFINED(env);
    }

    a2ui::registerEtsFunction(name, env, args[1]);
    
    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Receive screen information from the ETS side
 */
static napi_value SetDeviceInfo(napi_env env, napi_callback_info info) {
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

/**
 * @brief Get the property value from ComponentState
 */
static napi_value HybridFactory_getAttribute(napi_env env, napi_callback_info info) {
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


/**
 * @brief Get the full property JSON snapshot from ComponentState
 * @param args[0] ptr (number) - ComponentState pointer
 * @return string - Full property JSON snapshot
 */
static napi_value HybridFactory_getPropertiesJson(napi_env env, napi_callback_info info) {
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


/**
 * @brief Notify the engine that a component has finished rendering with its measured dimensions. Supports Markdown, Web, and other custom components.
 */
static napi_value ReportComponentRenderSize(napi_env env, napi_callback_info info) {
    size_t argc = 6;
    napi_value args[6];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 5) {
        HM_LOGE("ReportComponentRenderSize: Expected 5 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }
    
    std::string surfaceId   = napiGetString(env, args[0]);
    std::string componentId = napiGetString(env, args[1]);
    std::string type        = napiGetString(env, args[2]);

    double height = 0.0;
    napi_get_value_double(env, args[3], &height);

    double width = 0.0;
    napi_get_value_double(env, args[4], &width);

    HM_LOGI("ReportComponentRenderSize: surfaceId=%s, componentId=%s, type=%s, height=%f, width=%f", surfaceId.c_str(), componentId.c_str(), type.c_str(), height, width);

    int instanceId = agenui::A2UIMessageListener::findInstanceIdBySurfaceId(surfaceId);
    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("ReportComponentRenderSize: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }
    agenui::ISurfaceManager* sm = engine->findSurfaceManager(instanceId);
    if (!sm) {
        HM_LOGE("ReportComponentRenderSize: SurfaceManager not found for surfaceId=%s", surfaceId.c_str());
        NAPI_RETURN_UNDEFINED(env);
    }

    agenui::ComponentRenderInfo markdownInfo;
    markdownInfo.surfaceId   = surfaceId;
    markdownInfo.componentId = componentId;
    markdownInfo.type        = type;
    markdownInfo.height      = a2ui::UnitConverter::vpToA2ui(height);
    markdownInfo.width       = a2ui::UnitConverter::vpToA2ui(width);
    sm->onRenderFinish(markdownInfo);

    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Register ETS measurement layer implementation
 * @param args[0] instanceId (number)  - SurfaceManager instanceId
 * @param args[1] type     (string)  - Component type (e.g. "Text", "Image")
 * @param args[2] callback (Function) - (paramJson: string, widthMode: number, maxWidth: number,
 *                                       heightMode: number, maxHeight: number) => { width: number, height: number, calcType?: number }
 */
static napi_value RegisterMeasurement(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 3) {
        HM_LOGE("RegisterMeasurement: Expected 3 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    size_t typeSize = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &typeSize);
    char* typeBuf = new char[typeSize + 1];
    napi_get_value_string_utf8(env, args[1], typeBuf, typeSize + 1, &typeSize);
    std::string type(typeBuf);
    delete[] typeBuf;

    napi_valuetype callbackType = napi_undefined;
    napi_typeof(env, args[2], &callbackType);
    if (callbackType != napi_function) {
        HM_LOGE("RegisterMeasurement: Third argument is not a function");
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("RegisterMeasurement: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* mm = engine->getMeasurementManager();
    if (!mm) {
        HM_LOGE("RegisterMeasurement: No MeasurementManager for instanceId=%d", instanceId);
        NAPI_RETURN_UNDEFINED(env);
    }

    struct MeasureCallData {
        std::string paramJson;
        float widthMaxValue  = 0.0f;
        int   widthMode      = 0;
        float heightMaxValue = 0.0f;
        int   heightMode     = 0;
        float resultWidth    = 0.0f;
        float resultHeight   = 0.0f;
        int   resultCalcType = 0;  ///< 0=Sync 1=Async
        bool  done = false;
        std::mutex mtx;
        std::condition_variable cv;
    };
    auto measureCallJs = [](napi_env env, napi_value js_func, void* /*context*/, void* data) {
        if (!data) return;
        auto* cd = static_cast<MeasureCallData*>(data);

        napi_value jsParamJson = nullptr;
        napi_create_string_utf8(env, cd->paramJson.c_str(), cd->paramJson.size(), &jsParamJson);
        napi_value jsWidthMode = nullptr;
        napi_create_int32(env, cd->widthMode, &jsWidthMode);
        napi_value jsMaxWidth = nullptr;
        napi_create_double(env, cd->widthMaxValue, &jsMaxWidth);
        napi_value jsHeightMode = nullptr;
        napi_create_int32(env, cd->heightMode, &jsHeightMode);
        napi_value jsMaxHeight = nullptr;
        napi_create_double(env, cd->heightMaxValue, &jsMaxHeight);

        napi_value argv[5] = { jsParamJson, jsWidthMode, jsMaxWidth, jsHeightMode, jsMaxHeight };
        napi_value result = nullptr;
        napi_call_function(env, nullptr, js_func, 5, argv, &result);

        if (result) {
            napi_value widthVal = nullptr, heightVal = nullptr, calcTypeVal = nullptr;
            napi_get_named_property(env, result, "width",    &widthVal);
            napi_get_named_property(env, result, "height",   &heightVal);
            napi_get_named_property(env, result, "calcType", &calcTypeVal);

            double w = 0.0, h = 0.0;
            napi_get_value_double(env, widthVal,  &w);
            napi_get_value_double(env, heightVal, &h);
            cd->resultWidth  = static_cast<float>(w);
            cd->resultHeight = static_cast<float>(h);

            napi_valuetype ct = napi_undefined;
            if (calcTypeVal) napi_typeof(env, calcTypeVal, &ct);
            if (ct == napi_number) {
                int32_t calcType = 0;
                napi_get_value_int32(env, calcTypeVal, &calcType);
                cd->resultCalcType = calcType;
            }
        }

        {
            std::lock_guard<std::mutex> lk(cd->mtx);
            cd->done = true;
        }
        cd->cv.notify_one();
    };

    napi_value resourceName = nullptr;
    napi_create_string_utf8(env, "MeasurementCallback", NAPI_AUTO_LENGTH, &resourceName);
    napi_threadsafe_function tsfn = nullptr;
    napi_create_threadsafe_function(env, args[2], nullptr, resourceName,
                                    0, 1, nullptr, nullptr, nullptr,
                                    measureCallJs, &tsfn);
    if (!tsfn) {
        HM_LOGE("RegisterMeasurement: Failed to create threadsafe function");
        NAPI_RETURN_UNDEFINED(env);
    }

    class ETSMeasurement : public agenui::IMeasurement {
    public:
        ETSMeasurement(napi_threadsafe_function tsfn) : _tsfn(tsfn) {}
        ~ETSMeasurement() override {
            if (_tsfn) napi_release_threadsafe_function(_tsfn, napi_tsfn_release);
        }

        agenui::MeasureResult measure(
                const std::string& paramJson,
                const agenui::MeasureModes& modes) override {
            MeasureCallData data;
            data.paramJson       = paramJson;
            data.widthMaxValue   = modes.width.maxValue;
            data.widthMode       = modes.width.mode;
            data.heightMaxValue  = modes.height.maxValue;
            data.heightMode      = modes.height.mode;

            napi_call_threadsafe_function(
                _tsfn, &data, napi_tsfn_blocking);

            {
                std::unique_lock<std::mutex> lk(data.mtx);
                data.cv.wait(lk, [&]{ return data.done; });
            }
            const auto ct = (data.resultCalcType == 1)
                ? agenui::CalcType::Async
                : agenui::CalcType::Sync;
            return {ct, data.resultWidth, data.resultHeight, 0};
        }

    private:
        napi_threadsafe_function _tsfn = nullptr;
    };

    auto impl = std::make_shared<ETSMeasurement>(tsfn);
    mm->registerMeasurement(type, impl);
    HM_LOGI("RegisterMeasurement: instanceId=%d type=%s registered", instanceId, type.c_str());
    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Unregister ETS measurement layer implementation
 * @param args[0] instanceId (number) - SurfaceManager instanceId
 * @param args[1] type     (string) - Component type
 */
static napi_value UnregisterMeasurement(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("UnregisterMeasurement: Expected 2 arguments");
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    size_t typeSize = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &typeSize);
    char* typeBuf = new char[typeSize + 1];
    napi_get_value_string_utf8(env, args[1], typeBuf, typeSize + 1, &typeSize);
    std::string type(typeBuf);
    delete[] typeBuf;

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("UnregisterMeasurement: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }
    auto* mm = engine->getMeasurementManager();
    if (mm) {
        mm->unregisterMeasurement(type);
        HM_LOGI("UnregisterMeasurement: instanceId=%d type=%s removed", instanceId, type.c_str());
    }
    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Set the message thread factory
 */
static napi_value SetMessageThreadFactory(napi_env env, napi_callback_info info) {
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
    HM_LOGI("SetMessageThreadFactory: cached ptrValue=%llu", ptrValue);
    
    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Notify the C++ layer that the surface size changed
 */
static napi_value Surface_onSizeChanged(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 3) {
        HM_LOGE("OnSurfaceSizeChanged: Expected 3 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    std::string surfaceId = napiGetString(env, args[0]);

    double width = 0.0;
    napi_get_value_double(env, args[1], &width);

    double height = 0.0;
    napi_get_value_double(env, args[2], &height);

    HM_LOGI("OnSurfaceSizeChanged: surfaceId=%s, width=%f, height=%f", surfaceId.c_str(), width, height);

    int instanceId = agenui::A2UIMessageListener::findInstanceIdBySurfaceId(surfaceId);
    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("OnSurfaceSizeChanged: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }
    agenui::ISurfaceManager* sm = engine->findSurfaceManager(instanceId);
    if (!sm) {
        HM_LOGE("OnSurfaceSizeChanged: SurfaceManager not found for surfaceId=%s", surfaceId.c_str());
        NAPI_RETURN_UNDEFINED(env);
    }

    agenui::SurfaceLayoutInfo surfaceInfo;
    surfaceInfo.surfaceId = surfaceId;
    surfaceInfo.width     = a2ui::UnitConverter::vpToA2ui(width);
    surfaceInfo.height    = a2ui::UnitConverter::vpToA2ui(height);
    sm->onSurfaceSizeChanged(surfaceInfo);

    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Set the theme configuration
 */
static napi_value SetThemeConfig(napi_env env, napi_callback_info info) {
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

/**
 * @brief Set the DesignToken configuration
 */
static napi_value SetDesignTokenConfig(napi_env env, napi_callback_info info) {
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

/**
 * @brief Register a platform function (replaces registerSkill + setSkillInvoker)
 * Each skill owns a HarmonyPlatformFunction instance with its own callback
 * @param args[0] name (string) - Function name used for lifecycle management and unregistration
 * @param args[1] config (string) - Skill configuration JSON
 * @param args[2] callback (function) - Per-skill callback: (paramsJson: string) => string
 */
static napi_value RegisterFunction(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 3) {
        HM_LOGE("RegisterFunction: Expected 3 arguments, got %{public}zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    // Argument 1: name (string)
    std::string name = napiGetString(env, args[0]);

    // Argument 2: config (string)
    std::string config = napiGetString(env, args[1]);

    // Argument 3: callback (function)
    if (!napiCheckArgIsFunction(env, args[2], "RegisterFunction (third arg)")) {
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("RegisterFunction: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }

    // If a function with the same name already exists, first notify the engine to unregister it, then destroy the old instance to avoid dangling pointers
    {
        std::lock_guard<std::mutex> lock(g_platformFunctionsMutex);
        auto it = g_platformFunctions.find(name);
        if (it != g_platformFunctions.end()) {
            engine->unregisterFunction(name);
            delete it->second;
            g_platformFunctions.erase(it);
        }
    }

    // Create the HarmonyPlatformFunction instance and keep its callback reference
    // Pass g_mainTsfn and g_mainThreadId so callSync can be invoked safely from non-main threads
    auto* function = new agenui::HarmonyPlatformFunction(env, args[2], g_mainTsfn, g_mainThreadId);
    if (!function->isValid()) {
        HM_LOGE("RegisterFunction: Failed to create HarmonyPlatformFunction for %s", name.c_str());
        delete function;
        NAPI_RETURN_UNDEFINED(env);
    }

    // Register in the C++ engine
    engine->registerFunction(config, function);

    // Store in the NAPI-layer map for lifecycle management
    {
        std::lock_guard<std::mutex> lock(g_platformFunctionsMutex);
        g_platformFunctions[name] = function;
    }

    HM_LOGI("RegisterFunction: name=%s registered successfully", name.c_str());

    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Unregister the platform function
 * Before destruction, notify the engine that the function must no longer be called to avoid dangling pointers
 * @param args[0] name (string) - Function name
 */
static napi_value UnregisterFunction(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("UnregisterFunction: Expected 1 argument");
        NAPI_RETURN_UNDEFINED(env);
    }

    std::string name = napiGetString(env, args[0]);

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("UnregisterFunction: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }

    engine->unregisterFunction(name);

    {
        std::lock_guard<std::mutex> lock(g_platformFunctionsMutex);
        auto it = g_platformFunctions.find(name);
        if (it != g_platformFunctions.end()) {
            delete it->second;
            g_platformFunctions.erase(it);
            HM_LOGI("UnregisterFunction: destroyed HarmonyPlatformFunction for %s", name.c_str());
        }
    }

    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Set day/night mode (new in v2.0, same behavior as SetThemeMode)
 * @param args[0] mode (string) - "light" or "dark"
 */
static napi_value SetDayNightMode(napi_env env, napi_callback_info info) {
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

/**
 * @brief Register the default theme and DesignToken configuration (new in v2.0)
 * @param args[0] theme (string) - theme configuration JSON
 * @param args[1] designToken (string) - DesignToken configuration JSON
 * @return boolean - whether the operation succeeded
 */
static napi_value RegisterDefaultTheme(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("RegisterDefaultTheme: Expected 2 arguments, got %zu", argc);
        return napiBoolean(env, false);
    }

    // Get the theme string
    std::string theme = napiGetString(env, args[0]);

    // Get the designToken string
    std::string designToken = napiGetString(env, args[1]);

    HM_LOGI("RegisterDefaultTheme: theme length=%zu, designToken length=%zu", theme.size(), designToken.size());

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("RegisterDefaultTheme: Engine not initialized");
        return napiBoolean(env, false);
    }

    // Register the theme configuration
    std::string resultStr;
    bool themeResult = engine->loadThemeConfig(theme, resultStr);
    if (!themeResult) {
        HM_LOGE("RegisterDefaultTheme: Theme registration failed: %s", resultStr.c_str());
        return napiBoolean(env, false);
    }

    // Register the DesignToken configuration
    bool tokenResult = engine->loadDesignTokenConfig(designToken, resultStr);
    if (!tokenResult) {
        HM_LOGE("RegisterDefaultTheme: DesignToken registration failed: %s", resultStr.c_str());
        return napiBoolean(env, false);
    }

    HM_LOGI("RegisterDefaultTheme: success");
    return napiBoolean(env, true);
}

/**
 * @brief Begin a streaming data session (new in v2.0)
 * @param args[0] instanceId (number)
 */
static napi_value BeginTextStream(napi_env env, napi_callback_info info) {
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

/**
 * @brief End a streaming data session (new in v2.0)
 * @param args[0] instanceId (number)
 */
static napi_value EndTextStream(napi_env env, napi_callback_info info) {
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

/**
 * @brief Register the custom component factory (new in v2.0)
 * @param args[0] type (string) - component type name
 * @param args[1] creator (function) - component factory function
 */
static napi_value RegisterComponent(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("RegisterComponent: Expected 2 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    // Get the component type
    std::string type = napiGetString(env, args[0]);

    // Verify that the second argument is a function
    if (!napiCheckArgIsFunction(env, args[1], "RegisterComponent (second arg)")) {
        NAPI_RETURN_UNDEFINED(env);
    }

    HM_LOGI("RegisterComponent: type=%s", type.c_str());

    // Return success for now
    HM_LOGI("RegisterComponent: success (stub implementation)");

    NAPI_RETURN_UNDEFINED(env);
}

/**
 * @brief Register the ETS image loader
 * The host app injects an IImageLoader through SurfaceManager.setImageLoader(loader),
 * then passes the ETS loader object to the C++ bridge through this NAPI entry.
 * @param args[0] loader (object) - ETS object implementing IImageLoader
 * @return undefined
 */
static napi_value RegisterImageLoader(napi_env env, napi_callback_info info) {
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

/**
 * @brief Forward a UI action to ISurfaceManager::submitUIAction
 * @param args[0] instanceId (number)
 * @param args[1] surfaceId (string)
 * @param args[2] sourceComponentId (string)
 * @param args[3] contextJson (string)
 */
static napi_value SubmitUIAction(napi_env env, napi_callback_info info) {
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

/**
 * @brief Forward UI data model changes to ISurfaceManager::submitUIDataModel
 * @param args[0] instanceId (number)
 * @param args[1] surfaceId (string)
 * @param args[2] componentId (string)
 * @param args[3] change (string)
 */
static napi_value SubmitUIDataModel(napi_env env, napi_callback_info info) {
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

/**
 * @brief Destroy the specified surface
 * @param args[0] instanceId (number)
 * @param args[1] surfaceId (string)
 */
static napi_value DestroySurface(napi_env env, napi_callback_info info) {
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

    std::string requestContent = "{\"version\":\"v0.9\",\"deleteSurface\":{\"surfaceId\":\"" + surfaceId + "\"}}";
    HM_LOGI("DestroySurface: instanceId=%d, surfaceId=%s", instanceId, surfaceId.c_str());
    
    auto* sm = findSurfaceManagerByInstanceId(instanceId);
    if (sm) {
        sm->receiveTextChunk(requestContent);
    }

    NAPI_RETURN_UNDEFINED(env);
}


/**
 * @brief Create a DrawableDescriptor from raw pixel data supplied by ETS after image load success
 * This bypasses the unreliable OH_ArkUI_GetDrawableDescriptorFromNapiValue path.
 *
 * @param args[0] requestId   (string)      - Request ID
 * @param args[1] bytes       (ArrayBuffer) - Raw RGBA/BGRA pixel data
 * @param args[2] width       (number)      - Width in px
 * @param args[3] height      (number)      - Height(px)
 * @param args[4] pixelFormat (number)      - Pixel format: RGBA_8888=3, BGRA_8888=4
 * @param args[5] alphaType   (number)      - Alpha type
 * @return undefined
 */
static napi_value SetImagePixelMap(napi_env env, napi_callback_info info) {
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

    napi_valuetype bytesType = napi_undefined;
    napi_typeof(env, args[1], &bytesType);
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

    a2ui::ImageLoaderBridge::getInstance().setImagePixelMapFromBytes(
        requestId,
        static_cast<uint8_t*>(rawData),
        dataLen,
        static_cast<int32_t>(width),
        static_cast<int32_t>(height),
        static_cast<int32_t>(pixelFormat),
        static_cast<int32_t>(alphaType)
    );

    napi_value ret;
    napi_get_undefined(env, &ret);
    return ret;
}

/**
 * @brief Callback entry for ETS image load failure or cancellation
 * Successful loads use setImagePixelMap instead of this entry.
 *
 * @param args[0] requestId  (string)  - Request ID
 * @param args[1] isCancelled (boolean) - Whether the request was cancelled
 * @return undefined
 */
static napi_value OnImageLoadFailed(napi_env env, napi_callback_info info) {
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

/**
 * @brief Look up instanceId by surfaceId
 * @param args[0] surfaceId (string)
 * @return number - instanceId, or 0 if not found
 */
static napi_value FindInstanceIdBySurfaceId(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        HM_LOGE("FindInstanceIdBySurfaceId: Expected 1 argument, got %zu", argc);
        napi_value result;
        napi_create_int32(env, 0, &result);
        return result;
    }

    std::string surfaceId = napiGetString(env, args[0]);
    int instanceId = agenui::A2UIMessageListener::findInstanceIdBySurfaceId(surfaceId);

    HM_LOGI("FindInstanceIdBySurfaceId: surfaceId=%s -> instanceId=%d", surfaceId.c_str(), instanceId);

    napi_value result;
    napi_create_int32(env, instanceId, &result);
    return result;
}

/**
 * @brief Forward raw protocol streaming data to IAGenUIModule::transmitRawProtocolStreaming
 * @param args[0] data (string) - Raw protocol payload
 * @return undefined
 */
static napi_value ReceiveTextChunk(napi_env env, napi_callback_info info) {
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
            nullptr,    // async_resource
            asyncResourceName,
            0,
            1,
            nullptr,    // thread_finalize_data
            nullptr,    // thread_finalize_cb
            nullptr,    // context
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
        { "add", nullptr, Add, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "start", nullptr, Start, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "stop", nullptr, Stop, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setMinLogLevel", nullptr, SetMinLogLevel, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "createSurfaceManager", nullptr, CreateSurfaceManager, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "destroySurfaceManager", nullptr, DestroySurfaceManager, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "sendMockData", nullptr, SendMockData, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setPathConfig", nullptr, SetPathConfig, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "removeEventListener", nullptr, RemoveEventListener, nullptr, nullptr, nullptr, napi_default, nullptr },
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
        { "registerComponent", nullptr, RegisterComponent, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "registerImageLoader", nullptr, RegisterImageLoader, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setImagePixelMap", nullptr, SetImagePixelMap, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "onImageLoadFailed", nullptr, OnImageLoadFailed, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "findInstanceIdBySurfaceId", nullptr, FindInstanceIdBySurfaceId, nullptr, nullptr, nullptr, napi_default, nullptr },
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
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    g_mainThreadId = pthread_self();
    napi_module_register(&demoModule);
}
