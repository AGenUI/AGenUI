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
 * RISK33: Worker thread self-join crash via AGenUI.destroy() called from listener callback.
 *
 * <h3>Vulnerability</h3>
 * All ISurfaceManagerListener callbacks execute on the native worker thread (Thread-2).
 * If user code calls {@code AGenUI.getInstance().destroy()} inside any callback, the
 * destroy path calls:
 * <pre>
 *   destroyAGenUIEngine() → engine->stop() → ThreadManager::destroyThread(1)
 *     → thread->stop() → _workerThread.join()
 * </pre>
 * Since the current thread IS the worker thread, {@code pthread_join(self)} returns
 * EDEADLK, libcxx throws {@code std::system_error}, which is uncaught → std::terminate()
 * → SIGABRT.
 *
 * <h3>Difference from RISK30/31</h3>
 * RISK30/31: non-recursive mutex re-entry deadlock (thread blocks on same mutex).
 * RISK33: thread self-join UB (pthread_join returns EDEADLK → uncaught exception → crash).
 * Different mechanism, different code path, different crash signature (SIGABRT vs hang).
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeEngineSelfJoinCrashTest extends AGenUIBaseTest {

    private static final String TAG = "SDKRiskProbe33";

    /**
     * Override tearDown: if AGenUI.destroy() was called and the engine crashed,
     * we can't safely do normal cleanup.
     */
    @Override
    public void tearDown() {
        surfaceManager = null;
    }

    /**
     * Test 1: Call AGenUI.destroy() inside onCreateSurface callback.
     *
     * The onCreateSurface callback runs on the native worker thread.
     * Calling AGenUI.destroy() from there triggers:
     *   engine->stop() → destroyThread(1) → thread->stop() → join(self) → CRASH
     */
    @Test
    public void testSDKRISK33_engineDestroyInCreateSurfaceCallback() throws Exception {
        Log.i(TAG, "=== RISK33: AGenUI.destroy() inside onCreateSurface callback ===");

        final CountDownLatch callbackFired = new CountDownLatch(1);

        ISurfaceManagerListener listener = new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {
                Log.w(TAG, "[RISK33] onCreateSurface fired on thread: "
                        + Thread.currentThread().getName()
                        + " — calling AGenUI.getInstance().destroy()");
                callbackFired.countDown();

                // This triggers self-join: engine->stop() → destroyThread → join(self)
                // Expected: SIGABRT from std::terminate (uncaught std::system_error from join)
                // OR: deadlock (if implementation blocks instead of throwing)
                try {
                    AGenUI.getInstance().destroy();
                    Log.w(TAG, "[RISK33] AGenUI.destroy() returned (unexpected!)");
                } catch (Throwable t) {
                    Log.e(TAG, "[RISK33] AGenUI.destroy() threw: " + t.getMessage());
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

        surfaceManager.addListener(listener);

        // Feed a createSurface event to trigger the onCreateSurface callback
        String createChunk = "{\"version\":\"v0.9\",\"createSurface\":{"
                + "\"surfaceId\":\"risk33-cs\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(createChunk);
        surfaceManager.endTextStream();

        // Wait for the callback to fire or process to crash
        boolean fired = callbackFired.await(5, TimeUnit.SECONDS);

        if (!fired) {
            Log.w(TAG, "[RISK33] onCreateSurface callback did not fire within 5 seconds");
        }

        // Give time for the crash to propagate
        Thread.sleep(3000);

        // If we're still alive, test "passes" (no crash).
        // The instrumentation crash would be reported as "Test instrumentation process crashed"
        Log.w(TAG, "[RISK33] Test completed without crash");
    }

    /**
     * Test 2: Call AGenUI.destroy() inside surfaceSize() callback.
     *
     * surfaceSize() is explicitly @WorkerThread. Unlike RISK31 which deadlocks on
     * _surfaceSizeProviderMutex, here we call AGenUI.destroy() (engine level) which
     * goes through a different code path: engine->stop() → destroyThread → join(self).
     *
     * Note: RISK31 deadlocks on the mutex BEFORE reaching engine destroy.
     * This test bypasses the mutex issue by calling engine-level destroy directly.
     */
    @Test
    public void testSDKRISK33_engineDestroyInSurfaceSizeCallback() throws Exception {
        Log.i(TAG, "=== RISK33: AGenUI.destroy() inside surfaceSize() callback ===");

        final AtomicBoolean callbackFired = new AtomicBoolean(false);
        final CountDownLatch deadlockLatch = new CountDownLatch(1);

        ISurfaceManagerListener listener = new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {
                Log.i(TAG, "[RISK33-T2] onCreateSurface: " + surface.getSurfaceId());
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

            @Nullable
            @Override
            public SurfaceSize surfaceSize(@NonNull String surfaceId) {
                callbackFired.set(true);
                Log.w(TAG, "[RISK33-T2] surfaceSize() called on thread: "
                        + Thread.currentThread().getName()
                        + " — calling AGenUI.getInstance().destroy()");

                // This should trigger self-join (different path from RISK31's mutex deadlock)
                // RISK31: sm.destroy() → setSurfaceSizeProvider → mutex re-entry
                // RISK33: AGenUI.destroy() → engine.stop() → thread.join(self) → crash
                try {
                    AGenUI.getInstance().destroy();
                    Log.w(TAG, "[RISK33-T2] AGenUI.destroy() returned (unexpected)");
                } catch (Throwable t) {
                    Log.e(TAG, "[RISK33-T2] AGenUI.destroy() threw: " + t.getMessage());
                }

                deadlockLatch.countDown();
                return new SurfaceSize(360.0f, 640.0f);
            }
        };

        surfaceManager.addListener(listener);

        // createSurface + updateComponents to trigger first layout → surfaceSize pull
        String createJson = "{\"version\":\"v0.9\","
                + "\"createSurface\":{\"surfaceId\":\"risk33-ss\","
                + "\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}";

        String updateJson = "{\"version\":\"v0.9\","
                + "\"updateComponents\":{\"surfaceId\":\"risk33-ss\","
                + "\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"c1\"],"
                + "\"styles\":{\"width\":\"100%\",\"height\":\"auto\"}},"
                + "{\"id\":\"c1\",\"component\":\"Text\",\"attributes\":{\"text\":\"RISK33\"},"
                + "\"styles\":{\"width\":\"100\",\"height\":\"50\"}}"
                + "]}}";

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(createJson);
        surfaceManager.endTextStream();

        Thread.sleep(50);

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(updateJson);
        surfaceManager.endTextStream();

        boolean completed = deadlockLatch.await(8, TimeUnit.SECONDS);

        if (!completed && callbackFired.get()) {
            // surfaceSize was called but latch never counted down → crash or deadlock
            Log.e(TAG, "[RISK33-T2] surfaceSize() called but never returned — "
                    + "either crashed or deadlocked");
            fail("RISK33: AGenUI.destroy() in surfaceSize() caused crash or deadlock");
        }

        Thread.sleep(2000);
        Log.w(TAG, "[RISK33-T2] Test completed");
    }
}
