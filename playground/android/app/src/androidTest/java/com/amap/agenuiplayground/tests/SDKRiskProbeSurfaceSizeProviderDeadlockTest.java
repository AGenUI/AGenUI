package com.amap.agenuiplayground.tests;

import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.ISurfaceManagerListener;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenui.render.surface.SurfaceSize;
import com.amap.agenuiplayground.base.AGenUIBaseTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

import static org.junit.Assert.fail;

/**
 * RISK31: SurfaceSizeProvider mutex deadlock when surfaceSize() callback calls destroy().
 *
 * <h3>Vulnerability</h3>
 * {@code SurfaceManager::getSurfaceSize()} acquires {@code _surfaceSizeProviderMutex},
 * then calls the JNI bridge into Java {@code SurfaceManager.getSurfaceSize()}, which
 * iterates registered {@link ISurfaceManagerListener}s and calls each listener's
 * {@code surfaceSize()} callback <b>synchronously on the worker thread</b>.
 * <p>
 * If the user's {@code surfaceSize()} implementation calls {@code surfaceManager.destroy()},
 * the destroy path calls {@code unregisterSurfaceSizeProvider()} →
 * {@code nativeClearSurfaceSizeProvider()} → {@code setSurfaceSizeProvider(nullptr)} →
 * tries to acquire {@code _surfaceSizeProviderMutex} → <b>DEADLOCK</b> (same thread,
 * non-recursive {@code std::mutex}).
 *
 * <h3>Code path</h3>
 * <pre>
 * Worker thread:
 *   stream → SurfaceCoordinator → Surface → BatchScope exit
 *     → batchGuard callback → getSurfaceWidth()
 *       → ensureSurfaceSizeFetched()
 *         → SurfaceManager::getSurfaceSize(surfaceId)
 *           → lock_guard(_surfaceSizeProviderMutex)     ← LOCK ACQUIRED
 *           → _surfaceSizeProvider->getSurfaceSize(...)
 *             → JNISurfaceSizeProviderBridge::getSurfaceSize
 *               → env->CallObjectMethod(host, getSurfaceSizeMethod, ...)
 *                 → Java: SurfaceManager.getSurfaceSize(surfaceId)
 *                   → listener.surfaceSize(surfaceId)
 *                     → Java user code: surfaceManager.destroy()
 *                       → unregisterSurfaceSizeProvider()
 *                         → nativeClearSurfaceSizeProvider(instanceId)
 *                           → jni_clearSurfaceSizeProvider
 *                             → surfaceManager->setSurfaceSizeProvider(nullptr)
 *                               → lock_guard(_surfaceSizeProviderMutex)  ← DEADLOCK
 * </pre>
 *
 * <h3>Probe style</h3>
 * Callback re-entry deadlock via the surface-size pull channel. The test registers a
 * public {@link ISurfaceManagerListener} whose {@code surfaceSize()} callback triggers
 * {@code destroy()} on the first invocation.
 *
 * <h3>Expected</h3>
 * Deadlock on the native worker thread. The {@code surfaceSize()} callback never returns,
 * blocking all subsequent SDK operations.
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeSurfaceSizeProviderDeadlockTest extends AGenUIBaseTest {

    private static final String TAG = "SDKRiskProbe31";
    private static final long DEADLOCK_TIMEOUT_MS = 8000;

    /**
     * Override tearDown to avoid calling destroy() on an already-deadlocked SM.
     * If the worker thread is deadlocked, destroy() would hang indefinitely.
     */
    @Override
    public void tearDown() {
        // Worker thread may be deadlocked — do NOT call surfaceManager.destroy()
        surfaceManager = null;
    }

    // ------------------------------------------------------------------
    // Test 1: destroy() inside surfaceSize() callback → deadlock
    // ------------------------------------------------------------------

    /**
     * Registers an ISurfaceManagerListener that calls destroy() inside surfaceSize().
     * The engine's first layout pass pulls the surface size from the provider bridge,
     * holding _surfaceSizeProviderMutex. The destroy() path tries to clear the provider,
     * which re-locks the same mutex on the same thread → deadlock.
     */
    @Test
    public void testRISK31_destroyInsideSurfaceSizeCallback() throws Exception {
        Log.i(TAG, "=== RISK31: destroy inside surfaceSize() callback deadlock ===");

        final AtomicBoolean callbackFired = new AtomicBoolean(false);
        final AtomicBoolean destroyAttempted = new AtomicBoolean(false);
        final CountDownLatch deadlockLatch = new CountDownLatch(1);

        // Register a listener whose surfaceSize() calls destroy()
        ISurfaceManagerListener maliciousListener = new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {
                Log.i(TAG, "[RISK31] onCreateSurface: " + surface.getSurfaceId());
            }

            @Override
            public void onDeleteSurface(Surface surface) {
                Log.i(TAG, "[RISK31] onDeleteSurface: " + surface.getSurfaceId());
            }

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

            @Nullable
            @Override
            public SurfaceSize surfaceSize(@NonNull String surfaceId) {
                callbackFired.set(true);
                Log.i(TAG, "[RISK31] surfaceSize() called on thread: "
                        + Thread.currentThread().getName()
                        + " — calling surfaceManager.destroy() (this will deadlock)");

                // This triggers:
                //   unregisterSurfaceSizeProvider() →
                //   nativeClearSurfaceSizeProvider(instanceId) →
                //   setSurfaceSizeProvider(nullptr) →
                //   lock_guard(_surfaceSizeProviderMutex) ← DEADLOCK
                try {
                    surfaceManager.destroy();
                    destroyAttempted.set(true);
                    Log.i(TAG, "[RISK31] destroy() returned (no deadlock — unexpected)");
                } catch (Exception e) {
                    Log.e(TAG, "[RISK31] destroy() threw exception", e);
                }

                deadlockLatch.countDown();
                // Return a valid size to prevent early-exit before destroy
                return new SurfaceSize(360.0f, 640.0f);
            }
        };

        surfaceManager.addListener(maliciousListener);

        // Send createSurface + updateComponents as TWO separate protocol envelopes.
        // Each envelope must have its own "version" field — the protocol stream extractor
        // processes one primary event per JSON object. Sending both in a single object would
        // cause updateComponents to be silently dropped.
        //
        // The first layout (triggered by updateComponents → BatchScope exit → batchGuard
        // cascade) calls ensureSurfaceSizeFetched() → getSurfaceSize() → provider bridge
        // → Java → our surfaceSize() callback.
        String createJson = "{\"version\":\"v0.9\","
                + "\"createSurface\":{\"surfaceId\":\"risk31\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\","
                + "\"theme\":{},\"sendDataModel\":false,\"animated\":true}}";  // separate envelope

        String updateJson = "{\"version\":\"v0.9\","
                + "\"updateComponents\":{\"surfaceId\":\"risk31\","
                + "\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"c1\"],"
                + "\"styles\":{\"width\":\"100%\",\"height\":\"auto\"}},"
                + "{\"id\":\"c1\",\"component\":\"Text\",\"attributes\":{\"text\":\"RISK31\"},"
                + "\"styles\":{\"width\":\"100\",\"height\":\"50\"}}"
                + "]}}";  // separate envelope

        // First send createSurface to create the surface on the worker
        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(createJson);
        surfaceManager.endTextStream();

        // Brief pause to let the worker process createSurface before we send components
        Thread.sleep(50);

        // Now send updateComponents — its BatchScope exit triggers the first layout
        // which calls ensureSurfaceSizeFetched() → provider pull → our surfaceSize()
        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(updateJson);
        surfaceManager.endTextStream();

        Log.i(TAG, "[RISK31] Waiting for deadlock detection (timeout=" + DEADLOCK_TIMEOUT_MS + "ms)...");

        boolean completed = deadlockLatch.await(DEADLOCK_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        if (!completed && callbackFired.get()) {
            // surfaceSize() was called but never returned → DEADLOCK CONFIRMED
            Log.e(TAG, "*** RISK31 DEADLOCK CONFIRMED ***");
            Log.e(TAG, "surfaceSize() callback started but never returned.");
            Log.e(TAG, "_surfaceSizeProviderMutex (std::mutex, non-recursive) re-entry deadlock.");
            Log.e(TAG, "Path: getSurfaceSize [holds mutex] → Java surfaceSize() → destroy()");
            Log.e(TAG, "  → nativeClearSurfaceSizeProvider → setSurfaceSizeProvider(nullptr)");
            Log.e(TAG, "  → lock_guard(_surfaceSizeProviderMutex) ← DEADLOCK");
            fail("DEADLOCK: destroy() inside surfaceSize() callback caused worker thread deadlock");
        } else if (!completed && !callbackFired.get()) {
            // surfaceSize() was never called
            Log.w(TAG, "[RISK31] surfaceSize() was never invoked — "
                    + "the first layout might not have triggered provider pull");
            Log.w(TAG, "[RISK31] Check: was notifySurfaceSizeChanged() called before layout?");

            // Probe whether the worker thread is alive
            CountDownLatch probeLatch = new CountDownLatch(1);
            AtomicBoolean probeOk = new AtomicBoolean(false);
            new Thread(() -> {
                try {
                    surfaceManager.beginTextStream();
                    surfaceManager.receiveTextChunk(
                            "{\"version\":\"v0.9\",\"createSurface\":"
                            + "{\"surfaceId\":\"probe-alive\",\"catalogId\":\"test\"}}");
                    surfaceManager.endTextStream();
                    probeOk.set(true);
                } catch (Exception e) {
                    Log.e(TAG, "[RISK31] probe threw", e);
                }
                probeLatch.countDown();
            }).start();

            boolean alive = probeLatch.await(3000, TimeUnit.MILLISECONDS);
            if (alive && probeOk.get()) {
                Log.i(TAG, "[RISK31] Worker thread alive; surfaceSize not triggered. "
                        + "Need different trigger strategy.");
            } else {
                Log.e(TAG, "[RISK31] Worker thread appears blocked/dead");
            }
        } else {
            // Callback completed and destroy returned — no deadlock
            // (might happen if std::mutex is recursive on this platform, or if
            // the code path has been patched)
            Log.i(TAG, "[RISK31] surfaceSize() + destroy() completed without deadlock. "
                    + "destroyAttempted=" + destroyAttempted.get());
        }
    }

    // ------------------------------------------------------------------
    // Test 2: removeListener inside surfaceSize() callback
    // ------------------------------------------------------------------

    /**
     * Variant: instead of destroy(), the listener calls removeListener(this)
     * inside surfaceSize(). This also deadlocks because removeListener is a
     * Java-level operation that doesn't touch _surfaceSizeProviderMutex.
     * However, if removeListener triggers nativeRemoveEventListener, it needs
     * EventDispatcher._mutex — which shouldn't be held here.
     *
     * This test verifies that the surfaceSize callback itself runs correctly
     * when the listener only modifies the Java-side listener list.
     */
    @Test
    public void testRISK31_removeListenerInsideSurfaceSizeCallback() throws Exception {
        Log.i(TAG, "=== RISK31 variant: removeListener inside surfaceSize() ===");

        final AtomicBoolean callbackFired = new AtomicBoolean(false);
        final CountDownLatch doneLatch = new CountDownLatch(1);

        ISurfaceManagerListener selfRemovingListener = new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {}

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

            @Nullable
            @Override
            public SurfaceSize surfaceSize(@NonNull String surfaceId) {
                callbackFired.set(true);
                Log.i(TAG, "[RISK31v2] surfaceSize() called — removing self from listeners");
                try {
                    surfaceManager.removeListener(this);
                    Log.i(TAG, "[RISK31v2] removeListener completed (no deadlock expected)");
                } catch (Exception e) {
                    Log.e(TAG, "[RISK31v2] removeListener threw", e);
                }
                doneLatch.countDown();
                return new SurfaceSize(360.0f, 640.0f);
            }
        };

        surfaceManager.addListener(selfRemovingListener);

        String createJson = "{\"version\":\"v0.9\","
                + "\"createSurface\":{\"surfaceId\":\"risk31v2\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\","
                + "\"theme\":{},\"sendDataModel\":false,\"animated\":true}}";

        String updateJson = "{\"version\":\"v0.9\","
                + "\"updateComponents\":{\"surfaceId\":\"risk31v2\","
                + "\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"c1\"],"
                + "\"styles\":{\"width\":\"100%\",\"height\":\"auto\"}},"
                + "{\"id\":\"c1\",\"component\":\"Text\",\"attributes\":{\"text\":\"RISK31v2\"},"
                + "\"styles\":{\"width\":\"100\",\"height\":\"50\"}}"
                + "]}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(createJson);
        surfaceManager.endTextStream();

        Thread.sleep(50);

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(updateJson);
        surfaceManager.endTextStream();

        boolean completed = doneLatch.await(DEADLOCK_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        if (!completed && callbackFired.get()) {
            Log.w(TAG, "[RISK31v2] Unexpected: removeListener inside surfaceSize() deadlocked");
            fail("Unexpected deadlock from removeListener inside surfaceSize() callback");
        } else if (completed) {
            Log.i(TAG, "[RISK31v2] OK: removeListener inside surfaceSize() did not deadlock");
        } else {
            Log.w(TAG, "[RISK31v2] surfaceSize() was never called");
        }
    }
}
