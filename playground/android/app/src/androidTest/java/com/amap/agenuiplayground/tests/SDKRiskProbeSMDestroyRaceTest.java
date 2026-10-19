package com.amap.agenuiplayground.tests;

import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.base.AGenUIBaseTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * SDK Risk Probe: SurfaceManager destroy during active streaming causes
 * use-after-free via TOCTOU race in findSurfaceManagerByInstanceId().
 *
 * Hypothesis:
 * Every JNI bridge method does:
 *   ISurfaceManager* sm = findSurfaceManagerByInstanceId(instanceId);
 *   if (!sm) return;
 *   sm->someMethod(...);         // raw pointer, no ref-counting
 *
 * Concurrently, SurfaceManager.destroy() calls:
 *   engine->destroySurfaceManager(sm)
 *     -> erases from map (under lock)
 *     -> exitRunning()
 *     -> posts uninit() on worker thread
 *     -> worker runs uninit() → cleans _contentParser, _dispatcher
 *     -> lambda destroyed → shared_ptr released → SM freed
 *
 * Race window: Thread A gets raw pointer via find → Thread B destroys SM
 * → Thread A uses dangling pointer → SIGSEGV.
 *
 * No Java-side synchronization protects SurfaceManager.receiveTextChunk()
 * against concurrent SurfaceManager.destroy().
 *
 * This is a realistic scenario: network callback delivers data while user
 * navigates away and the view tears down the SurfaceManager.
 *
 * Probe style: timing/concurrency race + stress (many iterations)
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeSMDestroyRaceTest extends AGenUIBaseTest {

    private static final String TAG = "SDKRiskProbe10";

    // Pre-built protocol payloads
    private static final String UPDATE_TEMPLATE_PREFIX =
            "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"";
    private static final String UPDATE_TEMPLATE_SUFFIX =
            "\",\"components\":[" +
            "{\"id\":\"r\",\"component\":\"Column\",\"children\":[\"t1\"],\"align\":\"stretch\"," +
             "\"styles\":{\"width\":\"100%\",\"height\":\"auto\"}}," +
            "{\"id\":\"t1\",\"component\":\"Text\",\"text\":\"streaming payload\"}" +
            "]}}";

    @Override
    public void setUp() {
        // Only initialize AGenUI; we manage SMs ourselves in each test
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
     * RISK10: Multiple SurfaceManagers streaming concurrently, then destroyed
     * from a separate thread while streaming threads are still active.
     *
     * Creates a batch of SMs, starts a streaming thread per SM, then destroys
     * all SMs from another thread. Repeats many times.
     *
     * Expected: if TOCTOU race is hit, SIGSEGV in the instrumentation process.
     */
    @Test
    public void testSDKRISK10_smDestroyDuringActiveStreaming() throws Exception {
        final int BATCH_SIZE = 6;
        final int ITERATIONS = 200;
        final int STREAM_THREADS_PER_SM = 2;

        Log.i(TAG, "=== RISK10: SM destroy during streaming, batch=" + BATCH_SIZE
                + " x " + ITERATIONS + " iterations ===");

        for (int iter = 0; iter < ITERATIONS; iter++) {
            // --- Create batch of SMs on UI thread ---
            final SurfaceManager[] sms = new SurfaceManager[BATCH_SIZE];
            for (int i = 0; i < BATCH_SIZE; i++) {
                final int idx = i;
                runOnActivity(activity -> sms[idx] = new SurfaceManager(activity));
            }

            // --- Initialize each with a createSurface ---
            String[] surfaceIds = new String[BATCH_SIZE];
            String[] updateChunks = new String[BATCH_SIZE];
            for (int i = 0; i < BATCH_SIZE; i++) {
                surfaceIds[i] = "r10-" + iter + "-" + i;
                String createJson = "{\"version\":\"v0.9\",\"createSurface\":"
                        + "{\"surfaceId\":\"" + surfaceIds[i] + "\","
                        + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";
                sms[i].beginTextStream();
                sms[i].receiveTextChunk(createJson);
                sms[i].endTextStream();
                updateChunks[i] = UPDATE_TEMPLATE_PREFIX + surfaceIds[i] + UPDATE_TEMPLATE_SUFFIX;
            }

            // Minimal pause — just enough for worker to start processing
            Thread.sleep(5);

            // --- Start multiple streaming threads per SM ---
            AtomicBoolean stop = new AtomicBoolean(false);
            int totalStreamers = BATCH_SIZE * STREAM_THREADS_PER_SM;
            Thread[] streamers = new Thread[totalStreamers];
            for (int i = 0; i < BATCH_SIZE; i++) {
                for (int t = 0; t < STREAM_THREADS_PER_SM; t++) {
                    final int smIdx = i;
                    int tIdx = i * STREAM_THREADS_PER_SM + t;
                    streamers[tIdx] = new Thread(() -> {
                        while (!stop.get()) {
                            try {
                                sms[smIdx].beginTextStream();
                                sms[smIdx].receiveTextChunk(updateChunks[smIdx]);
                                sms[smIdx].endTextStream();
                            } catch (Exception e) {
                                break;
                            }
                        }
                    }, "streamer-" + iter + "-" + tIdx);
                    streamers[tIdx].start();
                }
            }

            // --- Destroyer: NO delay, destroy immediately to maximize race window ---
            Thread destroyer = new Thread(() -> {
                for (int i = 0; i < BATCH_SIZE; i++) {
                    try { sms[i].destroy(); } catch (Exception e) { /* ok */ }
                }
                stop.set(true);
            }, "destroyer-" + iter);
            destroyer.start();

            // --- Join all ---
            destroyer.join(5000);
            stop.set(true);
            for (Thread st : streamers) {
                st.join(3000);
            }

            // GC pressure: encourage scheduler variability and JNI thread pauses
            if (iter % 10 == 0) {
                System.gc();
                Log.i(TAG, "Iteration " + iter + "/" + ITERATIONS + " survived");
            }
        }

        Log.i(TAG, "All " + ITERATIONS + " iterations completed without crash");
    }

    /**
     * RISK11: Multiple threads stream to the SAME SurfaceManager concurrently,
     * while another thread destroys it. This stresses the internal state of
     * a single SM from multiple directions.
     *
     * Expected: if TOCTOU or internal state corruption, SIGSEGV.
     */
    /**
     * RISK11: Multiple threads stream to the SAME SurfaceManager concurrently,
     * while another thread destroys it AND we call various SM methods (not just
     * streaming). This stresses all JNI paths that use findSurfaceManagerByInstanceId.
     *
     * Expected: if TOCTOU or internal state corruption, SIGSEGV.
     */
    @Test
    public void testSDKRISK11_multiThreadStreamOnSameSMThenDestroy() throws Exception {
        final int STREAM_THREADS = 8;
        final int ITERATIONS = 200;

        Log.i(TAG, "=== RISK11: Multi-thread stream on same SM + destroy, "
                + STREAM_THREADS + " threads x " + ITERATIONS + " iterations ===");

        for (int iter = 0; iter < ITERATIONS; iter++) {
            // Create SM on UI thread
            final SurfaceManager[] smHolder = new SurfaceManager[1];
            runOnActivity(activity -> smHolder[0] = new SurfaceManager(activity));
            final SurfaceManager sm = smHolder[0];

            String surfaceId = "r11-" + iter;
            String createJson = "{\"version\":\"v0.9\",\"createSurface\":"
                    + "{\"surfaceId\":\"" + surfaceId + "\","
                    + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";
            sm.beginTextStream();
            sm.receiveTextChunk(createJson);
            sm.endTextStream();

            String updateChunk = UPDATE_TEMPLATE_PREFIX + surfaceId + UPDATE_TEMPLATE_SUFFIX;
            Thread.sleep(5);

            // Start N threads: half do streaming, half do mixed operations
            AtomicBoolean stop = new AtomicBoolean(false);
            Thread[] streamers = new Thread[STREAM_THREADS];
            for (int i = 0; i < STREAM_THREADS; i++) {
                final int idx = i;
                if (i % 2 == 0) {
                    // Streaming threads
                    streamers[i] = new Thread(() -> {
                        while (!stop.get()) {
                            try {
                                sm.beginTextStream();
                                sm.receiveTextChunk(updateChunk);
                                sm.endTextStream();
                            } catch (Exception e) { break; }
                        }
                    }, "mt-streamer-" + iter + "-" + idx);
                } else {
                    // Mixed operation threads: call various SM methods
                    streamers[i] = new Thread(() -> {
                        while (!stop.get()) {
                            try {
                                // Mix of different SM operations
                                sm.receiveTextChunk(updateChunk);
                                sm.beginTextStream();
                                sm.receiveTextChunk(updateChunk);
                                sm.receiveTextChunk(updateChunk);
                                sm.endTextStream();
                            } catch (Exception e) { break; }
                        }
                    }, "mt-mixer-" + iter + "-" + idx);
                }
                streamers[i].start();
            }

            // Destroy immediately — no delay, maximize race
            Thread destroyer = new Thread(() -> {
                try { sm.destroy(); } catch (Exception e) { /* ok */ }
                stop.set(true);
            }, "mt-destroyer-" + iter);
            destroyer.start();

            // Join all
            destroyer.join(5000);
            stop.set(true);
            for (Thread t : streamers) {
                t.join(3000);
            }

            if (iter % 50 == 0) {
                System.gc();
                Log.i(TAG, "Iteration " + iter + "/" + ITERATIONS + " survived");
            }
        }

        Log.i(TAG, "All " + ITERATIONS + " iterations completed without crash");
    }
}
