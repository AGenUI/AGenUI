#ifdef __ANDROID__

#include "jni_android_platform_function.h"
#include "jni_helper.h"
#include "agenui_logger_internal.h"
#include "jni/jni_scoped_local_ref.h"
#include "jni/jni_scoped_utf_chars.h"
#include "nlohmann/json.hpp"

namespace agenui {

namespace {

// Decodes the JSON envelope returned by the Java PlatformFunction.callSync.
// Schema (shared with iOS / Harmony):
//   {"status":"Success","data":<value>}
//   {"status":"Error","error":"..."}
//   {"status":"Pending","requestId":"..."}
// `data` may be a string or a JSON value; in the latter case it is re-serialized.
static FunctionCallResult parseFunctionCallEnvelope(const std::string& json) {
    FunctionCallResult r;
    r.status = FunctionCallStatus::Error;

    nlohmann::json envelope = nlohmann::json::parse(json, nullptr, false);
    if (envelope.is_discarded() || !envelope.is_object()) {
        r.error = "Invalid envelope JSON: " + json;
        return r;
    }

    auto statusIt = envelope.find("status");
    if (statusIt == envelope.end() || !statusIt->is_string()) {
        r.error = "Missing 'status' field in envelope: " + json;
        return r;
    }

    const std::string status = statusIt->get<std::string>();
    if (status == "Success") {
        r.status = FunctionCallStatus::Success;
        auto it = envelope.find("data");
        if (it != envelope.end() && !it->is_null()) {
            // Always dump as JSON so downstream `nlohmann::json::parse` can re-parse it.
            // For a string value this produces "\"...\"" (the JSON string literal),
            // not the raw text — required because fromPlatformResult parses data as JSON.
            r.data = it->dump();
        }
    } else if (status == "Error") {
        r.status = FunctionCallStatus::Error;
        auto it = envelope.find("error");
        r.error = (it != envelope.end() && it->is_string())
                      ? it->get<std::string>()
                      : "Unknown error";
    } else if (status == "Pending") {
        r.status = FunctionCallStatus::Pending;
    } else {
        r.error = "Unknown status: " + status;
    }
    return r;
}

}  // namespace

AndroidPlatformFunction::AndroidPlatformFunction(JNIEnv* env, jobject javaFunction)
    : _jvm(nullptr), _javaFunction(nullptr),
      _callSyncMethod(nullptr), _contextClass(nullptr), _contextConstructor(nullptr) {

    env->GetJavaVM(&_jvm);

    _javaFunction = env->NewGlobalRef(javaFunction);

    ScopedLocalRef<jclass> functionClass(env, env->GetObjectClass(javaFunction));
    if (functionClass.get() == nullptr) {
        AGENUI_LOG("[AndroidPlatformFunction] constructor: failed to get function class");
        return;
    }

    // Cache FunctionCallContext class and constructor
    ScopedLocalRef<jclass> ctxClass(env,
        env->FindClass("com/amap/agenui/function/FunctionCallContext"));
    if (ctxClass.get() == nullptr) {
        AGENUI_LOG("[AndroidPlatformFunction] constructor: failed to find FunctionCallContext class");
        return;
    }
    _contextClass = (jclass)env->NewGlobalRef(ctxClass.get());
    _contextConstructor = env->GetMethodID(_contextClass, "<init>", "(ILjava/lang/String;)V");
    if (_contextConstructor == nullptr) {
        AGENUI_LOG("[AndroidPlatformFunction] constructor: failed to get FunctionCallContext constructor");
        return;
    }

    // Cache callSync method ID
    _callSyncMethod = env->GetMethodID(functionClass.get(), "callSync",
        "(Lcom/amap/agenui/function/FunctionCallContext;Ljava/lang/String;)Ljava/lang/String;");

    if (_callSyncMethod == nullptr) {
        AGENUI_LOG("[AndroidPlatformFunction] constructor: failed to get method IDs");
    }
}

AndroidPlatformFunction::~AndroidPlatformFunction() {
    if (_jvm != nullptr) {
        JNIEnv* env = nullptr;
        _jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (env != nullptr) {
            if (_javaFunction != nullptr) {
                env->DeleteGlobalRef(_javaFunction);
            }
            if (_contextClass != nullptr) {
                env->DeleteGlobalRef(_contextClass);
            }
        }
    }
}

FunctionCallResult AndroidPlatformFunction::callSync(const FunctionCallContext& context, const std::string& params) {
    FunctionCallResult result;
    result.status = FunctionCallStatus::Error;

    JNIEnv* env = JNIHelper::getJNIEnv();
    if (!env) {
        result.error = "Failed to get JNIEnv";
        AGENUI_LOG("[AndroidPlatformFunction] callSync: failed to get JNIEnv");
        return result;
    }

    if (!_callSyncMethod || !_javaFunction) {
        result.error = "Method or function object is null";
        AGENUI_LOG("[AndroidPlatformFunction] callSync: method or object is null");
        return result;
    }

    // Build Java FunctionCallContext object
    ScopedLocalRef<jstring> jSurfaceId(env, env->NewStringUTF(context.surfaceId.c_str()));
    ScopedLocalRef<jobject> jContext(env, env->NewObject(
        _contextClass, _contextConstructor,
        static_cast<jint>(context.instanceId), jSurfaceId.get()));
    if (jContext.get() == nullptr) {
        result.error = "Failed to create FunctionCallContext object";
        AGENUI_LOG("[AndroidPlatformFunction] callSync: failed to create context object");
        return result;
    }

    ScopedLocalRef<jstring> jParams(env, env->NewStringUTF(params.c_str()));

    ScopedLocalRef<jstring> jResult(env, (jstring)env->CallObjectMethod(
        _javaFunction, _callSyncMethod, jContext.get(), jParams.get()));

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        result.error = "Exception occurred during callSync";
        AGENUI_LOG("[AndroidPlatformFunction] callSync: exception occurred");
        return result;
    }

    if (jResult.get() == nullptr) {
        result.error = "callSync returned null";
        AGENUI_LOG("[AndroidPlatformFunction] callSync: returned null");
        return result;
    }

    ScopedUtfChars resultChars(env, jResult.get());
    return parseFunctionCallEnvelope(resultChars.c_str());
}

} // namespace agenui
#endif
