package com.amap.agenuiplayground;

import android.content.Context;
import android.util.Log;

import com.amap.agenui.IAGenUILogger;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Custom RuntimeLogger implementation for AGenUI Playground
 * 
 * This logger forwards AGenUI engine logs to Android Logcat with proper formatting.
 * It also provides a callback interface for displaying logs in the UI.
 */
public class PlaygroundRuntimeLogger implements IAGenUILogger {
    private static final String TAG = "AGenUI-Engine";
    private static final String LOG_DIR_NAME = "agenui_log";
    private volatile boolean mEnabled = true;
    private final ExecutorService logExecutor;
    private File logDirectory;
    private File currentLogFile;
    private SimpleDateFormat fileDateFormat;
    
    public PlaygroundRuntimeLogger(Context context) {
        logExecutor = Executors.newSingleThreadExecutor();
        fileDateFormat = new SimpleDateFormat("yyyyMMdd_HHmmss_SSS", Locale.getDefault());
        setupLogDirectory(context);
        createNewLogFile();
    }

    public boolean ismEnabled() {
        return mEnabled;
    }

    public void setmEnabled(boolean mEnabled) {
        this.mEnabled = mEnabled;
    }
    
    @Override
    public void onLog(int level, String tag, String func, int line, String message) {
        if (!mEnabled) {
            return;
        }

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
        
        // Asynchronously save log to file
        saveLogToFile(level, timestamp, logTag, func, line, message);
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
    
    /**
     * Setup log directory in app's files directory
     */
    private void setupLogDirectory(Context context) {
        logDirectory = new File(context.getFilesDir(), LOG_DIR_NAME);
        if (!logDirectory.exists()) {
            boolean created = logDirectory.mkdirs();
            if (!created) {
                Log.e(TAG, "Failed to create log directory: " + logDirectory.getAbsolutePath());
            }
        }
    }
    
    /**
     * Create a new log file for this session
     */
    private void createNewLogFile() {
        if (logDirectory == null) {
            return;
        }
        
        String fileName = fileDateFormat.format(new Date()) + ".log";
        currentLogFile = new File(logDirectory, fileName);
        
        Log.i(TAG, "Created new log file: " + currentLogFile.getAbsolutePath());
    }
    
    /**
     * Asynchronously save log to file
     */
    private void saveLogToFile(int level, String timestamp, String tag, String func, int line, String message) {
        if (currentLogFile == null) {
            return;
        }
        
        logExecutor.execute(() -> {
            try {
                String logContent = String.format("[AGenUI/%s] %s [%s:%d] %s - %s%n",
                        getLevelName(level), timestamp, tag, line, func, message);
                
                // Append mode: true means append to existing file
                try (FileWriter writer = new FileWriter(currentLogFile, true)) {
                    writer.write(logContent);
                }
            } catch (IOException e) {
                Log.e(TAG, "Failed to save log file: " + e.getMessage());
            }
        });
    }
}
