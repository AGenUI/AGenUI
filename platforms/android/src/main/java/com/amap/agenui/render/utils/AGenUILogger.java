package com.amap.agenui.render.utils;

import android.util.Log;

import androidx.annotation.IntRange;

import com.amap.agenui.IAGenUILogger;

/**
 * @brief AGenUI Logger Manager (Internal Implementation)
 * 
 * Internal implementation class for managing the C++ IRuntimeLogger lifecycle.
 * This class is NOT part of the public API. Users should use AGenUI.setLoggerDelegate()
 * to set their custom logger implementation.
 * 
 * Dependency Inversion Principle:
 * - C++ modules only depend on IRuntimeLogger abstract interface
 * - Internal implementation uses C++ wrapper class to implement this interface, injected into C++ engine
 * - Java logger acts as pass-through channel, with concrete logic implemented by consumer
 * 
 * @hide This class is not part of the public SDK API
 */
public class AGenUILogger {
    private static final String TAG = "AGenUILogger";
    
    // Native pointer to C++ RuntimeLoggerImpl instance
    private long mNativeLoggerPtr = 0;
    private IAGenUILogger mCustomLogger;
    private volatile boolean mEnabled = true;
    // Minimum level forwarded to C++ via IRuntimeLogger::getMinLevel().
    // Field accessed by native code via JNI — do not rename without updating jni_logger_bridge.cpp.
    private volatile int mMinLogLevel = LEVEL_DEBUG;
    
    private static volatile AGenUILogger sInstance = null;
    private static final Object sLock = new Object();

    private AGenUILogger() {
        mNativeLoggerPtr = nativeInitLogger();
    }
    
    /**
     * Returns the AGenUILogger singleton instance
     * 
     * @return AGenUILogger singleton instance
     * @hide Internal use only
     */
    public static AGenUILogger getInstance() {
        if (sInstance == null) {
            synchronized (sLock) {
                if (sInstance == null) {
                    sInstance = new AGenUILogger();
                }
            }
        }
        return sInstance;
    }
    
    /**
     * Set the logger delegate that will receive log callbacks
     * Called by AGenUI.setCustomLogger()
     * 
     * @param customLogger Custom logger implementation
     */
    public void setCustomLogger(IAGenUILogger customLogger) {
        this.mCustomLogger = customLogger;

        if (customLogger != null) {
            nativeSetRuntimeLogger(mNativeLoggerPtr);
        } else {
            nativeSetRuntimeLogger(0L);
        }
    }
    
    /**
     * Enable or disable logging output.
     * When disabled, all log output from both Java and native layers is suppressed.
     *
     * @param enabled true to enable logging, false to disable
     */
    public void setEnabled(boolean enabled) {
        this.mEnabled = enabled;
    }
    
    /**
     * Check if logging is currently enabled.
     *
     * @return true if logging is enabled
     */
    public boolean isEnabled() {
        return mEnabled;
    }
    
    /**
     * Convenience static method to enable or disable logging.
     *
     * @param enabled true to enable logging, false to disable
     */
    public static void setLoggingEnabled(boolean enabled) {
        getInstance().setEnabled(enabled);
    }
    
    /**
     * Convenience static method to check if logging is enabled.
     *
     * @return true if logging is enabled
     */
    public static boolean isLoggingEnabled() {
        return getInstance().isEnabled();
    }

    /**
     * Set the minimum log level. Messages below this level are dropped on both
     * the Java direct path and the C++ native path (via IRuntimeLogger::getMinLevel()).
     *
     * @param level One of LEVEL_DEBUG(0)/INFO(1)/WARN(2)/ERROR(3)/FATAL(4)/PERFORMANCE(5).
     *              Out-of-range values are clamped to LEVEL_DEBUG.
     */
    public void setMinLogLevel(@IntRange(from = LEVEL_DEBUG, to = LEVEL_PERFORMANCE) int level) {
        if (level < LEVEL_DEBUG || level > LEVEL_PERFORMANCE) {
            level = LEVEL_DEBUG;
        }
        this.mMinLogLevel = level;
        // Mirror to the C++ wrapper so the macro-level filter sees the new threshold
        // without paying a JNI call per log emission.
        if (mNativeLoggerPtr != 0) {
            nativeSetMinLogLevel(mNativeLoggerPtr, level);
        }
    }

    public static void setMinLoggingLevel(@IntRange(from = LEVEL_DEBUG, to = LEVEL_PERFORMANCE) int level) {
        getInstance().setMinLogLevel(level);
    }

    /**
     * @return The currently configured minimum log level (0=DEBUG ... 5=PERFORMANCE).
     */
    public int getMinLogLevel() {
        return mMinLogLevel;
    }

    public static int getMinLoggingLevel() {
        return getInstance().getMinLogLevel();
    }

    /**
     * Native method called from C++ to forward log messages to Java
     * @hide Internal use only
     */
    private void onLogFromNative(int level, String tag, String func, int line, String message) {
        if (!mEnabled || level < mMinLogLevel) {
            return;
        }
        dispatchLog(level, tag, func, line, message);
    }
    
    /**
     * Fallback logging to Android Log system
     */
    private static void logToAndroidLog(int level, String tag, String func, int line, String message) {
        String fullMessage;
        if (func != null && !func.isEmpty()) {
            fullMessage = "[" + func + "@" + line + "] " + message;
        } else {
            fullMessage = message;
        }
        String logTag = (tag != null && !tag.isEmpty()) ? tag : "AGenUI";
        
        switch (level) {
            case LEVEL_DEBUG:
                Log.d(logTag, fullMessage);
                break;
            case LEVEL_INFO:
                Log.i(logTag, fullMessage);
                break;
            case LEVEL_WARN:
                Log.w(logTag, fullMessage);
                break;
            case LEVEL_ERROR:
                Log.e(logTag, fullMessage);
                break;
            case LEVEL_FATAL:
                Log.wtf(logTag, fullMessage);
                break;
            case LEVEL_PERFORMANCE:
                Log.d(logTag, "[PERF] " + fullMessage);
                break;
            default:
                Log.d(logTag, fullMessage);
                break;
        }
    }
    
    /**
     * Initialize native logger and return pointer to C++ RuntimeLoggerImpl
     */
    private native long nativeInitLogger();
    
    /**
     * Destroy native logger
     * @hide Internal use only
     */
    void destroy() {
        if (mNativeLoggerPtr != 0) {
            nativeDestroyLogger(mNativeLoggerPtr);
            mNativeLoggerPtr = 0;
        }
    }
    
    // Log level constants matching IAGenUILogger documentation
    public static final int LEVEL_DEBUG = 0;
    public static final int LEVEL_INFO = 1;
    public static final int LEVEL_WARN = 2;
    public static final int LEVEL_ERROR = 3;
    public static final int LEVEL_FATAL = 4;
    public static final int LEVEL_PERFORMANCE = 5;

    /**
     * Send a DEBUG log message.
     */
    public static void d(String tag, String message) {
        logInternal(LEVEL_DEBUG, tag, message, null);
    }

    /**
     * Send an INFO log message.
     */
    public static void i(String tag, String message) {
        logInternal(LEVEL_INFO, tag, message, null);
    }

    /**
     * Send a WARN log message.
     */
    public static void w(String tag, String message) {
        logInternal(LEVEL_WARN, tag, message, null);
    }

    /**
     * Send a WARN log message and log the exception.
     */
    public static void w(String tag, String message, Throwable tr) {
        logInternal(LEVEL_WARN, tag, message, tr);
    }

    /**
     * Send an ERROR log message.
     */
    public static void e(String tag, String message) {
        logInternal(LEVEL_ERROR, tag, message, null);
    }

    /**
     * Send an ERROR log message and log the exception.
     */
    public static void e(String tag, String message, Throwable tr) {
        logInternal(LEVEL_ERROR, tag, message, tr);
    }

    /**
     * Send a VERBOSE log message. Mapped to DEBUG level.
     */
    public static void v(String tag, String message) {
        logInternal(LEVEL_DEBUG, tag, message, null);
    }

    /**
     * Send a FATAL log message.
     */
    public static void wtf(String tag, String message) {
        logInternal(LEVEL_FATAL, tag, message, null);
    }

    /**
     * Send a PERFORMANCE log message.
     * <p>
     * Follows the same convention as C++ AGENUI_PERFORMANCE_LOG: the {@code eventTag}
     * parameter carries the event name (e.g. "components_applied") and {@code message}
     * carries contextual info (e.g. surfaceId).
     *
     * @param eventTag Performance event name (used as log tag for filtering)
     * @param message  Contextual information
     */
    public static void perf(String eventTag, String message) {
        logInternal(LEVEL_PERFORMANCE, eventTag, message, null);
    }

    /**
     * Unified log dispatch. Handles both custom logger delegation and fallback to Android Log.
     */
    private void dispatchLog(int level, String tag, String func, int line, String message) {
        if (mCustomLogger != null) {
            try {
                mCustomLogger.onLog(level, tag, func, line, message);
            } catch (Exception e) {
                Log.e(TAG, "Error in logger delegate", e);
            }
        } else {
            logToAndroidLog(level, tag, func, line, message);
        }
    }

    private static void logInternal(int level, String tag, String message, Throwable tr) {
        AGenUILogger logger = getInstance();
        if (!logger.mEnabled || level < logger.mMinLogLevel) {
            return;
        }
        String fullMessage = message;
        if (tr != null) {
            fullMessage = message + "\n" + Log.getStackTraceString(tr);
        }
        if (logger.mCustomLogger != null) {
            StackTraceElement[] stackTrace = Thread.currentThread().getStackTrace();
            String func = "";
            int line = 0;
            final String loggerClass = AGenUILogger.class.getName();
            for (int i = 2, len = stackTrace.length; i < len; i++) {
                if (!loggerClass.equals(stackTrace[i].getClassName())) {
                    func = stackTrace[i].getMethodName();
                    line = stackTrace[i].getLineNumber();
                    break;
                }
            }
            logger.dispatchLog(level, tag, func, line, fullMessage);
        } else {
            logToAndroidLog(level, tag, null, 0, fullMessage);
        }
    }

    private native void nativeDestroyLogger(long nativePtr);
    private native void nativeSetRuntimeLogger(long loggerPtr);
    private native void nativeSetMinLogLevel(long nativePtr, int level);
}
