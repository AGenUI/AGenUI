package com.amap.agenuiplayground.tests;

import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.function.FunctionCallContext;
import com.amap.agenui.function.FunctionConfig;
import com.amap.agenui.function.FunctionResult;
import com.amap.agenui.function.IFunction;
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
 * SDK Risk Probe: Engine config API TOCTOU race with destroy.
 *
 * RISK27 hypothesis:
 * Engine-level config APIs (registerFunction, unregisterFunction, setDayNightMode)
 * have a TOCTOU race with destroyAGenUIEngine(). The JNI bridge calls
 * getAGenUIEngine() which returns a valid raw pointer, then
 * destroyAGenUIEngine() on another thread atomically swaps it to nullptr,
 * calls engine->stop() (which deletes _functionCallManager,
 * _componentPropertySpecManager, etc.), then deletes the engine object.
 * The first thread then dereferences the stale pointer → UAF/SIGSEGV.
 *
 * Key difference from RISK06 (SDKRiskProbeEngineDestroyRaceTest):
 * - RISK06 tests SM streaming operations (beginTextStream, receiveTextChunk)
 *   which go through findSurfaceManager → SM methods
 * - RISK27 tests engine-level config operations which go directly through
 *   engine->registerFunction / engine->unregisterFunction /
 *   engine->setDayNightMode and touch different internal members
 *
 * Additional vulnerability: AGenUI.registerFunction() does NOT check
 * isInitialized() before calling JNI — it goes straight to native, so there
 * is no Java-level guard at all.
 *
 * Strategy:
 * - Create active SurfaceManagers to increase worker thread drain time (wider
 *   race window during engine.stop())
 * - Spawn threads doing tight-loop config API calls:
 *   setDayNightMode, registerFunction, unregisterFunction
 * - Destroy the engine from a separate thread while config workers are in flight
 * - If the race hits, the process aborts with SIGSEGV
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeConfigDestroyRaceTest extends AGenUIBaseTest {

    private static final String TAG = "RISK27";

    // R2 tuning: more threads per op and more SMs for heavier worker drain
    private static final int THREADS_PER_OP = 5;
    // 4 op types: daynight toggle, register, unregister, mixed SM streaming
    private static final int OP_TYPES = 4;
    private static final int TOTAL_THREADS = THREADS_PER_OP * OP_TYPES;

    private static final long WARMUP_MS = 800L;
    private static final long HAMMER_DURING_DESTROY_MS = 3000L;
    private static final long JOIN_TIMEOUT_MS = 10000L;

    private static final int SM_COUNT = 6;
    private static final int ROUNDS = 3;  // Repeat destroy/reinit cycles

    @Override
    public void tearDown() {
        // Engine is destroyed mid-test. Suppress base class tearDown.
        surfaceManager = null;
    }

    /**
     * RISK27: Concurrent config API calls racing with engine destroy.
     *
     * If SIGSEGV occurs, the instrumentation process crashes — the test itself
     * never finishes, which counts as a "hit".
     *
     * R2: increased threads/SMs, added heavy-payload chunk to widen race window.
     */
    @Test
    public void testSDKRISK27_configAPIsRacingWithEngineDestroy() throws Exception {
        AGenUI sdk = AGenUI.getInstance();

        // Create extra SurfaceManagers with active streaming to load the worker
        // thread queue (longer drain in stop() → wider race window).
        final SurfaceManager[] managers = new SurfaceManager[SM_COUNT];
        managers[0] = surfaceManager;
        runOnActivity(activity -> {
            for (int i = 1; i < SM_COUNT; i++) {
                managers[i] = new SurfaceManager(activity);
            }
        });

        // Pre-build streaming chunks — use larger payloads to increase worker
        // thread drain time and widen the race window.
        final String[] chunks = new String[SM_COUNT];
        for (int i = 0; i < SM_COUNT; i++) {
            chunks[i] = buildHeavyChunk("risk27_sm" + i);
        }

        // Pre-build function config JSON (reusable per thread)
        final String[] funcConfigs = new String[THREADS_PER_OP];
        for (int i = 0; i < THREADS_PER_OP; i++) {
            funcConfigs[i] = "{\"name\":\"risk27_func_" + i + "\"}";
        }

        // ---------- Spawn worker threads ----------
        final AtomicBoolean stop = new AtomicBoolean(false);
        final AtomicInteger opCount = new AtomicInteger(0);
        final CountDownLatch allReady = new CountDownLatch(TOTAL_THREADS);
        final CountDownLatch allDone = new CountDownLatch(TOTAL_THREADS);

        for (int t = 0; t < TOTAL_THREADS; t++) {
            final int threadIdx = t;
            final int opType = t / THREADS_PER_OP;
            final int opIdx = t % THREADS_PER_OP;
            final SurfaceManager sm = managers[threadIdx % SM_COUNT];
            final String chunk = chunks[threadIdx % SM_COUNT];

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
                        switch (opType) {
                            case 0: // setDayNightMode toggle
                                sdk.setDayNightMode(
                                        opCount.get() % 2 == 0 ? "light" : "dark");
                                break;

                            case 1: // registerFunction (each thread uses its own name)
                                String funcName = "risk27_func_" + opIdx;
                                sdk.registerFunction(new NoopFunction(funcName));
                                break;

                            case 2: // unregisterFunction
                                sdk.unregisterFunction("risk27_func_" + opIdx);
                                break;

                            case 3: // SM streaming (background noise to load worker thread)
                                sm.beginTextStream();
                                sm.receiveTextChunk(chunk);
                                sm.endTextStream();
                                break;
                        }
                        opCount.incrementAndGet();
                    } catch (Throwable ignored) {
                        // Java-level exceptions are expected once the engine is
                        // destroyed. Native crashes (SIGSEGV) abort the process
                        // and never reach this catch.
                    }
                }
                allDone.countDown();
            }, "config-race-" + opType + "-" + opIdx).start();
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
            } catch (Throwable thrown) {
                Log.e(TAG, "Destroy threw", thrown);
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
        Log.i(TAG, "Total ops: " + totalOps
                + ", destroy complete: " + destroyComplete.get());

        assertTrue("Worker threads did not finish in time", workersJoined);
        assertTrue("Engine destroy did not complete", destroyComplete.get());
        assertTrue("Expected some ops before engine destroy", opsBeforeDestroy > 0);
    }

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------

    /** Minimal IFunction that returns immediately. */
    private static class NoopFunction implements IFunction {
        private final FunctionConfig config;

        NoopFunction(String name) {
            config = new FunctionConfig(name);
        }

        @Override
        public FunctionResult execute(FunctionCallContext context,
                                       String jsonString) {
            return FunctionResult.createSuccess("noop");
        }

        @Override
        public FunctionConfig getConfig() {
            return config;
        }
    }

    private static String buildHeavyChunk(String surfaceId) {
        // Build a heavier payload: a Column with 20 Text children.
        // More components → more work on the worker thread → longer drain
        // during engine.stop() → wider TOCTOU race window.
        StringBuilder sb = new StringBuilder();
        sb.append("{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"")
          .append(surfaceId)
          .append("\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}");
        sb.append("{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"")
          .append(surfaceId)
          .append("\",\"components\":[");
        // Root column
        sb.append("{\"id\":\"root\",\"component\":\"Column\",\"children\":[");
        for (int i = 0; i < 20; i++) {
            if (i > 0) sb.append(",");
            sb.append("\"t").append(i).append("\"");
        }
        sb.append("],\"align\":\"stretch\","
                + "\"styles\":{\"width\":\"100%\",\"height\":\"auto\"}}");
        // 20 text children
        for (int i = 0; i < 20; i++) {
            sb.append(",{\"id\":\"t").append(i)
              .append("\",\"component\":\"Text\",")
              .append("\"text\":\"race test item ").append(i).append("\"}");
        }
        sb.append("]}}");
        return sb.toString();
    }
}
