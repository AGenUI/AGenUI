package com.amap.agenuiplayground.stability;

import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import com.amap.agenui.AGenUI;
import com.amap.agenui.IAGenUILogger;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenuiplayground.component.factory.ChartComponentFactory;
import com.amap.agenuiplayground.component.factory.LottieComponentFactory;
import com.amap.agenuiplayground.component.factory.MarkdownComponentFactory;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Locale;

/**
 * Self-driving stability test activity supporting dual mode:
 * - Stress mode: label-based metrics display (synchronous execution)
 * - Realistic mode: full-screen surface rendering with floating metrics overlay (async execution)
 *
 * Launch with intent extras:
 *   --es scenario "all_combined"     (default: all_combined)
 *   --ei duration_minutes 480        (default: 480 = 8h)
 *   --ei rounds 0                    (max rounds, 0 = unlimited)
 *   --ei interval_ms 100             (delay between rounds)
 *   --ei crash_threshold 5           (crashes before blacklisting)
 */
public class StabilityTestActivity extends AppCompatActivity {
    private static final String TAG = "StabilityTest";

    // Config
    private StabilityScenarioEngine.Scenario scenario;
    private long durationMs;
    private int maxRounds;
    private int intervalMs;

    // State
    private int currentRound = 0;
    private int errorCount = 0;
    private long startTimeMs;
    private boolean isRunning = false;
    private boolean isRealisticRoundInProgress = false;

    // Components
    private StabilityScenarioEngine engine;
    private RealisticScenarioEngine realisticEngine;
    private StabilityLogger logger;
    private MemoryMonitor memoryMonitor;
    private CrashTracker crashTracker;
    private Handler handler;

    // UI — Stress mode (labels)
    private View labelsContainer;
    private TextView tvScenario;
    private TextView tvRound;
    private TextView tvDuration;
    private TextView tvMemory;
    private TextView tvStatus;
    private TextView tvLastLog;
    private TextView tvErrors;

    // UI — Realistic mode (full-screen rendering)
    private ScrollView surfaceScrollView;
    private LinearLayout surfaceContentLayout;
    private MetricsOverlayView metricsOverlay;

    // UI update timer
    private final Runnable uiUpdateRunnable = new Runnable() {
        @Override
        public void run() {
            if (!isRunning) return;
            updateUI();
            updateMetricsOverlay();
            handler.postDelayed(this, 1000);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getSupportActionBar() != null) getSupportActionBar().hide();

        File outputDir = getExternalFilesDir("stability");
        logger = new StabilityLogger(outputDir);

        // Parse intent parameters
        scenario = StabilityScenarioEngine.parseScenario(
                getIntent().getStringExtra("scenario"));
        durationMs = getIntent().getIntExtra("duration_minutes", 480) * 60L * 1000L;
        maxRounds = getIntent().getIntExtra("rounds", 0);
        intervalMs = getIntent().getIntExtra("interval_ms", 100);

        // Initialize SDK
        AGenUI agenui = AGenUI.getInstance();
        agenui.initialize(getApplicationContext());
        agenui.registerComponent("Markdown", new MarkdownComponentFactory());
        agenui.registerComponent("Lottie", new LottieComponentFactory());
        agenui.registerComponent("Chart", new ChartComponentFactory());

        agenui.setCustomLogger(new IAGenUILogger() {
            @Override
            public void onLog(int level, String tag, String func, int line, String message) {
                Log.i(TAG, "sdk log:" + level + " " + tag + " " + func + " " + line + " " + message);
            }
        });

        // Initialize components
        memoryMonitor = new MemoryMonitor();
        engine = new StabilityScenarioEngine(this);

        String fixturesParam = getIntent().getStringExtra("fixtures");
        if (fixturesParam != null && !fixturesParam.isEmpty()) {
            engine.setFixtureFilter(Arrays.asList(fixturesParam.split(",")));
        }

        handler = new Handler(Looper.getMainLooper());

        realisticEngine = new RealisticScenarioEngine(this, engine);

        // Initialize crash tracker
        int crashThreshold = getIntent().getIntExtra("crash_threshold", 5);
        boolean resetCrashCounts = getIntent().getIntExtra("reset_crash_counts", 0) == 1;
        crashTracker = new CrashTracker(outputDir, crashThreshold);
        if (resetCrashCounts) {
            crashTracker.resetRegistry();
        }
        crashTracker.onProcessStart();

        String lastCrashed = crashTracker.getLastCrashedScenario();
        if (lastCrashed != null) {
            logger.logEvent("crash_detected", "crashed_scenario=" + lastCrashed +
                    ",blacklist=" + crashTracker.getBlacklistSummary());
            Log.w(TAG, "Previous crash detected: scenario=" + lastCrashed);
        }

        // Build UI
        buildUI();

        // Record baseline and start
        memoryMonitor.recordBaseline();
        startTimeMs = System.currentTimeMillis();
        isRunning = true;

        logger.logEvent("start", "scenario=" + scenario.name() +
                ",duration_min=" + (durationMs / 60000) +
                ",max_rounds=" + maxRounds +
                ",interval_ms=" + intervalMs);

        Log.i(TAG, "Stability test started: scenario=" + scenario.name());
        updateUI();

        // Start periodic UI update
        handler.postDelayed(uiUpdateRunnable, 1000);

        // Start the self-driving loop
        handler.post(this::runNextRound);
    }

    // MARK: - Test Loop

    private void runNextRound() {
        if (!isRunning || isRealisticRoundInProgress) return;

        // Check termination conditions
        long elapsed = System.currentTimeMillis() - startTimeMs;
        if (elapsed >= durationMs) {
            stopTest("Duration limit reached");
            return;
        }
        if (maxRounds > 0 && currentRound >= maxRounds) {
            stopTest("Max rounds reached");
            return;
        }

        currentRound++;

        // Determine effective scenario
        StabilityScenarioEngine.Scenario effectiveScenario = scenario;
        StabilityScenarioEngine.Scenario subScenario = null;

        if (scenario.isMeta()) {
            effectiveScenario = engine.selectCombinedScenario(scenario, crashTracker);
            if (effectiveScenario == null) {
                stopTest("All scenarios blacklisted");
                return;
            }
            subScenario = effectiveScenario;
        } else if (crashTracker.isBlacklisted(scenario)) {
            stopTest("Scenario " + scenario.name() + " blacklisted");
            return;
        }

        // Write-ahead crash state
        crashTracker.beforeRound(scenario, subScenario, currentRound, null);

        if (effectiveScenario.isRealistic()) {
            executeRealisticRound(effectiveScenario);
        } else {
            executeStressRound(effectiveScenario);
        }
    }

    // MARK: - Stress Round (synchronous)

    private void executeStressRound(StabilityScenarioEngine.Scenario effectiveScenario) {
        enterStressMode();

        long roundStart = System.currentTimeMillis();
        String fixture = null;
        String error = null;

        try {
            fixture = engine.executeRound(effectiveScenario);
        } catch (Exception e) {
            error = e.getClass().getSimpleName() + ": " + e.getMessage();
            errorCount++;
            Log.e(TAG, "Round " + currentRound + " error", e);
        }

        long durationRoundMs = System.currentTimeMillis() - roundStart;
        completeRound(effectiveScenario, fixture, error, durationRoundMs);
    }

    // MARK: - Realistic Round (asynchronous)

    private void executeRealisticRound(StabilityScenarioEngine.Scenario effectiveScenario) {
        enterRealisticMode();
        isRealisticRoundInProgress = true;

        // Clear previous surface views
        surfaceContentLayout.removeAllViews();

        final long roundStart = System.currentTimeMillis();
        final StabilityScenarioEngine.Scenario scenario = effectiveScenario;

        realisticEngine.executeRound(effectiveScenario, new RealisticScenarioEngine.Listener() {
            @Override
            public void onSurfaceCreated(Surface surface) {
                addSurfaceView(surface);
            }

            @Override
            public void onSurfaceDeleted(Surface surface) {
                removeSurfaceView(surface);
            }

            @Override
            public void onRoundCompleted(String fixture, String error) {
                long durationRoundMs = System.currentTimeMillis() - roundStart;
                isRealisticRoundInProgress = false;
                if (error != null) errorCount++;
                completeRound(scenario, fixture, error, durationRoundMs);
            }
        });
    }

    // MARK: - Round Completion

    private void completeRound(StabilityScenarioEngine.Scenario effectiveScenario,
                               String fixture, String error, long durationMs) {
        crashTracker.afterRound();

        String status = error == null ? "ok" : "error";
        String scenarioName = effectiveScenario.name();
        logger.logRound(currentRound, scenarioName, durationMs, status, fixture, error);

        final String logMsg = buildLogMessage(scenarioName, status, durationMs, fixture);
        runOnUiThread(() -> {
            tvLastLog.setText(logMsg);
            updateUI();
            updateMetricsOverlay();
        });

        // Schedule next round
        handler.postDelayed(this::runNextRound, intervalMs);
    }

    private String buildLogMessage(String scenarioName, String status, long durationMs, String fixture) {
        String shortName = scenarioName.length() > 16 ? scenarioName.substring(0, 16) : scenarioName;
        String msg = String.format(Locale.US, "R%d %s %s (%dms)",
                currentRound, shortName, status, durationMs);
        if (fixture != null) msg += " [" + fixture + "]";
        return msg;
    }

    private void stopTest(String reason) {
        isRunning = false;
        handler.removeCallbacks(uiUpdateRunnable);
        realisticEngine.teardown();
        crashTracker.markCleanExit();
        logger.logEvent("stop", reason + " | " + memoryMonitor.getSummary());
        logger.close();
        Log.i(TAG, "Stability test stopped: " + reason);

        // Write done marker so monitor.sh knows this is a graceful exit (not a crash)
        writeDoneMarker(reason);

        runOnUiThread(() -> {
            tvStatus.setText("STOPPED: " + reason);
            tvStatus.setTextColor(Color.parseColor("#FF6600"));
            updateMetricsOverlay();
        });

        // Finish activity after a brief delay to allow final UI update
        handler.postDelayed(this::finish, 500);
    }

    private void writeDoneMarker(String reason) {
        try {
            File outputDir = getExternalFilesDir("stability");
            if (outputDir == null) return;
            File doneFile = new File(outputDir, "stability_done.txt");
            try (FileOutputStream fos = new FileOutputStream(doneFile)) {
                String content = reason + "\n" + System.currentTimeMillis() + "\n";
                fos.write(content.getBytes(StandardCharsets.UTF_8));
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed to write done marker", e);
        }
    }

    // MARK: - UI Mode Switching

    private void enterRealisticMode() {
        runOnUiThread(() -> {
            labelsContainer.setVisibility(View.GONE);
            surfaceScrollView.setVisibility(View.VISIBLE);
            metricsOverlay.setVisibility(View.VISIBLE);
        });
    }

    private void enterStressMode() {
        runOnUiThread(() -> {
            labelsContainer.setVisibility(View.VISIBLE);
            surfaceScrollView.setVisibility(View.GONE);
            metricsOverlay.setVisibility(View.GONE);
        });
    }

    // MARK: - Surface View Management

    private void addSurfaceView(Surface surface) {
        ViewGroup container = surface.getContainer();
        if (container.getParent() != null) {
            ((ViewGroup) container.getParent()).removeView(container);
        }
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        surfaceContentLayout.addView(container, lp);
    }

    private void removeSurfaceView(Surface surface) {
        ViewGroup container = surface.getContainer();
        if (container.getParent() == surfaceContentLayout) {
            surfaceContentLayout.removeView(container);
        }
    }

    // MARK: - UI Updates

    private void updateUI() {
        long elapsed = System.currentTimeMillis() - startTimeMs;
        int hours = (int) (elapsed / 3600000);
        int minutes = (int) ((elapsed % 3600000) / 60000);
        int seconds = (int) ((elapsed % 60000) / 1000);

        tvScenario.setText("Scenario: " + scenario.name());
        String roundText = maxRounds > 0 ?
                String.format(Locale.US, "Round: %d / %d", currentRound, maxRounds) :
                String.format(Locale.US, "Round: %d", currentRound);
        tvRound.setText(roundText);
        tvDuration.setText(String.format(Locale.US, "Duration: %02d:%02d:%02d", hours, minutes, seconds));
        tvMemory.setText(String.format(Locale.US, "Memory: %dMB (baseline: %dMB, peak: %dMB)",
                memoryMonitor.getTotalMb(), memoryMonitor.getBaselineTotalMb(), memoryMonitor.getPeakTotalMb()));
        if (isRunning) {
            tvStatus.setText("Status: RUNNING");
            tvStatus.setTextColor(Color.parseColor("#00AA00"));
        }
        tvErrors.setText(String.format(Locale.US, "Errors: %d", errorCount));
    }

    private void updateMetricsOverlay() {
        if (metricsOverlay.getVisibility() != View.VISIBLE) return;
        long elapsed = System.currentTimeMillis() - startTimeMs;
        metricsOverlay.update(
                tvLastLog.getText().toString(),
                currentRound, maxRounds, elapsed,
                (int) memoryMonitor.getTotalMb(), (int) memoryMonitor.getPeakTotalMb(),
                errorCount, isRunning);
    }

    // MARK: - Build UI

    private void buildUI() {
        // Root: FrameLayout to stack layers
        FrameLayout root = new FrameLayout(this);
        root.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        // Layer 1: Surface scroll view (realistic mode, initially hidden)
        surfaceScrollView = new ScrollView(this);
        surfaceScrollView.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        surfaceScrollView.setBackgroundColor(Color.WHITE);
        surfaceScrollView.setVisibility(View.GONE);

        surfaceContentLayout = new LinearLayout(this);
        surfaceContentLayout.setOrientation(LinearLayout.VERTICAL);
        surfaceContentLayout.setLayoutParams(new ScrollView.LayoutParams(
                ScrollView.LayoutParams.MATCH_PARENT,
                ScrollView.LayoutParams.WRAP_CONTENT));
        surfaceScrollView.addView(surfaceContentLayout);
        root.addView(surfaceScrollView);

        // Layer 2: Labels container (stress mode)
        ScrollView labelsScroll = new ScrollView(this);
        labelsScroll.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setBackgroundColor(Color.parseColor("#1A1A2E"));
        layout.setPadding(40, 60, 40, 40);

        TextView tvTitle = createTextView("AGenUI Stability Test", 20, Color.WHITE, true);
        tvTitle.setGravity(Gravity.CENTER);
        tvTitle.setPadding(0, 0, 0, 30);
        layout.addView(tvTitle);
        layout.addView(createDivider());

        tvScenario = createTextView("Scenario: -", 14, Color.parseColor("#E0E0E0"), false);
        layout.addView(tvScenario);
        tvRound = createTextView("Round: 0", 14, Color.parseColor("#E0E0E0"), false);
        layout.addView(tvRound);
        tvDuration = createTextView("Duration: 00:00:00", 14, Color.parseColor("#E0E0E0"), false);
        layout.addView(tvDuration);
        tvMemory = createTextView("Memory: -", 14, Color.parseColor("#87CEEB"), false);
        layout.addView(tvMemory);
        tvStatus = createTextView("Status: INITIALIZING", 14, Color.parseColor("#FFAA00"), true);
        layout.addView(tvStatus);
        layout.addView(createDivider());

        tvLastLog = createTextView("Waiting for first round...", 12, Color.parseColor("#AAAAAA"), false);
        tvLastLog.setPadding(0, 10, 0, 10);
        layout.addView(tvLastLog);
        tvErrors = createTextView("Errors: 0", 12, Color.parseColor("#FF6666"), false);
        layout.addView(tvErrors);
        layout.addView(createDivider());

        File outputDir = getExternalFilesDir("stability");
        TextView tvPath = createTextView("Log: " + (outputDir != null ? outputDir.getAbsolutePath() : "N/A"),
                10, Color.parseColor("#666666"), false);
        tvPath.setPadding(0, 20, 0, 0);
        layout.addView(tvPath);

        labelsScroll.addView(layout);
        labelsContainer = labelsScroll;
        root.addView(labelsContainer);

        // Layer 3: Metrics overlay (top-right, initially hidden)
        metricsOverlay = new MetricsOverlayView(this);
        FrameLayout.LayoutParams overlayLp = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT);
        overlayLp.gravity = Gravity.TOP | Gravity.END;
        overlayLp.topMargin = 40;
        overlayLp.rightMargin = 16;
        metricsOverlay.setLayoutParams(overlayLp);
        metricsOverlay.setVisibility(View.GONE);
        root.addView(metricsOverlay);

        setContentView(root);
    }

    private TextView createTextView(String text, int sizeSp, int color, boolean bold) {
        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setTextSize(sizeSp);
        tv.setTextColor(color);
        if (bold) tv.setTypeface(null, Typeface.BOLD);
        tv.setPadding(0, 8, 0, 8);
        return tv;
    }

    private View createDivider() {
        View divider = new View(this);
        divider.setBackgroundColor(Color.parseColor("#333355"));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 2);
        params.setMargins(0, 16, 0, 16);
        divider.setLayoutParams(params);
        return divider;
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (isRunning) {
            stopTest("Activity destroyed");
        }
        handler.removeCallbacksAndMessages(null);
        realisticEngine.teardown();
    }
}
