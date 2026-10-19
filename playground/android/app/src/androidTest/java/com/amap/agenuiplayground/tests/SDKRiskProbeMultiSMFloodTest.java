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

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * RISK36: Multi-SM flood + concurrent destroy on SAME instance.
 *
 * Two attack vectors:
 *
 * Test 1: "Double-destroy" — Two threads call destroy() on the SAME SurfaceManager
 * simultaneously. Java's destroy() has ZERO synchronization, so both threads enter
 * nativeRemoveEventListener concurrently:
 *   Thread A: findBridge → gets bridge ptr
 *   Thread B: findBridge → gets SAME bridge ptr
 *   Thread A: SAFELY_DELETE(bridge) → frees
 *   Thread B: SAFELY_DELETE(bridge) → use-after-free / double-free → SIGSEGV
 *
 * Test 2: "Flood + destroy" — 20 SurfaceManagers alive and actively streaming complex
 * layout data. A destroyer thread calls destroy() on random SMs while data threads
 * continue sending chunks to those SMs. The JNI layer's findSurfaceManagerByInstanceId
 * returns a raw pointer (no shared_ptr protection), creating a TOCTOU window where
 * the SM can be freed between the find and the method call.
 *
 * Test 3: "Destroy during active streaming" — Begin streaming on an SM, send many
 * layout-triggering chunks, then call destroy() from another thread while the worker
 * is processing layout (Yoga measure → surfaceSize callback → JNI). This exercises
 * the gap where Java destroy() tears down bridges while C++ worker is mid-callback.
 */
@RunWith(AndroidJUnit4.class)
@LargeTest
public class SDKRiskProbeMultiSMFloodTest {

    private static final String TAG = "RISK36_MultiSMFlood";

    @Rule
    public ActivityScenarioRule<A2UIPlaygroundActivity> activityRule =
            new ActivityScenarioRule<>(A2UIPlaygroundActivity.class);

    private Activity activity;

    @Before
    public void setUp() {
        activityRule.getScenario().onActivity(a -> activity = a);
        AGenUI.getInstance().initialize(activity.getApplicationContext());
        Log.i(TAG, "=== RISK36 Multi-SM Flood Test Setup ===");
    }

    @After
    public void tearDown() {
        Log.i(TAG, "=== RISK36 Test Teardown ===");
    }

    /**
     * Test 1: Double-destroy on the SAME SurfaceManager from two threads.
     * Expected: SIGSEGV from double-free of JNI listener bridge.
     */
    @Test
    public void testRISK36_doubleDestroySameSM() throws Exception {
        Log.i(TAG, "--- Test 1: Double-destroy same SM (10 rounds x 2 threads) ---");

        final int ROUNDS = 10;
        final AtomicInteger crashes = new AtomicInteger(0);

        for (int round = 0; round < ROUNDS; round++) {
            final SurfaceManager sm = new SurfaceManager(activity);
            sm.beginTextStream();
            // Feed some data to ensure bridges are active
            String sid = "r36dd-" + round;
            sm.receiveTextChunk(
                    "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"" + sid
                    + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}"
                    + "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + sid
                    + "\",\"components\":[{\"id\":\"root\",\"component\":\"Column\","
                    + "\"children\":[\"c1\"],\"align\":\"stretch\"},"
                    + "{\"id\":\"c1\",\"component\":\"Text\",\"text\":\"dd test\","
                    + "\"style\":{\"width\":\"100%\"}}]}}");

            // Brief pause to let worker start processing
            Thread.sleep(50);

            // Two threads race to destroy the same SM
            CountDownLatch startGate = new CountDownLatch(1);
            CountDownLatch done = new CountDownLatch(2);

            for (int t = 0; t < 2; t++) {
                new Thread(() -> {
                    try {
                        startGate.await();
                        sm.destroy();
                    } catch (Throwable e) {
                        crashes.incrementAndGet();
                        Log.w(TAG, "destroy threw: " + e.getMessage());
                    } finally {
                        done.countDown();
                    }
                }, "risk36-dd-" + round + "-" + t).start();
            }

            startGate.countDown(); // Release both threads simultaneously
            done.await();
            Log.i(TAG, "Round " + round + " complete");
        }

        Log.i(TAG, "=== RESULT: " + ROUNDS + " double-destroy rounds, "
                + crashes.get() + " Java exceptions ===");
        Log.i(TAG, "If we reached here without SIGSEGV, double-destroy is handled safely");
    }

    /**
     * Test 2: 20 SMs alive, 5 data threads flooding them, 2 destroyer threads
     * randomly destroying SMs while data continues flowing.
     * Expected: SIGSEGV from JNI TOCTOU (findSurfaceManager returns raw ptr,
     * SM freed before method call).
     */
    @Test
    public void testRISK36_floodWithConcurrentDestroy() throws Exception {
        Log.i(TAG, "--- Test 2: 20 SMs + 5 data threads + 2 destroyers x 6s ---");

        final int SM_COUNT = 20;
        final AtomicBoolean stop = new AtomicBoolean(false);
        final AtomicInteger totalChunks = new AtomicInteger(0);
        final AtomicInteger destroyCount = new AtomicInteger(0);

        // Create SMs array (synchronized access)
        final SurfaceManager[] sms = new SurfaceManager[SM_COUNT];
        final Object[] smLocks = new Object[SM_COUNT];
        for (int i = 0; i < SM_COUNT; i++) {
            sms[i] = new SurfaceManager(activity);
            smLocks[i] = new Object();
            sms[i].beginTextStream();
            String sid = "r36flood-" + i;
            sms[i].receiveTextChunk(
                    "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"" + sid
                    + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}");
        }

        // Layout chunk template with complex percent-based layout
        final String chunkTemplate =
                "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"%s\","
                + "\"components\":[{\"id\":\"root\",\"component\":\"Column\","
                + "\"children\":[\"a\",\"b\",\"c\",\"d\"],\"align\":\"stretch\"},"
                + "{\"id\":\"a\",\"component\":\"Text\",\"text\":\"chunk %d\","
                + "\"style\":{\"width\":\"100%%\",\"padding\":\"8\"}},"
                + "{\"id\":\"b\",\"component\":\"Text\",\"text\":\"pressure\","
                + "\"style\":{\"width\":\"75%%\",\"margin\":\"4\"}},"
                + "{\"id\":\"c\",\"component\":\"Text\",\"text\":\"test\","
                + "\"style\":{\"width\":\"50%%\",\"padding\":\"12\"}},"
                + "{\"id\":\"d\",\"component\":\"Text\",\"text\":\"flood\","
                + "\"style\":{\"width\":\"80%%\",\"margin\":\"6\"}}]}}";

        // 5 data flood threads
        Thread[] dataThreads = new Thread[5];
        for (int i = 0; i < dataThreads.length; i++) {
            final int tid = i;
            dataThreads[i] = new Thread(() -> {
                int chunks = 0;
                while (!stop.get()) {
                    int idx = (tid * 4 + chunks) % SM_COUNT;
                    try {
                        synchronized (smLocks[idx]) {
                            SurfaceManager sm = sms[idx];
                            if (sm != null) {
                                String sid = "r36flood-" + idx;
                                sm.receiveTextChunk(String.format(chunkTemplate, sid, chunks));
                                chunks++;
                            }
                        }
                    } catch (Throwable ignored) {}
                }
                totalChunks.addAndGet(chunks);
            }, "risk36-data-" + i);
        }

        // 2 destroyer threads: randomly pick an SM, destroy it, and recreate
        Thread[] destroyers = new Thread[2];
        for (int i = 0; i < destroyers.length; i++) {
            final int did = i;
            destroyers[i] = new Thread(() -> {
                int destroyed = 0;
                while (!stop.get()) {
                    int idx = (int)(Math.random() * SM_COUNT);
                    try {
                        synchronized (smLocks[idx]) {
                            SurfaceManager sm = sms[idx];
                            if (sm != null) {
                                sm.destroy();
                                // Recreate immediately to keep pressure up
                                SurfaceManager newSm = new SurfaceManager(activity);
                                newSm.beginTextStream();
                                String sid = "r36flood-" + idx;
                                newSm.receiveTextChunk(
                                        "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\""
                                        + sid + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}");
                                sms[idx] = newSm;
                                destroyed++;
                            }
                        }
                    } catch (Throwable ignored) {}
                    try { Thread.sleep(50); } catch (InterruptedException ignored) {}
                }
                destroyCount.addAndGet(destroyed);
            }, "risk36-destroyer-" + i);
        }

        Log.i(TAG, "Starting 5 data threads + 2 destroyers on " + SM_COUNT + " SMs");
        long startTime = System.currentTimeMillis();

        for (Thread t : dataThreads) t.start();
        for (Thread t : destroyers) t.start();

        Thread.sleep(6000);
        stop.set(true);

        for (Thread t : dataThreads) t.join(3000);
        for (Thread t : destroyers) t.join(3000);

        // Cleanup remaining SMs
        for (int i = 0; i < SM_COUNT; i++) {
            try {
                synchronized (smLocks[i]) {
                    if (sms[i] != null) {
                        sms[i].destroy();
                        sms[i] = null;
                    }
                }
            } catch (Throwable ignored) {}
        }

        long elapsed = System.currentTimeMillis() - startTime;
        Log.i(TAG, "=== RESULT: " + totalChunks.get() + " chunks sent, "
                + destroyCount.get() + " destroy/recreate cycles in " + elapsed + "ms ===");
    }

    /**
     * Test 3: "Unguarded concurrent operations" — Multiple threads operate on the
     * SAME SurfaceManager simultaneously WITHOUT any external synchronization.
     * Java SurfaceManager has zero internal synchronization.
     * Specifically: one thread streams data, another calls destroy() without coordination.
     * No smLocks — purely exercises the SDK's internal thread safety.
     */
    @Test
    public void testRISK36_unsynchronizedConcurrentOps() throws Exception {
        Log.i(TAG, "--- Test 3: Unguarded concurrent ops on same SM (20 rounds) ---");

        final int ROUNDS = 20;
        final AtomicInteger crashes = new AtomicInteger(0);

        for (int round = 0; round < ROUNDS; round++) {
            final SurfaceManager sm = new SurfaceManager(activity);
            final String sid = "r36unguarded-" + round;
            final AtomicBoolean streamDone = new AtomicBoolean(false);

            sm.beginTextStream();
            sm.receiveTextChunk(
                    "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"" + sid
                    + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}");

            // Thread 1: continuously streams data (no external lock)
            Thread streamer = new Thread(() -> {
                int chunk = 0;
                while (!streamDone.get()) {
                    try {
                        sm.receiveTextChunk(
                                "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + sid
                                + "\",\"components\":[{\"id\":\"root\",\"component\":\"Column\","
                                + "\"children\":[\"t\"],\"align\":\"stretch\"},"
                                + "{\"id\":\"t\",\"component\":\"Text\",\"text\":\"chunk " + (chunk++) + "\","
                                + "\"style\":{\"width\":\"100%\"}}]}}");
                    } catch (Throwable t) {
                        crashes.incrementAndGet();
                    }
                }
            }, "risk36-streamer-" + round);

            // Thread 2: waits briefly then calls destroy (no external lock)
            Thread destroyer = new Thread(() -> {
                try {
                    Thread.sleep(100 + (int)(Math.random() * 200));
                } catch (InterruptedException ignored) {}
                try {
                    sm.destroy();
                } catch (Throwable t) {
                    crashes.incrementAndGet();
                    Log.w(TAG, "Round " + sid + " destroy error: " + t.getMessage());
                }
                streamDone.set(true);
            }, "risk36-destroyer-" + round);

            streamer.start();
            destroyer.start();

            streamer.join(5000);
            destroyer.join(5000);

            // Ensure cleanup even if threads timed out
            streamDone.set(true);
        }

        Log.i(TAG, "=== RESULT: " + ROUNDS + " rounds, " + crashes.get() + " Java exceptions ===");
        Log.i(TAG, "If we reached here without SIGSEGV, concurrent ops on same SM are safe");
    }
}
