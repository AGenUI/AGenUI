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
 * SDK Risk Probes: Extreme width tree and large payload boundary tests.
 *
 * RISK54: Extremely wide component tree (thousands of siblings under one root)
 *   - Tests Yoga layout under high node count (O(n) but memory pressure)
 *   - Verifies no OOM crash during layout calculation with many siblings
 *
 * RISK55: Very large single-field payload (multi-MB string)
 *   - Tests nlohmann JSON parsing with multi-MB text content
 *   - Verifies string copy path doesn't crash under memory pressure
 *
 * These are boundary probes — not deep stress tests.
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeWidthAndPayloadTest extends AGenUIBaseTest {

    private static final String TAG = "SDKRiskProbe_WidthPayload";

    @Override
    public void setUp() {
        activityRule.getScenario().onActivity(activity -> {
            if (!AGenUI.getInstance().isInitialized()) {
                AGenUI.getInstance().initialize(activity.getApplicationContext());
            }
        });
    }

    @Override
    public void tearDown() {
        // Each test manages its own SM lifecycle
    }

    // ========================================================================
    // RISK54: Extremely wide tree — 3000 siblings under a single Column
    // ========================================================================

    /**
     * RISK54: Create a surface with 3000 sibling Text components under one root Column.
     *
     * Risk hypothesis:
     * - Yoga layout with 3000 child nodes might hit memory limits on constrained devices
     * - VirtualDOM tree traversal for 3000 siblings may be slow but should not crash
     * - ComponentPropertySpec lookup × 3000 may expose allocation hotspots
     *
     * Probe style: boundary test (extreme width, not depth)
     * Success criteria: completes without crash/ANR within 30 seconds
     */
    @Test
    public void testSDKRISK54_extremelyWideTree_3000siblings() throws Exception {
        final String TAG54 = "RISK54_WideTree";
        final int SIBLING_COUNT = 3000;

        Log.i(TAG54, "=== RISK54: Wide tree test — " + SIBLING_COUNT + " siblings ===");

        // Create SM
        final SurfaceManager[] holder = new SurfaceManager[1];
        final CountDownLatch created = new CountDownLatch(1);
        runOnActivity(activity -> {
            holder[0] = new SurfaceManager(activity);
            created.countDown();
        });
        assertTrue("SM creation timeout", created.await(5, TimeUnit.SECONDS));
        SurfaceManager sm = holder[0];

        // Measure native heap before
        forceGC();
        long heapBefore = Debug.getNativeHeapAllocatedSize();
        Log.i(TAG54, "Native heap before: " + formatBytes(heapBefore));

        long startTime = System.currentTimeMillis();

        // Stream: createSurface + updateComponents with 3000 siblings
        sm.beginTextStream();
        sm.receiveTextChunk(buildCreateSurfacePayload("wide-tree-54"));
        sm.receiveTextChunk(buildWideTreePayload("wide-tree-54", SIBLING_COUNT));
        sm.endTextStream();

        // Wait for worker thread to process (3000 nodes may take time)
        Thread.sleep(3000);

        long elapsed = System.currentTimeMillis() - startTime;
        forceGC();
        long heapAfter = Debug.getNativeHeapAllocatedSize();
        long heapDelta = heapAfter - heapBefore;

        Log.i(TAG54, "=== RESULT ===");
        Log.i(TAG54, "  Siblings: " + SIBLING_COUNT);
        Log.i(TAG54, "  Processing time: " + elapsed + " ms");
        Log.i(TAG54, "  Native heap delta: " + formatBytes(heapDelta)
                + " (" + heapDelta + " bytes)");
        Log.i(TAG54, "  Per-node overhead: ~" + (heapDelta / SIBLING_COUNT) + " bytes/node");

        // Destroy
        final CountDownLatch destroyed = new CountDownLatch(1);
        runOnActivity(activity -> {
            sm.destroy();
            destroyed.countDown();
        });
        assertTrue("SM destroy timeout", destroyed.await(10, TimeUnit.SECONDS));

        // Post-destroy measurement
        Thread.sleep(500);
        forceGC();
        long heapPostDestroy = Debug.getNativeHeapAllocatedSize();
        long retained = heapPostDestroy - heapBefore;
        Log.i(TAG54, "  Heap post-destroy delta: " + formatBytes(retained));

        // Verdict: should complete without crash. If we got here, it passed.
        // Log warning if per-node overhead is unexpectedly high (> 10KB/node)
        long perNode = heapDelta / Math.max(1, SIBLING_COUNT);
        if (perNode > 10240) {
            Log.w(TAG54, "*** WARNING: Per-node overhead is " + perNode
                    + " bytes (> 10KB). Possible allocation issue.");
        }

        Log.i(TAG54, "=== RISK54 PASSED: No crash with " + SIBLING_COUNT + " siblings ===");
        assertTrue("Processing took > 30s (possible hang)",
                elapsed < 30000);
    }

    // ========================================================================
    // RISK55: Very large payload — 5MB text field
    // ========================================================================

    /**
     * RISK55: Stream a chunk containing a single Text component with a 5MB string value.
     *
     * Risk hypothesis:
     * - nlohmann::json parse of 5MB+ payload may allocate excessive intermediate memory
     * - std::string copy of 5MB text content through the parser pipeline
     * - ProtocolStreamExtractor's 10MB buffer limit means this should be accepted
     * - But the downstream VirtualDOM text storage may not handle multi-MB strings well
     *
     * Probe style: boundary test (extreme payload size)
     * Success criteria: completes without crash/OOM within 30 seconds
     */
    @Test
    public void testSDKRISK55_largePayload_5MB_text() throws Exception {
        final String TAG55 = "RISK55_LargePayload";
        final int TEXT_SIZE_MB = 5;
        final int TEXT_SIZE_BYTES = TEXT_SIZE_MB * 1024 * 1024;

        Log.i(TAG55, "=== RISK55: Large payload test — " + TEXT_SIZE_MB + "MB text field ===");

        // Create SM
        final SurfaceManager[] holder = new SurfaceManager[1];
        final CountDownLatch created = new CountDownLatch(1);
        runOnActivity(activity -> {
            holder[0] = new SurfaceManager(activity);
            created.countDown();
        });
        assertTrue("SM creation timeout", created.await(5, TimeUnit.SECONDS));
        SurfaceManager sm = holder[0];

        // Measure native heap before
        forceGC();
        long heapBefore = Debug.getNativeHeapAllocatedSize();
        Log.i(TAG55, "Native heap before: " + formatBytes(heapBefore));

        // Build the large payload
        Log.i(TAG55, "Building " + TEXT_SIZE_MB + "MB payload...");
        long buildStart = System.currentTimeMillis();
        String largePayload = buildLargeTextPayload("large-55", TEXT_SIZE_BYTES);
        long buildTime = System.currentTimeMillis() - buildStart;
        Log.i(TAG55, "Payload built in " + buildTime + "ms, size: "
                + formatBytes(largePayload.length()));

        long startTime = System.currentTimeMillis();

        // Stream the large payload
        sm.beginTextStream();
        sm.receiveTextChunk(buildCreateSurfacePayload("large-55"));
        sm.receiveTextChunk(largePayload);
        sm.endTextStream();

        // Wait for processing (large payload needs more time)
        Thread.sleep(5000);

        long elapsed = System.currentTimeMillis() - startTime;
        forceGC();
        long heapAfter = Debug.getNativeHeapAllocatedSize();
        long heapDelta = heapAfter - heapBefore;

        Log.i(TAG55, "=== RESULT ===");
        Log.i(TAG55, "  Payload size: " + formatBytes(largePayload.length()));
        Log.i(TAG55, "  Processing time: " + elapsed + " ms");
        Log.i(TAG55, "  Native heap delta: " + formatBytes(heapDelta));

        // Release payload reference to help GC
        largePayload = null;

        // Destroy
        final CountDownLatch destroyed = new CountDownLatch(1);
        runOnActivity(activity -> {
            sm.destroy();
            destroyed.countDown();
        });
        assertTrue("SM destroy timeout", destroyed.await(10, TimeUnit.SECONDS));

        // Post-destroy measurement
        Thread.sleep(500);
        forceGC();
        long heapPostDestroy = Debug.getNativeHeapAllocatedSize();
        long retained = heapPostDestroy - heapBefore;
        Log.i(TAG55, "  Heap post-destroy delta: " + formatBytes(retained));

        Log.i(TAG55, "=== RISK55 PASSED: No crash with " + TEXT_SIZE_MB + "MB payload ===");
        assertTrue("Processing took > 30s (possible hang)",
                elapsed < 30000);
    }

    // ========================================================================
    // Helpers
    // ========================================================================

    private static String buildCreateSurfacePayload(String surfaceId) {
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\""
                + surfaceId + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";
    }

    /**
     * Build a payload with N sibling Text components under a single Column root.
     */
    private static String buildWideTreePayload(String surfaceId, int siblingCount) {
        StringBuilder sb = new StringBuilder(siblingCount * 80);
        sb.append("{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"")
                .append(surfaceId)
                .append("\",\"components\":[");

        // Root Column with N children
        sb.append("{\"id\":\"root\",\"component\":\"Column\",\"children\":[");
        for (int i = 0; i < siblingCount; i++) {
            if (i > 0) sb.append(",");
            sb.append("\"n").append(i).append("\"");
        }
        sb.append("],\"styles\":{\"width\":\"100%\"}}");

        // N sibling Text components
        for (int i = 0; i < siblingCount; i++) {
            sb.append(",{\"id\":\"n").append(i)
                    .append("\",\"component\":\"Text\",\"text\":\"Item ")
                    .append(i).append("\",\"styles\":{\"fontSize\":\"14px\"}}");
        }

        sb.append("]}}");
        return sb.toString();
    }

    /**
     * Build a payload with a single Text component containing a very large text field.
     */
    private static String buildLargeTextPayload(String surfaceId, int textSizeBytes) {
        // Build the large text content (repeating pattern)
        StringBuilder textContent = new StringBuilder(textSizeBytes + 100);
        String pattern = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. ";
        while (textContent.length() < textSizeBytes) {
            textContent.append(pattern);
        }
        // Truncate to exact size and escape for JSON (no special chars in pattern)
        String text = textContent.substring(0, textSizeBytes);

        // Wrap in A2UI payload
        StringBuilder sb = new StringBuilder(textSizeBytes + 256);
        sb.append("{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"")
                .append(surfaceId)
                .append("\",\"components\":[")
                .append("{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"t1\"],")
                .append("\"styles\":{\"width\":\"100%\"}},")
                .append("{\"id\":\"t1\",\"component\":\"Text\",\"text\":\"")
                .append(text)
                .append("\",\"styles\":{\"fontSize\":\"12px\"}}")
                .append("]}}");
        return sb.toString();
    }

    private void forceGC() {
        System.gc();
        System.runFinalization();
        System.gc();
        try { Thread.sleep(100); } catch (InterruptedException ignored) {}
    }

    private static String formatBytes(long bytes) {
        if (Math.abs(bytes) < 1024) return bytes + " B";
        if (Math.abs(bytes) < 1024 * 1024) return String.format("%.1f KB", bytes / 1024.0);
        return String.format("%.2f MB", bytes / (1024.0 * 1024.0));
    }

}
