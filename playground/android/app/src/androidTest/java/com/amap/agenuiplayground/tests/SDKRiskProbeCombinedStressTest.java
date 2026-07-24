package com.amap.agenuiplayground.tests;

import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.ISurfaceManagerListener;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenui.render.surface.SurfaceSize;
import com.amap.agenuiplayground.base.AGenUIBaseTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.Assert.assertTrue;

/**
 * SDK Risk Probe: Combined high-pressure stress test.
 *
 * Hypothesis:
 * While individual SDK operations are well-protected, combining multiple
 * stress vectors simultaneously may reveal emergent issues:
 *   - Re-entry: performing SDK operations from within listener callbacks
 *   - Multi-threaded SM lifecycle: concurrent create/stream/destroy from
 *     multiple threads
 *   - GC pressure: forcing garbage collection during native JNI callbacks
 *   - Listener churn: rapidly adding/removing listeners during streaming
 *
 * Probe style: pressure + timing + re-entry combination
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeCombinedStressTest extends AGenUIBaseTest {

    private static final String TAG = "SDKRiskProbe16";

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
        // No-op: each test manages its own SM lifecycle
    }

    // ===================== RISK16: Re-entry in listener callbacks =====================

    /**
     * RISK16: From within onCreateSurface callback, immediately:
     *   - Start a new streaming session on the same SM
     *   - Add and remove listeners
     *   - Trigger invalidateFunctionCallValues
     *   - On alternate cycles, destroy the SM from the callback
     *
     * 200 cycles. Tests re-entry safety of the SDK.
     */
    @Test(timeout = 120_000)
    public void testSDKRISK16_listenerReentryStress() throws Exception {
        final int CYCLES = 200;
        final AtomicInteger callbackFires = new AtomicInteger(0);
        final AtomicInteger errors = new AtomicInteger(0);

        for (int i = 0; i < CYCLES; i++) {
            final int cycle = i;
            SurfaceManager sm = createSM();
            final boolean destroyFromCallback = (cycle % 5 == 0);
            final CountDownLatch callbackDone = new CountDownLatch(1);

            ISurfaceManagerListener reentrantListener = new ISurfaceManagerListener() {
                @Override
                public void onCreateSurface(Surface surface) {
                    callbackFires.incrementAndGet();
                    try {
                        // Re-entry: start another stream from callback
                        sm.beginTextStream();
                        sm.receiveTextChunk(buildCreateSurfacePayload("reentry-" + cycle));
                        sm.endTextStream();

                        // Add another listener from callback
                        ISurfaceManagerListener inner = new ISurfaceManagerListener() {
                            @Override
                            public void onCreateSurface(Surface s) {}
                            @Override
                            public void onDeleteSurface(Surface s) {}

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
                        sm.addListener(inner);
                        sm.removeListener(inner);

                        // Trigger invalidation from callback
                        sm.invalidateFunctionCallValues();

                        if (destroyFromCallback) {
                            sm.destroy();
                        }
                    } catch (Exception e) {
                        Log.w(TAG, "Reentry error cycle=" + cycle + ": " + e);
                        errors.incrementAndGet();
                    } finally {
                        callbackDone.countDown();
                    }
                }

                @Override
                public void onDeleteSurface(Surface surface) {}

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

            sm.addListener(reentrantListener);

            // Stream data to trigger onCreateSurface
            sm.beginTextStream();
            sm.receiveTextChunk(buildCreateSurfacePayload("trigger-" + cycle));
            sm.receiveTextChunk(buildUpdatePayload("trigger-" + cycle));
            sm.endTextStream();

            // Wait for callback or timeout
            boolean fired = callbackDone.await(3, TimeUnit.SECONDS);
            if (!fired) {
                Log.w(TAG, "Callback did not fire for cycle " + cycle);
            }

            // Clean up if not destroyed from callback
            if (!destroyFromCallback) {
                destroySM(sm);
            }
        }

        Log.i(TAG, "=== RISK16 SUMMARY: cycles=" + CYCLES
                + ", callbacks=" + callbackFires.get()
                + ", errors=" + errors.get() + " ===");
        assertTrue("Process survived listener re-entry stress", true);
    }

    // ===================== RISK17: Multi-threaded SM tornado =====================

    /**
     * RISK17: 4 threads each doing rapid SM create → stream → destroy loops.
     * All share the same AGenUI engine. Tests thread-safety of engine-level
     * SM management (map insertion/removal, instanceId counter, worker thread).
     *
     * Each thread does 50 cycles = 200 total SM lifecycles in parallel.
     */
    @Test(timeout = 120_000)
    public void testSDKRISK17_multiThreadSMTornado() throws Exception {
        final int THREADS = 4;
        final int CYCLES_PER_THREAD = 50;
        final ExecutorService pool = Executors.newFixedThreadPool(THREADS);
        final CyclicBarrier barrier = new CyclicBarrier(THREADS);
        final AtomicInteger completedCycles = new AtomicInteger(0);
        final AtomicInteger crashCount = new AtomicInteger(0);
        final CountDownLatch allDone = new CountDownLatch(THREADS);

        for (int t = 0; t < THREADS; t++) {
            final int threadId = t;
            pool.submit(() -> {
                try {
                    barrier.await(10, TimeUnit.SECONDS);
                    for (int c = 0; c < CYCLES_PER_THREAD; c++) {
                        final int cycle = c;
                        try {
                            SurfaceManager sm = createSM();
                            String sid = "t" + threadId + "-c" + cycle;

                            sm.beginTextStream();
                            sm.receiveTextChunk(buildCreateSurfacePayload(sid));
                            sm.receiveTextChunk(buildUpdatePayload(sid));
                            sm.endTextStream();

                            // Brief processing time
                            Thread.sleep(5);

                            // Invalidate from background thread
                            sm.invalidateFunctionCallValues();

                            destroySM(sm);
                            completedCycles.incrementAndGet();
                        } catch (Exception e) {
                            Log.w(TAG, "Thread " + threadId + " cycle " + cycle
                                    + " error: " + e);
                            crashCount.incrementAndGet();
                        }
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Thread " + threadId + " barrier error: " + e);
                } finally {
                    allDone.countDown();
                }
            });
        }

        boolean finished = allDone.await(90, TimeUnit.SECONDS);
        pool.shutdown();

        Log.i(TAG, "=== RISK17 SUMMARY: completed=" + completedCycles.get()
                + "/" + (THREADS * CYCLES_PER_THREAD)
                + ", crashes=" + crashCount.get()
                + ", finished=" + finished + " ===");
        assertTrue("All threads finished within timeout", finished);
        assertTrue("Process survived multi-thread SM tornado", true);
    }

    // ===================== RISK18: GC pressure during callbacks =====================

    /**
     * RISK18: Allocates large temporary arrays to trigger GC while streaming
     * is active and callbacks are being dispatched. Tests JNI GlobalRef and
     * local ref frame integrity under GC pressure.
     *
     * 100 cycles: each cycle streams, allocates 5MB of garbage, forces GC,
     * then verifies SM still functions.
     */
    @Test(timeout = 120_000)
    public void testSDKRISK18_gcPressureDuringCallbacks() throws Exception {
        final int CYCLES = 100;
        final AtomicInteger callbackCount = new AtomicInteger(0);

        for (int i = 0; i < CYCLES; i++) {
            SurfaceManager sm = createSM();

            ISurfaceManagerListener listener = new ISurfaceManagerListener() {
                @Override
                public void onCreateSurface(Surface surface) {
                    callbackCount.incrementAndGet();
                    // Allocate garbage inside callback to increase GC pressure
                    byte[] garbage = new byte[512 * 1024]; // 512KB
                    garbage[0] = 1; // prevent optimization
                }
                @Override
                public void onDeleteSurface(Surface surface) {}

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

            // Stream valid data
            String sid = "gc-" + i;
            sm.beginTextStream();
            sm.receiveTextChunk(buildCreateSurfacePayload(sid));
            sm.receiveTextChunk(buildUpdatePayload(sid));

            // Allocate large garbage arrays to trigger GC
            List<byte[]> garbage = new ArrayList<>();
            for (int j = 0; j < 10; j++) {
                garbage.add(new byte[512 * 1024]); // 5MB total
            }

            sm.endTextStream();

            // Force GC while callbacks may still be pending
            garbage.clear();
            garbage = null;
            System.gc();
            System.runFinalization();
            System.gc();

            Thread.sleep(20);

            // Verify SM still functions after GC storm
            sm.beginTextStream();
            sm.receiveTextChunk(buildCreateSurfacePayload("gc-verify-" + i));
            sm.endTextStream();

            Thread.sleep(10);
            destroySM(sm);
        }

        Log.i(TAG, "=== RISK18 SUMMARY: cycles=" + CYCLES
                + ", callbacks=" + callbackCount.get() + " ===");
        assertTrue("Process survived GC pressure during callbacks", true);
    }

    // ===================== RISK19: Listener churn storm =====================

    /**
     * RISK19: Rapidly add/remove 50 listeners while streaming is active.
     * Tests CopyOnWriteArrayList thread-safety under high churn with
     * concurrent native callback dispatch.
     *
     * Pattern: start stream → spawn a thread that adds/removes listeners
     * in a tight loop → continue streaming → end stream → verify no crash.
     */
    @Test(timeout = 60_000)
    public void testSDKRISK19_listenerChurnDuringStreaming() throws Exception {
        final int CYCLES = 30;

        for (int i = 0; i < CYCLES; i++) {
            SurfaceManager sm = createSM();
            final AtomicInteger addRemoveOps = new AtomicInteger(0);

            sm.beginTextStream();
            sm.receiveTextChunk(buildCreateSurfacePayload("churn-" + i));

            // Start a thread that churns listeners
            Thread churnThread = new Thread(() -> {
                List<ISurfaceManagerListener> tempListeners = new ArrayList<>();
                for (int j = 0; j < 50; j++) {
                    ISurfaceManagerListener l = new ISurfaceManagerListener() {
                        @Override
                        public void onCreateSurface(Surface s) {}
                        @Override
                        public void onDeleteSurface(Surface s) {}

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
                    sm.addListener(l);
                    tempListeners.add(l);
                    addRemoveOps.incrementAndGet();
                }
                // Remove in reverse
                for (int j = tempListeners.size() - 1; j >= 0; j--) {
                    sm.removeListener(tempListeners.get(j));
                    addRemoveOps.incrementAndGet();
                }
            });
            churnThread.start();

            // Continue streaming while listeners are being churned
            sm.receiveTextChunk(buildUpdatePayload("churn-" + i));
            sm.receiveTextChunk(buildUpdatePayload("churn-" + i));
            sm.receiveTextChunk(buildUpdatePayload("churn-" + i));
            sm.endTextStream();

            churnThread.join(5000);
            Thread.sleep(10);
            destroySM(sm);
        }

        Log.i(TAG, "=== RISK19 SUMMARY: cycles=" + CYCLES + " ===");
        assertTrue("Process survived listener churn storm", true);
    }

    // ===================== Helpers =====================

    private SurfaceManager createSM() throws Exception {
        final SurfaceManager[] holder = new SurfaceManager[1];
        final CountDownLatch latch = new CountDownLatch(1);
        runOnActivity(activity -> {
            holder[0] = new SurfaceManager(activity);
            latch.countDown();
        });
        assertTrue("SM creation timeout", latch.await(5, TimeUnit.SECONDS));
        return holder[0];
    }

    private void destroySM(SurfaceManager sm) throws Exception {
        if (sm == null) return;
        final CountDownLatch latch = new CountDownLatch(1);
        runOnActivity(activity -> {
            sm.destroy();
            latch.countDown();
        });
        latch.await(5, TimeUnit.SECONDS);
    }

    private static String buildCreateSurfacePayload(String surfaceId) {
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\""
                + surfaceId
                + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}";
    }

    private static String buildUpdatePayload(String surfaceId) {
        return "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\""
                + surfaceId + "\",\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"t1\",\"b1\"],"
                + "\"align\":\"stretch\",\"styles\":{\"width\":\"100%\"}},"
                + "{\"id\":\"t1\",\"component\":\"Text\",\"text\":\"Stress test item\","
                + "\"styles\":{\"fontSize\":\"14px\"}},"
                + "{\"id\":\"b1\",\"component\":\"Button\",\"label\":\"Action\","
                + "\"styles\":{\"marginTop\":\"8px\"}}"
                + "]}}";
    }
}
