package com.amap.agenuiplayground.tests;

import android.app.Activity;
import android.os.Debug;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.IAGenUILogger;
import com.amap.agenui.render.surface.ISurfaceManagerListener;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenui.render.surface.SurfaceSize;
import com.amap.agenuiplayground.A2UIPlaygroundActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertTrue;

import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

/**
 * RISK20/21: Extended lifecycle stress probe inspired by iOS SDK_INTERFACE_STABILITY crash.
 *
 * iOS stability tests crashed at round 1209/1348 with:
 *   - Crash 1: SIGABRT on AGenUI-1 thread during Array.append in logger callback
 *     (receiveTextChunk -> processDataAssembling -> log -> InterfaceLogger.onLog)
 *   - Crash 2: EXC_BAD_ACCESS/SIGSEGV on main thread during object dealloc
 *     while AGenUI-1 was doing createSurfaceManager -> initFunctionCalls
 *
 * Both crashes indicate progressive heap corruption after 1000+ rounds of SM lifecycle ops.
 * This probe mirrors the iOS SDK_INTERFACE_STABILITY pattern on Android, running 2000+ rounds
 * to detect similar progressive issues (native crash, JNI GlobalRef leak, memory growth).
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeExtendedLifecycleTest {

    private static final String TAG = "RiskProbe-ExtLifecycle";
    private static final long TIMEOUT_MS = 10000;

    @Rule
    public ActivityScenarioRule<A2UIPlaygroundActivity> activityRule =
            new ActivityScenarioRule<>(A2UIPlaygroundActivity.class);

    private Activity activity;

    @Before
    public void setUp() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        activityRule.getScenario().onActivity(a -> {
            activity = a;
            AGenUI.getInstance().initialize(a.getApplicationContext());
            latch.countDown();
        });
        assertTrue("Activity setup timed out", latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @After
    public void tearDown() {
        activity = null;
    }

    // ==================== Helper JSON builders ====================

    private String buildCreateSurfaceJSON(String surfaceId) {
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"" + surfaceId
                + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}";
    }

    private String buildDeleteSurfaceJSON(String surfaceId) {
        return "{\"version\":\"v0.9\",\"deleteSurface\":{\"surfaceId\":\"" + surfaceId + "\"}}";
    }

    private String buildUpdateComponentsJSON(String surfaceId, String text) {
        return "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId
                + "\",\"components\":[{\"type\":\"Text\",\"id\":\"txt-" + surfaceId
                + "\",\"properties\":{\"text\":\"" + text + "\"}}]}}";
    }

    private String buildThemeJSON(String colorHex) {
        return "{\"colors\":{\"textPrimary\":\"#" + colorHex
                + "\",\"backgroundPrimary\":\"#" + colorHex + "\"}}";
    }

    private String buildDesignTokenJSON(String colorHex) {
        return "{\"color\":{\"text\":{\"primary\":\"" + colorHex
                + "\"},\"bg\":{\"primary\":\"" + colorHex + "\"}}}";
    }

    // ==================== Helper: create SM on main thread ====================

    private SurfaceManager createSMOnMainThread() throws Exception {
        AtomicReference<SurfaceManager> ref = new AtomicReference<>();
        CountDownLatch latch = new CountDownLatch(1);
        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                ref.set(new SurfaceManager(activity));
            } catch (Exception e) {
                Log.e(TAG, "createSM failed", e);
            }
            latch.countDown();
        });
        assertTrue("createSM timed out", latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        return ref.get();
    }

    private void runOnMainThread(Runnable r) throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                r.run();
            } catch (Exception e) {
                Log.e(TAG, "runOnMain failed", e);
            }
            latch.countDown();
        });
        assertTrue("runOnMain timed out", latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    // ==================== RISK20: Extended SM lifecycle (2000 rounds) ====================

    /**
     * RISK20: Mirror iOS SDK_INTERFACE_STABILITY pattern for 2000 rounds.
     *
     * Each round performs a compressed version of the iOS I1-I10 tests:
     *   - Create SM, stream data (createSurface + updateComponents + deleteSurface)
     *   - Toggle day/night mode
     *   - invalidateFunctionCallValues
     *   - Add/remove listeners
     *   - Destroy SM
     *
     * Monitors: native crashes (test would abort), Java exceptions, memory growth.
     */
    @Test(timeout = 600_000) // 10 minute limit
    public void RISK20_extendedSmLifecycleStress() throws Exception {
        final int TOTAL_ROUNDS = 2000;
        final int MEMORY_SAMPLE_INTERVAL = 200;
        final AtomicInteger javaExceptions = new AtomicInteger(0);
        final AtomicLong baselineNativeMB = new AtomicLong(0);

        Log.i(TAG, "=== RISK20: Extended SM lifecycle stress ===");
        Log.i(TAG, "Rounds: " + TOTAL_ROUNDS);

        // Record baseline memory
        System.gc();
        Thread.sleep(200);
        long baseNative = Debug.getNativeHeapAllocatedSize() / (1024 * 1024);
        baselineNativeMB.set(baseNative);
        Log.i(TAG, "Baseline native heap: " + baseNative + " MB");

        for (int round = 0; round < TOTAL_ROUNDS; round++) {
            final int r = round;

            try {
                runOnMainThread(() -> {
                    try {
                        // --- Sub-test I1: Theme registration ---
                        AGenUI sdk = AGenUI.getInstance();
                        sdk.registerDefaultTheme(buildThemeJSON("ffffff"),
                                buildDesignTokenJSON("#ffffff"));
                        sdk.registerDefaultTheme("{invalid",
                                buildDesignTokenJSON("#000000"));
                        sdk.registerDefaultTheme(buildThemeJSON("000000"),
                                "{invalid");

                        // --- Sub-test I2: Path config ---
                        sdk.setPathConfig("{\"templateDir\":\"/tmp/agenui-stability\"}");
                        sdk.setPathConfig("{invalid");

                        // --- Sub-test I5: Day/night mode toggle ---
                        sdk.setDayNightMode(r % 2 == 0 ? "light" : "dark");
                        sdk.setDayNightMode("invalid-mode");

                        // --- Sub-test I6-I9: SM lifecycle with streaming ---
                        SurfaceManager sm = new SurfaceManager(activity);
                        ISurfaceManagerListener listener = new ISurfaceManagerListener() {
                            @Override
                            public void onCreateSurface(Surface surface) { }
                            @Override
                            public void onDeleteSurface(Surface surface) { }

                            @Override
                            public void onReceiveActionEvent(String event) {}

                            @Override
                            public void onRootComponentUpdate(Surface surface, Map<String, String> props) {}

                            @Override
                            public void onError(Surface surface, int code, String message) {}

                            @Override
                            public void onBlankCheckResult(Surface surface, boolean isBlank) {}

                            @Override
                            public void onComponentAppeared(Surface surface, String parentComponentId, String parentType, Map<String, Object> properties) {}

                            @Override
                            public SurfaceSize surfaceSize(String surfaceId) {
                                return null;
                            }
                        };
                        sm.addListener(listener);

                        // Stream: createSurface + updateComponents + deleteSurface
                        sm.beginTextStream();
                        sm.receiveTextChunk(buildCreateSurfaceJSON("r20-" + r));
                        sm.receiveTextChunk(buildUpdateComponentsJSON("r20-" + r,
                                "round-" + r));
                        sm.receiveTextChunk(buildDeleteSurfaceJSON("r20-" + r));
                        sm.endTextStream();

                        // FunctionCall invalidation
                        sm.invalidateFunctionCallValues();

                        // Day/night mode with active SM
                        sdk.setDayNightMode(r % 3 == 0 ? "light" : "dark");

                        // Listener churn
                        sm.removeListener(listener);
                        sm.addListener(listener);
                        sm.removeListener(listener);

                        // Destroy SM
                        sm.destroy();
                    } catch (Exception e) {
                        javaExceptions.incrementAndGet();
                        Log.e(TAG, "Round " + r + " exception", e);
                    }
                });
            } catch (Exception e) {
                javaExceptions.incrementAndGet();
                Log.e(TAG, "Round " + r + " outer exception", e);
            }

            // Periodically log progress and memory
            if ((round + 1) % MEMORY_SAMPLE_INTERVAL == 0) {
                System.gc();
                Thread.sleep(100);
                long currentNative = Debug.getNativeHeapAllocatedSize() / (1024 * 1024);
                long growth = currentNative - baselineNativeMB.get();
                Log.i(TAG, "Round " + (round + 1) + "/" + TOTAL_ROUNDS
                        + " | native=" + currentNative + "MB (+" + growth + "MB)"
                        + " | exceptions=" + javaExceptions.get());
            }
        }

        // Allow worker thread to drain
        Thread.sleep(2000);

        // Final memory check
        System.gc();
        Thread.sleep(500);
        long finalNative = Debug.getNativeHeapAllocatedSize() / (1024 * 1024);
        long totalGrowth = finalNative - baselineNativeMB.get();

        Log.i(TAG, "=== RISK20 RESULT ===");
        Log.i(TAG, "Rounds completed: " + TOTAL_ROUNDS);
        Log.i(TAG, "Java exceptions: " + javaExceptions.get());
        Log.i(TAG, "Native heap: baseline=" + baselineNativeMB.get()
                + "MB, final=" + finalNative + "MB, growth=" + totalGrowth + "MB");

        if (totalGrowth > 50) {
            Log.w(TAG, "SUSPICIOUS: native memory grew by " + totalGrowth
                    + "MB over " + TOTAL_ROUNDS + " rounds");
        }
    }

    // ==================== RISK21: Logger hot-swap during streaming (2000 rounds) ====================

    /**
     * RISK21: Mirror iOS crash #1 pattern - logger callback from AGenUI-1 thread
     * racing with logger swap on main thread.
     *
     * iOS crash: AGenUI-1 calls log() -> InterfaceLogger.onLog() -> Array.append
     * -> swift_deallocClassInstance.cold.1 (SIGABRT)
     *
     * This suggests the C++ gRuntimeLogger pointer read races with
     * setRuntimeLoggerInternal(). On Android, the RuntimeLoggerImpl holds a JNI
     * GlobalRef, so the Java object won't be GC'd. But the C++ pointer itself
     * could be stale if read without barriers.
     *
     * Pattern: set custom logger -> create SM -> stream (triggers many logs on
     * AGenUI-1 thread) -> immediately clear logger -> destroy SM. Repeat 2000x.
     */
    @Test(timeout = 600_000) // 10 minute limit
    public void RISK21_loggerHotSwapDuringStreaming() throws Exception {
        final int TOTAL_ROUNDS = 2000;
        final AtomicInteger javaExceptions = new AtomicInteger(0);
        final AtomicInteger logCallCount = new AtomicInteger(0);

        Log.i(TAG, "=== RISK21: Logger hot-swap during streaming ===");
        Log.i(TAG, "Rounds: " + TOTAL_ROUNDS);

        System.gc();
        Thread.sleep(200);
        long baseNative = Debug.getNativeHeapAllocatedSize() / (1024 * 1024);

        for (int round = 0; round < TOTAL_ROUNDS; round++) {
            final int r = round;

            try {
                // Create a fresh logger for this round
                final IAGenUILogger roundLogger = new IAGenUILogger() {
                    @Override
                    public void onLog(int level, String tag, String func, int line, String message) {
                        logCallCount.incrementAndGet();
                        // Simulate some work in the callback
                        if (level >= 2) {
                            String processed = message.toUpperCase();
                            if (processed.length() > 1000) {
                                Log.d(TAG, "long log");
                            }
                        }
                    }
                };

                runOnMainThread(() -> {
                    try {
                        AGenUI sdk = AGenUI.getInstance();

                        // Set custom logger
                        sdk.setCustomLogger(roundLogger);

                        // Create SM and stream (generates many logs on AGenUI-1)
                        SurfaceManager sm = new SurfaceManager(activity);
                        sm.beginTextStream();
                        sm.receiveTextChunk(buildCreateSurfaceJSON("r21-" + r + "-a"));
                        sm.receiveTextChunk(buildUpdateComponentsJSON("r21-" + r + "-a",
                                "text-" + r));
                        sm.receiveTextChunk(buildDeleteSurfaceJSON("r21-" + r + "-a"));

                        // Second surface in same stream
                        sm.receiveTextChunk(buildCreateSurfaceJSON("r21-" + r + "-b"));
                        sm.receiveTextChunk(buildDeleteSurfaceJSON("r21-" + r + "-b"));
                        sm.endTextStream();

                        // IMMEDIATELY clear logger while AGenUI-1 is still processing
                        sdk.setCustomLogger(null);

                        // Toggle theme (generates more logs)
                        sdk.setDayNightMode(r % 2 == 0 ? "light" : "dark");

                        // Set logger again briefly
                        sdk.setCustomLogger(roundLogger);
                        sm.invalidateFunctionCallValues();
                        sdk.setCustomLogger(null);

                        // Destroy SM
                        sm.destroy();
                    } catch (Exception e) {
                        javaExceptions.incrementAndGet();
                        Log.e(TAG, "Round " + r + " exception", e);
                    }
                });
            } catch (Exception e) {
                javaExceptions.incrementAndGet();
                Log.e(TAG, "Round " + r + " outer exception", e);
            }

            // Progress logging
            if ((round + 1) % 500 == 0) {
                System.gc();
                Thread.sleep(100);
                long currentNative = Debug.getNativeHeapAllocatedSize() / (1024 * 1024);
                Log.i(TAG, "Round " + (round + 1) + "/" + TOTAL_ROUNDS
                        + " | native=" + currentNative + "MB"
                        + " | logCalls=" + logCallCount.get()
                        + " | exceptions=" + javaExceptions.get());
            }
        }

        // Allow worker thread to drain
        Thread.sleep(3000);

        // Ensure logger is cleared
        runOnMainThread(() -> AGenUI.getInstance().setCustomLogger(null));

        System.gc();
        Thread.sleep(500);
        long finalNative = Debug.getNativeHeapAllocatedSize() / (1024 * 1024);
        long growth = finalNative - baseNative;

        Log.i(TAG, "=== RISK21 RESULT ===");
        Log.i(TAG, "Rounds completed: " + TOTAL_ROUNDS);
        Log.i(TAG, "Total log callbacks: " + logCallCount.get());
        Log.i(TAG, "Java exceptions: " + javaExceptions.get());
        Log.i(TAG, "Native heap growth: " + growth + "MB");

        if (growth > 50) {
            Log.w(TAG, "SUSPICIOUS: native memory grew by " + growth
                    + "MB over " + TOTAL_ROUNDS + " rounds");
        }
    }
}
