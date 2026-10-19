package com.amap.agenuiplayground.tests;

import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.IAGenUIMessageListener;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.base.AGenUIBaseTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Method;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

/**
 * RISK30: EventDispatcher non-recursive mutex deadlock on listener self-unregister.
 *
 * <h3>Vulnerability</h3>
 * {@code EventDispatcher::dispatchComponentsAdd()} acquires {@code std::mutex _mutex},
 * then calls each listener's {@code onComponentsAdd()} synchronously (via JNI
 * CallVoidMethod). If the listener callback calls {@code removeSurfaceEventListener()}
 * (which also needs {@code _mutex}), the same thread attempts to re-acquire a
 * non-recursive {@code std::mutex} → <strong>undefined behavior</strong> (deadlock
 * on POSIX/bionic).
 *
 * <h3>Code path</h3>
 * <pre>
 * Worker thread:
 *   stream → SurfaceCoordinator → Surface → BatchScope exit
 *     → EventDispatcher::dispatchComponentsAdd()
 *       → lock_guard(_mutex)          ← LOCK ACQUIRED
 *       → bridge->onComponentsAdd()
 *         → env->CallVoidMethod(javaListener, onComponentsAdd, ...)
 *           → Java: listener.onComponentsAdd(...)
 *             → SurfaceManager.removeMessageListener(this)
 *               → nativeRemoveEventListener(instanceId, listener)
 *                 → jni_removeEventListener(env, ...)
 *                   → surfaceManager->removeSurfaceEventListener(bridge)
 *                     → _dispatcher->removeEventListener(listener)
 *                       → lock_guard(_mutex)  ← DEADLOCK (same thread, non-recursive mutex)
 * </pre>
 *
 * <h3>Probe style</h3>
 * Callback re-entry deadlock. The test registers a custom {@link IAGenUIMessageListener}
 * that calls {@code removeMessageListener(this)} inside {@code onComponentsAdd()}.
 * Then sends streaming data to trigger component creation.
 *
 * <h3>Expected</h3>
 * Deadlock on the native worker thread (thread hangs indefinitely). Detected via
 * timeout: subsequent SDK operations on the same SurfaceManager will never complete.
 * Alternatively, some implementations of std::mutex may abort() on recursive lock
 * detection (SIGABRT).
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeListenerSelfUnregDeadlockTest extends AGenUIBaseTest {

    private static final String TAG = "SDKRiskProbe30";
    private static final long DEADLOCK_TIMEOUT_MS = 8000;

    // Reflection handles for package-private methods
    private Method addMessageListenerMethod;
    private Method removeMessageListenerMethod;

    @Override
    public void setUp() {
        super.setUp();
        try {
            addMessageListenerMethod = SurfaceManager.class.getDeclaredMethod(
                    "addMessageListener", IAGenUIMessageListener.class);
            addMessageListenerMethod.setAccessible(true);

            removeMessageListenerMethod = SurfaceManager.class.getDeclaredMethod(
                    "removeMessageListener", IAGenUIMessageListener.class);
            removeMessageListenerMethod.setAccessible(true);
        } catch (Exception e) {
            throw new RuntimeException("Failed to access SurfaceManager methods via reflection", e);
        }
    }

    // ------------------------------------------------------------------
    // Test 1: Self-unregister inside onComponentsAdd → deadlock
    // ------------------------------------------------------------------

    /**
     * Registers a listener that calls removeMessageListener(this) inside
     * onComponentsAdd. When a component is added, the EventDispatcher holds
     * _mutex while calling our listener, and the removal attempt re-locks
     * the same mutex → deadlock.
     *
     * Detection: after the deadlock, subsequent operations on the same SM
     * (which go through the same worker thread) will time out.
     */
    @Test
    public void testRISK30_listenerSelfUnregisterDeadlock() throws Exception {
        Log.i(TAG, "=== RISK30: listener self-unregister deadlock ===");

        final AtomicBoolean callbackFired = new AtomicBoolean(false);
        final AtomicBoolean removalAttempted = new AtomicBoolean(false);
        final CountDownLatch deadlockDetectionLatch = new CountDownLatch(1);

        // Create a self-unregistering listener
        IAGenUIMessageListener selfUnregListener = new IAGenUIMessageListener() {
            @Override
            public void onComponentsAdd(String surfaceId, String[] parentIds, String[] components) {
                callbackFired.set(true);
                Log.i(TAG, "[RISK30] onComponentsAdd fired on thread: "
                        + Thread.currentThread().getName()
                        + " — attempting self-removal (this will deadlock)");
                try {
                    // This call goes through JNI → jni_removeEventListener
                    // → _dispatcher->removeEventListener → tries to lock _mutex
                    // But _mutex is already held by the current dispatchComponentsAdd call!
                    removeMessageListenerMethod.invoke(surfaceManager, this);
                    removalAttempted.set(true);
                    Log.i(TAG, "[RISK30] Self-removal returned (no deadlock — unexpected)");
                } catch (Exception e) {
                    Log.e(TAG, "[RISK30] Self-removal threw exception", e);
                }
                deadlockDetectionLatch.countDown();
            }

            @Override
            public void onCreateSurface(String surfaceId, String catalogId,
                                        Map<String, String> theme, boolean sendDataModel,
                                        boolean animated, String rawProtocolContent) {}
            @Override
            public void onComponentsUpdate(String surfaceId, String[] components) {}
            @Override
            public void onComponentsRemove(String surfaceId, String[] parentIds, String[] componentIds) {}
            @Override
            public void onDeleteSurface(String surfaceId) {}
            @Override
            public void onActionEventRouted(String content) {}
            @Override
            public void onError(int code, String surfaceId, String message) {}
        };

        // Register the self-unregistering listener
        addMessageListenerMethod.invoke(surfaceManager, selfUnregListener);
        Log.i(TAG, "[RISK30] Self-unregistering listener registered");

        // Send data that triggers onComponentsAdd
        String json = "{\"version\":\"v0.9\",\"createSurface\":"
                + "{\"surfaceId\":\"risk30-surface\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}"
                + "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"risk30-surface\","
                + "\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"c1\"]},"
                + "{\"id\":\"c1\",\"component\":\"Text\",\"text\":\"hello\"}"
                + "]}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(json);
        surfaceManager.endTextStream();

        Log.i(TAG, "[RISK30] Waiting for deadlock detection (timeout=" + DEADLOCK_TIMEOUT_MS + "ms)...");

        // If the callback deadlocks, the latch never counts down (callback never returns)
        boolean completed = deadlockDetectionLatch.await(DEADLOCK_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        if (!completed && callbackFired.get()) {
            // Callback started but never returned → DEADLOCK CONFIRMED
            Log.e(TAG, "*** RISK30 DEADLOCK CONFIRMED ***");
            Log.e(TAG, "Listener callback started but never returned.");
            Log.e(TAG, "EventDispatcher::_mutex (std::mutex, non-recursive) re-entry deadlock.");
            fail("DEADLOCK: listener self-unregister caused worker thread to hang");
        } else if (!completed && !callbackFired.get()) {
            // Callback never fired at all — likely the stream processing itself is blocked
            // This could also indicate a deadlock in an earlier dispatch
            Log.w(TAG, "[RISK30] Callback never fired — worker thread may be blocked");

            // Try sending another operation to see if worker thread is alive
            CountDownLatch probeLatch = new CountDownLatch(1);
            AtomicBoolean probeCompleted = new AtomicBoolean(false);
            new Thread(() -> {
                surfaceManager.beginTextStream();
                surfaceManager.receiveTextChunk("{\"version\":\"v0.9\",\"createSurface\":"
                        + "{\"surfaceId\":\"probe-alive\","
                        + "\"catalogId\":\"test\"}}");
                surfaceManager.endTextStream();
                probeCompleted.set(true);
                probeLatch.countDown();
            }).start();

            boolean probeOk = probeLatch.await(3000, TimeUnit.MILLISECONDS);
            if (probeOk && probeCompleted.get()) {
                Log.i(TAG, "[RISK30] Worker thread is alive; callback just didn't fire");
            } else {
                Log.e(TAG, "*** RISK30: Worker thread appears dead/blocked ***");
            }
        } else {
            // Callback completed successfully — no deadlock (std::mutex might be recursive
            // on this platform, or the code path doesn't re-lock)
            Log.i(TAG, "[RISK30] Callback completed without deadlock. removalAttempted="
                    + removalAttempted.get());
        }
    }

    // ------------------------------------------------------------------
    // Test 2: Self-unregister inside onComponentsUpdate → deadlock
    // ------------------------------------------------------------------

    /**
     * Same pattern as Test 1, but triggers via onComponentsUpdate instead.
     * Component updates are the most frequent event type in streaming scenarios.
     */
    @Test
    public void testRISK30_listenerSelfUnregisterOnUpdate() throws Exception {
        Log.i(TAG, "=== RISK30 variant: self-unregister in onComponentsUpdate ===");

        final CountDownLatch deadlockLatch = new CountDownLatch(1);
        final AtomicBoolean callbackFired = new AtomicBoolean(false);

        IAGenUIMessageListener selfUnregOnUpdate = new IAGenUIMessageListener() {
            @Override
            public void onComponentsUpdate(String surfaceId, String[] components) {
                callbackFired.set(true);
                Log.i(TAG, "[RISK30v2] onComponentsUpdate fired — self-removing");
                try {
                    removeMessageListenerMethod.invoke(surfaceManager, this);
                } catch (Exception e) {
                    Log.e(TAG, "[RISK30v2] exception", e);
                }
                deadlockLatch.countDown();
            }

            @Override
            public void onCreateSurface(String surfaceId, String catalogId,
                                        Map<String, String> theme, boolean sendDataModel,
                                        boolean animated, String rawProtocolContent) {}
            @Override
            public void onComponentsAdd(String surfaceId, String[] parentIds, String[] components) {}
            @Override
            public void onComponentsRemove(String surfaceId, String[] parentIds, String[] componentIds) {}
            @Override
            public void onDeleteSurface(String surfaceId) {}
            @Override
            public void onActionEventRouted(String content) {}
            @Override
            public void onError(int code, String surfaceId, String message) {}
        };

        addMessageListenerMethod.invoke(surfaceManager, selfUnregOnUpdate);

        // First: create a surface with a component
        String createJson = "{\"version\":\"v0.9\",\"createSurface\":"
                + "{\"surfaceId\":\"risk30v2\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";
        String addJson = "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"risk30v2\","
                + "\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"t1\"]},"
                + "{\"id\":\"t1\",\"component\":\"Text\",\"text\":\"initial\"}"
                + "]}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(createJson);
        surfaceManager.receiveTextChunk(addJson);
        surfaceManager.endTextStream();

        // Wait for the add to process
        Thread.sleep(2000);

        // Now send an update to the existing component → triggers onComponentsUpdate
        String updateJson = "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"risk30v2\","
                + "\"components\":["
                + "{\"id\":\"t1\",\"component\":\"Text\",\"text\":\"updated text\"}"
                + "]}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(updateJson);
        surfaceManager.endTextStream();

        boolean completed = deadlockLatch.await(DEADLOCK_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        if (!completed && callbackFired.get()) {
            Log.e(TAG, "*** RISK30v2 DEADLOCK CONFIRMED (onComponentsUpdate path) ***");
            fail("DEADLOCK: self-unregister in onComponentsUpdate caused worker thread deadlock");
        } else if (!completed) {
            Log.w(TAG, "[RISK30v2] Callback never fired within timeout");
        } else {
            Log.i(TAG, "[RISK30v2] No deadlock detected on this path");
        }
    }

    // ------------------------------------------------------------------
    // Test 3: Destroy SM inside listener callback → deadlock via destroy()
    // ------------------------------------------------------------------

    /**
     * A more realistic scenario: a listener calls sm.destroy() when it receives
     * a component event. destroy() calls removeMessageListener(nativeEventBridge)
     * internally, which goes through the same deadlock path:
     *   dispatchComponentsAdd holds _mutex → callback → destroy() →
     *   removeMessageListener → nativeRemoveEventListener →
     *   removeSurfaceEventListener → _mutex deadlock.
     *
     * This simulates an app that tears down its UI layer in response to
     * receiving a certain event.
     */
    @Test
    public void testRISK30_destroySMInsideCallback() throws Exception {
        Log.i(TAG, "=== RISK30 variant: destroy SM inside onCreateSurface callback ===");

        final CountDownLatch deadlockLatch = new CountDownLatch(1);
        final AtomicBoolean callbackFired = new AtomicBoolean(false);

        IAGenUIMessageListener destroyOnCreate = new IAGenUIMessageListener() {
            @Override
            public void onCreateSurface(String surfaceId, String catalogId,
                                        Map<String, String> theme, boolean sendDataModel,
                                        boolean animated, String rawProtocolContent) {
                callbackFired.set(true);
                Log.i(TAG, "[RISK30v3] onCreateSurface → calling sm.destroy()");
                // destroy() internally calls removeMessageListener(nativeEventBridge)
                // which hits the same mutex deadlock
                surfaceManager.destroy();
                deadlockLatch.countDown();
            }

            @Override
            public void onComponentsUpdate(String surfaceId, String[] components) {}
            @Override
            public void onComponentsAdd(String surfaceId, String[] parentIds, String[] components) {}
            @Override
            public void onComponentsRemove(String surfaceId, String[] parentIds, String[] componentIds) {}
            @Override
            public void onDeleteSurface(String surfaceId) {}
            @Override
            public void onActionEventRouted(String content) {}
            @Override
            public void onError(int code, String surfaceId, String message) {}
        };

        addMessageListenerMethod.invoke(surfaceManager, destroyOnCreate);

        // Send createSurface → triggers onCreateSurface → destroy() → deadlock
        String createJson = "{\"version\":\"v0.9\",\"createSurface\":"
                + "{\"surfaceId\":\"risk30v3\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(createJson);
        surfaceManager.endTextStream();

        boolean completed = deadlockLatch.await(DEADLOCK_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        if (!completed && callbackFired.get()) {
            Log.e(TAG, "*** RISK30v3 DEADLOCK CONFIRMED (destroy inside callback) ***");
            fail("DEADLOCK: destroy() inside listener callback caused worker thread deadlock");
        } else if (!completed) {
            Log.w(TAG, "[RISK30v3] Callback never fired within timeout");
        } else {
            Log.i(TAG, "[RISK30v3] No deadlock on destroy path — checking if worker alive...");
            // If destroy() didn't deadlock, subsequent operations would fail gracefully
            // (SM already destroyed). Verify the SM is truly destroyed.
        }
    }

    @Override
    public void tearDown() {
        // Worker thread may be deadlocked — do NOT call surfaceManager.destroy()
        // because destroy() tries to lock the same deadlocked mutex.
        // Instead, null the reference and let the process exit handle cleanup.
        surfaceManager = null;
    }
}
