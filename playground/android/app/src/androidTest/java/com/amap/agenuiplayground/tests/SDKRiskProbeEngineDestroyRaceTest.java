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

import static org.junit.Assert.assertTrue;

/**
 * SDK Risk Probe: Engine-level use-after-free during concurrent destroy.
 *
 * Hypothesis:
 * destroyAGenUIEngine() atomically swaps the global engine pointer to nullptr,
 * then calls engine->stop() + delete. Any JNI thread that loaded the engine
 * pointer via getAGenUIEngine() before the swap but dereferences it after the
 * delete triggers a use-after-free (SIGSEGV).
 *
 * Two specific race windows:
 * 1. getAGenUIEngine() returns non-null → engine deleted → engine->findSurfaceManager()
 *    dereferences freed memory.
 * 2. findSurfaceManager() returns valid raw SM pointer → engine->stop() swaps the
 *    SM map and destroys shared_ptrs → raw SM pointer is dangling → SM method call
 *    triggers UAF.
 *
 * Strategy:
 * - Create multiple SurfaceManagers
 * - Spawn many threads, each doing a single tight-loop operation type to maximize
 *   the number of JNI crossings per second
 * - Destroy the engine from a SEPARATE thread (non-blocking) while workers are
 *   in flight
 * - If the race hits, the native process aborts with SIGSEGV
 *
 * R2 fixes (from R1 timeout):
 * - destroy() on separate thread to avoid blocking test flow
 * - Each thread does one op type (tighter loop, more JNI crossings)
 * - Pre-built chunk strings to minimize Java-side allocation overhead
 * - Removed testSDKRISK07 (std::call_once prevents re-init after destroy)
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeEngineDestroyRaceTest extends AGenUIBaseTest {

    private static final String TAG = "SDKRiskProbe06";
    private static final int SM_COUNT = 5;
    private static final int THREADS_PER_SM = 4;
    private static final int TOTAL_THREADS = SM_COUNT * THREADS_PER_SM;
    private static final long WARMUP_MS = 500L;
    private static final long HAMMER_DURING_DESTROY_MS = 2000L;
    private static final long JOIN_TIMEOUT_MS = 8000L;

    @Override
    public void tearDown() {
        // Engine is destroyed mid-test. Suppress base class tearDown.
        surfaceManager = null;
    }

    @Test
    public void testSDKRISK06_engineDestroyDuringActiveStreaming() throws Exception {
        // ---------- Create extra SurfaceManagers ----------
        final SurfaceManager[] managers = new SurfaceManager[SM_COUNT];
        managers[0] = surfaceManager;
        runOnActivity(activity -> {
            for (int i = 1; i < SM_COUNT; i++) {
                managers[i] = new SurfaceManager(activity);
            }
        });

        // Pre-build chunk strings to minimize per-iteration allocation.
        // Larger payloads widen the JNI race window (ScopedUtfChars + std::string
        // copy between findSurfaceManagerByInstanceId and SM method call).
        final String[] chunks = new String[SM_COUNT];
        for (int i = 0; i < SM_COUNT; i++) {
            chunks[i] = buildChunk("sm" + i);
        }

        // ---------- Spawn worker threads ----------
        final AtomicBoolean stop = new AtomicBoolean(false);
        final AtomicInteger opCount = new AtomicInteger(0);
        final CountDownLatch allReady = new CountDownLatch(TOTAL_THREADS);
        final CountDownLatch allDone = new CountDownLatch(TOTAL_THREADS);

        for (int t = 0; t < TOTAL_THREADS; t++) {
            final SurfaceManager sm = managers[t % SM_COUNT];
            final String chunk = chunks[t % SM_COUNT];
            // Each thread does ONE operation type for a tighter loop
            final int kind = t % THREADS_PER_SM;
            new Thread(() -> {
                allReady.countDown();
                try {
                    allReady.await(JOIN_TIMEOUT_MS, TimeUnit.MILLISECONDS);
                } catch (InterruptedException ignored) {
                    allDone.countDown();
                    return;
                }
                while (!stop.get()) {
                    try {
                        switch (kind) {
                            case 0:
                                sm.beginTextStream();
                                break;
                            case 1:
                                sm.receiveTextChunk(chunk);
                                break;
                            case 2:
                                sm.endTextStream();
                                break;
                            case 3:
                                sm.invalidateFunctionCallValues();
                                break;
                        }
                        opCount.incrementAndGet();
                    } catch (Throwable ignored) {
                        // Java-level exceptions are expected once the engine
                        // is destroyed. Native crashes (SIGSEGV) abort the
                        // process and never reach this catch.
                    }
                }
                allDone.countDown();
            }, "engine-destroy-racer-" + t).start();
        }

        assertTrue("Workers failed to start",
                allReady.await(JOIN_TIMEOUT_MS, TimeUnit.MILLISECONDS));

        // Brief warmup so workers are actively in JNI calls
        Thread.sleep(WARMUP_MS);
        int opsBeforeDestroy = opCount.get();
        Log.i(TAG, "Ops before destroy: " + opsBeforeDestroy);

        // ========== CRITICAL: destroy engine on a SEPARATE thread ==========
        // This avoids blocking the test thread (destroy joins the shared
        // worker thread which may take time draining queued tasks).
        final AtomicBoolean destroyComplete = new AtomicBoolean(false);
        Thread destroyThread = new Thread(() -> {
            try {
                Log.i(TAG, "Destroying engine...");
                AGenUI.getInstance().destroy();
                Log.i(TAG, "Engine destroyed.");
            } catch (Throwable t) {
                Log.e(TAG, "Destroy threw", t);
            } finally {
                destroyComplete.set(true);
            }
        }, "engine-destroyer");
        destroyThread.start();

        // Keep workers hammering JNI during the destroy window.
        // If SIGSEGV occurs, the instrumentation process crashes here.
        Thread.sleep(HAMMER_DURING_DESTROY_MS);

        // Signal workers to stop
        stop.set(true);

        boolean workersJoined = allDone.await(JOIN_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        destroyThread.join(JOIN_TIMEOUT_MS);

        int totalOps = opCount.get();
        Log.i(TAG, "Total ops: " + totalOps + ", destroy complete: " + destroyComplete.get());

        assertTrue("Worker threads did not finish in time", workersJoined);
        assertTrue("Engine destroy did not complete", destroyComplete.get());
        assertTrue("Expected some ops before engine destroy", opsBeforeDestroy > 0);
    }

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------

    private static String buildChunk(String surfaceId) {
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"" + surfaceId
                + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}"
                + "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId
                + "\",\"components\":[{\"id\":\"root\",\"component\":\"Column\","
                + "\"children\":[\"t1\"],\"align\":\"stretch\","
                + "\"styles\":{\"width\":\"100%\",\"height\":\"auto\"}},"
                + "{\"id\":\"t1\",\"component\":\"Text\",\"text\":\"engine destroy race\"}]}}";
    }
}
