package com.amap.agenuiplayground;

import android.util.Log;

import com.amap.agenui.IAGenUILogger;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Custom RuntimeLogger implementation for AGenUI Playground
 * 
 * This logger forwards AGenUI engine logs to Android Logcat with proper formatting.
 * It also provides a callback interface for displaying logs in the UI.
 */
public class PlaygroundRuntimeLogger implements IAGenUILogger {
    private static final String TAG = "AGenUI-Engine";
    
    public PlaygroundRuntimeLogger() {
        // Default constructor
    }
    
    @Override
    public void onLog(int level, String tag, String func, int line, String message) {
        // Get current timestamp
        String timestamp = getTimestamp();
        
        // Format the log message with timestamp
        String formattedMessage = String.format("%s [%s@%d] %s", timestamp, func, line, message);
        String logTag = (tag != null && !tag.isEmpty()) ? tag : TAG;

        // Always log to Android Logcat based on level
        switch (level) {
            case 0: // DEBUG
                Log.d(logTag, formattedMessage);
                break;
            case 1: // INFO
                Log.i(logTag, formattedMessage);
                break;
            case 2: // WARN
                Log.w(logTag, formattedMessage);
                break;
            case 3: // ERROR
                Log.e(logTag, formattedMessage);
                break;
            case 4: // FATAL
                Log.wtf(logTag, formattedMessage);
                break;
            case 5: // PERFORMANCE
                Log.d(logTag, "[PERF] " + formattedMessage);
                break;
            default:
                Log.d(logTag, formattedMessage);
                break;
        }
    }
    
    /**
     * Get human-readable log level name
     */
    public static String getLevelName(int level) {
        switch (level) {
            case 0: return "DEBUG";
            case 1: return "INFO";
            case 2: return "WARN";
            case 3: return "ERROR";
            case 4: return "FATAL";
            case 5: return "PERF";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * Get formatted timestamp for log messages
     * Format: HH:mm:ss.SSS (e.g., 14:30:25.123)
     */
    private String getTimestamp() {
        SimpleDateFormat sdf = new SimpleDateFormat("HH:mm:ss.SSS", Locale.getDefault());
        return sdf.format(new Date());
    }
}
