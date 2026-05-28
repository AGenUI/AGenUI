#include "jni_surface_size_provider_bridge.h"

#include "jni_helper.h"
#include "jni_scoped_local_ref.h"
#include "agenui_logger_internal.h"

namespace agenui {

namespace {

constexpr const char* kSurfaceSizeClassName =
        "com/amap/agenui/render/surface/SurfaceSize";
constexpr const char* kGetSurfaceSizeMethodName = "getSurfaceSize";
// (Ljava/lang/String;)Lcom/amap/agenui/render/surface/SurfaceSize;
constexpr const char* kGetSurfaceSizeMethodSig =
        "(Ljava/lang/String;)Lcom/amap/agenui/render/surface/SurfaceSize;";

}  // namespace

JNISurfaceSizeProviderBridge::JNISurfaceSizeProviderBridge(JNIEnv* env, jobject javaHost)
    : _javaHost(nullptr),
      _getSurfaceSizeMethod(nullptr),
      _surfaceSizeClass(nullptr),
      _widthField(nullptr),
      _heightField(nullptr) {
    if (env == nullptr || javaHost == nullptr) {
        AGENUI_LOG("[JNISurfaceSizeProviderBridge] env or javaHost is null");
        return;
    }

    _javaHost = env->NewGlobalRef(javaHost);
    if (_javaHost == nullptr) {
        AGENUI_LOG("[JNISurfaceSizeProviderBridge] NewGlobalRef on host failed");
        return;
    }

    ScopedLocalRef<jclass> hostClassLocal(env, env->GetObjectClass(javaHost));
    if (hostClassLocal.get() == nullptr) {
        AGENUI_LOG("[JNISurfaceSizeProviderBridge] failed to get host class");
        return;
    }

    _getSurfaceSizeMethod = env->GetMethodID(
            hostClassLocal.get(),
            kGetSurfaceSizeMethodName,
            kGetSurfaceSizeMethodSig);
    if (_getSurfaceSizeMethod == nullptr) {
        env->ExceptionClear();
        AGENUI_LOG("[JNISurfaceSizeProviderBridge] %s method not found", kGetSurfaceSizeMethodName);
        return;
    }

    ScopedLocalRef<jclass> surfaceSizeClassLocal(env, env->FindClass(kSurfaceSizeClassName));
    if (surfaceSizeClassLocal.get() == nullptr) {
        env->ExceptionClear();
        AGENUI_LOG("[JNISurfaceSizeProviderBridge] %s class not found", kSurfaceSizeClassName);
        return;
    }

    jclass surfaceSizeGlobal =
            reinterpret_cast<jclass>(env->NewGlobalRef(surfaceSizeClassLocal.get()));
    if (surfaceSizeGlobal == nullptr) {
        AGENUI_LOG("[JNISurfaceSizeProviderBridge] NewGlobalRef on SurfaceSize failed");
        return;
    }
    _surfaceSizeClass = surfaceSizeGlobal;

    _widthField = env->GetFieldID(_surfaceSizeClass, "width", "F");
    _heightField = env->GetFieldID(_surfaceSizeClass, "height", "F");
    if (_widthField == nullptr || _heightField == nullptr) {
        env->ExceptionClear();
        AGENUI_LOG("[JNISurfaceSizeProviderBridge] SurfaceSize.width/height field not found");
    }
}

JNISurfaceSizeProviderBridge::~JNISurfaceSizeProviderBridge() {
    JNIEnv* env = JNIHelper::getJNIEnv();
    if (env == nullptr) {
        AGENUI_LOG("[JNISurfaceSizeProviderBridge] dtor: failed to acquire JNIEnv");
        return;
    }
    if (_surfaceSizeClass != nullptr) {
        env->DeleteGlobalRef(_surfaceSizeClass);
        _surfaceSizeClass = nullptr;
    }
    if (_javaHost != nullptr) {
        env->DeleteGlobalRef(_javaHost);
        _javaHost = nullptr;
    }
}

SurfaceSize JNISurfaceSizeProviderBridge::getSurfaceSize(const std::string& surfaceId) {
    // Treat any uninitialized field as "not measurable yet" — cheaper than crashing,
    // and the engine policy already accepts {0, 0} as the bootstrap fallback.
    if (_javaHost == nullptr || _getSurfaceSizeMethod == nullptr ||
        _widthField == nullptr || _heightField == nullptr) {
        return SurfaceSize{0.0f, 0.0f};
    }

    JNIEnv* env = JNIHelper::getJNIEnv();
    if (env == nullptr) {
        AGENUI_LOG("[JNISurfaceSizeProviderBridge] getSurfaceSize: failed to acquire JNIEnv");
        return SurfaceSize{0.0f, 0.0f};
    }

    ScopedLocalRef<jstring> jSurfaceId(env, env->NewStringUTF(surfaceId.c_str()));
    if (jSurfaceId.get() == nullptr) {
        env->ExceptionClear();
        return SurfaceSize{0.0f, 0.0f};
    }

    ScopedLocalRef<jobject> resultObj(
            env,
            env->CallObjectMethod(_javaHost, _getSurfaceSizeMethod, jSurfaceId.get()));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        AGENUI_LOG("[JNISurfaceSizeProviderBridge] getSurfaceSize: Java threw, surfaceId=%s",
                   surfaceId.c_str());
        return SurfaceSize{0.0f, 0.0f};
    }
    if (resultObj.get() == nullptr) {
        // Java returned null → "not measurable yet"
        return SurfaceSize{0.0f, 0.0f};
    }

    SurfaceSize size;
    size.width = env->GetFloatField(resultObj.get(), _widthField);
    size.height = env->GetFloatField(resultObj.get(), _heightField);
    return size;
}

}  // namespace agenui
