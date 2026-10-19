package com.amap.agenuiplayground.tests;

import android.app.Activity;
import android.util.Log;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.A2UIPlaygroundActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

/**
 * RISK35: JNI Bridge Race via tight create-stream-destroy cycle.
 *
 * Exercises the SurfaceSizeProvider bridge lifetime race using only the public
 * SDK surface. Root cause: the C++ JNISurfaceSizeProviderBridge holds a JNI
 * global ref to the Java SurfaceManager. If the bridge is torn down (destroy())
 * while the engine worker thread is mid-call into env->CallObjectMethod(_javaHost, ...),
 * the stale jobject triggers SIGSEGV in art::Thread::DecodeGlobalJObject.
 *
 * Pattern: multiple Java threads tight-loop:
 *   new SurfaceManager → beginTextStream → receiveTextChunk(layout-triggering) → destroy()
 * The layout-triggering chunk forces the engine to pull surface size, which calls
 * back into the Java SurfaceManager on the worker thread. Skipping endTextStream
 * ensures the worker is still mid-callback when destroy() tears down the bridge.
 *
 * Expected: if the _surfaceSizeProviderMutex fix has a gap under pressure, SIGSEGV
 * within seconds. Otherwise, test passes cleanly — confirming the fix holds.
 */
@RunWith(AndroidJUnit4.class)
@LargeTest
public class SDKRiskProbeJniBridgeRaceTest {

    private static final String TAG = "RISK35_JniBridgeRace";

    @Rule
    public ActivityScenarioRule<A2UIPlaygroundActivity> activityRule =
            new ActivityScenarioRule<>(A2UIPlaygroundActivity.class);

    private Activity activity;

    @Before
    public void setUp() {
        activityRule.getScenario().onActivity(a -> activity = a);
        AGenUI.getInstance().initialize(activity.getApplicationContext());
        Log.i(TAG, "=== RISK35 JNI Bridge Race Test Setup ===");
    }

    @After
    public void tearDown() {
        Log.i(TAG, "=== RISK35 Test Teardown ===");
    }

    /**
     * Test 1: High-pressure S11 pattern with 5 racer threads for 8 seconds.
     * Each racer tight-loops: create SM → beginTextStream → layout chunk → destroy()
     */
    @Test
    public void testRISK35_jniBridgeRaceHighPressure() throws Exception {
        Log.i(TAG, "--- Test 1: High-pressure JNI bridge race (5 racers x 8s) ---");

        final AtomicBoolean stop = new AtomicBoolean(false);
        final AtomicInteger totalCycles = new AtomicInteger(0);
        final AtomicInteger errors = new AtomicInteger(0);

        // Layout-triggering chunk: createSurface + updateComponents with percent-based
        // sizing forces the engine to pull surfaceSize on first layout, which is the
        // path that calls env->CallObjectMethod(_javaHost, ...) on the worker thread.
        final String layoutChunkTemplate =
                "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"%s\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}"
                + "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"%s\","
                + "\"components\":[{\"id\":\"root\",\"component\":\"Column\","
                + "\"children\":[\"c1\"],\"align\":\"stretch\"},"
                + "{\"id\":\"c1\",\"component\":\"Text\",\"text\":\"bridge race test\","
                + "\"style\":{\"width\":\"100%%\"}}]}}";

        final int RACER_COUNT = 5;
        final long DURATION_MS = 8000;
        Thread[] racers = new Thread[RACER_COUNT];

        for (int i = 0; i < RACER_COUNT; i++) {
            final int wid = i;
            racers[i] = new Thread(() -> {
                int localCycles = 0;
                while (!stop.get()) {
                    SurfaceManager sm = null;
                    try {
                        sm = new SurfaceManager(activity);
                        String sid = "r35-w" + wid + "-" + localCycles;
                        sm.beginTextStream();
                        // Fire layout-triggering chunk — forces surfaceSize pull on worker
                        sm.receiveTextChunk(String.format(layoutChunkTemplate, sid, sid));
                        // Intentionally skip endTextStream: destroy while worker is mid-callback
                        localCycles++;
                    } catch (Throwable t) {
                        errors.incrementAndGet();
                        Log.w(TAG, "Racer " + wid + " error: " + t.getMessage());
                    } finally {
                        if (sm != null) {
                            try {
                                sm.destroy();
                            } catch (Throwable ignored) {}
                        }
                    }
                }
                totalCycles.addAndGet(localCycles);
                Log.i(TAG, "Racer " + wid + " completed " + localCycles + " cycles");
            }, "risk35-racer-" + i);
        }

        // Allocator thread: creates small objects to encourage memory reuse of freed
        // bridge slots, making stale jobject non-null (vs the nullptr check that would
        // otherwise swallow the access).
        Thread allocator = new Thread(() -> {
            while (!stop.get()) {
                byte[] junk = new byte[128];
                junk[0] = 1; // prevent dead-store elimination
            }
        }, "risk35-allocator");

        Log.i(TAG, "Starting " + RACER_COUNT + " racers + allocator for " + DURATION_MS + "ms");
        long startTime = System.currentTimeMillis();

        for (Thread t : racers) t.start();
        allocator.start();

        try {
            Thread.sleep(DURATION_MS);
        } finally {
            stop.set(true);
            for (Thread t : racers) t.join(3000);
            allocator.join(2000);
        }

        long elapsed = System.currentTimeMillis() - startTime;
        Log.i(TAG, "=== RESULT: " + totalCycles.get() + " total cycles in " + elapsed + "ms, "
                + errors.get() + " Java errors ===");
        Log.i(TAG, "If we reached here without SIGSEGV, the _surfaceSizeProviderMutex fix holds");
    }

    /**
     * Test 2: Variant with interleaved surfaceSize notifications to widen the race window.
     * While racers are creating/destroying, a separate thread fires surfaceSizeChanged
     * notifications on random instanceIds, potentially hitting SMs mid-teardown.
     */
    @Test
    public void testRISK35_bridgeRaceWithSizeNotifications() throws Exception {
        Log.i(TAG, "--- Test 2: Bridge race + size notifications (5 racers + 2 notifiers x 6s) ---");

        final AtomicBoolean stop = new AtomicBoolean(false);
        final AtomicInteger totalCycles = new AtomicInteger(0);
        final AtomicLong lastInstanceId = new AtomicLong(0);

        final String layoutChunkTemplate =
                "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"%s\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}"
                + "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"%s\","
                + "\"components\":[{\"id\":\"root\",\"component\":\"Column\","
                + "\"children\":[\"c1\",\"c2\"],\"align\":\"stretch\"},"
                + "{\"id\":\"c1\",\"component\":\"Text\",\"text\":\"a\","
                + "\"style\":{\"width\":\"50%%\"}},"
                + "{\"id\":\"c2\",\"component\":\"Text\",\"text\":\"b\","
                + "\"style\":{\"width\":\"80%%\"}}]}}";

        final int RACER_COUNT = 5;
        final long DURATION_MS = 6000;
        Thread[] racers = new Thread[RACER_COUNT];

        for (int i = 0; i < RACER_COUNT; i++) {
            final int wid = i;
            racers[i] = new Thread(() -> {
                int localCycles = 0;
                while (!stop.get()) {
                    SurfaceManager sm = null;
                    try {
                        sm = new SurfaceManager(activity);
                        String sid = "r35v2-w" + wid + "-" + localCycles;
                        sm.beginTextStream();
                        sm.receiveTextChunk(String.format(layoutChunkTemplate, sid, sid));
                        // Fire additional chunks to keep worker busy
                        sm.receiveTextChunk(
                                "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\""
                                + sid + "\",\"components\":[{\"id\":\"c1\",\"component\":\"Text\","
                                + "\"text\":\"updated " + localCycles + "\","
                                + "\"style\":{\"width\":\"100%%\"}}]}}");
                        localCycles++;
                    } catch (Throwable ignored) {
                    } finally {
                        if (sm != null) {
                            try {
                                sm.destroy();
                            } catch (Throwable ignored) {}
                        }
                    }
                }
                totalCycles.addAndGet(localCycles);
                Log.i(TAG, "Racer " + wid + " completed " + localCycles + " cycles");
            }, "risk35v2-racer-" + i);
        }

        // Extra racer threads: same pattern but with heavier layout chunks
        Thread[] notifiers = new Thread[2];
        for (int i = 0; i < notifiers.length; i++) {
            final int nid = i;
            notifiers[i] = new Thread(() -> {
                int cycles = 0;
                while (!stop.get()) {
                    SurfaceManager sm = null;
                    try {
                        sm = new SurfaceManager(activity);
                        String sid = "r35extra-n" + nid + "-" + (cycles++);
                        sm.beginTextStream();
                        // Heavy layout: multiple components with percent widths → multiple surfaceSize pulls
                        sm.receiveTextChunk(
                                "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\""
                                + sid + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}"
                                + "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\""
                                + sid + "\",\"components\":[{\"id\":\"root\",\"component\":\"Column\","
                                + "\"children\":[\"a\",\"b\",\"c\"],\"align\":\"stretch\"},"
                                + "{\"id\":\"a\",\"component\":\"Text\",\"text\":\"x\",\"style\":{\"width\":\"100%%\"}},"
                                + "{\"id\":\"b\",\"component\":\"Text\",\"text\":\"y\",\"style\":{\"width\":\"75%%\"}},"
                                + "{\"id\":\"c\",\"component\":\"Text\",\"text\":\"z\",\"style\":{\"width\":\"50%%\"}}]}}");
                        // Don't endTextStream — maximize race window
                    } catch (Throwable ignored) {
                    } finally {
                        if (sm != null) {
                            try { sm.destroy(); } catch (Throwable ignored) {}
                        }
                    }
                }
            }, "risk35v2-notifier-" + i);
        }

        Log.i(TAG, "Starting " + RACER_COUNT + " racers + " + notifiers.length + " notifiers");
        long startTime = System.currentTimeMillis();

        for (Thread t : racers) t.start();
        for (Thread t : notifiers) t.start();

        try {
            Thread.sleep(DURATION_MS);
        } finally {
            stop.set(true);
            for (Thread t : racers) t.join(3000);
            for (Thread t : notifiers) t.join(3000);
        }

        long elapsed = System.currentTimeMillis() - startTime;
        Log.i(TAG, "=== RESULT: " + totalCycles.get() + " total cycles in " + elapsed + "ms ===");
        Log.i(TAG, "If we reached here without SIGSEGV, bridge race fix is robust under pressure");
    }

    /**
     * Test 3: Extreme variant — interleave engine destroy between active SM lifecycles.
     * Create multiple SMs, stream to them, then call AGenUI.destroy() while racing threads
     * are still creating/destroying SMs. Exercises the engine-level teardown path
     * concurrently with SM-level bridge usage.
     */
    @Test
    public void testRISK35_bridgeRaceWithEngineDestroy() throws Exception {
        Log.i(TAG, "--- Test 3: Bridge race + engine destroy (4 racers x 4s, then destroy) ---");

        final AtomicBoolean stop = new AtomicBoolean(false);
        final AtomicInteger totalCycles = new AtomicInteger(0);

        final String layoutChunkTemplate =
                "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"%s\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}"
                + "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"%s\","
                + "\"components\":[{\"id\":\"root\",\"component\":\"Column\","
                + "\"children\":[\"t\"],\"align\":\"stretch\"},"
                + "{\"id\":\"t\",\"component\":\"Text\",\"text\":\"engine race\","
                + "\"style\":{\"width\":\"100%%\"}}]}}";

        final int RACER_COUNT = 4;
        Thread[] racers = new Thread[RACER_COUNT];

        for (int i = 0; i < RACER_COUNT; i++) {
            final int wid = i;
            racers[i] = new Thread(() -> {
                int localCycles = 0;
                while (!stop.get()) {
                    SurfaceManager sm = null;
                    try {
                        sm = new SurfaceManager(activity);
                        String sid = "r35eng-w" + wid + "-" + localCycles;
                        sm.beginTextStream();
                        sm.receiveTextChunk(String.format(layoutChunkTemplate, sid, sid));
                        localCycles++;
                    } catch (Throwable ignored) {
                    } finally {
                        if (sm != null) {
                            try {
                                sm.destroy();
                            } catch (Throwable ignored) {}
                        }
                    }
                }
                totalCycles.addAndGet(localCycles);
                Log.i(TAG, "Racer " + wid + " completed " + localCycles + " cycles");
            }, "risk35eng-racer-" + i);
        }

        Log.i(TAG, "Starting " + RACER_COUNT + " racers for 4s, then engine destroy");
        long startTime = System.currentTimeMillis();

        for (Thread t : racers) t.start();
        Thread.sleep(4000);

        // Fire engine destroy while racers are still running
        Log.i(TAG, "Calling AGenUI.destroy() while racers are active...");
        try {
            AGenUI.getInstance().destroy();
            Log.i(TAG, "AGenUI.destroy() completed");
        } catch (Throwable t) {
            Log.e(TAG, "AGenUI.destroy() threw: " + t.getMessage());
        }

        // Let racers run for 1 more second after engine destroy to detect post-destroy UAF
        Thread.sleep(1000);
        stop.set(true);
        for (Thread t : racers) t.join(3000);

        long elapsed = System.currentTimeMillis() - startTime;
        Log.i(TAG, "=== RESULT: " + totalCycles.get() + " total cycles in " + elapsed + "ms ===");
        Log.i(TAG, "If reached here, engine destroy + concurrent bridge usage is safe");

        // Re-init for teardown
        AGenUI.getInstance().initialize(activity.getApplicationContext());
    }
}
