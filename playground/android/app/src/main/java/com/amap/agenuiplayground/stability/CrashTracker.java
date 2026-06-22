package com.amap.agenuiplayground.stability;

import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;

/**
 * Tracks crash attribution and manages scenario blacklisting for stability tests.
 *
 * Uses a write-ahead pattern: before each round, a state file is written identifying the
 * current scenario. If the process crashes, the state file persists on disk. On next startup,
 * reading the state file reveals which scenario caused the crash.
 *
 * Crash counts are accumulated in a registry file. When a scenario exceeds the configured
 * threshold, it is blacklisted and skipped in subsequent rounds.
 */
public class CrashTracker {
    private static final String TAG = "CrashTracker";
    private static final String STATE_FILE = "crash_state.json";
    private static final String STATE_TMP_FILE = "crash_state.json.tmp";
    private static final String REGISTRY_FILE = "crash_registry.json";
    private static final String REGISTRY_TMP_FILE = "crash_registry.json.tmp";
    // Written by onProcessStart() after crash detection; read by monitor.sh for attribution.
    // Never deleted — only overwritten on each crash. This avoids the race where onProcessStart()
    // deletes crash_state.json before monitor.sh has a chance to read it.
    private static final String LAST_CRASH_FILE = "last_crash_scenario.txt";

    private final File outputDir;
    private final int threshold;
    private JSONObject registry;
    private String lastCrashedScenario;

    public CrashTracker(File outputDir, int threshold) {
        this.outputDir = outputDir;
        this.threshold = Math.max(1, threshold);
        this.registry = new JSONObject();
        if (outputDir != null && !outputDir.exists()) {
            outputDir.mkdirs();
        }
    }

    /**
     * Called on process start. Detects if the previous run crashed (state file exists)
     * and attributes the crash to the scenario recorded in that file.
     *
     * The state file persists across rounds (afterRound marks it "completed" but does not
     * delete it). Only a clean exit (markCleanExit) removes it. Therefore, if the state
     * file exists on startup, the previous process terminated abnormally — either during
     * round execution (status=running) or from async native work after the round completed
     * (status=completed).
     */
    public void onProcessStart() {
        File stateFile = new File(outputDir, STATE_FILE);
        File stateTmpFile = new File(outputDir, STATE_TMP_FILE);

        // Load existing registry
        loadRegistry();

        if (stateFile.exists()) {
            // Previous run crashed — state file was never cleaned by markCleanExit()
            String content = readFileContent(stateFile);
            if (content != null) {
                try {
                    JSONObject state = new JSONObject(content);
                    String status = state.optString("status", "running");
                    // Use sub_scenario if available (ALL_COMBINED mode), else use scenario
                    String crashedScenario = state.optString("sub_scenario", "");
                    if (crashedScenario.isEmpty()) {
                        crashedScenario = state.optString("scenario", "unknown");
                    }
                    String fixture = state.optString("fixture", null);
                    int round = state.optInt("round", -1);

                    lastCrashedScenario = crashedScenario;
                    incrementCrashCount(crashedScenario, fixture, round);

                    // Write to a separate durable file for monitor.sh to read.
                    // This file is never deleted — only overwritten on each crash.
                    // Solves the race where this method deletes crash_state.json before
                    // monitor.sh's 10-second check interval can read it.
                    writeLastCrashFile(crashedScenario, round, status);

                    String phase = "running".equals(status) ? "during execution" : "after completion (async)";
                    Log.w(TAG, "Crash detected " + phase + ": scenario=" + crashedScenario +
                            ", round=" + round + ", fixture=" + fixture);
                } catch (JSONException e) {
                    Log.e(TAG, "Failed to parse crash state", e);
                    lastCrashedScenario = "unknown";
                    incrementCrashCount("unknown", null, -1);
                    writeLastCrashFile("unknown", -1, "parse_error");
                }
            } else {
                lastCrashedScenario = "unknown";
                incrementCrashCount("unknown", null, -1);
                writeLastCrashFile("unknown", -1, "empty_file");
            }
            // Delete the state file (evidence consumed)
            stateFile.delete();
        } else if (stateTmpFile.exists()) {
            // Crash occurred during state file write itself
            lastCrashedScenario = "write_interrupted";
            incrementCrashCount("write_interrupted", null, -1);
            writeLastCrashFile("write_interrupted", -1, "write_interrupted");
            stateTmpFile.delete();
            Log.w(TAG, "Crash during state write detected");
        } else {
            // Clean start (previous run exited via markCleanExit)
            lastCrashedScenario = null;
        }
    }

    /**
     * Called before each round execution (write-ahead).
     * Persists the current scenario to disk so crashes can be attributed.
     */
    public void beforeRound(StabilityScenarioEngine.Scenario scenario,
                            StabilityScenarioEngine.Scenario subScenario,
                            int round, String fixture) {
        JSONObject state = new JSONObject();
        try {
            state.put("scenario", scenario.name());
            if (subScenario != null) {
                state.put("sub_scenario", subScenario.name());
            }
            state.put("round", round);
            if (fixture != null) {
                state.put("fixture", fixture);
            }
            state.put("status", "running");
            state.put("started_at", getTimestamp());
        } catch (JSONException e) {
            Log.e(TAG, "Failed to build state JSON", e);
            return;
        }

        atomicWrite(new File(outputDir, STATE_FILE),
                new File(outputDir, STATE_TMP_FILE),
                state.toString());
    }

    /**
     * Called after a round completes successfully.
     * Updates the state file status to "completed" but does NOT delete it.
     * This ensures that async native crashes (e.g., use-after-free in worker threads)
     * that happen after the Java round finishes can still be attributed to the correct scenario.
     * Only markCleanExit() deletes the state file.
     */
    public void afterRound() {
        File stateFile = new File(outputDir, STATE_FILE);
        if (stateFile.exists()) {
            String content = readFileContent(stateFile);
            if (content != null) {
                try {
                    JSONObject state = new JSONObject(content);
                    state.put("status", "completed");
                    state.put("completed_at", getTimestamp());
                    atomicWrite(stateFile, new File(outputDir, STATE_TMP_FILE), state.toString());
                } catch (JSONException e) {
                    Log.e(TAG, "Failed to update state file status", e);
                }
            }
        }
    }

    /**
     * Called when the test finishes cleanly (duration reached, max rounds, or explicit stop).
     * Removes the state file so the next process start does not falsely detect a crash.
     */
    public void markCleanExit() {
        File stateFile = new File(outputDir, STATE_FILE);
        if (stateFile.exists()) {
            stateFile.delete();
        }
        File stateTmpFile = new File(outputDir, STATE_TMP_FILE);
        if (stateTmpFile.exists()) {
            stateTmpFile.delete();
        }
        Log.i(TAG, "Clean exit marked — state file removed");
    }

    /**
     * Check if a scenario is blacklisted (exceeded crash threshold).
     */
    public boolean isBlacklisted(StabilityScenarioEngine.Scenario scenario) {
        try {
            JSONObject scenarios = registry.optJSONObject("scenarios");
            if (scenarios == null) return false;
            JSONObject entry = scenarios.optJSONObject(scenario.name());
            if (entry == null) return false;
            return entry.optBoolean("blacklisted", false);
        } catch (Exception e) {
            return false;
        }
    }

    /**
     * Returns the list of S1-S7 scenarios that are NOT blacklisted.
     * @deprecated Use {@link #getAvailableScenarios(StabilityScenarioEngine.Scenario)} instead.
     */
    @Deprecated
    public List<StabilityScenarioEngine.Scenario> getAvailableScenarios() {
        return getAvailableScenarios(StabilityScenarioEngine.Scenario.ALL_COMBINED);
    }

    /**
     * Returns available (non-blacklisted) scenarios filtered by mode.
     * @param mode ALL_STRESS returns only stress, ALL_REALISTIC returns only realistic,
     *             ALL_COMBINED returns both.
     */
    public List<StabilityScenarioEngine.Scenario> getAvailableScenarios(StabilityScenarioEngine.Scenario mode) {
        List<StabilityScenarioEngine.Scenario> pool;
        switch (mode) {
            case ALL_STRESS:
                pool = StabilityScenarioEngine.Scenario.stressScenarios();
                break;
            case ALL_REALISTIC:
                pool = StabilityScenarioEngine.Scenario.realisticScenarios();
                break;
            default:
                pool = StabilityScenarioEngine.Scenario.allIndividualScenarios();
                break;
        }
        List<StabilityScenarioEngine.Scenario> available = new ArrayList<>();
        for (StabilityScenarioEngine.Scenario s : pool) {
            if (!isBlacklisted(s)) {
                available.add(s);
            }
        }
        return available;
    }

    /**
     * Returns the scenario name that crashed on previous run, or null if clean start.
     */
    public String getLastCrashedScenario() {
        return lastCrashedScenario;
    }

    /**
     * Resets the crash registry (clears all counts and blacklists).
     */
    public void resetRegistry() {
        registry = new JSONObject();
        File registryFile = new File(outputDir, REGISTRY_FILE);
        if (registryFile.exists()) {
            registryFile.delete();
        }
        Log.i(TAG, "Crash registry reset");
    }

    /**
     * Returns a summary string of blacklisted scenarios for UI display.
     */
    public String getBlacklistSummary() {
        List<String> blacklisted = new ArrayList<>();
        try {
            JSONObject scenarios = registry.optJSONObject("scenarios");
            if (scenarios == null) return "none";
            Iterator<String> keys = scenarios.keys();
            while (keys.hasNext()) {
                String key = keys.next();
                JSONObject entry = scenarios.getJSONObject(key);
                if (entry.optBoolean("blacklisted", false)) {
                    blacklisted.add(key + "(" + entry.optInt("crash_count", 0) + ")");
                }
            }
        } catch (JSONException e) {
            // ignore
        }
        return blacklisted.isEmpty() ? "none" : String.join(", ", blacklisted);
    }

    // --- Private helpers ---

    private void incrementCrashCount(String scenarioName, String fixture, int round) {
        try {
            JSONObject scenarios = registry.optJSONObject("scenarios");
            if (scenarios == null) {
                scenarios = new JSONObject();
                registry.put("scenarios", scenarios);
            }

            JSONObject entry = scenarios.optJSONObject(scenarioName);
            if (entry == null) {
                entry = new JSONObject();
                entry.put("crash_count", 0);
                entry.put("blacklisted", false);
                scenarios.put(scenarioName, entry);
            }

            int count = entry.optInt("crash_count", 0) + 1;
            entry.put("crash_count", count);
            entry.put("last_crash_at", getTimestamp());
            entry.put("last_round", round);
            if (fixture != null) {
                entry.put("last_fixture", fixture);
            }

            if (count >= threshold) {
                entry.put("blacklisted", true);
                entry.put("blacklisted_at", getTimestamp());
                Log.w(TAG, "Scenario BLACKLISTED: " + scenarioName +
                        " (crashes=" + count + ", threshold=" + threshold + ")");
            }

            // Update totals
            int totalCrashes = registry.optInt("total_crashes", 0) + 1;
            registry.put("total_crashes", totalCrashes);
            registry.put("threshold", threshold);

            saveRegistry();
        } catch (JSONException e) {
            Log.e(TAG, "Failed to update crash registry", e);
        }
    }

    private void loadRegistry() {
        File registryFile = new File(outputDir, REGISTRY_FILE);
        if (!registryFile.exists()) {
            registry = new JSONObject();
            return;
        }
        String content = readFileContent(registryFile);
        if (content != null) {
            try {
                registry = new JSONObject(content);
                return;
            } catch (JSONException e) {
                Log.e(TAG, "Registry corrupted, resetting", e);
            }
        }
        registry = new JSONObject();
    }

    private void saveRegistry() {
        atomicWrite(new File(outputDir, REGISTRY_FILE),
                new File(outputDir, REGISTRY_TMP_FILE),
                registry.toString());
    }

    private void atomicWrite(File target, File tmp, String content) {
        try {
            // Write to tmp
            FileOutputStream fos = new FileOutputStream(tmp);
            fos.write(content.getBytes(StandardCharsets.UTF_8));
            fos.getFD().sync();
            fos.close();

            // Atomic rename
            if (!tmp.renameTo(target)) {
                // Fallback: copy content directly
                Log.w(TAG, "rename failed, falling back to direct write");
                fos = new FileOutputStream(target);
                fos.write(content.getBytes(StandardCharsets.UTF_8));
                fos.getFD().sync();
                fos.close();
                tmp.delete();
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to write file: " + target.getName(), e);
        }
    }

    private String readFileContent(File file) {
        try {
            FileInputStream fis = new FileInputStream(file);
            byte[] data = new byte[(int) file.length()];
            fis.read(data);
            fis.close();
            return new String(data, StandardCharsets.UTF_8);
        } catch (IOException e) {
            Log.e(TAG, "Failed to read file: " + file.getName(), e);
            return null;
        }
    }

    private String getTimestamp() {
        return new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS", Locale.US).format(new Date());
    }

    /**
     * Writes the crash scenario to a durable file that monitor.sh can read at any time.
     * This file is NEVER deleted — only overwritten on each new crash detection.
     * It solves the race condition where onProcessStart() deletes crash_state.json
     * before monitor.sh's periodic check can read it.
     */
    private void writeLastCrashFile(String scenario, int round, String status) {
        try {
            JSONObject obj = new JSONObject();
            obj.put("scenario", scenario);
            obj.put("round", round);
            obj.put("status", status);
            obj.put("detected_at", getTimestamp());
            File target = new File(outputDir, LAST_CRASH_FILE);
            FileOutputStream fos = new FileOutputStream(target);
            fos.write(obj.toString().getBytes(StandardCharsets.UTF_8));
            fos.getFD().sync();
            fos.close();
        } catch (Exception e) {
            Log.e(TAG, "Failed to write last crash file", e);
        }
    }
}
