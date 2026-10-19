package com.amap.agenuiplayground.tests;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.base.AGenUIBaseTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

/**
 * SDK Risk Probe: Deeply nested JSON stack overflow via nlohmann::json::parse.
 *
 * Risk hypothesis:
 *   nlohmann::json uses recursive descent parsing. The C++ worker thread
 *   (typically ~1MB stack on Android) calls nlohmann::json::parse on user-provided
 *   JSON data without a recursion depth limit. A deeply nested JSON object
 *   (~1000-2000 levels) will overflow the worker thread's stack, causing
 *   SIGSEGV that cannot be caught by C++ try-catch.
 *
 * Entry surface: SurfaceManager.receiveTextChunk() with deeply nested JSON
 * Probe style: abnormal input (boundary/extreme value)
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeDeepJsonCrashTest extends AGenUIBaseTest {

    /**
     * Test 1: Deeply nested JSON in createSurface message.
     * The nlohmann::json::parse in parseCreateSurfaceData has no depth guard.
     */
    @Test
    public void testSDKRISK_deepNestedCreateSurface() throws Exception {
        // Nesting depth that should overflow a 1MB thread stack.
        // Each recursion frame in nlohmann is ~200-500 bytes.
        // 2000 levels × 500 bytes ≈ 1MB → stack overflow on worker thread.
        final int DEPTH = 2000;

        String chunk = buildDeeplyNestedCreateSurface("deep-cs-1", DEPTH);

        // This runs on a background thread; the native crash would kill the process.
        // If we survive, the test passes (no crash).
        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(chunk);
        surfaceManager.endTextStream();

        // Wait for worker thread to process the data
        Thread.sleep(3000);

        // If we reach here, no crash occurred (the SDK handled deep nesting safely)
        // The test "passes" in the normal sense, but we WANT the crash.
    }

    /**
     * Test 2: Deeply nested JSON in component styles (via updateComponents).
     * The nlohmann::json::parse in parseUpdateComponentsData has no depth guard.
     */
    @Test
    public void testSDKRISK_deepNestedComponentStyles() throws Exception {
        final int DEPTH = 2000;

        // First create a surface, then send deeply nested component data
        String createChunk = "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"deep-uc-1\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(createChunk);
        surfaceManager.endTextStream();

        // Wait for surface creation
        Thread.sleep(500);

        // Now send an updateComponents with deeply nested styles
        String deepComponentChunk = buildDeeplyNestedUpdateComponents("deep-uc-1", DEPTH);
        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(deepComponentChunk);
        surfaceManager.endTextStream();

        // Wait for worker thread to process
        Thread.sleep(3000);
    }

    /**
     * Test 3: Moderate nesting (500 levels) with multiple concurrent SMs.
     * Stress the worker thread with multiple deep JSON parses simultaneously.
     */
    @Test
    public void testSDKRISK_moderateNestedMultipleSMs() throws Exception {
        final int DEPTH = 1000;
        final int SM_COUNT = 3;
        AtomicBoolean crashed = new AtomicBoolean(false);
        CountDownLatch done = new CountDownLatch(SM_COUNT);

        for (int i = 0; i < SM_COUNT; i++) {
            final int idx = i;
            new Thread(() -> {
                SurfaceManager sm = null;
                try {
                    final SurfaceManager[] holder = new SurfaceManager[1];
                    runOnActivity(activity -> holder[0] = new SurfaceManager(activity));
                    sm = holder[0];
                    if (sm == null) return;

                    String chunk = buildDeeplyNestedCreateSurface("deep-multi-" + idx, DEPTH);
                    sm.beginTextStream();
                    sm.receiveTextChunk(chunk);
                    sm.endTextStream();

                    // Wait for processing
                    Thread.sleep(2000);
                } catch (Throwable t) {
                    // Java exceptions are not the target; native crash aborts the process
                } finally {
                    if (sm != null) {
                        SurfaceManager finalSm = sm;
                        try {
                            runOnActivity(activity -> finalSm.destroy());
                        } catch (Throwable ignored) {}
                    }
                    done.countDown();
                }
            }, "deep-json-" + i).start();
        }

        boolean allDone = done.await(10000, TimeUnit.MILLISECONDS);
        assertTrue("Deep JSON workers did not finish (possible crash/hang)", allDone);
    }

    /**
     * Build a createSurface JSON with deeply nested extra data.
     * Structure: {"version":"v0.9","createSurface":{"surfaceId":"...","catalogId":"...","extra":{"a":{"a":...N levels...}}}}
     */
    private static String buildDeeplyNestedCreateSurface(String surfaceId, int depth) {
        StringBuilder sb = new StringBuilder(depth * 10);
        sb.append("{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"");
        sb.append(surfaceId);
        sb.append("\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\",\"extra\":");
        // Build nested object: {"a":{"a":{"a":...}}}
        for (int i = 0; i < depth; i++) {
            sb.append("{\"a\":");
        }
        sb.append("1"); // innermost value
        for (int i = 0; i < depth; i++) {
            sb.append("}");
        }
        sb.append("}}");
        return sb.toString();
    }

    /**
     * Build an updateComponents JSON with deeply nested styles.
     * The component's "styles" value is deeply nested.
     */
    private static String buildDeeplyNestedUpdateComponents(String surfaceId, int depth) {
        StringBuilder sb = new StringBuilder(depth * 10);
        sb.append("{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"");
        sb.append(surfaceId);
        sb.append("\",\"components\":[{\"id\":\"root\",\"component\":\"Column\",\"styles\":");
        // Build nested object
        for (int i = 0; i < depth; i++) {
            sb.append("{\"n\":");
        }
        sb.append("1");
        for (int i = 0; i < depth; i++) {
            sb.append("}");
        }
        sb.append("}]}}");
        return sb.toString();
    }
}
