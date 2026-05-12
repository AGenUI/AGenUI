#include <jni.h>
#include <string>
#include <cstdarg>
#include <android/log.h>
#include "agenui_logger_interface.h"
#include "agenui_logger_internal.h"
#include "jni_helper.h"

namespace agenui {

// — C++ Wrapper Class for Android — 
class RuntimeLoggerImpl : public IRuntimeLogger {
public:
    RuntimeLoggerImpl(JNIEnv* env, jobject javaLogger) 
        : mJavaLogger(nullptr), mJClass(nullptr), mOnLogMethodID(nullptr) {
        // Create global reference to Java logger object
        mJavaLogger = env->NewGlobalRef(javaLogger);
        
        // Get class and method ID for callback
        mJClass = env->GetObjectClass(mJavaLogger);
        mOnLogMethodID = env->GetMethodID(mJClass, "onLogFromNative", 
                                          "(ILjava/lang/String;Ljava/lang/String;ILjava/lang/String;)V");
    }
    
    ~RuntimeLoggerImpl() {
        if (mJavaLogger) {
            JNIEnv* env = agenui::JNIHelper::getJNIEnv();
            if (env) {
                env->DeleteGlobalRef(mJavaLogger);
                mJavaLogger = nullptr;
                env->DeleteGlobalRef(mJClass);
                mJClass = nullptr;
            }
        }
    }
    
    void log(LogLevel level, const char* tag, const char* func, int line, const char* format, ...) override {
        if (!mJavaLogger || !mOnLogMethodID) {
            return;
        }
        
        // Re-entrancy guard: prevent infinite recursion when JNIHelper::getJNIEnv() logs
        static thread_local bool sIsLogging = false;
        if (sIsLogging) {
            // Fallback to Android logcat directly to avoid recursion
            va_list args;
            va_start(args, format);
            char buffer[4096];
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            __android_log_print(ANDROID_LOG_DEBUG, tag ? tag : "AGenUI", "[%s@%d] %s", func ? func : "", line, buffer);
            return;
        }
        sIsLogging = true;
        
        // Format the message using variadic arguments
        va_list args;
        va_start(args, format);
        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        // Get JNI environment
        JNIEnv* env = agenui::JNIHelper::getJNIEnv();
        if (!env) {
            // Fallback to logcat if JNI env unavailable
            __android_log_print(ANDROID_LOG_DEBUG, tag ? tag : "AGenUI", "[%s@%d] %s", func ? func : "", line, buffer);
            sIsLogging = false;
            return;
        }
        
        // Convert C strings to Java strings
        jstring jTag = tag ? env->NewStringUTF(tag) : env->NewStringUTF("");
        jstring jFunc = func ? env->NewStringUTF(func) : env->NewStringUTF("");
        jstring jMessage = env->NewStringUTF(buffer);
        
        // Call Java method
        env->CallVoidMethod(mJavaLogger, mOnLogMethodID, 
                           static_cast<jint>(level), jTag, jFunc, static_cast<jint>(line), jMessage);

        // Check and clear JNI exception
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        // Clean up local references
        env->DeleteLocalRef(jTag);
        env->DeleteLocalRef(jFunc);
        env->DeleteLocalRef(jMessage);
        
        sIsLogging = false;
    }
    
    jobject getJavaLogger() const { return mJavaLogger; }
    
private:
    jobject mJavaLogger;
    jclass mJClass;
    jmethodID mOnLogMethodID;
};

} // namespace agenui

// Global logger instance
static agenui::RuntimeLoggerImpl* gRuntimeLoggerImpl = nullptr;

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_amap_agenui_render_utils_AGenUILogger_nativeInitLogger(JNIEnv* env, jobject thiz) {
    if (gRuntimeLoggerImpl) {
        delete gRuntimeLoggerImpl;
        gRuntimeLoggerImpl = nullptr;
    }
    
    gRuntimeLoggerImpl = new agenui::RuntimeLoggerImpl(env, thiz);
    return reinterpret_cast<jlong>(gRuntimeLoggerImpl);
}

JNIEXPORT void JNICALL
Java_com_amap_agenui_render_utils_AGenUILogger_nativeDestroyLogger(JNIEnv* env, jobject thiz, jlong nativePtr) {
    auto* logger = reinterpret_cast<agenui::RuntimeLoggerImpl*>(nativePtr);
    if (logger) {
        if (logger == gRuntimeLoggerImpl) {
            gRuntimeLoggerImpl = nullptr;
        }
        delete logger;
    }
}

JNIEXPORT void JNICALL
Java_com_amap_agenui_render_utils_AGenUILogger_nativeSetRuntimeLogger(JNIEnv* env, jobject thiz, jlong loggerPtr) {
    if (loggerPtr != 0) {
        auto* logger = reinterpret_cast<agenui::IRuntimeLogger*>(loggerPtr);
        agenui::setRuntimeLoggerInternal(logger);
    } else {
        agenui::setRuntimeLoggerInternal(nullptr);
    }
}

} // extern "C"
