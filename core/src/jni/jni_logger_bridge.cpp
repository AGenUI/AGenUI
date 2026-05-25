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
        : mJavaLogger(nullptr), mJClass(nullptr), mOnLogMethodID(nullptr), mMinLevel(LOG_LEVEL_DEBUG) {
        // Create global reference to Java logger object
        mJavaLogger = env->NewGlobalRef(javaLogger);

        // Get class and method ID for callback
        mJClass = env->GetObjectClass(mJavaLogger);
        mOnLogMethodID = env->GetMethodID(mJClass, "onLogFromNative",
                                          "(ILjava/lang/String;Ljava/lang/String;ILjava/lang/String;)V");
    }

    LogLevel getMinLevel() const override {
        return mMinLevel;
    }

    // Pushed in from Java via nativeSetMinLogLevel to avoid a JNI call on every log emission.
    void setMinLevel(LogLevel level) {
        mMinLevel = level;
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

        // Max formatted log size. Logs whose formatted length reaches this cap are dropped
        // to avoid (a) JNI NewStringUTF aborting on a mid-UTF-8 truncation, and (b) flooding
        // the Java/logcat side with oversized payloads.
        static constexpr size_t kMaxLogSize = 8 * 1024; // 8K

        // Re-entrancy guard: prevent infinite recursion when the main path (e.g. JNIHelper::getJNIEnv())
        // internally emits logs that would re-enter log() on the same thread. If we are already inside
        // log() on this thread, drop the nested call to break the recursion chain. The outer (originating)
        // log() call is unaffected and will still be delivered normally.
        static thread_local bool sIsLogging = false;
        if (sIsLogging) {
            return;
        }
        sIsLogging = true;

        // Pre-measure the formatted length so we never write a truncated (and potentially
        // mid-UTF-8-codepoint) string into the buffer. If the content would not fit into
        // the 8K buffer, the log is dropped entirely.
        va_list args;
        va_start(args, format);
        va_list args_copy;
        va_copy(args_copy, args);
        int needed = vsnprintf(nullptr, 0, format, args_copy);
        va_end(args_copy);

        if (needed < 0 || static_cast<size_t>(needed) >= kMaxLogSize) {
            // Oversized log (>= 8K) or formatting error: drop to avoid JNI abort / noise.
            va_end(args);
            sIsLogging = false;
            return;
        }

        char buffer[kMaxLogSize];
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
    LogLevel mMinLevel;
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

JNIEXPORT void JNICALL
Java_com_amap_agenui_render_utils_AGenUILogger_nativeSetMinLogLevel(JNIEnv* env, jobject thiz,
                                                                    jlong nativePtr, jint level) {
    auto* logger = reinterpret_cast<agenui::RuntimeLoggerImpl*>(nativePtr);
    if (logger) {
        logger->setMinLevel(static_cast<agenui::LogLevel>(level));
    }
    // Also keep the built-in default logger's threshold in sync, so the same Java setter
    // works whether or not a custom logger is currently injected into the engine.
    agenui::setDefaultLogMinLevel(static_cast<agenui::LogLevel>(level));
}

} // extern "C"
