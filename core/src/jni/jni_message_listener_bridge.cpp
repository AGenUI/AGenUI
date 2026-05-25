#include "jni_message_listener_bridge.h"
#include "jni_scoped_local_ref.h"
#include "jni_scoped_utf_chars.h"
#include "agenui_logger_internal.h"
#include <sstream>

namespace agenui {

namespace {

jobjectArray createJavaStringArray(JNIEnv* env, const std::vector<std::string>& values) {
    ScopedLocalRef<jclass> stringClass(env, env->FindClass("java/lang/String"));
    if (stringClass.get() == nullptr) {
        return nullptr;
    }

    jobjectArray array = env->NewObjectArray(static_cast<jsize>(values.size()), stringClass.get(), nullptr);
    if (array == nullptr) {
        return nullptr;
    }

    for (size_t i = 0; i < values.size(); ++i) {
        ScopedLocalRef<jstring> item(env, env->NewStringUTF(values[i].c_str()));
        env->SetObjectArrayElement(array, static_cast<jsize>(i), item.get());
    }
    return array;
}

}  // namespace

class JNIEnvGuard {
public:
    explicit JNIEnvGuard(JavaVM* jvm) : _jvm(jvm), _env(nullptr), _needsDetach(false) {
        if (!_jvm) return;
        jint result = _jvm->GetEnv((void**)&_env, JNI_VERSION_1_6);
        if (result == JNI_OK) {
            return;
        }
        if (result == JNI_EDETACHED) {
            AGENUI_LOG("[JNIEnvGuard] Thread not attached, attaching...");
            if (_jvm->AttachCurrentThread(&_env, nullptr) == JNI_OK) {
                _needsDetach = true;
            } else {
                AGENUI_LOG("[JNIEnvGuard] AttachCurrentThread failed");
                _env = nullptr;
            }
        } else {
            AGENUI_LOG("[JNIEnvGuard] GetEnv failed with result: %d", result);
        }
    }
    ~JNIEnvGuard() {
        if (_needsDetach && _jvm) {
            _jvm->DetachCurrentThread();
        }
    }
    JNIEnv* env() const { return _env; }
    JNIEnvGuard(const JNIEnvGuard&) = delete;
    JNIEnvGuard& operator=(const JNIEnvGuard&) = delete;
private:
    JavaVM* _jvm;
    JNIEnv* _env;
    bool _needsDetach;
};

JNIMessageListenerBridge::JNIMessageListenerBridge(JNIEnv* env, jobject javaListener)
    : _jvm(nullptr), _javaListener(nullptr), _onCreateSurfaceMethod(nullptr),
      _onComponentsUpdateMethod(nullptr),
      _onComponentsAddMethod(nullptr), _onComponentsRemoveMethod(nullptr),
      _onDeleteSurfaceMethod(nullptr), _onErrorMethod(nullptr) {

    env->GetJavaVM(&_jvm);

    _javaListener = env->NewGlobalRef(javaListener);

    ScopedLocalRef<jclass> listenerClass(env, env->GetObjectClass(javaListener));
    if (listenerClass.get() == nullptr) {
        AGENUI_LOG("JNIMessageListenerBridge: failed to get listener class");
        return;
    }

    // Cache method IDs
    _onCreateSurfaceMethod = env->GetMethodID(listenerClass.get(), "onCreateSurface", "(Ljava/lang/String;Ljava/lang/String;Ljava/util/Map;ZZLjava/lang/String;)V");
    _onComponentsUpdateMethod = env->GetMethodID(listenerClass.get(), "onComponentsUpdate", "(Ljava/lang/String;[Ljava/lang/String;)V");
    _onComponentsAddMethod = env->GetMethodID(listenerClass.get(), "onComponentsAdd", "(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)V");
    _onComponentsRemoveMethod = env->GetMethodID(listenerClass.get(), "onComponentsRemove", "(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)V");
    _onDeleteSurfaceMethod = env->GetMethodID(listenerClass.get(), "onDeleteSurface", "(Ljava/lang/String;)V");
    _onActionEventRouted = env->GetMethodID(listenerClass.get(), "onActionEventRouted", "(Ljava/lang/String;)V");
    _onErrorMethod = env->GetMethodID(listenerClass.get(), "onError", "(ILjava/lang/String;Ljava/lang/String;)V");
    
    if (_onCreateSurfaceMethod == nullptr ||
        _onComponentsUpdateMethod == nullptr || _onComponentsAddMethod == nullptr ||
        _onComponentsRemoveMethod == nullptr || _onDeleteSurfaceMethod == nullptr ||
            _onActionEventRouted == nullptr ||
        _onErrorMethod == nullptr) {
        AGENUI_LOG("JNIMessageListenerBridge: failed to get method IDs");
    }
}

JNIMessageListenerBridge::~JNIMessageListenerBridge() {
    if (_javaListener != nullptr && _jvm != nullptr) {
        JNIEnvGuard envGuard(_jvm);
        JNIEnv* env = envGuard.env();
        if (env != nullptr) {
            env->DeleteGlobalRef(_javaListener);
        }
    }
}

void JNIMessageListenerBridge::onCreateSurface(const CreateSurfaceMessage& msg) {
    if (_jvm == nullptr || _javaListener == nullptr || _onCreateSurfaceMethod == nullptr) {
        return;
    }
    
    JNIEnvGuard envGuard(_jvm);
    JNIEnv* env = envGuard.env();
    if (env == nullptr) {
        AGENUI_LOG("[JNI] onCreateSurface: failed to acquire JNIEnv");
        return;
    }
    
    ScopedLocalRef<jstring> jSurfaceId(env, env->NewStringUTF(msg.surfaceId.c_str()));
    ScopedLocalRef<jstring> jCatalogId(env, env->NewStringUTF(msg.catalogId.c_str()));
    ScopedLocalRef<jobject> jTheme(env, createJavaHashMap(env, msg.theme));
    jboolean jSendDataModel = msg.sendDataModel ? JNI_TRUE : JNI_FALSE;
    jboolean jAnimated = msg.animated ? JNI_TRUE : JNI_FALSE;
    ScopedLocalRef<jstring> jRawProtocolContent(env, env->NewStringUTF(msg.rawProtocolContent.c_str()));

    env->CallVoidMethod(_javaListener, _onCreateSurfaceMethod, jSurfaceId.get(), jCatalogId.get(), jTheme.get(), jSendDataModel, jAnimated, jRawProtocolContent.get());
}

void JNIMessageListenerBridge::onComponentsUpdate(
        const std::string& surfaceId,
        const std::vector<ComponentsUpdateMessage>& msg) {
    if (_jvm == nullptr || _javaListener == nullptr || _onComponentsUpdateMethod == nullptr) {
        return;
    }

    JNIEnvGuard envGuard(_jvm);
    JNIEnv* env = envGuard.env();
    if (env == nullptr) {
        AGENUI_LOG("[JNI] onComponentsUpdate: failed to acquire JNIEnv");
        return;
    }

    std::vector<std::string> components;
    components.reserve(msg.size());
    for (const auto& item : msg) {
        components.emplace_back(item.component);
    }

    ScopedLocalRef<jstring> jSurfaceId(env, env->NewStringUTF(surfaceId.c_str()));
    ScopedLocalRef<jobjectArray> jComponentsArray(env, createJavaStringArray(env, components));
    if (jComponentsArray.get() == nullptr) {
        return;
    }

    env->CallVoidMethod(_javaListener, _onComponentsUpdateMethod, jSurfaceId.get(), jComponentsArray.get());
}

void JNIMessageListenerBridge::onComponentsAdd(
        const std::string& surfaceId,
        const std::vector<ComponentsAddMessage>& msg) {
    if (_jvm == nullptr || _javaListener == nullptr || _onComponentsAddMethod == nullptr) {
        return;
    }

    JNIEnvGuard envGuard(_jvm);
    JNIEnv* env = envGuard.env();
    if (env == nullptr) {
        AGENUI_LOG("[JNI] onComponentsAdd: failed to acquire JNIEnv");
        return;
    }

    std::vector<std::string> parentIds;
    std::vector<std::string> components;
    parentIds.reserve(msg.size());
    components.reserve(msg.size());
    for (const auto& item : msg) {
        parentIds.emplace_back(item.parentId);
        components.emplace_back(item.component);
    }

    ScopedLocalRef<jstring> jSurfaceId(env, env->NewStringUTF(surfaceId.c_str()));
    ScopedLocalRef<jobjectArray> jParentIdsArray(env, createJavaStringArray(env, parentIds));
    ScopedLocalRef<jobjectArray> jComponentsArray(env, createJavaStringArray(env, components));
    if (jParentIdsArray.get() == nullptr || jComponentsArray.get() == nullptr) {
        return;
    }

    env->CallVoidMethod(
            _javaListener,
            _onComponentsAddMethod,
            jSurfaceId.get(),
            jParentIdsArray.get(),
            jComponentsArray.get());
}

void JNIMessageListenerBridge::onComponentsRemove(
        const std::string& surfaceId,
        const std::vector<ComponentsRemoveMessage>& msg) {
    if (_jvm == nullptr || _javaListener == nullptr || _onComponentsRemoveMethod == nullptr) {
        return;
    }

    JNIEnvGuard envGuard(_jvm);
    JNIEnv* env = envGuard.env();
    if (env == nullptr) {
        AGENUI_LOG("[JNI] onComponentsRemove: failed to acquire JNIEnv");
        return;
    }

    std::vector<std::string> parentIds;
    std::vector<std::string> componentIds;
    parentIds.reserve(msg.size());
    componentIds.reserve(msg.size());
    for (const auto& item : msg) {
        parentIds.emplace_back(item.parentId);
        componentIds.emplace_back(item.componentId);
    }

    ScopedLocalRef<jstring> jSurfaceId(env, env->NewStringUTF(surfaceId.c_str()));
    ScopedLocalRef<jobjectArray> jParentIdsArray(env, createJavaStringArray(env, parentIds));
    ScopedLocalRef<jobjectArray> jComponentIdsArray(env, createJavaStringArray(env, componentIds));
    if (jParentIdsArray.get() == nullptr || jComponentIdsArray.get() == nullptr) {
        return;
    }

    env->CallVoidMethod(
            _javaListener,
            _onComponentsRemoveMethod,
            jSurfaceId.get(),
            jParentIdsArray.get(),
            jComponentIdsArray.get());
}

void JNIMessageListenerBridge::onDeleteSurface(const DeleteSurfaceMessage& msg) {
    if (_jvm == nullptr || _javaListener == nullptr || _onDeleteSurfaceMethod == nullptr) {
        return;
    }

    JNIEnvGuard envGuard(_jvm);
    JNIEnv* env = envGuard.env();
    if (env == nullptr) {
        AGENUI_LOG("[JNI] onDeleteSurface: failed to acquire JNIEnv");
        return;
    }

    ScopedLocalRef<jstring> jSurfaceId(env, env->NewStringUTF(msg.surfaceId.c_str()));

    env->CallVoidMethod(_javaListener, _onDeleteSurfaceMethod, jSurfaceId.get());
}

void JNIMessageListenerBridge::onActionEventRouted(const std::string &content) {
    if (_jvm == nullptr || _javaListener == nullptr || _onActionEventRouted == nullptr) {
        return;
    }

    JNIEnvGuard envGuard(_jvm);
    JNIEnv* env = envGuard.env();
    if (env == nullptr) {
        AGENUI_LOG("[JNI] onActionEventRouted: failed to acquire JNIEnv");
        return;
    }

    ScopedLocalRef<jstring> jContent(env, env->NewStringUTF(content.c_str()));

    env->CallVoidMethod(_javaListener, _onActionEventRouted, jContent.get());
}

void JNIMessageListenerBridge::onError(const ErrorMessage& msg) {
    if (_jvm == nullptr || _javaListener == nullptr || _onErrorMethod == nullptr) {
        return;
    }

    JNIEnvGuard envGuard(_jvm);
    JNIEnv* env = envGuard.env();
    if (env == nullptr) {
        AGENUI_LOG("[JNI] onError: failed to acquire JNIEnv");
        return;
    }

    ScopedLocalRef<jstring> jSurfaceId(env, env->NewStringUTF(msg.surfaceId.c_str()));
    ScopedLocalRef<jstring> jMessage(env, env->NewStringUTF(msg.message.c_str()));

    env->CallVoidMethod(_javaListener, _onErrorMethod, static_cast<jint>(msg.code), jSurfaceId.get(), jMessage.get());
}

jobject JNIMessageListenerBridge::createJavaHashMap(JNIEnv* env, const std::map<std::string, std::string>& map) {
    ScopedLocalRef<jclass> hashMapClass(env, env->FindClass("java/util/HashMap"));
    if (hashMapClass.get() == nullptr) {
        return nullptr;
    }
    
    jmethodID hashMapConstructor = env->GetMethodID(hashMapClass.get(), "<init>", "()V");
    if (hashMapConstructor == nullptr) {
        return nullptr;
    }
    
    ScopedLocalRef<jobject> hashMap(env, env->NewObject(hashMapClass.get(), hashMapConstructor));
    if (hashMap.get() == nullptr) {
        return nullptr;
    }
    
    jmethodID putMethod = env->GetMethodID(hashMapClass.get(), "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    if (putMethod == nullptr) {
        return nullptr;
    }
    
    for (const auto& pair : map) {
        ScopedLocalRef<jstring> jKey(env, env->NewStringUTF(pair.first.c_str()));
        ScopedLocalRef<jstring> jValue(env, env->NewStringUTF(pair.second.c_str()));
        env->CallObjectMethod(hashMap.get(), putMethod, jKey.get(), jValue.get());
    }
    
    return env->NewLocalRef(hashMap.get());
}

ListenerBridgeManager& ListenerBridgeManager::getInstance() {
    static ListenerBridgeManager instance;
    return instance;
}

void ListenerBridgeManager::addMapping(JNIEnv* env, jobject javaListener, JNIMessageListenerBridge* bridge) {
    std::lock_guard<std::mutex> lock(_mutex);
    jobject globalRef = env->NewGlobalRef(javaListener);
    _listenerMap[globalRef] = bridge;
}

JNIMessageListenerBridge* ListenerBridgeManager::findBridge(JNIEnv* env, jobject javaListener) {
    std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& pair : _listenerMap) {
        if (env->IsSameObject(pair.first, javaListener)) {
            return pair.second;
        }
    }
    return nullptr;
}

void ListenerBridgeManager::removeMapping(JNIEnv* env, jobject javaListener) {
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto it = _listenerMap.begin(); it != _listenerMap.end(); ++it) {
        if (env->IsSameObject(it->first, javaListener)) {
            env->DeleteGlobalRef(it->first);
            _listenerMap.erase(it);
            return;
        }
    }
}

} // namespace agenui
