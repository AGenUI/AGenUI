package com.amap.agenuiplayground.stability;

import android.os.Debug;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * JSONL logger for stability test metrics.
 * Writes one JSON line per round to a local file for later analysis.
 */
public class StabilityLogger {
    private final File logFile;
    private FileWriter writer;
    private final SimpleDateFormat dateFormat =
            new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS", Locale.US);

    public StabilityLogger(File outputDir) {
        this.logFile = new File(outputDir, "stability_log.jsonl");
        try {
            outputDir.mkdirs();
            // Append mode: launch.sh clears the file before first launch,
            // but crash-restarts by monitor.sh must preserve previous data.
            writer = new FileWriter(logFile, true);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public void logRound(int round, String scenario, long durationMs, String status,
                         String fixture, String error) {
        long nativeHeap = Debug.getNativeHeapAllocatedSize() / (1024 * 1024);
        long javaUsed = (Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory()) / (1024 * 1024);
        double memoryMb = nativeHeap + javaUsed;

        StringBuilder sb = new StringBuilder();
        sb.append("{\"ts\":\"").append(dateFormat.format(new Date())).append("\"");
        sb.append(",\"round\":").append(round);
        sb.append(",\"scenario\":\"").append(escapeJson(scenario)).append("\"");
        sb.append(",\"duration_ms\":").append(durationMs);
        sb.append(",\"memory_native_mb\":").append(nativeHeap);
        sb.append(",\"memory_java_mb\":").append(javaUsed);
        sb.append(",\"memory_total_mb\":").append(String.format(Locale.US, "%.1f", memoryMb));
        sb.append(",\"status\":\"").append(status).append("\"");
        if (fixture != null) {
            sb.append(",\"fixture\":\"").append(escapeJson(fixture)).append("\"");
        }
        if (error != null) {
            sb.append(",\"error\":\"").append(escapeJson(error)).append("\"");
        }
        sb.append("}\n");

        writeLine(sb.toString());
    }

    public void logEvent(String event, String detail) {
        StringBuilder sb = new StringBuilder();
        sb.append("{\"ts\":\"").append(dateFormat.format(new Date())).append("\"");
        sb.append(",\"event\":\"").append(escapeJson(event)).append("\"");
        if (detail != null) {
            sb.append(",\"detail\":\"").append(escapeJson(detail)).append("\"");
        }
        sb.append("}\n");
        writeLine(sb.toString());
    }

    private synchronized void writeLine(String line) {
        if (writer != null) {
            try {
                writer.write(line);
                writer.flush();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    public void close() {
        if (writer != null) {
            try {
                writer.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    public String getLogFilePath() {
        return logFile.getAbsolutePath();
    }

    private static String escapeJson(String s) {
        if (s == null) return "";
        return s.replace("\\", "\\\\")
                .replace("\"", "\\\"")
                .replace("\n", "\\n")
                .replace("\r", "\\r")
                .replace("\t", "\\t");
    }
}
