#include "jni_scoped_local_ref.h"
#include <jni.h>
#include "jni_scoped_utf_chars.h"
#include "agenui_engine_entry.h"
#include "agenui_dispatcher_types.h"
#include "agenui_platform_function.h"
#include "jni_message_listener_bridge.h"
#include "jni_android_platform_function.h"
#include "jni_agenui_android_platform_layout_bridge.h"
#include "jni_agenui_measurement.h"
#include "agenui_type_define.h"
#include "agenui_logger_internal.h"
#include "agenui_message_listener.h"
#include "module/agenui_engine_impl.h"
#include "module/agenui_surface_manager.h"
#include <map>
#include <mutex>
#include <string>

namespace agenui {

namespace {
    std::mutex sPlatformFunctionsMutex;
    std::map<std::string, AndroidPlatformFunction*> sPlatformFunctions;
}

static jlong jni_initAGenUIEngine(JNIEnv *env, jobject /* thiz */) {
    AGENUI_LOG("[JNI] initAGenUIEngine");
    auto *engine = initAGenUIEngine();
    ensureAndroidPlatformLayoutBridge(engine);
    registerAndroidMeasurements();
    if (!initializeAndroidMeasurementBridge(env)) {
        AGENUI_LOG("[JNI] initAGenUIEngine: initializeAndroidMeasurementBridge failed");
    }
    return (jlong)engine;
}

static void jni_destroyAGenUIEngine(JNIEnv *env, jclass jcls) {
    AGENUI_LOG("[JNI] destroyAGenUIEngine");
    destroyAGenUIEngine();
}


static jint jni_createSurfaceManager(JNIEnv *env, jclass jcls) {
    AGENUI_LOG("[JNI] createSurfaceManager");
    IAGenUIEngine* engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("[JNI] createSurfaceManager: engine is null");
        return 0;
    }
    ISurfaceManager* sm = engine->createSurfaceManager();
    if (!sm) {
        AGENUI_LOG("[JNI] createSurfaceManager: failed to create SurfaceManager");
        return 0;
    }
    int instanceId = sm->getInstanceId();
    AGENUI_LOG("[JNI] createSurfaceManager: created with instanceId=%d", instanceId);
    return (jint)instanceId;
}

static void jni_updatePlatformLayoutInfo(JNIEnv* env, jclass jcls, jint widthPx, jint heightPx, jfloat density) {
    updateAndroidPlatformDeviceInfo(static_cast<int>(widthPx), static_cast<int>(heightPx), density);
}

static void jni_destroySurfaceManager(JNIEnv *env, jclass jcls, jint instanceId) {
    AGENUI_LOG("[JNI] destroySurfaceManager: instanceId=%d", instanceId);
    IAGenUIEngine* engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("[JNI] destroySurfaceManager: engine is null");
        return;
    }
    ISurfaceManager* sm = engine->findSurfaceManager(instanceId);
    if (!sm) {
        AGENUI_LOG("[JNI] destroySurfaceManager: SurfaceManager not found for instanceId=%d", instanceId);
        return;
    }
    engine->destroySurfaceManager(sm);
    AGENUI_LOG("[JNI] destroySurfaceManager: destroyed instanceId=%d", instanceId);
}

static jboolean jni_setPathConfig(JNIEnv* env, jclass clazz, jstring jConfigJson) {
    if (jConfigJson == nullptr) {
        AGENUI_LOG("[JNI] setPathConfig: configJson is null");
        return JNI_FALSE;
    }
    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("[JNI] setPathConfig: engine is null");
        return JNI_FALSE;
    }
    ScopedUtfChars configObj(env, jConfigJson);
    std::string configJson = configObj.c_str();
    AGENUI_LOG("[JNI] setPathConfig");
    bool success = engine->setPathConfig(configJson);
    return success ? JNI_TRUE : JNI_FALSE;
}

static jboolean jni_loadThemeConfig(JNIEnv* env, jclass clazz, jstring jThemeConfig) {
    if (jThemeConfig == nullptr) {
        AGENUI_LOG("[JNI] loadThemeConfig: themeConfig is null");
        return JNI_FALSE;
    }
    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("[JNI] loadThemeConfig: engine is null");
        return JNI_FALSE;
    }
    ScopedUtfChars themeConfigObj(env, jThemeConfig);
    std::string themeConfig = themeConfigObj.c_str();
    std::string result;
    AGENUI_LOG("[JNI] loadThemeConfig");
    bool success = engine->loadThemeConfig(themeConfig, result);
    if (!success) {
        AGENUI_LOG("[JNI] loadThemeConfig failed: %s", result.c_str());
    }
    return success ? JNI_TRUE : JNI_FALSE;
}

static jboolean jni_loadDesignTokenConfig(JNIEnv* env, jclass clazz, jstring jDesignTokenConfig) {
    if (jDesignTokenConfig == nullptr) {
        AGENUI_LOG("[JNI] loadDesignTokenConfig: designTokenConfig is null");
        return JNI_FALSE;
    }
    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("[JNI] loadDesignTokenConfig: engine is null");
        return JNI_FALSE;
    }
    ScopedUtfChars designTokenConfigObj(env, jDesignTokenConfig);
    std::string designTokenConfig = designTokenConfigObj.c_str();
    std::string result;
    AGENUI_LOG("[JNI] loadDesignTokenConfig");
    bool success = engine->loadDesignTokenConfig(designTokenConfig, result);
    if (!success) {
        AGENUI_LOG("[JNI] loadDesignTokenConfig failed: %s", result.c_str());
    }
    return success ? JNI_TRUE : JNI_FALSE;
}

static void jni_setDayNightMode(JNIEnv* env, jclass clazz, jstring jMode) {
    if (jMode == nullptr) {
        AGENUI_LOG("[JNI] setDayNightMode: mode is null");
        return;
    }
    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("[JNI] setDayNightMode: engine is null");
        return;
    }
    ScopedUtfChars modeObj(env, jMode);
    std::string mode = modeObj.c_str();
    AGENUI_LOG("[JNI] setDayNightMode: mode=%s", mode.c_str());
    engine->setDayNightMode(mode);
}

static void jni_registerFunction(JNIEnv* env, jclass clazz, jstring jName, jstring jConfig, jobject javaFunction) {
    if (jName == nullptr || jConfig == nullptr || javaFunction == nullptr) {
        AGENUI_LOG("[JNI] registerFunction: invalid params");
        return;
    }

    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("[JNI] registerFunction: engine is null");
        return;
    }

    ScopedUtfChars nameObj(env, jName);
    std::string name = nameObj.c_str();

    ScopedUtfChars configObj(env, jConfig);
    std::string config = configObj.c_str();

    auto* function = new AndroidPlatformFunction(env, javaFunction);

    bool registered = engine->registerFunction(config, function);
    if (!registered) {
        // Registration failed: engine did not take ownership, release here to avoid leak
        AGENUI_LOG("[JNI] registerFunction: engine reject, release function, name=%s", name.c_str());
        delete function;
        return;
    }

    // Record in JNI-layer map only after successful registration, for lifecycle management
    {
        std::lock_guard<std::mutex> lock(sPlatformFunctionsMutex);
        auto it = sPlatformFunctions.find(name);
        if (it != sPlatformFunctions.end()) {
            // Old function was overridden in the engine layer; safely release the old instance
            delete it->second;
        }
        sPlatformFunctions[name] = function;
    }

    AGENUI_LOG("[JNI] registerFunction: name=%s", name.c_str());
}

static void jni_unregisterFunction(JNIEnv* env, jclass clazz, jstring jName) {
    if (jName == nullptr) {
        AGENUI_LOG("[JNI] unregisterFunction: name is null");
        return;
    }
    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("[JNI] unregisterFunction: engine is null");
        return;
    }
    ScopedUtfChars nameObj(env, jName);
    std::string name = nameObj.c_str();
    AGENUI_LOG("[JNI] unregisterFunction: name=%s", name.c_str());
    bool unregistered = engine->unregisterFunction(name);

    // Only destroy the JNI-layer instance after the engine successfully unregisters it
    if (!unregistered) {
        AGENUI_LOG("[JNI] unregisterFunction: engine not registered, skip destroy, name=%s", name.c_str());
        return;
    }
    {
        std::lock_guard<std::mutex> lock(sPlatformFunctionsMutex);
        auto it = sPlatformFunctions.find(name);
        if (it != sPlatformFunctions.end()) {
            delete it->second;
            sPlatformFunctions.erase(it);
            AGENUI_LOG("[JNI] unregisterFunction: destroyed AndroidPlatformFunction for %s", name.c_str());
        }
    }
}

static jstring jni_getVersion(JNIEnv *env, jclass /* clazz */) {
    return env->NewStringUTF(agenui::getAGenUIVersion());
}

jint register_jni_AGenUIEngine(JNIEnv* env) {
    AGENUI_LOG("[JNI] register_jni_AGenUIEngine");
    // Register all methods for AGenUI class
    ScopedLocalRef<jclass> engineClz(env, env->FindClass("com/amap/agenui/AGenUI"));
    if (engineClz.get() == nullptr) {
        AGENUI_LOG("[JNI] register_jni_AGenUIEngine: AGenUI class not found");
        return JNI_ERR;
    }
    
    JNINativeMethod nativeMethods[] = {
        // Engine lifecycle
        {"nativeInitAGenUIEngine", "()J", (void *) jni_initAGenUIEngine},
        {"nativeDestroyAGenUIEngine", "()V", (void *) jni_destroyAGenUIEngine},
        // SurfaceManager lifecycle
        {"nativeCreateSurfaceManager", "()I", (void *) jni_createSurfaceManager},
        {"nativeDestroySurfaceManager", "(I)V", (void *) jni_destroySurfaceManager},
        // Engine-level methods
        {"nativeSetPathConfig", "(Ljava/lang/String;)Z", (void*)jni_setPathConfig},
        {"nativeUpdatePlatformLayoutInfo", "(IIF)V", (void*)jni_updatePlatformLayoutInfo},
        {"nativeLoadThemeConfig", "(Ljava/lang/String;)Z", (void*)jni_loadThemeConfig},
        {"nativeLoadDesignTokenConfig", "(Ljava/lang/String;)Z", (void*)jni_loadDesignTokenConfig},
        {"nativeSetDayNightMode", "(Ljava/lang/String;)V", (void*)jni_setDayNightMode},
        // Function registration
        {"nativeRegisterFunction", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)V", (void*)jni_registerFunction},
        {"nativeUnregisterFunction", "(Ljava/lang/String;)V", (void*)jni_unregisterFunction},
        // Version
        {"nativeGetVersion", "()Ljava/lang/String;", (void*)jni_getVersion},
    };
    
    jint result = env->RegisterNatives(engineClz.get(), nativeMethods, sizeof(nativeMethods) / sizeof(nativeMethods[0]));
    if (result != JNI_OK) {
        AGENUI_LOG("[JNI] register_jni_AGenUIEngine: RegisterNatives failed");
        return JNI_ERR;
    }
    
    return JNI_OK;
}

}
