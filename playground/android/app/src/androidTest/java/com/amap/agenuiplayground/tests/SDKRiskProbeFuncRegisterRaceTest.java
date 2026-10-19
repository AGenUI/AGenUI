package com.amap.agenuiplayground.tests;

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
 * SDK Risk Probe: Function register/unregister race with concurrent
 * SurfaceManager lifecycle churn and streaming data push.
 *
 * Risk hypothesis:
 *   FunctionCallManager holds a mutex while invoking callSync() on the worker thread.
 *   Concurrent registerFunction/unregisterFunction from another thread contends the same
 *   mutex. When combined with rapid SurfaceManager create/destroy (which triggers
 *   initFunctionCalls on the worker thread), the mutex + JNI bridge lifecycle can race
 *   and produce native crash or deadlock.
 *
 * Probe style: concurrency race + stress
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeFuncRegisterRaceTest extends AGenUIBaseTest {

    private static final int SM_RACER_COUNT = 5;
    private static final int FUNC_RACER_COUNT = 2;
    private static final long ROUND_DURATION_MS = 10000L;
    private static final long JOIN_TIMEOUT_MS = 5000L;
    private static final int FUNC_POOL_SIZE = 10;

    @Test
    public void testSDKRISK_funcRegisterRaceWithSMChurn() throws Exception {
        AtomicBoolean stop = new AtomicBoolean(false);
        AtomicInteger smIterations = new AtomicInteger(0);
        AtomicInteger funcIterations = new AtomicInteger(0);
        int totalThreads = SM_RACER_COUNT + FUNC_RACER_COUNT;
        CountDownLatch done = new CountDownLatch(totalThreads);

        Thread[] threads = new Thread[totalThreads];

        // --- SurfaceManager lifecycle churn + streaming threads ---
        for (int i = 0; i < SM_RACER_COUNT; i++) {
            final int workerId = i;
            threads[i] = new Thread(() -> {
                try {
                    while (!stop.get()) {
                        SurfaceManager manager = null;
                        try {
                            final SurfaceManager[] holder = new SurfaceManager[1];
                            runOnActivity(activity -> holder[0] = new SurfaceManager(activity));
                            manager = holder[0];
                            if (manager == null) continue;

                            int iter = smIterations.incrementAndGet();
                            String surfaceId = "risk-func-sm-" + workerId + "-" + iter;

                            // Strategy A: rapid multi-chunk then immediate destroy (no endTextStream)
                            if (iter % 4 == 0) {
                                manager.beginTextStream();
                                manager.receiveTextChunk(buildChunk(surfaceId));
                                manager.receiveTextChunk(buildChunk(surfaceId + "-extra"));
                                // Intentionally skip endTextStream - destroy with stream still open
                            }
                            // Strategy B: double beginTextStream without endTextStream
                            else if (iter % 4 == 1) {
                                manager.beginTextStream();
                                manager.receiveTextChunk(buildChunk(surfaceId));
                                // Begin a new stream without ending the previous one
                                manager.beginTextStream();
                                manager.receiveTextChunk(buildChunk(surfaceId + "-b"));
                            }
                            // Strategy C: normal flow but with zero delay before destroy
                            else if (iter % 4 == 2) {
                                manager.beginTextStream();
                                manager.receiveTextChunk(buildChunk(surfaceId));
                                manager.endTextStream();
                                // No delay - immediate destroy
                            }
                            // Strategy D: empty chunk edge case
                            else {
                                manager.beginTextStream();
                                manager.receiveTextChunk("");
                                manager.receiveTextChunk(buildChunk(surfaceId));
                                manager.endTextStream();
                            }
                        } catch (Throwable ignored) {
                            // Native crash aborts instrumentation; Java exceptions are noise.
                        } finally {
                            if (manager != null) {
                                try {
                                    SurfaceManager fm = manager;
                                    runOnActivity(activity -> fm.destroy());
                                } catch (Throwable ignored) {}
                            }
                        }
                    }
                } finally {
                    done.countDown();
                }
            }, "sm-churn-" + workerId);
        }

        // --- Function register/unregister race threads ---
        for (int i = 0; i < FUNC_RACER_COUNT; i++) {
            final int workerId = i;
            threads[SM_RACER_COUNT + i] = new Thread(() -> {
                try {
                    AGenUI engine = AGenUI.getInstance();
                    while (!stop.get()) {
                        int iter = funcIterations.incrementAndGet();
                        String funcName = "probe_func_" + workerId + "_" + (iter % FUNC_POOL_SIZE);
                        try {
                            // Register a function that does blocking work (simulates real SDK usage)
                            engine.registerFunction(new SlowFunction(funcName, iter % 7 == 0));
                            // Immediately unregister to maximize contention window
                            Thread.sleep(iter % 3); // 0-2ms jitter
                            engine.unregisterFunction(funcName);
                        } catch (Throwable ignored) {
                            // Catch any exception; native crash will abort the process.
                        }
                    }
                } finally {
                    done.countDown();
                }
            }, "func-race-" + workerId);
        }

        // Start all threads
        for (Thread t : threads) {
            t.start();
        }

        // Let the race run
        Thread.sleep(ROUND_DURATION_MS);
        stop.set(true);

        boolean allJoined = done.await(JOIN_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        assertTrue("Race workers did not finish in time (possible deadlock/freeze)", allJoined);
        assertTrue("Expected SM iterations > 0", smIterations.get() > 0);
        assertTrue("Expected func iterations > 0", funcIterations.get() > 0);
    }

    /**
     * A function implementation that optionally sleeps to simulate real-world blocking behavior.
     * When the C++ worker thread calls this via JNI while holding the FunctionCallManager mutex,
     * concurrent unregisterFunction calls will contend on the same mutex.
     */
    private static class SlowFunction implements IFunction {
        private final FunctionConfig config;
        private final boolean slow;

        SlowFunction(String name, boolean slow) {
            this.config = new FunctionConfig(name);
            this.slow = slow;
        }

        @Override
        public FunctionResult execute(FunctionCallContext context, String jsonString) {
            if (slow) {
                try {
                    Thread.sleep(5);
                } catch (InterruptedException ignored) {}
            }
            return FunctionResult.createSuccess("ok");
        }

        @Override
        public FunctionConfig getConfig() {
            return config;
        }
    }

    private static String buildChunk(String surfaceId) {
        // A2UI protocol: createSurface + updateComponents with a function call trigger
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"" + surfaceId
                + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}"
                + "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId
                + "\",\"components\":[{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"btn\"],"
                + "\"align\":\"stretch\",\"styles\":{\"width\":\"100%\",\"height\":\"100%\"}},"
                + "{\"id\":\"btn\",\"component\":\"Button\",\"text\":\"func race probe\","
                + "\"onClick\":{\"action\":\"functionCall\","
                + "\"functionName\":\"probe_func_0_0\",\"params\":{}}}]}}";
    }
}
