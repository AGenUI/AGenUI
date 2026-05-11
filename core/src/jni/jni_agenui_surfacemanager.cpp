#include "jni_scoped_local_ref.h"
#include <jni.h>
#include "jni_scoped_utf_chars.h"
#include "agenui_engine_entry.h"
#include "agenui_dispatcher_types.h"
#include "jni_message_listener_bridge.h"
#include "agenui_type_define.h"
#include "agenui_log.h"
#include "agenui_message_listener.h"
#include "module/agenui_engine_impl.h"
#include "module/agenui_surface_manager.h"

namespace agenui {

static ISurfaceManager* findSurfaceManagerByInstanceId(jint instanceId) {
    IAGenUIEngine* engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("engine is null");
        return nullptr;
    }
    ISurfaceManager* sm = engine->findSurfaceManager(instanceId);
    if (!sm) {
        AGENUI_LOG("SurfaceManager not found for instanceId:%d", instanceId);
    }
    return sm;
}

static void jni_addEventListener(JNIEnv* env, jclass clazz, jint instanceId, jobject javaListener) {
    AGENUI_LOG("instanceId:%d", instanceId);
    if (javaListener == nullptr) {
        AGENUI_LOG("listener is null");
        return;
    }
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }
    
    // Create bridge object
    auto* bridge = new JNIMessageListenerBridge(env, javaListener);
    
    // Register to SurfaceManager
    surfaceManager->addSurfaceEventListener(bridge);
    
    // Save mapping
    ListenerBridgeManager::getInstance().addMapping(env, javaListener, bridge);

    AGENUI_LOG("success, instanceId:%d", instanceId);
}

static void jni_removeEventListener(JNIEnv* env, jclass clazz, jint instanceId, jobject javaListener) {
    AGENUI_LOG("instanceId:%d", instanceId);
    if (javaListener == nullptr) {
        AGENUI_LOG("listener is null");
        return;
    }
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }
    
    // Find bridge
    auto* bridge = ListenerBridgeManager::getInstance().findBridge(env, javaListener);
    if (bridge == nullptr) {
        AGENUI_LOG("bridge not found");
        return;
    }
    
    // Remove from SurfaceManager
    surfaceManager->removeSurfaceEventListener(bridge);
    
    // Clean up
    ListenerBridgeManager::getInstance().removeMapping(env, javaListener);
    SAFELY_DELETE(bridge);
}

static void jni_submitUIAction(JNIEnv* env, jclass clazz, jint instanceId, jstring jSurfaceId, jstring jSourceComponentId, jstring jContextJson) {
    AGENUI_LOG("instanceId:%d", instanceId);
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }
    ActionMessage msg;
    
    if (jSurfaceId != nullptr) {
        ScopedUtfChars surfaceId(env, jSurfaceId);
        msg.surfaceId = surfaceId.c_str();
    }
    
    if (jSourceComponentId != nullptr) {
        ScopedUtfChars sourceComponentId(env, jSourceComponentId);
        msg.sourceComponentId = sourceComponentId.c_str();
    }
    
    if (jContextJson != nullptr) {
        ScopedUtfChars contextJson(env, jContextJson);
        msg.contextJson = contextJson.c_str();
    }

    surfaceManager->submitUIAction(msg);
}

static void jni_submitUIDataModel(JNIEnv* env, jclass clazz, jint instanceId, jstring jSurfaceId, jstring jComponentId, jstring jChange) {
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }
    SyncUIToDataMessage msg;
    if (jSurfaceId != nullptr) {
        ScopedUtfChars surfaceId(env, jSurfaceId);
        msg.surfaceId = surfaceId.c_str();
    }
    
    if (jComponentId != nullptr) {
        ScopedUtfChars componentId(env, jComponentId);
        msg.componentId = componentId.c_str();
    }
    
    if (jChange != nullptr) {
        ScopedUtfChars change(env, jChange);
        msg.change = change.c_str();
    }
    
    AGENUI_LOG("instanceId:%d, surfaceId:%s, componentId:%s", instanceId, msg.surfaceId.c_str(), msg.componentId.c_str());

    surfaceManager->submitUIDataModel(msg);
}

static void jni_beginTextStream(JNIEnv* env, jclass clazz, jint instanceId) {
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }
    surfaceManager->beginTextStream();
}

static void jni_receiveTextChunk(JNIEnv* env, jclass clazz, jint instanceId, jstring jContent) {
    if (jContent == nullptr) {
        AGENUI_LOG("content is null");
        return;
    }
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }
    ScopedUtfChars contentObj(env, jContent);
    std::string inputContent = contentObj.c_str();
    AGENUI_LOG("instanceId:%d, content:%s", instanceId, inputContent.c_str());
    surfaceManager->receiveTextChunk(inputContent);
}

static void jni_endTextStream(JNIEnv* env, jclass clazz, jint instanceId) {
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }
    surfaceManager->endTextStream();
}

jint register_jni_AGenUISurfaceManager(JNIEnv* env) {
    // Register all methods for AGenUI class
    ScopedLocalRef<jclass> engineClz(env, env->FindClass("com/amap/agenui/render/surface/SurfaceManager"));
    if (engineClz.get() == nullptr) {
        AGENUI_LOG("SurfaceManager class not found");
        return JNI_ERR;
    }
    
    JNINativeMethod nativeMethods[] = {
        // SurfaceManager event listener
        {"nativeAddEventListener", "(ILcom/amap/agenui/IAGenUIMessageListener;)V", (void*)jni_addEventListener},
        {"nativeRemoveEventListener", "(ILcom/amap/agenui/IAGenUIMessageListener;)V", (void*)jni_removeEventListener},
        // SurfaceManager UI interaction
        {"nativeSubmitUIAction", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", (void*)jni_submitUIAction},
        {"nativeSubmitUIDataModel", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", (void*)jni_submitUIDataModel},
        // SurfaceManager data input
        {"nativeBeginTextStream", "(I)V", (void*)jni_beginTextStream},
        {"nativeReceiveTextChunk", "(ILjava/lang/String;)V", (void*)jni_receiveTextChunk},
        {"nativeEndTextStream", "(I)V", (void*)jni_endTextStream},
    };
    
    jint result = env->RegisterNatives(engineClz.get(), nativeMethods, sizeof(nativeMethods) / sizeof(nativeMethods[0]));
    if (result != JNI_OK) {
        AGENUI_LOG("RegisterNatives failed");
        return JNI_ERR;
    }
    
    return JNI_OK;
}

}
