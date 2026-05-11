#include "jni_scoped_local_ref.h"
#include <jni.h>
#include "jni_scoped_utf_chars.h"
#include "agenui_engine_entry.h"
#include "agenui_dispatcher_types.h"
#include "agenui_platform_function.h"
#include "jni_message_listener_bridge.h"
#include "jni_android_platform_function.h"
#include "agenui_type_define.h"
#include "agenui_log.h"
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

static jlong jni_initAGenUIEngine(JNIEnv *env, jclass jcls) {
    auto *engine = initAGenUIEngine();
    return (jlong)engine;
}

static void jni_destroyAGenUIEngine(JNIEnv *env, jclass jcls) {
    destroyAGenUIEngine();
}


static jint jni_createSurfaceManager(JNIEnv *env, jclass jcls) {
    IAGenUIEngine* engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("engine is null");
        return 0;
    }
    ISurfaceManager* sm = engine->createSurfaceManager();
    if (!sm) {
        AGENUI_LOG("failed to create SurfaceManager");
        return 0;
    }
    int instanceId = sm->getInstanceId();
    AGENUI_LOG("created with instanceId:%d", instanceId);
    return (jint)instanceId;
}

static void jni_destroySurfaceManager(JNIEnv *env, jclass jcls, jint instanceId) {
    AGENUI_LOG("instanceId:%d", instanceId);
    IAGenUIEngine* engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("engine is null");
        return;
    }
    ISurfaceManager* sm = engine->findSurfaceManager(instanceId);
    if (!sm) {
        AGENUI_LOG("SurfaceManager not found for instanceId:%d", instanceId);
        return;
    }
    engine->destroySurfaceManager(sm);
}

static void jni_setWorkdir(JNIEnv* env, jclass clazz, jstring jworkDir) {
    if (jworkDir == nullptr) {
        AGENUI_LOG("workDir is null");
        return;
    }
    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        return;
    }
    ScopedUtfChars contentObj(env, jworkDir);
    std::string workDir = contentObj.c_str();
    AGENUI_LOG("%s", workDir.c_str());
    engine->setWorkingDir(workDir);
}

static jboolean jni_loadThemeConfig(JNIEnv* env, jclass clazz, jstring jThemeConfig) {
    if (jThemeConfig == nullptr) {
        AGENUI_LOG("themeConfig is null");
        return JNI_FALSE;
    }
    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("engine is null");
        return JNI_FALSE;
    }
    ScopedUtfChars themeConfigObj(env, jThemeConfig);
    std::string themeConfig = themeConfigObj.c_str();
    std::string result;
    bool success = engine->loadThemeConfig(themeConfig, result);
    if (!success) {
        AGENUI_LOG("failed: %s", result.c_str());
    }
    return success ? JNI_TRUE : JNI_FALSE;
}

static jboolean jni_loadDesignTokenConfig(JNIEnv* env, jclass clazz, jstring jDesignTokenConfig) {
    if (jDesignTokenConfig == nullptr) {
        AGENUI_LOG("designTokenConfig is null");
        return JNI_FALSE;
    }
    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("engine is null");
        return JNI_FALSE;
    }
    ScopedUtfChars designTokenConfigObj(env, jDesignTokenConfig);
    std::string designTokenConfig = designTokenConfigObj.c_str();
    std::string result;
    bool success = engine->loadDesignTokenConfig(designTokenConfig, result);
    if (!success) {
        AGENUI_LOG("failed: %s", result.c_str());
    }
    return success ? JNI_TRUE : JNI_FALSE;
}

static void jni_setDayNightMode(JNIEnv* env, jclass clazz, jstring jMode) {
    if (jMode == nullptr) {
        AGENUI_LOG("mode is null");
        return;
    }
    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("engine is null");
        return;
    }
    ScopedUtfChars modeObj(env, jMode);
    std::string mode = modeObj.c_str();
    engine->setDayNightMode(mode);
}

static void jni_registerFunction(JNIEnv* env, jclass clazz, jstring jName, jstring jConfig, jobject javaFunction) {
    if (jName == nullptr || jConfig == nullptr || javaFunction == nullptr) {
        AGENUI_LOG("invalid params");
        return;
    }

    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("engine is null");
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
        AGENUI_LOG("engine reject, release function, name:%s", name.c_str());
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
}

static void jni_onAsyncCallbackResult(JNIEnv* env, jclass clazz, jlong callbackPtr,
                                       jint status, jstring jData, jstring jError) {
    auto* callback = reinterpret_cast<FunctionCallCallback*>(callbackPtr);
    if (callback == nullptr) {
        AGENUI_LOG("callback is null");
        return;
    }

    // Double-invoke guard: check and consume the callback pointer
    if (!consumeAsyncCallback(callback)) {
        AGENUI_LOG("callback already consumed or invalid, ptr:%lld",
                   (long long)callbackPtr);
        return;
    }

    FunctionCallResult result;
    result.status = static_cast<FunctionCallStatus>(status);

    if (jData != nullptr) {
        ScopedUtfChars dataChars(env, jData);
        result.data = dataChars.c_str();
    }
    if (jError != nullptr) {
        ScopedUtfChars errorChars(env, jError);
        result.error = errorChars.c_str();
    }

    (*callback)(result);
    delete callback;  // one-shot callback, release after use
}

static void jni_unregisterFunction(JNIEnv* env, jclass clazz, jstring jName) {
    if (jName == nullptr) {
        AGENUI_LOG("name is null");
        return;
    }
    IAGenUIEngine *engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("engine is null");
        return;
    }
    ScopedUtfChars nameObj(env, jName);
    std::string name = nameObj.c_str();
    AGENUI_LOG("name:%s", name.c_str());
    bool unregistered = engine->unregisterFunction(name);

    // Only destroy the JNI-layer instance after the engine successfully unregisters it
    if (!unregistered) {
        AGENUI_LOG("engine not registered, skip destroy, name:%s", name.c_str());
        return;
    }
    {
        std::lock_guard<std::mutex> lock(sPlatformFunctionsMutex);
        auto it = sPlatformFunctions.find(name);
        if (it != sPlatformFunctions.end()) {
            delete it->second;
            sPlatformFunctions.erase(it);
            AGENUI_LOG("destroyed AndroidPlatformFunction for %s", name.c_str());
        }
    }
}

jint register_jni_AGenUIEngine(JNIEnv* env) {
    // Register all methods for AGenUI class
    ScopedLocalRef<jclass> engineClz(env, env->FindClass("com/amap/agenui/AGenUI"));
    if (engineClz.get() == nullptr) {
        AGENUI_LOG("AGenUI class not found");
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
        {"nativeSetWorkdir", "(Ljava/lang/String;)V", (void*)jni_setWorkdir},
        {"nativeLoadThemeConfig", "(Ljava/lang/String;)Z", (void*)jni_loadThemeConfig},
        {"nativeLoadDesignTokenConfig", "(Ljava/lang/String;)Z", (void*)jni_loadDesignTokenConfig},
        {"nativeSetDayNightMode", "(Ljava/lang/String;)V", (void*)jni_setDayNightMode},
        // Function registration
        {"nativeRegisterFunction", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)V", (void*)jni_registerFunction},
        {"nativeUnregisterFunction", "(Ljava/lang/String;)V", (void*)jni_unregisterFunction},
        // Async callback
        {"nativeOnAsyncCallbackResult", "(JILjava/lang/String;Ljava/lang/String;)V", (void*)jni_onAsyncCallbackResult},
    };
    
    jint result = env->RegisterNatives(engineClz.get(), nativeMethods, sizeof(nativeMethods) / sizeof(nativeMethods[0]));
    if (result != JNI_OK) {
        AGENUI_LOG("RegisterNatives failed");
        return JNI_ERR;
    }
    
    return JNI_OK;
}

}
