package com.amap.agenuiplayground.tests;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.function.FunctionCallContext;
import com.amap.agenui.function.FunctionConfig;
import com.amap.agenui.function.FunctionResult;
import com.amap.agenui.function.IFunction;
import com.amap.agenui.render.surface.ISurfaceManagerListener;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenui.render.surface.SurfaceSize;
import com.amap.agenuiplayground.A2UIPlaygroundActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * RISK22/23: FunctionCallManager re-entrant deadlock probe.
 *
 * Hypothesis 8: FunctionCallManager uses std::mutex (non-recursive). If a platform
 * function's execute() callback registers or unregisters another function while
 * executeFunctionCallSync holds the mutex, the same thread (worker thread) attempts
 * to re-acquire the same non-recursive mutex → permanent deadlock.
 *
 * Root cause chain (C++ code analysis):
 *   agenui_functioncall_manager.h:82  → mutable std::mutex _mutex  (NOT recursive!)
 *   agenui_functioncall_manager.cpp:72 → executeFunctionCallSync: lock_guard<mutex> lock(_mutex)
 *   agenui_functioncall_manager.cpp:90 →   calls entry->function->callSync() via JNI
 *   jni_android_platform_function.cpp:144 →   env->CallObjectMethod → Java IFunction.execute()
 *     ↓ (user code in Java, still on worker thread)
 *   AGenUI.registerFunction() → nativeRegisterFunction → engine->registerFunction()
 *   agenui_functioncall_manager.cpp:32 → registerFunctionCall: lock_guard<mutex> lock(_mutex)
 *   → DEADLOCK! Same thread, same std::mutex, non-recursive.
 *
 * Trigger path A (initial render):
 *   Stream a component with text: {"call":"reEntrantFunc","args":{}} → worker thread
 *   renders component → getValueData() → executeFunctionCallSync → JNI → Java → re-enter
 *
 * Trigger path B (setDayNightMode):
 *   setDayNightMode("dark") → invalidateFunctionCallValues → re-evaluate all function
 *   call data values → executeFunctionCallSync → JNI → Java → re-enter
 *
 * Detection: after triggering the deadlock, verify that subsequent SM operations
 * (posted to the same worker thread) never complete (timeout).
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeReentrantDeadlockTest {

    private static final String TAG = "RiskProbe-Deadlock";
    private static final long SETUP_TIMEOUT_MS = 10000;

    @Rule
    public ActivityScenarioRule<A2UIPlaygroundActivity> activityRule =
            new ActivityScenarioRule<>(A2UIPlaygroundActivity.class);

    private Activity activity;
    private final AtomicInteger functionCallCount = new AtomicInteger(0);
    private final AtomicBoolean deadlockDetected = new AtomicBoolean(false);

    @Before
    public void setUp() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        activityRule.getScenario().onActivity(a -> {
            activity = a;
            AGenUI.getInstance().initialize(a.getApplicationContext());
            latch.countDown();
        });
        assertTrue("Activity setup timed out", latch.await(SETUP_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    @After
    public void tearDown() {
        activity = null;
    }

    // ==================== JSON builders ====================

    private String buildCreateSurfaceJSON(String surfaceId) {
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"" + surfaceId
                + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}";
    }

    /**
     * Build updateComponents with a Text component whose "text" property is a
     * function call data binding: {"call":"funcName","args":{"input":"test"}}.
     *
     * When the component is rendered (or re-evaluated via invalidateFunctionCallValues),
     * the engine calls executeFunctionCallSync("funcName") which goes through JNI to
     * the registered IFunction.execute().
     */
    private String buildUpdateWithFunctionCallBinding(String surfaceId, String funcName) {
        return "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId
                + "\",\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"txt1\"]},"
                + "{\"id\":\"txt1\",\"component\":\"Text\","
                + "\"text\":{\"call\":\"" + funcName + "\",\"args\":{\"input\":\"test\"}}}"
                + "]}}";
    }

    /**
     * Build a simple updateComponents with a plain Text (no function call binding).
     * Used to verify worker thread liveness.
     */
    private String buildSimpleUpdate(String surfaceId) {
        return "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + surfaceId
                + "\",\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"txt1\"]},"
                + "{\"id\":\"txt1\",\"component\":\"Text\",\"text\":\"alive\"}"
                + "]}}";
    }

    // ==================== Helper ====================

    private SurfaceManager createSMOnMainThread() throws Exception {
        AtomicReference<SurfaceManager> ref = new AtomicReference<>();
        CountDownLatch latch = new CountDownLatch(1);
        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                ref.set(new SurfaceManager(activity));
            } catch (Exception e) {
                Log.e(TAG, "createSM failed", e);
            }
            latch.countDown();
        });
        assertTrue("createSM timed out", latch.await(SETUP_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        assertNotNull("SM should be created", ref.get());
        return ref.get();
    }

    private void destroySMOnMainThread(SurfaceManager sm) {
        if (sm == null) return;
        CountDownLatch latch = new CountDownLatch(1);
        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                sm.destroy();
            } catch (Exception e) {
                Log.w(TAG, "destroy SM failed (expected if deadlocked)", e);
            }
            latch.countDown();
        });
        try {
            latch.await(5, TimeUnit.SECONDS);
        } catch (InterruptedException ignored) {
        }
    }

    /**
     * Test whether the worker thread responds to a new stream operation.
     * Returns true if the worker thread processes within the timeout.
     */
    private boolean isWorkerThreadAlive(SurfaceManager sm, String surfaceId, long timeoutMs) throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        ISurfaceManagerListener listener = new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {
                latch.countDown();
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
        sm.beginTextStream();
        sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId));
        sm.endTextStream();
        boolean alive = latch.await(timeoutMs, TimeUnit.MILLISECONDS);
        sm.removeListener(listener);
        return alive;
    }

    // ==================== RISK22: Re-entrant deadlock via registerFunction ====================

    /**
     * RISK22: Register a function whose execute() calls registerFunction().
     * When the engine calls executeFunctionCallSync (holding the mutex), the
     * Java callback re-enters through registerFunction → registerFunctionCall
     * which tries to acquire the same non-recursive mutex → deadlock.
     *
     * Protocol path: component text property with function call data binding
     * → DataValueParser::parseFunctionCallDataValue → FunctionCallDataValue
     * → getValueData() → executeFunctionCallSync (locks _mutex) → JNI
     * → IFunction.execute() → AGenUI.registerFunction() → JNI
     * → engine->registerFunction() → FunctionCallManager::registerFunctionCall
     * → tries to lock _mutex (SAME THREAD) → DEADLOCK!
     */
    @Test(timeout = 60_000)
    public void RISK22_reentrantDeadlockViaRegisterFunction() throws Exception {
        Log.i(TAG, "=== RISK22: Re-entrant deadlock via registerFunction ===");

        AGenUI sdk = AGenUI.getInstance();

        // Register a function that re-enters via registerFunction
        sdk.registerFunction(new IFunction() {
            @Override
            public FunctionResult execute(FunctionCallContext context, String jsonString) {
                int count = functionCallCount.incrementAndGet();
                Log.w(TAG, "reEntrantFunc called (count=" + count + "), about to re-enter...");
                try {
                    // RE-ENTRANT CALL: This runs on the worker thread while
                    // executeFunctionCallSync holds FunctionCallManager._mutex.
                    // registerFunction → nativeRegisterFunction → engine->registerFunction()
                    // → _functionCallManager->registerFunctionCall() → lock(_mutex)
                    // → DEADLOCK (std::mutex is non-recursive)
                    AGenUI.getInstance().registerFunction(new IFunction() {
                        @Override
                        public FunctionResult execute(FunctionCallContext ctx, String json) {
                            return FunctionResult.createSuccess("inner");
                        }
                        @Override
                        public FunctionConfig getConfig() {
                            return new FunctionConfig("innerFunc_" + System.nanoTime());
                        }
                    });
                    Log.i(TAG, "reEntrantFunc: registerFunction returned (no deadlock)");
                } catch (Exception e) {
                    Log.e(TAG, "reEntrantFunc: registerFunction threw", e);
                }
                return FunctionResult.createSuccess("done");
            }
            @Override
            public FunctionConfig getConfig() {
                return new FunctionConfig("reEntrantFunc");
            }
        });
        Log.i(TAG, "Registered reEntrantFunc");

        // Create SM
        SurfaceManager sm = createSMOnMainThread();

        // Stream content with function call data binding
        String surfaceId = "deadlock-s1";
        String stream = buildCreateSurfaceJSON(surfaceId)
                + buildUpdateWithFunctionCallBinding(surfaceId, "reEntrantFunc");

        Log.i(TAG, "Streaming content with function call binding...");
        sm.beginTextStream();
        sm.receiveTextChunk(stream);
        sm.endTextStream();

        // Wait for worker thread to process and potentially deadlock
        Thread.sleep(3000);
        Log.i(TAG, "Function call count after initial render: " + functionCallCount.get());

        // Check if worker thread is still alive
        String probeSurfaceId = "probe-alive-1";
        boolean workerAlive = isWorkerThreadAlive(sm, probeSurfaceId, 5000);
        Log.i(TAG, "Worker thread alive after initial render: " + workerAlive);

        if (!workerAlive && functionCallCount.get() > 0) {
            deadlockDetected.set(true);
            Log.e(TAG, "*** DEADLOCK DETECTED (path A: initial render) ***");
            Log.e(TAG, "Function was called " + functionCallCount.get() + " time(s) but worker thread is frozen");
        }

        // If no deadlock from initial render, try setDayNightMode trigger
        if (!deadlockDetected.get()) {
            Log.i(TAG, "No deadlock from initial render, trying setDayNightMode...");
            sdk.setDayNightMode("dark");
            Thread.sleep(3000);
            Log.i(TAG, "Function call count after setDayNightMode: " + functionCallCount.get());

            String probeSurfaceId2 = "probe-alive-2";
            boolean workerAlive2 = isWorkerThreadAlive(sm, probeSurfaceId2, 5000);
            Log.i(TAG, "Worker thread alive after setDayNightMode: " + workerAlive2);

            if (!workerAlive2 && functionCallCount.get() > 0) {
                deadlockDetected.set(true);
                Log.e(TAG, "*** DEADLOCK DETECTED (path B: setDayNightMode) ***");
            }
        }

        // Report BEFORE cleanup — cleanup may block if mutex is held
        Log.i(TAG, "=== RISK22 RESULT ===");
        Log.i(TAG, "Deadlock detected: " + deadlockDetected.get());
        Log.i(TAG, "Function call count: " + functionCallCount.get());
        if (deadlockDetected.get()) {
            Log.e(TAG, "ROOT CAUSE: FunctionCallManager._mutex is std::mutex (non-recursive)");
            Log.e(TAG, "  executeFunctionCallSync holds _mutex while calling platform function");
            Log.e(TAG, "  If callback calls registerFunction/unregisterFunction, same thread");
            Log.e(TAG, "  re-acquires _mutex → permanent deadlock on worker thread.");
            Log.e(TAG, "  All subsequent SM operations (stream, create, invalidate) hang.");
            Log.e(TAG, "  Even main-thread unregisterFunction blocks (mutex held forever).");
        }

        // Cleanup: SM destroy is partially safe (Java-side cleanup works, native
        // uninit posts to deadlocked worker thread and leaks). Do NOT call
        // unregisterFunction — it would block on the deadlocked mutex.
        destroySMOnMainThread(sm);
        // Skip: sdk.unregisterFunction("reEntrantFunc") — would hang!

        assertTrue("RISK22: Deadlock should be detected (FunctionCallManager mutex is non-recursive)",
                deadlockDetected.get());
    }

    // ==================== RISK23: Re-entrant deadlock via unregisterFunction ====================

    /**
     * RISK23: Same mechanism but triggered via unregisterFunction in the callback.
     * This simulates a common "one-shot function" pattern where a function
     * unregisters itself after first execution.
     *
     * executeFunctionCallSync (locks _mutex) → callSync → IFunction.execute()
     * → AGenUI.unregisterFunction("selfFunc") → nativeUnregisterFunction
     * → engine->unregisterFunction() → FunctionCallManager::unregisterFunctionCall
     * → tries to lock _mutex → DEADLOCK!
     */
    @Test(timeout = 60_000)
    public void RISK23_reentrantDeadlockViaUnregisterFunction() throws Exception {
        Log.i(TAG, "=== RISK23: Re-entrant deadlock via unregisterFunction ===");

        AGenUI sdk = AGenUI.getInstance();
        AtomicInteger callCount = new AtomicInteger(0);
        AtomicBoolean deadlock = new AtomicBoolean(false);

        // Register a "one-shot" function that unregisters itself after execution
        sdk.registerFunction(new IFunction() {
            @Override
            public FunctionResult execute(FunctionCallContext context, String jsonString) {
                int count = callCount.incrementAndGet();
                Log.w(TAG, "selfUnregFunc called (count=" + count + "), about to unregister self...");
                try {
                    // RE-ENTRANT CALL: unregister self while holding the mutex
                    AGenUI.getInstance().unregisterFunction("selfUnregFunc");
                    Log.i(TAG, "selfUnregFunc: unregisterFunction returned (no deadlock)");
                } catch (Exception e) {
                    Log.e(TAG, "selfUnregFunc: unregisterFunction threw", e);
                }
                return FunctionResult.createSuccess("one-shot done");
            }
            @Override
            public FunctionConfig getConfig() {
                return new FunctionConfig("selfUnregFunc");
            }
        });
        Log.i(TAG, "Registered selfUnregFunc (one-shot pattern)");

        // Create SM
        SurfaceManager sm = createSMOnMainThread();

        // Stream content with function call data binding
        String surfaceId = "deadlock-unreg-s1";
        String stream = buildCreateSurfaceJSON(surfaceId)
                + buildUpdateWithFunctionCallBinding(surfaceId, "selfUnregFunc");

        Log.i(TAG, "Streaming content with selfUnregFunc binding...");
        sm.beginTextStream();
        sm.receiveTextChunk(stream);
        sm.endTextStream();

        // Wait for processing
        Thread.sleep(3000);
        Log.i(TAG, "Function call count after initial render: " + callCount.get());

        // Check worker thread liveness
        String probeSurfaceId = "probe-unreg-1";
        boolean workerAlive = isWorkerThreadAlive(sm, probeSurfaceId, 5000);
        Log.i(TAG, "Worker thread alive: " + workerAlive);

        if (!workerAlive && callCount.get() > 0) {
            deadlock.set(true);
            Log.e(TAG, "*** DEADLOCK DETECTED (unregisterFunction path) ***");
        }

        // If no deadlock from initial render, try setDayNightMode
        if (!deadlock.get()) {
            // Re-register for the setDayNightMode attempt
            sdk.registerFunction(new IFunction() {
                @Override
                public FunctionResult execute(FunctionCallContext context, String jsonString) {
                    int count = callCount.incrementAndGet();
                    Log.w(TAG, "selfUnregFunc re-registered, called (count=" + count + ")");
                    try {
                        AGenUI.getInstance().unregisterFunction("selfUnregFunc");
                    } catch (Exception e) {
                        Log.e(TAG, "unregister threw", e);
                    }
                    return FunctionResult.createSuccess("done");
                }
                @Override
                public FunctionConfig getConfig() {
                    return new FunctionConfig("selfUnregFunc");
                }
            });

            sdk.setDayNightMode("light");
            Thread.sleep(3000);

            String probeSurfaceId2 = "probe-unreg-2";
            boolean workerAlive2 = isWorkerThreadAlive(sm, probeSurfaceId2, 5000);
            Log.i(TAG, "Worker thread alive after setDayNightMode: " + workerAlive2);

            if (!workerAlive2 && callCount.get() > 0) {
                deadlock.set(true);
                Log.e(TAG, "*** DEADLOCK DETECTED (setDayNightMode + unregisterFunction) ***");
            }
        }

        // Report BEFORE cleanup
        Log.i(TAG, "=== RISK23 RESULT ===");
        Log.i(TAG, "Deadlock detected: " + deadlock.get());
        Log.i(TAG, "Function call count: " + callCount.get());
        if (deadlock.get()) {
            Log.e(TAG, "One-shot pattern (unregister self) triggers same mutex deadlock");
        }

        // Cleanup: do NOT call unregisterFunction if deadlocked
        destroySMOnMainThread(sm);

        assertTrue("RISK23: Deadlock should be detected (one-shot unregister self)",
                deadlock.get());
    }
}
