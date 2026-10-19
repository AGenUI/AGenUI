package com.amap.agenuiplayground.tests;

import android.os.Debug;
import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.base.AGenUIBaseTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static org.junit.Assert.assertTrue;

/**
 * SDK Risk Probe: Native memory leak detection from repeated
 * SurfaceManager create → stream → destroy cycles.
 *
 * Hypothesis:
 * The native cleanup chain is complex:
 *   SurfaceManager → SurfaceCoordinator → Surface (unique_ptr)
 *     → VirtualDOM → VirtualDOMNode → YogaNode
 *   + EventDispatcher, StreamingContentParser, ProtocolStreamExtractor
 *   + FunctionCallManager with builtin FunctionCalls
 *   + cached listeners, worker-thread lambdas holding shared_from_this()
 *
 * Any missed cleanup path leaks native memory that accumulates
 * over repeated create/stream/destroy cycles.
 *
 * Probe style: pressure test (many cycles) + native heap monitoring
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeNativeMemoryLeakTest extends AGenUIBaseTest {

    private static final String TAG = "SDKRiskProbe12";

    @Override
    public void setUp() {
        // Only initialize AGenUI; we manage SMs ourselves
        activityRule.getScenario().onActivity(activity -> {
            if (!AGenUI.getInstance().isInitialized()) {
                AGenUI.getInstance().initialize(activity.getApplicationContext());
            }
        });
    }

    @Override
    public void tearDown() {
        // No-op: each test manages its own SM lifecycle
    }

    /**
     * RISK12: Repeated SM lifecycle cycles with full protocol streaming.
     *
     * Creates an SM, streams a full createSurface + updateComponents payload
     * creating multiple components (Column, Text, Button, Card), then destroys.
     * Repeats 300 times. Logs native heap size every 30 cycles.
     *
     * Leak detection: compares native heap at cycle 30 (warm-up) vs cycle 300.
     * If growth exceeds 2MB, flags as potential leak.
     */
    @Test
    public void testSDKRISK12_nativeMemoryLeakSMLifecycle() throws Exception {
        final int TOTAL_CYCLES = 300;
        final int LOG_INTERVAL = 30;
        final long LEAK_THRESHOLD_BYTES = 2 * 1024 * 1024; // 2MB

        long[] heapSamples = new long[TOTAL_CYCLES / LOG_INTERVAL + 1];
        int sampleIndex = 0;

        // Initial warm-up: create and destroy a few SMs to stabilize allocators
        for (int w = 0; w < 5; w++) {
            doOneLifecycle(w);
        }

        // Force GC and take baseline
        forceGC();
        long baseline = Debug.getNativeHeapAllocatedSize();
        Log.i(TAG, "=== BASELINE native heap: " + formatBytes(baseline));

        for (int cycle = 1; cycle <= TOTAL_CYCLES; cycle++) {
            doOneLifecycle(cycle);

            if (cycle % LOG_INTERVAL == 0) {
                forceGC();
                long currentHeap = Debug.getNativeHeapAllocatedSize();
                long delta = currentHeap - baseline;
                heapSamples[sampleIndex++] = currentHeap;
                Log.i(TAG, "=== Cycle " + cycle + "/" + TOTAL_CYCLES
                        + " native heap: " + formatBytes(currentHeap)
                        + " (delta from baseline: " + formatBytes(delta)
                        + ", " + (delta >= 0 ? "+" : "") + delta + " bytes)");
            }
        }

        // Final measurement
        forceGC();
        long finalHeap = Debug.getNativeHeapAllocatedSize();
        long totalGrowth = finalHeap - baseline;
        Log.i(TAG, "=== FINAL native heap: " + formatBytes(finalHeap)
                + " (total growth: " + formatBytes(totalGrowth) + ")");

        // Log all samples for trend analysis
        Log.i(TAG, "=== Heap samples (every " + LOG_INTERVAL + " cycles):");
        for (int i = 0; i < sampleIndex; i++) {
            Log.i(TAG, "  Sample " + (i + 1) + ": " + formatBytes(heapSamples[i])
                    + " (delta: " + formatBytes(heapSamples[i] - baseline) + ")");
        }

        // Use sample at cycle 30 as warm-up completed reference
        long warmUpHeap = heapSamples[0]; // at cycle 30
        long growthSinceWarmUp = finalHeap - warmUpHeap;
        Log.i(TAG, "=== Growth since warm-up (cycle 30→" + TOTAL_CYCLES + "): "
                + formatBytes(growthSinceWarmUp));

        if (growthSinceWarmUp > LEAK_THRESHOLD_BYTES) {
            Log.e(TAG, "*** POTENTIAL NATIVE MEMORY LEAK DETECTED ***");
            Log.e(TAG, "  Growth: " + formatBytes(growthSinceWarmUp)
                    + " exceeds threshold: " + formatBytes(LEAK_THRESHOLD_BYTES));
        }

        // Assert: growth since warm-up should not exceed threshold
        assertTrue("Native memory grew by " + formatBytes(growthSinceWarmUp)
                        + " (>" + formatBytes(LEAK_THRESHOLD_BYTES)
                        + ") over " + (TOTAL_CYCLES - LOG_INTERVAL)
                        + " SM lifecycle cycles — possible native leak",
                growthSinceWarmUp <= LEAK_THRESHOLD_BYTES);
    }

    /**
     * RISK13: Same as RISK12 but with richer protocol content per cycle.
     *
     * Each cycle streams createSurface + two updateComponents batches,
     * then a deleteSurface, then endTextStream, then SM destroy.
     * This exercises deeper native object graph creation and teardown.
     */
    @Test
    public void testSDKRISK13_nativeMemoryLeakRichContent() throws Exception {
        final int TOTAL_CYCLES = 200;
        final int LOG_INTERVAL = 20;
        final long LEAK_THRESHOLD_BYTES = 3 * 1024 * 1024; // 3MB

        // Warm-up
        for (int w = 0; w < 5; w++) {
            doRichLifecycle(w);
        }

        forceGC();
        long baseline = Debug.getNativeHeapAllocatedSize();
        Log.i(TAG, "=== RICH BASELINE native heap: " + formatBytes(baseline));

        long warmUpHeap = 0;
        for (int cycle = 1; cycle <= TOTAL_CYCLES; cycle++) {
            doRichLifecycle(cycle);

            if (cycle % LOG_INTERVAL == 0) {
                forceGC();
                long currentHeap = Debug.getNativeHeapAllocatedSize();
                long delta = currentHeap - baseline;
                if (cycle == LOG_INTERVAL) {
                    warmUpHeap = currentHeap;
                }
                Log.i(TAG, "=== RICH Cycle " + cycle + "/" + TOTAL_CYCLES
                        + " native heap: " + formatBytes(currentHeap)
                        + " (delta: " + formatBytes(delta) + ")");
            }
        }

        forceGC();
        long finalHeap = Debug.getNativeHeapAllocatedSize();
        long growthSinceWarmUp = finalHeap - (warmUpHeap > 0 ? warmUpHeap : baseline);
        Log.i(TAG, "=== RICH FINAL native heap: " + formatBytes(finalHeap)
                + " (growth since warm-up: " + formatBytes(growthSinceWarmUp) + ")");

        if (growthSinceWarmUp > LEAK_THRESHOLD_BYTES) {
            Log.e(TAG, "*** POTENTIAL NATIVE MEMORY LEAK (RICH) ***");
        }

        assertTrue("Native memory grew by " + formatBytes(growthSinceWarmUp)
                        + " (>" + formatBytes(LEAK_THRESHOLD_BYTES)
                        + ") over " + TOTAL_CYCLES
                        + " rich SM lifecycle cycles — possible native leak",
                growthSinceWarmUp <= LEAK_THRESHOLD_BYTES);
    }

    // --------------- helpers ---------------

    private void doOneLifecycle(int cycleId) throws Exception {
        final SurfaceManager[] holder = new SurfaceManager[1];
        final CountDownLatch created = new CountDownLatch(1);

        runOnActivity(activity -> {
            holder[0] = new SurfaceManager(activity);
            created.countDown();
        });
        assertTrue("SM creation timeout", created.await(5, TimeUnit.SECONDS));

        SurfaceManager sm = holder[0];
        String sid = "leak-" + cycleId;

        // Stream a full lifecycle: begin → createSurface + components → end
        sm.beginTextStream();
        sm.receiveTextChunk(buildCreateSurfacePayload(sid));
        sm.receiveTextChunk(buildUpdateComponentsPayload(sid));
        sm.endTextStream();

        // Brief pause to let worker thread process
        Thread.sleep(20);

        // Destroy on main thread
        final CountDownLatch destroyed = new CountDownLatch(1);
        runOnActivity(activity -> {
            sm.destroy();
            destroyed.countDown();
        });
        assertTrue("SM destroy timeout", destroyed.await(5, TimeUnit.SECONDS));
    }

    private void doRichLifecycle(int cycleId) throws Exception {
        final SurfaceManager[] holder = new SurfaceManager[1];
        final CountDownLatch created = new CountDownLatch(1);

        runOnActivity(activity -> {
            holder[0] = new SurfaceManager(activity);
            created.countDown();
        });
        assertTrue("SM creation timeout", created.await(5, TimeUnit.SECONDS));

        SurfaceManager sm = holder[0];

        // Create 3 surfaces per cycle
        sm.beginTextStream();
        for (int s = 0; s < 3; s++) {
            String sid = "rich-" + cycleId + "-s" + s;
            sm.receiveTextChunk(buildCreateSurfacePayload(sid));
            sm.receiveTextChunk(buildRichUpdatePayload(sid, 8)); // 8 components per surface
        }
        sm.endTextStream();

        Thread.sleep(30);

        // Destroy on main thread
        final CountDownLatch destroyed = new CountDownLatch(1);
        runOnActivity(activity -> {
            sm.destroy();
            destroyed.countDown();
        });
        assertTrue("SM destroy timeout", destroyed.await(5, TimeUnit.SECONDS));
    }

    private static String buildCreateSurfacePayload(String surfaceId) {
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\""
                + surfaceId + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";
    }

    private static String buildUpdateComponentsPayload(String surfaceId) {
        return "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId
                + "\",\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"t1\",\"t2\",\"b1\"],"
                + "\"align\":\"stretch\",\"styles\":{\"width\":\"100%\",\"height\":\"auto\"}},"
                + "{\"id\":\"t1\",\"component\":\"Text\",\"text\":\"Leak test title\","
                + "\"styles\":{\"fontSize\":\"18px\",\"fontWeight\":\"bold\"}},"
                + "{\"id\":\"t2\",\"component\":\"Text\",\"text\":\"Leak test body\","
                + "\"styles\":{\"fontSize\":\"14px\"}},"
                + "{\"id\":\"b1\",\"component\":\"Button\",\"label\":\"Action\","
                + "\"styles\":{\"marginTop\":\"12px\"}}"
                + "]}}";
    }

    private static String buildRichUpdatePayload(String surfaceId, int componentCount) {
        StringBuilder sb = new StringBuilder();
        sb.append("{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"")
                .append(surfaceId)
                .append("\",\"components\":[");

        // Root Column
        sb.append("{\"id\":\"root\",\"component\":\"Column\",\"children\":[");
        for (int i = 0; i < componentCount; i++) {
            if (i > 0) sb.append(",");
            sb.append("\"c").append(i).append("\"");
        }
        sb.append("],\"align\":\"stretch\",\"styles\":{\"width\":\"100%\",\"height\":\"auto\"}}");

        // Child components: alternate Text and Button
        for (int i = 0; i < componentCount; i++) {
            sb.append(",{\"id\":\"c").append(i).append("\",\"component\":\"");
            if (i % 2 == 0) {
                sb.append("Text\",\"text\":\"Item ").append(i).append("\"");
            } else {
                sb.append("Button\",\"label\":\"Btn ").append(i).append("\"");
            }
            sb.append(",\"styles\":{\"marginTop\":\"4px\"}}");
        }
    
        sb.append("]}}");
        return sb.toString();
    }

    /**
     * RISK50: Same-SM repeated surface create/delete/dataModel cycle — native memory leak detection.
     *
     * Hypothesis: Keeping the SAME SurfaceManager alive across many stream sessions
     * (each session creates surfaces with components + data models, then deletes them)
     * may leak native memory from:
     * - SurfaceCoordinator residual allocations after surface deletion
     * - DataModel binding table entries not fully unbound on surface destroy
     * - VirtualDOMNode / YogaNode allocations not returned to the system
     * - ProtocolStreamExtractor _dataBuffer capacity never shrinking
     *
     * This simulates a real chat session: SM lives long, surfaces come and go.
     * RISK12/13 tested create-SM-destroy-SM cycles; RISK50 tests surface lifecycle
     * WITHIN a persistent SM.
     *
     * Evidence standard: Per-cycle leak rate calculated from linear growth over
     * 500 surface lifecycle iterations. Time-based extrapolation provided.
     */
    @Test
    public void testSDKRISK50_sameSM_surfaceLifecycleLeak() throws Exception {
        final String TAG50 = "RISK50_SameSM_Leak";
        final int TOTAL_ITERATIONS = 500;
        final int CHECKPOINT_INTERVAL = 50;
        final int WARMUP_CYCLES = 20;
        // Threshold: if per-iteration leak > 200 bytes consistently, that's a real leak
        // 200 bytes × 10000 iterations/hour (typical usage) = ~2MB/hour
        final long PER_ITER_LEAK_THRESHOLD_BYTES = 200;

        Log.i(TAG50, "=== RISK50: Same-SM surface lifecycle leak detection ===");
        Log.i(TAG50, "Config: " + TOTAL_ITERATIONS + " iterations, checkpoint every " + CHECKPOINT_INTERVAL);

        // Create a single SM that lives for the entire test
        final SurfaceManager[] holder = new SurfaceManager[1];
        final CountDownLatch created = new CountDownLatch(1);
        runOnActivity(activity -> {
            holder[0] = new SurfaceManager(activity);
            created.countDown();
        });
        assertTrue("SM creation timeout", created.await(5, TimeUnit.SECONDS));
        SurfaceManager sm = holder[0];

        // Warm-up: stabilize allocators with initial cycles
        Log.i(TAG50, "Warming up with " + WARMUP_CYCLES + " cycles...");
        for (int w = 0; w < WARMUP_CYCLES; w++) {
            doSurfaceLifecycleOnSameSM(sm, "warmup-" + w);
        }
        Thread.sleep(200); // Let async tasks complete

        // Take baseline measurement
        forceGC();
        long baselineNative = Debug.getNativeHeapAllocatedSize();
        long baselineJava = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();
        Log.i(TAG50, "=== BASELINE: native=" + formatBytes(baselineNative)
                + ", java=" + formatBytes(baselineJava));

        // Measurement phase
        long[] nativeCheckpoints = new long[(TOTAL_ITERATIONS / CHECKPOINT_INTERVAL) + 1];
        int checkpointIdx = 0;
        long startTimeMs = System.currentTimeMillis();

        for (int iter = 1; iter <= TOTAL_ITERATIONS; iter++) {
            doSurfaceLifecycleOnSameSM(sm, "iter-" + iter);

            if (iter % CHECKPOINT_INTERVAL == 0) {
                // Brief pause to let worker thread process deletions
                Thread.sleep(50);
                forceGC();
                long currentNative = Debug.getNativeHeapAllocatedSize();
                long deltaNative = currentNative - baselineNative;
                nativeCheckpoints[checkpointIdx++] = currentNative;

                Log.i(TAG50, String.format("  Checkpoint %d/%d (iter %d): native=%s (delta=%s, %+d bytes)",
                        checkpointIdx, TOTAL_ITERATIONS / CHECKPOINT_INTERVAL,
                        iter, formatBytes(currentNative), formatBytes(deltaNative), deltaNative));
            }
        }

        long endTimeMs = System.currentTimeMillis();
        long elapsedMs = endTimeMs - startTimeMs;

        // Final measurement
        Thread.sleep(300); // Extra time for async cleanup
        forceGC();
        long finalNative = Debug.getNativeHeapAllocatedSize();
        long totalGrowth = finalNative - baselineNative;
        long finalJava = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();

        Log.i(TAG50, "=== FINAL: native=" + formatBytes(finalNative)
                + " (growth=" + formatBytes(totalGrowth) + ")"
                + ", java=" + formatBytes(finalJava)
                + " (java growth=" + formatBytes(finalJava - baselineJava) + ")");

        // Statistical analysis: calculate per-iteration leak rate
        // Use growth from first checkpoint to last checkpoint (skip warmup noise)
        long firstCheckpoint = nativeCheckpoints[0];
        long lastCheckpoint = nativeCheckpoints[checkpointIdx - 1];
        long growthPostWarmup = lastCheckpoint - firstCheckpoint;
        int iterationsPostWarmup = TOTAL_ITERATIONS - CHECKPOINT_INTERVAL;
        double perIterLeak = (double) growthPostWarmup / iterationsPostWarmup;

        // Time extrapolation
        double msPerIter = (double) elapsedMs / TOTAL_ITERATIONS;
        double itersPerHour = 3600000.0 / msPerIter;
        double leakPerHourMB = (perIterLeak * itersPerHour) / (1024.0 * 1024.0);

        Log.i(TAG50, "=== ANALYSIS ===");
        Log.i(TAG50, String.format("  Total elapsed: %d ms (%.1f ms/iter)", elapsedMs, msPerIter));
        Log.i(TAG50, String.format("  Growth (checkpoint 1→last): %s over %d iters",
                formatBytes(growthPostWarmup), iterationsPostWarmup));
        Log.i(TAG50, String.format("  Per-iteration leak rate: %.1f bytes/iter", perIterLeak));
        Log.i(TAG50, String.format("  Extrapolated: %.2f MB/hour (at test speed)", leakPerHourMB));

        // Log all checkpoints for trend visualization
        Log.i(TAG50, "=== CHECKPOINT TREND ===");
        for (int i = 0; i < checkpointIdx; i++) {
            long cp = nativeCheckpoints[i];
            Log.i(TAG50, String.format("  [%3d] iter=%3d native=%s delta=%s",
                    i + 1, (i + 1) * CHECKPOINT_INTERVAL,
                    formatBytes(cp), formatBytes(cp - baselineNative)));
        }

        // Monotonic growth detection: count how many checkpoints show growth vs previous
        int growthCount = 0;
        for (int i = 1; i < checkpointIdx; i++) {
            if (nativeCheckpoints[i] > nativeCheckpoints[i - 1]) {
                growthCount++;
            }
        }
        double growthRatio = (double) growthCount / (checkpointIdx - 1);
        Log.i(TAG50, String.format("  Monotonic growth ratio: %.0f%% (%d/%d checkpoints growing)",
                growthRatio * 100, growthCount, checkpointIdx - 1));

        // Destroy the SM
        final CountDownLatch destroyed = new CountDownLatch(1);
        runOnActivity(activity -> {
            sm.destroy();
            destroyed.countDown();
        });
        assertTrue("SM destroy timeout", destroyed.await(5, TimeUnit.SECONDS));

        // Verdict
        boolean isLeak = perIterLeak > PER_ITER_LEAK_THRESHOLD_BYTES && growthRatio > 0.6;
        if (isLeak) {
            Log.e(TAG50, "*** MEMORY LEAK DETECTED ***");
            Log.e(TAG50, String.format("  Per-iter leak: %.1f bytes (threshold: %d bytes)",
                    perIterLeak, PER_ITER_LEAK_THRESHOLD_BYTES));
            Log.e(TAG50, String.format("  Monotonic growth: %.0f%% (threshold: 60%%)", growthRatio * 100));
            Log.e(TAG50, String.format("  Projected: %.2f MB/hour", leakPerHourMB));
        } else {
            Log.i(TAG50, "RISK50 NOT HIT: No significant monotonic native memory growth detected");
            Log.i(TAG50, String.format("  Per-iter: %.1f bytes (threshold: %d)", perIterLeak, PER_ITER_LEAK_THRESHOLD_BYTES));
        }

        // Fail the test if leak is confirmed (provides JUnit evidence)
        if (isLeak) {
            assertTrue("RISK50 LEAK: native memory grew by " + formatBytes(totalGrowth)
                            + " over " + TOTAL_ITERATIONS + " iterations (" 
                            + String.format("%.1f bytes/iter, %.2f MB/hour projected", perIterLeak, leakPerHourMB) + ")",
                    false);
        }
    }

    /**
     * RISK51: Minimal bare surface create/delete cycle — native memory leak diagnostic.
     *
     * Hypothesis: Even WITHOUT any updateComponents or updateDataModel, just creating
     * and destroying empty surfaces within a persistent SM leaks native memory.
     * This isolates whether RISK50's leak comes from:
     *   A) Surface object lifecycle itself (DataModel, VirtualDOM, ComponentManager alloc/dealloc)
     *   B) The component/dataModel operations within a surface
     *
     * If RISK51 leaks similarly to RISK50 → the leak is in Surface creation/destruction.
     * If RISK51 does NOT leak → the leak is in updateComponents/updateDataModel paths.
     */
    @Test
    public void testSDKRISK51_bareSurfaceCreateDeleteLeak() throws Exception {
        final String TAG51 = "RISK51_BareSurface_Leak";
        final int TOTAL_ITERATIONS = 500;
        final int CHECKPOINT_INTERVAL = 50;
        final int WARMUP_CYCLES = 20;
        final long PER_ITER_LEAK_THRESHOLD_BYTES = 200;

        Log.i(TAG51, "=== RISK51: Bare surface create/delete leak diagnostic ===");
        Log.i(TAG51, "Config: " + TOTAL_ITERATIONS + " iterations, checkpoint every " + CHECKPOINT_INTERVAL);
        Log.i(TAG51, "NOTE: No updateComponents, no updateDataModel — just createSurface + deleteSurface");

        // Create a single SM that lives for the entire test
        final SurfaceManager[] holder = new SurfaceManager[1];
        final CountDownLatch created = new CountDownLatch(1);
        runOnActivity(activity -> {
            holder[0] = new SurfaceManager(activity);
            created.countDown();
        });
        assertTrue("SM creation timeout", created.await(5, TimeUnit.SECONDS));
        SurfaceManager sm = holder[0];

        // Warm-up
        Log.i(TAG51, "Warming up with " + WARMUP_CYCLES + " bare cycles...");
        for (int w = 0; w < WARMUP_CYCLES; w++) {
            doBareSurfaceLifecycle(sm, "warmup-" + w);
        }
        Thread.sleep(200);

        // Baseline
        forceGC();
        long baselineNative = Debug.getNativeHeapAllocatedSize();
        Log.i(TAG51, "=== BASELINE: native=" + formatBytes(baselineNative));

        // Measurement phase
        long[] nativeCheckpoints = new long[(TOTAL_ITERATIONS / CHECKPOINT_INTERVAL) + 1];
        int checkpointIdx = 0;
        long startTimeMs = System.currentTimeMillis();

        for (int iter = 1; iter <= TOTAL_ITERATIONS; iter++) {
            doBareSurfaceLifecycle(sm, "bare-" + iter);

            if (iter % CHECKPOINT_INTERVAL == 0) {
                Thread.sleep(50);
                forceGC();
                long currentNative = Debug.getNativeHeapAllocatedSize();
                long deltaNative = currentNative - baselineNative;
                nativeCheckpoints[checkpointIdx++] = currentNative;

                Log.i(TAG51, String.format("  Checkpoint %d/%d (iter %d): native=%s (delta=%s, %+d bytes)",
                        checkpointIdx, TOTAL_ITERATIONS / CHECKPOINT_INTERVAL,
                        iter, formatBytes(currentNative), formatBytes(deltaNative), deltaNative));
            }
        }

        long endTimeMs = System.currentTimeMillis();
        long elapsedMs = endTimeMs - startTimeMs;

        // Final
        Thread.sleep(300);
        forceGC();
        long finalNative = Debug.getNativeHeapAllocatedSize();
        long totalGrowth = finalNative - baselineNative;

        Log.i(TAG51, "=== FINAL: native=" + formatBytes(finalNative)
                + " (growth=" + formatBytes(totalGrowth) + ")");

        // Statistical analysis
        long firstCheckpoint = nativeCheckpoints[0];
        long lastCheckpoint = nativeCheckpoints[checkpointIdx - 1];
        long growthPostWarmup = lastCheckpoint - firstCheckpoint;
        int iterationsPostWarmup = TOTAL_ITERATIONS - CHECKPOINT_INTERVAL;
        double perIterLeak = (double) growthPostWarmup / iterationsPostWarmup;

        // Time extrapolation
        double msPerIter = (double) elapsedMs / TOTAL_ITERATIONS;
        double itersPerHour = 3600000.0 / msPerIter;
        double leakPerHourMB = (perIterLeak * itersPerHour) / (1024.0 * 1024.0);

        Log.i(TAG51, "=== ANALYSIS ===");
        Log.i(TAG51, String.format("  Total elapsed: %d ms (%.1f ms/iter)", elapsedMs, msPerIter));
        Log.i(TAG51, String.format("  Growth (checkpoint 1->last): %s over %d iters",
                formatBytes(growthPostWarmup), iterationsPostWarmup));
        Log.i(TAG51, String.format("  Per-iteration leak rate: %.1f bytes/iter", perIterLeak));
        Log.i(TAG51, String.format("  Extrapolated: %.2f MB/hour (at test speed)", leakPerHourMB));

        // Comparison with RISK50
        Log.i(TAG51, "=== COMPARISON WITH RISK50 ===");
        Log.i(TAG51, "  RISK50 per-iter: ~3513 bytes/iter (with 15 components + dataModel)");
        Log.i(TAG51, String.format("  RISK51 per-iter: %.1f bytes/iter (bare create/delete)", perIterLeak));
        if (perIterLeak > 100) {
            double componentContribution = 1.0 - (perIterLeak / 3513.0);
            Log.i(TAG51, String.format("  Component/DataModel contribution to RISK50: %.0f%%",
                    componentContribution * 100));
        }

        // Checkpoint trend
        Log.i(TAG51, "=== CHECKPOINT TREND ===");
        for (int i = 0; i < checkpointIdx; i++) {
            long cp = nativeCheckpoints[i];
            Log.i(TAG51, String.format("  [%3d] iter=%3d native=%s delta=%s",
                    i + 1, (i + 1) * CHECKPOINT_INTERVAL,
                    formatBytes(cp), formatBytes(cp - baselineNative)));
        }

        // Monotonic growth detection
        int growthCount = 0;
        for (int i = 1; i < checkpointIdx; i++) {
            if (nativeCheckpoints[i] > nativeCheckpoints[i - 1]) {
                growthCount++;
            }
        }
        double growthRatio = (double) growthCount / (checkpointIdx - 1);
        Log.i(TAG51, String.format("  Monotonic growth ratio: %.0f%% (%d/%d checkpoints growing)",
                growthRatio * 100, growthCount, checkpointIdx - 1));

        // Destroy SM
        final CountDownLatch destroyed = new CountDownLatch(1);
        runOnActivity(activity -> {
            sm.destroy();
            destroyed.countDown();
        });
        assertTrue("SM destroy timeout", destroyed.await(5, TimeUnit.SECONDS));

        // Verdict
        boolean isLeak = perIterLeak > PER_ITER_LEAK_THRESHOLD_BYTES && growthRatio > 0.6;
        if (isLeak) {
            Log.e(TAG51, "*** BARE SURFACE LEAK DETECTED ***");
            Log.e(TAG51, String.format("  Per-iter leak: %.1f bytes (threshold: %d bytes)",
                    perIterLeak, PER_ITER_LEAK_THRESHOLD_BYTES));
            Log.e(TAG51, String.format("  Monotonic growth: %.0f%% (threshold: 60%%)", growthRatio * 100));
            Log.e(TAG51, "  CONCLUSION: Leak is in Surface object lifecycle itself,");
            Log.e(TAG51, "  NOT in updateComponents/updateDataModel paths.");
        } else {
            Log.i(TAG51, "RISK51 NOT HIT: No significant leak in bare surface lifecycle");
            Log.i(TAG51, "  CONCLUSION: RISK50 leak is in updateComponents/updateDataModel paths.");
        }

        // Fail on leak detection
        if (isLeak) {
            assertTrue("RISK51 LEAK: bare surface create/delete grew by " + formatBytes(totalGrowth)
                            + " over " + TOTAL_ITERATIONS + " iterations ("
                            + String.format("%.1f bytes/iter, %.2f MB/hour projected", perIterLeak, leakPerHourMB) + ")",
                    false);
        }
    }

    /**
     * Performs one BARE surface lifecycle: just createSurface + deleteSurface, no components.
     */
    private void doBareSurfaceLifecycle(SurfaceManager sm, String surfaceId) throws Exception {
        sm.beginTextStream();
        sm.receiveTextChunk(buildCreateSurfacePayload(surfaceId));
        sm.receiveTextChunk("{\"version\":\"v0.9\",\"deleteSurface\":{\"surfaceId\":\"" + surfaceId + "\"}}");
        sm.endTextStream();
        Thread.sleep(5);
    }

    /**
     * RISK52: Surface + Components only (NO DataModel) — leak isolation diagnostic.
     *
     * Hypothesis: The RISK50 leak is specifically in the updateComponents → VirtualDOM/Yoga/
     * ComponentManager path, NOT in DataModel operations.
     *
     * Test: createSurface + updateComponents(15 components) + deleteSurface
     *       (same components as RISK50, but without updateDataModel/appendDataModel)
     *
     * Expected outcomes:
     *   - If RISK52 leaks ~3500 bytes/iter → leak is 100% in component chain
     *   - If RISK52 leaks ~2000 bytes/iter → leak is split between components and DataModel
     *   - If RISK52 does NOT leak → leak is in DataModel operations
     */
    @Test
    public void testSDKRISK52_componentsOnlyLeak() throws Exception {
        final String TAG52 = "RISK52_ComponentsOnly";
        final int TOTAL_ITERATIONS = 500;
        final int CHECKPOINT_INTERVAL = 50;
        final int WARMUP_CYCLES = 20;
        final long PER_ITER_LEAK_THRESHOLD_BYTES = 200;

        Log.i(TAG52, "=== RISK52: Components-only leak isolation ===");
        Log.i(TAG52, "Config: " + TOTAL_ITERATIONS + " iterations, checkpoint every " + CHECKPOINT_INTERVAL);
        Log.i(TAG52, "NOTE: createSurface + updateComponents(15) + deleteSurface. NO DataModel ops.");

        // Create persistent SM
        final SurfaceManager[] holder = new SurfaceManager[1];
        final CountDownLatch created = new CountDownLatch(1);
        runOnActivity(activity -> {
            holder[0] = new SurfaceManager(activity);
            created.countDown();
        });
        assertTrue("SM creation timeout", created.await(5, TimeUnit.SECONDS));
        SurfaceManager sm = holder[0];

        // Warm-up
        for (int w = 0; w < WARMUP_CYCLES; w++) {
            doComponentsOnlyLifecycle(sm, "warmup-" + w);
        }
        Thread.sleep(200);

        // Baseline
        forceGC();
        long baselineNative = Debug.getNativeHeapAllocatedSize();
        Log.i(TAG52, "=== BASELINE: native=" + formatBytes(baselineNative));

        // Measurement
        long[] nativeCheckpoints = new long[(TOTAL_ITERATIONS / CHECKPOINT_INTERVAL) + 1];
        int checkpointIdx = 0;
        long startTimeMs = System.currentTimeMillis();

        for (int iter = 1; iter <= TOTAL_ITERATIONS; iter++) {
            doComponentsOnlyLifecycle(sm, "comp-" + iter);

            if (iter % CHECKPOINT_INTERVAL == 0) {
                Thread.sleep(50);
                forceGC();
                long currentNative = Debug.getNativeHeapAllocatedSize();
                long deltaNative = currentNative - baselineNative;
                nativeCheckpoints[checkpointIdx++] = currentNative;

                Log.i(TAG52, String.format("  Checkpoint %d/%d (iter %d): native=%s (delta=%s, %+d bytes)",
                        checkpointIdx, TOTAL_ITERATIONS / CHECKPOINT_INTERVAL,
                        iter, formatBytes(currentNative), formatBytes(deltaNative), deltaNative));
            }
        }

        long endTimeMs = System.currentTimeMillis();
        long elapsedMs = endTimeMs - startTimeMs;

        // Final
        Thread.sleep(300);
        forceGC();
        long finalNative = Debug.getNativeHeapAllocatedSize();
        long totalGrowth = finalNative - baselineNative;

        Log.i(TAG52, "=== FINAL: native=" + formatBytes(finalNative)
                + " (growth=" + formatBytes(totalGrowth) + ")");

        // Analysis
        long firstCheckpoint = nativeCheckpoints[0];
        long lastCheckpoint = nativeCheckpoints[checkpointIdx - 1];
        long growthPostWarmup = lastCheckpoint - firstCheckpoint;
        int iterationsPostWarmup = TOTAL_ITERATIONS - CHECKPOINT_INTERVAL;
        double perIterLeak = (double) growthPostWarmup / iterationsPostWarmup;
        double msPerIter = (double) elapsedMs / TOTAL_ITERATIONS;
        double itersPerHour = 3600000.0 / msPerIter;
        double leakPerHourMB = (perIterLeak * itersPerHour) / (1024.0 * 1024.0);

        Log.i(TAG52, "=== ANALYSIS ===");
        Log.i(TAG52, String.format("  Total elapsed: %d ms (%.1f ms/iter)", elapsedMs, msPerIter));
        Log.i(TAG52, String.format("  Growth (checkpoint 1->last): %s over %d iters",
                formatBytes(growthPostWarmup), iterationsPostWarmup));
        Log.i(TAG52, String.format("  Per-iteration leak rate: %.1f bytes/iter", perIterLeak));
        Log.i(TAG52, String.format("  Extrapolated: %.2f MB/hour", leakPerHourMB));

        // Comparison summary
        Log.i(TAG52, "=== LEAK SOURCE ISOLATION ===");
        Log.i(TAG52, "  RISK51 (bare create/delete):    3.8 bytes/iter  [no leak]");
        Log.i(TAG52, String.format("  RISK52 (components only):     %.1f bytes/iter", perIterLeak));
        Log.i(TAG52, "  RISK50 (components + dataModel): 3513 bytes/iter [confirmed leak]");

        // Checkpoint trend
        Log.i(TAG52, "=== CHECKPOINT TREND ===");
        for (int i = 0; i < checkpointIdx; i++) {
            long cp = nativeCheckpoints[i];
            Log.i(TAG52, String.format("  [%3d] iter=%3d native=%s delta=%s",
                    i + 1, (i + 1) * CHECKPOINT_INTERVAL,
                    formatBytes(cp), formatBytes(cp - baselineNative)));
        }

        // Monotonic growth
        int growthCount = 0;
        for (int i = 1; i < checkpointIdx; i++) {
            if (nativeCheckpoints[i] > nativeCheckpoints[i - 1]) {
                growthCount++;
            }
        }
        double growthRatio = (double) growthCount / (checkpointIdx - 1);
        Log.i(TAG52, String.format("  Monotonic growth ratio: %.0f%% (%d/%d)",
                growthRatio * 100, growthCount, checkpointIdx - 1));

        // Destroy SM
        final CountDownLatch destroyed = new CountDownLatch(1);
        runOnActivity(activity -> {
            sm.destroy();
            destroyed.countDown();
        });
        assertTrue("SM destroy timeout", destroyed.await(5, TimeUnit.SECONDS));

        // Verdict
        boolean isLeak = perIterLeak > PER_ITER_LEAK_THRESHOLD_BYTES && growthRatio > 0.6;
        if (isLeak) {
            Log.e(TAG52, "*** COMPONENTS-ONLY LEAK DETECTED ***");
            Log.e(TAG52, "  CONCLUSION: VirtualDOM/Yoga/ComponentManager teardown leaks memory.");
            double dataModelContribution = 1.0 - (perIterLeak / 3513.0);
            Log.e(TAG52, String.format("  DataModel contribution to RISK50: ~%.0f%%", dataModelContribution * 100));
        } else {
            Log.i(TAG52, "RISK52 NOT HIT: Components-only path does not leak significantly.");
            Log.i(TAG52, "  CONCLUSION: RISK50 leak is primarily in DataModel operations.");
        }

        if (isLeak) {
            assertTrue("RISK52 LEAK: components-only grew by " + formatBytes(totalGrowth)
                            + " over " + TOTAL_ITERATIONS + " iterations ("
                            + String.format("%.1f bytes/iter, %.2f MB/hour", perIterLeak, leakPerHourMB) + ")",
                    false);
        }
    }

    /**
     * Components-only lifecycle: createSurface + updateComponents(15) + deleteSurface. No DataModel.
     */
    private void doComponentsOnlyLifecycle(SurfaceManager sm, String surfaceId) throws Exception {
        sm.beginTextStream();
        sm.receiveTextChunk(buildCreateSurfacePayload(surfaceId));
        sm.receiveTextChunk(buildHeavyUpdatePayload(surfaceId));
        sm.receiveTextChunk("{\"version\":\"v0.9\",\"deleteSurface\":{\"surfaceId\":\"" + surfaceId + "\"}}");
        sm.endTextStream();
        Thread.sleep(5);
    }

    /**
     * RISK53: Batch surface create/delete cycles — memory not reclaimed.
     *
     * Hypothesis: Creating many surfaces simultaneously (each with components),
     * then deleting all of them, may not fully reclaim native memory due to:
     * - SurfaceCoordinator map fragmentation from batch insert/erase
     * - Cross-surface residual state in FunctionCallManager or EventDispatcher
     * - System allocator fragmentation from many concurrent alloc/dealloc patterns
     *
     * Test: 100 rounds. Each round creates 20 surfaces (each with 8 components),
     * then deletes all 20. Measures native heap growth across rounds.
     */
    @Test
    public void testSDKRISK53_batchSurfaceCreateDeleteLeak() throws Exception {
        final String TAG53 = "RISK53_BatchSurface";
        final int TOTAL_ROUNDS = 100;
        final int SURFACES_PER_ROUND = 20;
        final int CHECKPOINT_INTERVAL = 10;
        final int WARMUP_ROUNDS = 5;
        final long PER_ROUND_LEAK_THRESHOLD_BYTES = 4000; // 4KB per round of 20 surfaces

        Log.i(TAG53, "=== RISK53: Batch surface create/delete leak detection ===");
        Log.i(TAG53, "Config: " + TOTAL_ROUNDS + " rounds, " + SURFACES_PER_ROUND
                + " surfaces/round, checkpoint every " + CHECKPOINT_INTERVAL);

        // Create persistent SM
        final SurfaceManager[] holder = new SurfaceManager[1];
        final CountDownLatch created = new CountDownLatch(1);
        runOnActivity(activity -> {
            holder[0] = new SurfaceManager(activity);
            created.countDown();
        });
        assertTrue("SM creation timeout", created.await(5, TimeUnit.SECONDS));
        SurfaceManager sm = holder[0];

        // Warm-up
        for (int w = 0; w < WARMUP_ROUNDS; w++) {
            doBatchSurfaceRound(sm, w, SURFACES_PER_ROUND);
        }
        Thread.sleep(300);

        // Baseline
        forceGC();
        long baselineNative = Debug.getNativeHeapAllocatedSize();
        Log.i(TAG53, "=== BASELINE: native=" + formatBytes(baselineNative));

        // Measurement
        long[] nativeCheckpoints = new long[(TOTAL_ROUNDS / CHECKPOINT_INTERVAL) + 1];
        int checkpointIdx = 0;
        long startTimeMs = System.currentTimeMillis();

        for (int round = 1; round <= TOTAL_ROUNDS; round++) {
            doBatchSurfaceRound(sm, round, SURFACES_PER_ROUND);

            if (round % CHECKPOINT_INTERVAL == 0) {
                Thread.sleep(100);
                forceGC();
                long currentNative = Debug.getNativeHeapAllocatedSize();
                long deltaNative = currentNative - baselineNative;
                nativeCheckpoints[checkpointIdx++] = currentNative;

                Log.i(TAG53, String.format("  Checkpoint %d/%d (round %d): native=%s (delta=%s, %+d bytes)",
                        checkpointIdx, TOTAL_ROUNDS / CHECKPOINT_INTERVAL,
                        round, formatBytes(currentNative), formatBytes(deltaNative), deltaNative));
            }
        }

        long endTimeMs = System.currentTimeMillis();
        long elapsedMs = endTimeMs - startTimeMs;

        // Final
        Thread.sleep(500);
        forceGC();
        long finalNative = Debug.getNativeHeapAllocatedSize();
        long totalGrowth = finalNative - baselineNative;

        Log.i(TAG53, "=== FINAL: native=" + formatBytes(finalNative)
                + " (growth=" + formatBytes(totalGrowth) + ")");

        // Analysis
        long firstCheckpoint = nativeCheckpoints[0];
        long lastCheckpoint = nativeCheckpoints[checkpointIdx - 1];
        long growthPostWarmup = lastCheckpoint - firstCheckpoint;
        int roundsPostWarmup = TOTAL_ROUNDS - CHECKPOINT_INTERVAL;
        double perRoundLeak = (double) growthPostWarmup / roundsPostWarmup;
        double perSurfaceLeak = perRoundLeak / SURFACES_PER_ROUND;
        double msPerRound = (double) elapsedMs / TOTAL_ROUNDS;
        double roundsPerHour = 3600000.0 / msPerRound;
        double leakPerHourMB = (perRoundLeak * roundsPerHour) / (1024.0 * 1024.0);

        Log.i(TAG53, "=== ANALYSIS ===");
        Log.i(TAG53, String.format("  Total elapsed: %d ms (%.1f ms/round)", elapsedMs, msPerRound));
        Log.i(TAG53, String.format("  Growth (checkpoint 1->last): %s over %d rounds",
                formatBytes(growthPostWarmup), roundsPostWarmup));
        Log.i(TAG53, String.format("  Per-round leak: %.1f bytes/round (%.1f bytes/surface)",
                perRoundLeak, perSurfaceLeak));
        Log.i(TAG53, String.format("  Extrapolated: %.2f MB/hour", leakPerHourMB));

        // Comparison with RISK50 single-surface rate
        Log.i(TAG53, "=== COMPARISON ===");
        Log.i(TAG53, String.format("  RISK50 per-surface: ~3513 bytes (1 surface at a time)"));
        Log.i(TAG53, String.format("  RISK53 per-surface: %.1f bytes (%d surfaces in batch)",
                perSurfaceLeak, SURFACES_PER_ROUND));

        // Checkpoint trend
        Log.i(TAG53, "=== CHECKPOINT TREND ===");
        for (int i = 0; i < checkpointIdx; i++) {
            long cp = nativeCheckpoints[i];
            Log.i(TAG53, String.format("  [%3d] round=%3d native=%s delta=%s",
                    i + 1, (i + 1) * CHECKPOINT_INTERVAL,
                    formatBytes(cp), formatBytes(cp - baselineNative)));
        }

        // Monotonic growth
        int growthCount = 0;
        for (int i = 1; i < checkpointIdx; i++) {
            if (nativeCheckpoints[i] > nativeCheckpoints[i - 1]) {
                growthCount++;
            }
        }
        double growthRatio = (double) growthCount / (checkpointIdx - 1);
        Log.i(TAG53, String.format("  Monotonic growth ratio: %.0f%% (%d/%d)",
                growthRatio * 100, growthCount, checkpointIdx - 1));

        // Destroy SM
        final CountDownLatch destroyed = new CountDownLatch(1);
        runOnActivity(activity -> {
            sm.destroy();
            destroyed.countDown();
        });
        assertTrue("SM destroy timeout", destroyed.await(5, TimeUnit.SECONDS));

        // Verdict
        boolean isLeak = perRoundLeak > PER_ROUND_LEAK_THRESHOLD_BYTES && growthRatio > 0.6;
        if (isLeak) {
            Log.e(TAG53, "*** BATCH SURFACE LEAK DETECTED ***");
            Log.e(TAG53, String.format("  Per-round leak: %.1f bytes (threshold: %d)",
                    perRoundLeak, PER_ROUND_LEAK_THRESHOLD_BYTES));
            Log.e(TAG53, String.format("  Per-surface leak: %.1f bytes", perSurfaceLeak));
        } else {
            Log.i(TAG53, "RISK53 NOT HIT: No significant batch surface leak detected");
        }

        if (isLeak) {
            assertTrue("RISK53 LEAK: batch create/delete grew by " + formatBytes(totalGrowth)
                            + " over " + TOTAL_ROUNDS + " rounds ("
                            + String.format("%.1f bytes/round, %.2f MB/hour", perRoundLeak, leakPerHourMB) + ")",
                    false);
        }
    }

    /**
     * One batch round: create N surfaces with components, then delete all.
     */
    private void doBatchSurfaceRound(SurfaceManager sm, int roundId, int surfaceCount)
            throws Exception {
        // Create all surfaces in one stream session
        sm.beginTextStream();
        for (int s = 0; s < surfaceCount; s++) {
            String sid = "batch-" + roundId + "-" + s;
            sm.receiveTextChunk(buildCreateSurfacePayload(sid));
            sm.receiveTextChunk(buildRichUpdatePayload(sid, 8));
        }
        sm.endTextStream();

        Thread.sleep(10);

        // Delete all surfaces
        sm.beginTextStream();
        for (int s = 0; s < surfaceCount; s++) {
            String sid = "batch-" + roundId + "-" + s;
            sm.receiveTextChunk("{\"version\":\"v0.9\",\"deleteSurface\":{\"surfaceId\":\"" + sid + "\"}}");
        }
        sm.endTextStream();

        Thread.sleep(10);
    }

    /**
     * Performs one surface lifecycle iteration on an EXISTING SurfaceManager:
     * begin -> createSurface -> updateComponents (15 components) -> updateDataModel -> deleteSurface -> end
     */
    private void doSurfaceLifecycleOnSameSM(SurfaceManager sm, String surfaceId) throws Exception {
        // Session 1: Create surface with rich components
        sm.beginTextStream();
        sm.receiveTextChunk(buildCreateSurfacePayload(surfaceId));
        sm.receiveTextChunk(buildHeavyUpdatePayload(surfaceId));
        sm.endTextStream();

        // Session 2: Update data model
        sm.beginTextStream();
        sm.receiveTextChunk(buildUpdateDataModelPayload(surfaceId));
        sm.receiveTextChunk(buildAppendDataModelPayload(surfaceId));
        sm.endTextStream();

        // Session 3: Delete the surface
        sm.beginTextStream();
        sm.receiveTextChunk("{\"version\":\"v0.9\",\"deleteSurface\":{\"surfaceId\":\"" + surfaceId + "\"}}");
        sm.endTextStream();

        // Brief pause to let worker thread process
        Thread.sleep(5);
    }

    /**
     * Heavy updateComponents payload: 15 components with nested layout,
     * styles, data bindings to stress Yoga + VirtualDOM + ComponentManager.
     */
    private static String buildHeavyUpdatePayload(String surfaceId) {
        StringBuilder sb = new StringBuilder(4096);
        sb.append("{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"")
                .append(surfaceId)
                .append("\",\"components\":[");

        // Root Column
        sb.append("{\"id\":\"root\",\"component\":\"Column\",\"children\":[");
        for (int i = 0; i < 15; i++) {
            if (i > 0) sb.append(",");
            sb.append("\"c").append(i).append("\"");
        }
        sb.append("],\"align\":\"stretch\",\"styles\":{\"width\":\"100%\",\"padding\":\"16px\"}}");

        // 15 child components: Text, Button, Card mix with styles
        for (int i = 0; i < 15; i++) {
            sb.append(",{\"id\":\"c").append(i).append("\",\"component\":\"");
            switch (i % 3) {
                case 0:
                    sb.append("Text\",\"text\":\"Content item ").append(i)
                            .append("\",\"styles\":{\"fontSize\":\"16px\",\"fontWeight\":\"bold\",")
                            .append("\"color\":\"#333333\",\"marginBottom\":\"8px\",\"lineHeight\":\"1.5\"}");
                    break;
                case 1:
                    sb.append("Button\",\"label\":\"Action ").append(i)
                            .append("\",\"styles\":{\"backgroundColor\":\"#007AFF\",\"borderRadius\":\"8px\",")
                            .append("\"paddingHorizontal\":\"16px\",\"paddingVertical\":\"10px\",\"marginBottom\":\"8px\"}");
                    break;
                case 2:
                    sb.append("Text\",\"text\":{\"bindingPath\":\"/items/").append(i).append("/value\"")
                            .append("},\"styles\":{\"fontSize\":\"14px\",\"color\":\"#666666\",\"marginBottom\":\"4px\"}");
                    break;
            }
            sb.append("}");
        }

        sb.append("]}}");
        return sb.toString();
    }

    /**
     * updateDataModel payload with nested JSON data.
     */
    private static String buildUpdateDataModelPayload(String surfaceId) {
        return "{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":\"" + surfaceId
                + "\",\"path\":\"/items\",\"value\":{\"0\":{\"value\":\"First item data\"},"
                + "\"1\":{\"value\":\"Second item data\"},\"2\":{\"value\":\"Third item data\"},"
                + "\"3\":{\"value\":\"Fourth item data\"},\"4\":{\"value\":\"Fifth item data\"}}}}";
    }

    /**
     * appendDataModel payload.
     */
    private static String buildAppendDataModelPayload(String surfaceId) {
        return "{\"version\":\"v0.9\",\"appendDataModel\":{\"surfaceId\":\"" + surfaceId
                + "\",\"path\":\"/extra\",\"value\":{\"timestamp\":\"2024-01-01T00:00:00Z\","
                + "\"metadata\":{\"source\":\"test\",\"version\":\"1.0\"}}}}";
    }

    private static void forceGC() {
        System.gc();
        System.runFinalization();
        System.gc();
        try {
            Thread.sleep(100);
        } catch (InterruptedException ignored) {
        }
    }

    private static String formatBytes(long bytes) {
        if (Math.abs(bytes) < 1024) return bytes + " B";
        double kb = bytes / 1024.0;
        if (Math.abs(kb) < 1024) return String.format("%.1f KB", kb);
        double mb = kb / 1024.0;
        return String.format("%.2f MB", mb);
    }
}
