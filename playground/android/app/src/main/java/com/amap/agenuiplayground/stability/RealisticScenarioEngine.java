package com.amap.agenuiplayground.stability;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.amap.agenui.render.surface.ISurfaceManagerListener;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenui.render.surface.SurfaceManager;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Async engine that executes realistic scenarios with visible UI rendering.
 * Each scenario loads a fixture JSON with timed steps, renders surfaces full-screen,
 * and reports results via listener callback.
 */
public class RealisticScenarioEngine implements ISurfaceManagerListener {
    private static final String TAG = "RealisticEngine";
    private static final String FIXTURES_DIR = "stability_fixtures/realistic_scenarios";

    // Fixture step model
    private static class FixtureStep {
        final String action;
        final int delayMs;
        final String message;
        final String raw;

        FixtureStep(String action, int delayMs, String message, String raw) {
            this.action = action;
            this.delayMs = delayMs;
            this.message = message;
            this.raw = raw;
        }
    }

    // Listener interface for Activity integration
    public interface Listener {
        void onSurfaceCreated(Surface surface);
        void onSurfaceDeleted(Surface surface);
        void onRoundCompleted(String fixture, String error);
    }

    // Scenario to fixture name mapping
    private static final Map<StabilityScenarioEngine.Scenario, String> FIXTURE_MAP = new HashMap<>();
    static {
        FIXTURE_MAP.put(StabilityScenarioEngine.Scenario.REALISTIC_ARTICLE_STREAM, "article_stream");
        FIXTURE_MAP.put(StabilityScenarioEngine.Scenario.REALISTIC_MULTI_CARD, "multi_card");
        FIXTURE_MAP.put(StabilityScenarioEngine.Scenario.REALISTIC_FORM_FILL, "form_fill");
        FIXTURE_MAP.put(StabilityScenarioEngine.Scenario.REALISTIC_CHART_REFRESH, "chart_refresh");
        FIXTURE_MAP.put(StabilityScenarioEngine.Scenario.REALISTIC_LONG_LIST, "long_list");
        FIXTURE_MAP.put(StabilityScenarioEngine.Scenario.REALISTIC_PAGE_SWITCH, "page_switch");
        FIXTURE_MAP.put(StabilityScenarioEngine.Scenario.REALISTIC_TAB_NAVIGATION, "tab_navigation");
        FIXTURE_MAP.put(StabilityScenarioEngine.Scenario.REALISTIC_LOTTIE_CAROUSEL, "lottie_carousel");
        FIXTURE_MAP.put(StabilityScenarioEngine.Scenario.REALISTIC_MIXED_DASHBOARD, "mixed_dashboard");
        FIXTURE_MAP.put(StabilityScenarioEngine.Scenario.REALISTIC_ERROR_RECOVERY, "error_recovery");
    }

    private final Activity activity;
    private final Handler handler;
    private final StabilityScenarioEngine stressEngine;

    // Execution state
    private SurfaceManager surfaceManager;
    private final Map<String, Surface> activeSurfaces = new HashMap<>();
    private boolean isExecuting = false;
    private boolean cancelled = false;
    private String currentFixtureName;
    private List<FixtureStep> currentSteps;
    private int stepIndex;
    private Listener listener;

    public RealisticScenarioEngine(Activity activity, StabilityScenarioEngine stressEngine) {
        this.activity = activity;
        this.handler = new Handler(Looper.getMainLooper());
        this.stressEngine = stressEngine;
    }

    /**
     * Execute a realistic scenario round asynchronously.
     * Must be called on the main thread.
     */
    public void executeRound(StabilityScenarioEngine.Scenario scenario, Listener listener) {
        if (isExecuting) {
            listener.onRoundCompleted(null, "Engine busy");
            return;
        }
        if (!scenario.isRealistic()) {
            listener.onRoundCompleted(null, "Not a realistic scenario");
            return;
        }

        String fixtureName = FIXTURE_MAP.get(scenario);
        if (fixtureName == null) {
            listener.onRoundCompleted(null, "No fixture for " + scenario.name());
            return;
        }

        List<FixtureStep> steps = loadFixture(fixtureName);
        if (steps == null) {
            listener.onRoundCompleted(fixtureName, "Failed to load fixture");
            return;
        }

        isExecuting = true;
        cancelled = false;
        this.listener = listener;
        this.currentFixtureName = fixtureName;
        this.currentSteps = steps;
        this.stepIndex = 0;

        // Create fresh SurfaceManager
        surfaceManager = new SurfaceManager(activity);
        surfaceManager.addListener(this);
        surfaceManager.beginTextStream();

        // Start executing steps
        executeNextStep();
    }

    /**
     * Cancel current execution.
     */
    public void cancel() {
        if (!isExecuting) return;
        cancelled = true;
        finishRound("Cancelled");
    }

    /**
     * Tear down any in-progress state.
     */
    public void teardown() {
        cancelled = true;
        handler.removeCallbacksAndMessages(null);
        cleanupSurfaces();
        if (surfaceManager != null) {
            surfaceManager.removeListener(this);
            surfaceManager.destroy();
            surfaceManager = null;
        }
        isExecuting = false;
        listener = null;
    }

    // MARK: - Step Execution

    private void executeNextStep() {
        if (cancelled) return;
        if (stepIndex >= currentSteps.size()) {
            // All steps done
            if (surfaceManager != null) {
                surfaceManager.endTextStream();
            }
            handler.postDelayed(() -> finishRound(null), 200);
            return;
        }

        FixtureStep step = currentSteps.get(stepIndex);
        stepIndex++;

        int delay = Math.max(step.delayMs, 0);
        handler.postDelayed(() -> {
            if (cancelled) return;
            performStep(step);
            executeNextStep();
        }, delay);
    }

    private void performStep(FixtureStep step) {
        if (surfaceManager == null) return;

        switch (step.action) {
            case "createSurface":
            case "updateComponents":
            case "updateDataModel":
            case "deleteSurface":
                if (step.message != null) {
                    surfaceManager.receiveTextChunk(step.message);
                }
                break;

            case "rawChunk":
                if (step.raw != null) {
                    surfaceManager.receiveTextChunk(step.raw);
                }
                break;

            case "beginNewStream":
                surfaceManager.endTextStream();
                surfaceManager.beginTextStream();
                break;

            default:
                if (step.message != null) {
                    surfaceManager.receiveTextChunk(step.message);
                }
                break;
        }
    }

    // MARK: - ISurfaceManagerListener

    @Override
    public void onCreateSurface(Surface surface) {
        activeSurfaces.put(surface.getSurfaceId(), surface);
        if (listener != null) {
            listener.onSurfaceCreated(surface);
        }
    }

    @Override
    public void onDeleteSurface(Surface surface) {
        activeSurfaces.remove(surface.getSurfaceId());
        if (listener != null) {
            listener.onSurfaceDeleted(surface);
        }
    }

    // MARK: - Finish & Cleanup

    private void finishRound(String error) {
        handler.removeCallbacksAndMessages(null);
        cleanupSurfaces();
        if (surfaceManager != null) {
            surfaceManager.removeListener(this);
            surfaceManager.destroy();
            surfaceManager = null;
        }
        isExecuting = false;

        String fixture = currentFixtureName;
        Listener cb = listener;
        currentFixtureName = null;
        currentSteps = null;
        stepIndex = 0;
        listener = null;

        if (cb != null) {
            cb.onRoundCompleted(fixture, error);
        }
    }

    private void cleanupSurfaces() {
        if (listener != null) {
            for (Surface surface : activeSurfaces.values()) {
                listener.onSurfaceDeleted(surface);
            }
        }
        activeSurfaces.clear();
    }

    // MARK: - Fixture Loading

    private List<FixtureStep> loadFixture(String name) {
        String path = FIXTURES_DIR + "/" + name + ".json";
        String json = stressEngine.loadAssetFile(path);
        if (json == null) return null;

        try {
            JSONObject root = new JSONObject(json);
            JSONArray stepsArray = root.getJSONArray("steps");

            List<FixtureStep> steps = new ArrayList<>();
            for (int i = 0; i < stepsArray.length(); i++) {
                JSONObject stepObj = stepsArray.getJSONObject(i);
                String action = stepObj.optString("action", "unknown");
                int delayMs = stepObj.optInt("delay_ms", 0);

                String message = null;
                if (stepObj.has("message")) {
                    message = stepObj.getJSONObject("message").toString();
                }

                String raw = stepObj.optString("raw", null);

                steps.add(new FixtureStep(action, delayMs, message, raw));
            }
            return steps;
        } catch (Exception e) {
            Log.e(TAG, "Failed to parse fixture: " + name, e);
            return null;
        }
    }
}
