#include "jni_scoped_local_ref.h"
#include <jni.h>
#include "jni_scoped_utf_chars.h"
#include "agenui_engine_entry.h"
#include "agenui_dispatcher_types.h"
#include "jni_message_listener_bridge.h"
#include "agenui_type_define.h"
#include "agenui_logger_internal.h"
#include "agenui_message_listener.h"
#include "module/agenui_engine_impl.h"
#include "module/agenui_surface_manager.h"

namespace agenui {

static ISurfaceManager* findSurfaceManagerByInstanceId(jint instanceId) {
    IAGenUIEngine* engine = getAGenUIEngine();
    if (!engine) {
        AGENUI_LOG("[JNI] findSurfaceManagerByInstanceId: engine is null");
        return nullptr;
    }
    ISurfaceManager* sm = engine->findSurfaceManager(instanceId);
    if (!sm) {
        AGENUI_LOG("[JNI] findSurfaceManagerByInstanceId: SurfaceManager not found for instanceId=%d", instanceId);
    }
    return sm;
}

static void jni_addEventListener(JNIEnv* env, jclass clazz, jint instanceId, jobject javaListener) {
    AGENUI_LOG("[JNI] addEventListener: instanceId=%d", instanceId);
    if (javaListener == nullptr) {
        AGENUI_LOG("[JNI] addEventListener: listener is null");
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

    AGENUI_LOG("[JNI] addEventListener: success, instanceId=%d", instanceId);
}

static void jni_removeEventListener(JNIEnv* env, jclass clazz, jint instanceId, jobject javaListener) {
    AGENUI_LOG("[JNI] removeEventListener: instanceId=%d", instanceId);
    if (javaListener == nullptr) {
        AGENUI_LOG("[JNI] removeEventListener: listener is null");
        return;
    }
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        AGENUI_LOG("%d does not exist", instanceId);
        return;
    }
    
    // Find bridge
    auto* bridge = ListenerBridgeManager::getInstance().findBridge(env, javaListener);
    if (bridge == nullptr) {
        AGENUI_LOG("[JNI] removeEventListener: bridge not found");
        return;
    }
    
    // Remove from SurfaceManager
    surfaceManager->removeSurfaceEventListener(bridge);
    
    // Clean up
    ListenerBridgeManager::getInstance().removeMapping(env, javaListener);
    SAFELY_DELETE(bridge);
}

static void jni_submitUIAction(JNIEnv* env, jclass clazz, jint instanceId, jstring jSurfaceId, jstring jSourceComponentId, jstring jContextJson) {
    AGENUI_LOG("[JNI] submitUIAction: instanceId=%d", instanceId);
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
    
    AGENUI_LOG("[JNI] submitUIDataModel: instanceId=%d, surfaceId=%s, componentId=%s", instanceId, msg.surfaceId.c_str(), msg.componentId.c_str());

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
        AGENUI_LOG("[JNI] receiveTextChunk: content is null");
        return;
    }
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }
    ScopedUtfChars contentObj(env, jContent);
    std::string inputContent = contentObj.c_str();
    surfaceManager->receiveTextChunk(inputContent);
}

static void jni_endTextStream(JNIEnv* env, jclass clazz, jint instanceId) {
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }
    surfaceManager->endTextStream();
}

static void jni_notifyRenderFinish(JNIEnv* env, jclass clazz, jint instanceId,
                                   jstring jSurfaceId, jstring jComponentId, jstring jType,
                                   jfloat width, jfloat height, jint selectedIndex) {
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }

    ComponentRenderInfo info;
    if (jSurfaceId != nullptr) {
        ScopedUtfChars surfaceId(env, jSurfaceId);
        info.surfaceId = surfaceId.c_str();
    }
    if (jComponentId != nullptr) {
        ScopedUtfChars componentId(env, jComponentId);
        info.componentId = componentId.c_str();
    }
    if (jType != nullptr) {
        ScopedUtfChars type(env, jType);
        info.type = type.c_str();
    }
    info.width = width;
    info.height = height;
    info.selectedIndex = selectedIndex;

    AGENUI_LOG("[JNI] notifyRenderFinish: instanceId=%d, surfaceId=%s, componentId=%s, type=%s, width=%.1f, height=%.1f, selectedIndex=%d",
               instanceId,
               info.surfaceId.c_str(),
               info.componentId.c_str(),
               info.type.c_str(),
               info.width,
               info.height,
               info.selectedIndex);
    surfaceManager->onRenderFinish(info);
}

static void jni_notifySurfaceSizeChanged(JNIEnv* env, jclass clazz, jint instanceId,
                                         jstring jSurfaceId, jfloat width, jfloat height) {
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }
    
    SurfaceLayoutInfo info;
    if (jSurfaceId != nullptr) {
        ScopedUtfChars surfaceId(env, jSurfaceId);
        info.surfaceId = surfaceId.c_str();
    }
    info.width = width;
    info.height = height;
    surfaceManager->onSurfaceSizeChanged(info);
}

static void jni_invalidateFunctionCallValues(JNIEnv* env, jclass clazz, jint instanceId) {
    ISurfaceManager* surfaceManager = findSurfaceManagerByInstanceId(instanceId);
    if (!surfaceManager) {
        return;
    }
    surfaceManager->invalidateFunctionCallValues();
}

jint register_jni_AGenUISurfaceManager(JNIEnv* env) {
    AGENUI_LOG("[JNI] register_jni_AGenUIEngine");
    // Register all methods for AGenUI class
    ScopedLocalRef<jclass> engineClz(env, env->FindClass("com/amap/agenui/render/surface/SurfaceManager"));
    if (engineClz.get() == nullptr) {
        AGENUI_LOG("[JNI] register_jni_AGenUIEngine: AGenUI class not found");
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
        {"nativeNotifyRenderFinish", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;FFI)V", (void*)jni_notifyRenderFinish},
        {"nativeNotifySurfaceSizeChanged", "(ILjava/lang/String;FF)V", (void*)jni_notifySurfaceSizeChanged},
        // SurfaceManager FunctionCall value invalidation
        {"nativeInvalidateFunctionCallValues", "(I)V", (void*)jni_invalidateFunctionCallValues},
    };
    
    jint result = env->RegisterNatives(engineClz.get(), nativeMethods, sizeof(nativeMethods) / sizeof(nativeMethods[0]));
    if (result != JNI_OK) {
        AGENUI_LOG("[JNI] register_jni_AGenUIEngine: RegisterNatives failed");
        return JNI_ERR;
    }
    
    return JNI_OK;
}

}
