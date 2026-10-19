package com.amap.agenuiplayground.tests;

import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.base.AGenUIBaseTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * RISK28: SurfaceManager concurrent destroy — double-free of listener bridge.
 *
 * <h3>Vulnerability</h3>
 * {@code SurfaceManager.destroy()} calls {@code removeMessageListener(nativeEventBridge)}
 * which routes to JNI {@code jni_removeEventListener}:
 * <pre>
 *   1. bridge = ListenerBridgeManager.findBridge(javaListener)  // mutex-guarded lookup, returns raw ptr
 *   2. surfaceManager->removeSurfaceEventListener(bridge)        // mutex + list traversal
 *   3. ListenerBridgeManager.removeMapping(javaListener)         // mutex-guarded erase
 *   4. SAFELY_DELETE(bridge)                                     // delete bridge; bridge = nullptr (LOCAL only)
 * </pre>
 *
 * If two threads call {@code destroy()} concurrently on the same SurfaceManager:
 * <ul>
 *   <li>Thread A: findBridge → gets bridge B (mutex released after return)</li>
 *   <li>Thread B: findBridge → gets bridge B (still in map; A hasn't called removeMapping yet)</li>
 *   <li>Thread A: removeMapping + SAFELY_DELETE(B) → B freed</li>
 *   <li>Thread B: removeSurfaceEventListener(B) → UAF (freed pointer as argument)</li>
 *   <li>Thread B: SAFELY_DELETE(B) → <strong>DOUBLE FREE</strong></li>
 * </ul>
 *
 * {@code SAFELY_DELETE} only zeroes the <em>local</em> variable; it cannot prevent
 * another thread from deleting the same pointer via its own local copy.
 *
 * <h3>Probe style</h3>
 * Timing/concurrency race. Many iterations × many concurrent destroyer threads.
 *
 * <h3>Expected</h3>
 * SIGABRT (double-free detected by allocator) or SIGSEGV (heap corruption / UAF).
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeConcurrentDestroyBridgeTest extends AGenUIBaseTest {

    private static final String TAG = "SDKRiskProbe28";

    // Tuning knobs
    private static final int DESTROY_THREADS = 10;
    private static final int ITERATIONS = 400;

    @Override
    public void setUp() {
        // Only initialize AGenUI; we manage SMs ourselves per iteration
        activityRule.getScenario().onActivity(activity -> {
            if (!AGenUI.getInstance().isInitialized()) {
                AGenUI.getInstance().initialize(activity.getApplicationContext());
            }
        });
    }

    @Override
    public void tearDown() {
        // Each test manages its own SM lifecycle — suppress base tearDown
    }

    // ------------------------------------------------------------------
    // Test 1: Pure concurrent destroy
    // ------------------------------------------------------------------

    /**
     * Multiple threads call {@code sm.destroy()} on the SAME SurfaceManager
     * simultaneously. Each invocation triggers {@code jni_removeEventListener}
     * for the same nativeEventBridge listener, racing on findBridge/SAFELY_DELETE.
     */
    @Test
    public void testRISK28_concurrentDestroyDoubleFree() throws Exception {
        Log.i(TAG, "=== RISK28: concurrent destroy, " + DESTROY_THREADS
                + " threads x " + ITERATIONS + " iterations ===");

        final AtomicInteger totalDestroys = new AtomicInteger(0);

        for (int iter = 0; iter < ITERATIONS; iter++) {
            // --- Create SM on UI thread ---
            final SurfaceManager[] smHolder = new SurfaceManager[1];
            runOnActivity(activity -> smHolder[0] = new SurfaceManager(activity));
            final SurfaceManager sm = smHolder[0];

            // Warm up: send a createSurface so the SM has an active surface + listener bridge
            String surfaceId = "r28-" + iter;
            String createJson = "{\"version\":\"v0.9\",\"createSurface\":"
                    + "{\"surfaceId\":\"" + surfaceId + "\","
                    + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";
            sm.beginTextStream();
            sm.receiveTextChunk(createJson);
            // Intentionally leave stream open — mid-stream state maximizes internal complexity
            // (ContentParser holds partial state, dispatcher has pending work)

            // --- Barrier: all threads start destroy simultaneously ---
            CountDownLatch ready = new CountDownLatch(DESTROY_THREADS);
            CountDownLatch go = new CountDownLatch(1);
            CountDownLatch done = new CountDownLatch(DESTROY_THREADS);

            for (int t = 0; t < DESTROY_THREADS; t++) {
                final int threadIdx = t;
                new Thread(() -> {
                    ready.countDown();
                    try {
                        go.await(5, TimeUnit.SECONDS);
                    } catch (InterruptedException ignored) {
                    }
                    try {
                        sm.destroy();
                        totalDestroys.incrementAndGet();
                    } catch (Exception e) {
                        // Java-level exceptions are expected; native crashes
                        // (SIGSEGV/SIGABRT) abort the process before reaching here.
                    }
                    done.countDown();
                }, "destroyer-" + iter + "-" + threadIdx).start();
            }

            // Fire!
            ready.await(5, TimeUnit.SECONDS);
            go.countDown();

            if (!done.await(10, TimeUnit.SECONDS)) {
                Log.w(TAG, "Iteration " + iter + " timed out — possible deadlock/hang");
            }

            if (iter % 100 == 0) {
                System.gc();
                Log.i(TAG, "Iteration " + iter + "/" + ITERATIONS
                        + " survived, totalDestroys=" + totalDestroys.get());
            }
        }

        Log.i(TAG, "All " + ITERATIONS + " iterations completed without crash, totalDestroys="
                + totalDestroys.get());
    }

    // ------------------------------------------------------------------
    // Test 2: Concurrent destroy + active streaming
    // ------------------------------------------------------------------

    /**
     * Variant: streaming threads are actively feeding data while multiple threads
     * simultaneously call {@code destroy()}. This combines the concurrent-destroy
     * bridge race with the SM-destroy-during-streaming TOCTOU for maximum state
     * contention.
     */
    @Test
    public void testRISK28b_concurrentDestroyWhileStreaming() throws Exception {
        final int STREAM_THREADS = 4;

        Log.i(TAG, "=== RISK28b: concurrent destroy + streaming, "
                + DESTROY_THREADS + " destroyers + " + STREAM_THREADS + " streamers x "
                + ITERATIONS + " iterations ===");

        final AtomicInteger totalOps = new AtomicInteger(0);

        for (int iter = 0; iter < ITERATIONS; iter++) {
            final SurfaceManager[] smHolder = new SurfaceManager[1];
            runOnActivity(activity -> smHolder[0] = new SurfaceManager(activity));
            final SurfaceManager sm = smHolder[0];

            // Initialize with a surface
            String surfaceId = "r28b-" + iter;
            String createJson = "{\"version\":\"v0.9\",\"createSurface\":"
                    + "{\"surfaceId\":\"" + surfaceId + "\","
                    + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";
            sm.beginTextStream();
            sm.receiveTextChunk(createJson);
            sm.endTextStream();

            // Pre-build an update chunk for streaming threads
            String updateChunk = "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\""
                    + surfaceId + "\",\"components\":["
                    + "{\"id\":\"r\",\"component\":\"Column\",\"children\":[\"t1\"],"
                    + "\"styles\":{\"width\":\"100%\",\"height\":\"auto\"}},"
                    + "{\"id\":\"t1\",\"component\":\"Text\",\"text\":\"bridge race\"}"
                    + "]}}";

            // Tiny pause so the worker thread processes the createSurface
            Thread.sleep(2);

            // --- Launch all threads with a shared barrier ---
            int totalThreads = DESTROY_THREADS + STREAM_THREADS;
            CountDownLatch ready = new CountDownLatch(totalThreads);
            CountDownLatch go = new CountDownLatch(1);
            CountDownLatch done = new CountDownLatch(totalThreads);
            AtomicBoolean stop = new AtomicBoolean(false);

            // Streamer threads: tight loop of begin/receive/end
            for (int t = 0; t < STREAM_THREADS; t++) {
                new Thread(() -> {
                    ready.countDown();
                    try {
                        go.await(5, TimeUnit.SECONDS);
                    } catch (InterruptedException ignored) {
                    }
                    while (!stop.get()) {
                        try {
                            sm.beginTextStream();
                            sm.receiveTextChunk(updateChunk);
                            sm.endTextStream();
                            totalOps.incrementAndGet();
                        } catch (Exception e) {
                            break;
                        }
                    }
                    done.countDown();
                }, "streamer-" + iter + "-" + t).start();
            }

            // Destroyer threads: all call sm.destroy() at once
            for (int t = 0; t < DESTROY_THREADS; t++) {
                new Thread(() -> {
                    ready.countDown();
                    try {
                        go.await(5, TimeUnit.SECONDS);
                    } catch (InterruptedException ignored) {
                    }
                    try {
                        sm.destroy();
                        totalOps.incrementAndGet();
                    } catch (Exception e) {
                        // expected
                    }
                    stop.set(true);
                    done.countDown();
                }, "destroyer-" + iter + "-" + t).start();
            }

            // Fire!
            ready.await(5, TimeUnit.SECONDS);
            go.countDown();

            if (!done.await(10, TimeUnit.SECONDS)) {
                Log.w(TAG, "Iteration " + iter + " timed out");
            }
            stop.set(true);

            if (iter % 100 == 0) {
                System.gc();
                Log.i(TAG, "Iteration " + iter + "/" + ITERATIONS
                        + " survived, totalOps=" + totalOps.get());
            }
        }

        Log.i(TAG, "All " + ITERATIONS + " iterations completed, totalOps=" + totalOps.get());
    }

    // ------------------------------------------------------------------
    // Test 3: Rapid create-destroy cycles with concurrent destroyers
    // ------------------------------------------------------------------

    /**
     * Creates and destroys SurfaceManagers in a tight loop. Each destroy is
     * contested by multiple threads. This stresses the ListenerBridgeManager's
     * global map under rapid entry/removal turnover.
     */
    @Test
    public void testRISK28c_rapidCreateDestroyCycles() throws Exception {
        final int BATCH_SIZE = 4;
        final int DESTROYERS_PER_SM = 6;
        final int CYCLES = 200;

        Log.i(TAG, "=== RISK28c: rapid create-destroy cycles, batch=" + BATCH_SIZE
                + " x " + DESTROYERS_PER_SM + " destroyers x " + CYCLES + " cycles ===");

        final AtomicInteger totalOps = new AtomicInteger(0);

        for (int cycle = 0; cycle < CYCLES; cycle++) {
            // Create a batch of SMs
            final SurfaceManager[] sms = new SurfaceManager[BATCH_SIZE];
            for (int i = 0; i < BATCH_SIZE; i++) {
                final int idx = i;
                runOnActivity(activity -> sms[idx] = new SurfaceManager(activity));
            }

            // Feed each SM a createSurface to activate the bridge
            for (int i = 0; i < BATCH_SIZE; i++) {
                String sid = "r28c-" + cycle + "-" + i;
                String json = "{\"version\":\"v0.9\",\"createSurface\":"
                        + "{\"surfaceId\":\"" + sid + "\","
                        + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";
                sms[i].beginTextStream();
                sms[i].receiveTextChunk(json);
                // Leave stream open for some, close for others
                if (i % 2 == 0) {
                    sms[i].endTextStream();
                }
            }

            // Concurrent destroy: each SM gets multiple destroyer threads
            int totalThreads = BATCH_SIZE * DESTROYERS_PER_SM;
            CountDownLatch ready = new CountDownLatch(totalThreads);
            CountDownLatch go = new CountDownLatch(1);
            CountDownLatch done = new CountDownLatch(totalThreads);

            for (int i = 0; i < BATCH_SIZE; i++) {
                final SurfaceManager sm = sms[i];
                for (int d = 0; d < DESTROYERS_PER_SM; d++) {
                    new Thread(() -> {
                        ready.countDown();
                        try {
                            go.await(5, TimeUnit.SECONDS);
                        } catch (InterruptedException ignored) {
                        }
                        try {
                            sm.destroy();
                            totalOps.incrementAndGet();
                        } catch (Exception e) {
                            // expected
                        }
                        done.countDown();
                    }, "batch-destroyer-" + cycle).start();
                }
            }

            ready.await(5, TimeUnit.SECONDS);
            go.countDown();

            if (!done.await(15, TimeUnit.SECONDS)) {
                Log.w(TAG, "Cycle " + cycle + " timed out");
            }

            if (cycle % 50 == 0) {
                System.gc();
                Log.i(TAG, "Cycle " + cycle + "/" + CYCLES
                        + " survived, totalOps=" + totalOps.get());
            }
        }

        Log.i(TAG, "All " + CYCLES + " cycles completed, totalOps=" + totalOps.get());
    }
}
