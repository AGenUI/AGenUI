package com.amap.agenui.render.utils;

import android.util.Log;

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
     * Native method called from C++ to forward log messages to Java
     * @hide Internal use only
     */
    private void onLogFromNative(int level, String tag, String func, int line, String message) {
        if (mCustomLogger != null) {
            try {
                mCustomLogger.onLog(level, tag, func, line, message);
            } catch (Exception e) {
                Log.e(TAG, "Error in logger delegate", e);
            }
        } else {
            // Fallback to default Android logging
            logToAndroidLog(level, tag, func, line, message);
        }
    }
    
    /**
     * Fallback logging to Android Log system
     */
    private void logToAndroidLog(int level, String tag, String func, int line, String message) {
        String fullMessage = String.format("[%s@%d] %s", func, line, message);
        String logTag = (tag != null && !tag.isEmpty()) ? tag : "AGenUI";
        
        switch (level) {
            case 0: // DEBUG
                Log.d(logTag, fullMessage);
                break;
            case 1: // INFO
                Log.i(logTag, fullMessage);
                break;
            case 2: // WARN
                Log.w(logTag, fullMessage);
                break;
            case 3: // ERROR
                Log.e(logTag, fullMessage);
                break;
            case 4: // FATAL
                Log.wtf(logTag, fullMessage);
                break;
            case 5: // PERFORMANCE
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
    
    private native void nativeDestroyLogger(long nativePtr);
    private native void nativeSetRuntimeLogger(long loggerPtr);
}
