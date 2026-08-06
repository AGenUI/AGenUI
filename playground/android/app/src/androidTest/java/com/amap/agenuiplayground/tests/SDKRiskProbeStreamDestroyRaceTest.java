package com.amap.agenuiplayground.tests;

import android.app.Activity;
import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.A2UIPlaygroundActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

/**
 * RISK67: streaming receiveTextChunk racing destroy() — the real-world
 * "response arrives as the user navigates away" lifecycle race.
 *
 * USER PERSPECTIVE (integrator): A2UI content streams in on a network/background
 * thread → the app calls sm.receiveTextChunk(chunk) there. Meanwhile the user
 * closes the chat/card → the app calls sm.destroy() on the main thread. These
 * are BOTH legitimate public-API calls; the integrator has no documented
 * contract forcing them onto one thread.
 *
 * MECHANISM: JNI receiveTextChunk → engine->findSurfaceManager(instanceId)
 * returns a RAW pointer under _surfaceManagersMutex then releases the lock (the
 * lock guards the MAP, not object LIFETIME). Concurrently destroySurfaceManager
 * erases the entry and tears the SurfaceManager down. If the teardown completes
 * between findSurfaceManager() returning the raw ptr and the caller using it,
 * the raw ptr is dangling → use-after-free (SIGSEGV). SurfaceManager.java only
 * catches RuntimeException, so a native UAF crashes the process.
 *
 * Distinct from RISK28/RISK36 (concurrent destroy-vs-destroy / bridge double-free);
 * this is receiveTextChunk-vs-destroy on a single SurfaceManager.
 *
 * All calls are public SDK API. Shared core/ engine registry logic.
 */
@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeStreamDestroyRaceTest {

    private static final String TAG = "RISK67_StreamDestroy";

    @Rule
    public ActivityTestRule<A2UIPlaygroundActivity> activityRule =
            new ActivityTestRule<>(A2UIPlaygroundActivity.class);

    private Activity activity;

    @Before
    public void setUp() throws Exception {
        activity = activityRule.getActivity();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            if (!AGenUI.getInstance().isInitialized()) {
                AGenUI.getInstance().initialize(activity.getApplicationContext());
            }
        });
        Thread.sleep(300);
    }

    @After
    public void tearDown() {}

    private static String createSurface(String sid) {
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"" + sid + "\",\"catalogId\":\"test\"}}";
    }

    private static String updateChunk(String sid, int i) {
        return "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"" + sid + "\",\"components\":["
                + "{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"t" + i + "\"]},"
                + "{\"id\":\"t" + i + "\",\"component\":\"Text\",\"attributes\":{\"text\":\"\\\"chunk " + i + "\\\"\"}}"
                + "]}}";
    }

    /**
     * RISK67-01: one streamer thread per SM racing destroy() on the test thread.
     * Many iterations to widen the find-then-use race window.
     */
    @Test(timeout = 120000)
    public void test_streamChunkRacesDestroy() throws Exception {
        Log.i(TAG, "=== RISK67-01: receiveTextChunk (streamer thread) races destroy() ===");
        for (int iter = 0; iter < 300; iter++) {
            final SurfaceManager sm = new SurfaceManager(activity);
            final String sid = "s_r67_" + iter;
            sm.beginTextStream();
            sm.receiveTextChunk(createSurface(sid));
            sm.endTextStream();

            final Thread streamer = new Thread(() -> {
                for (int i = 0; i < 100; i++) {
                    sm.beginTextStream();
                    sm.receiveTextChunk(updateChunk(sid, i));
                    sm.endTextStream();
                }
            }, "r67-streamer-" + iter);
            streamer.start();

            // Destroy while the streamer is mid-flight (no fixed delay → maximal overlap).
            sm.destroy();
            streamer.join();
            if (iter % 50 == 0) {
                Log.i(TAG, "iter " + iter + " survived");
            }
        }
        Log.i(TAG, "RISK67-01 survived all iterations");
    }

    /**
     * RISK67-02: multiple streamer threads on the SAME SM, then destroy — models
     * concurrent stream sources / re-entrant callbacks colliding with teardown.
     */
    @Test(timeout = 120000)
    public void test_multiStreamerRacesDestroy() throws Exception {
        Log.i(TAG, "=== RISK67-02: N streamer threads race destroy() ===");
        for (int iter = 0; iter < 120; iter++) {
            final SurfaceManager sm = new SurfaceManager(activity);
            final String sid = "s_r67m_" + iter;
            sm.beginTextStream();
            sm.receiveTextChunk(createSurface(sid));
            sm.endTextStream();

            List<Thread> threads = new ArrayList<>();
            for (int t = 0; t < 4; t++) {
                Thread th = new Thread(() -> {
                    for (int i = 0; i < 60; i++) {
                        sm.beginTextStream();
                        sm.receiveTextChunk(updateChunk(sid, i));
                        sm.endTextStream();
                    }
                }, "r67m-" + iter + "-" + t);
                threads.add(th);
                th.start();
            }
            sm.destroy();
            for (Thread th : threads) th.join();
        }
        Log.i(TAG, "RISK67-02 survived all iterations");
    }

    /**
     * RISK67-03: idempotency — use-after-destroy + double-destroy on a single SM.
     * A real integrator error path (retry/cleanup calling destroy twice, or a late
     * chunk after destroy).
     */
    @Test(timeout = 60000)
    public void test_useAfterDestroyAndDoubleDestroy() throws Exception {
        Log.i(TAG, "=== RISK67-03: use-after-destroy + double-destroy ===");
        for (int iter = 0; iter < 200; iter++) {
            SurfaceManager sm = new SurfaceManager(activity);
            String sid = "s_r67d_" + iter;
            sm.beginTextStream();
            sm.receiveTextChunk(createSurface(sid));
            sm.endTextStream();

            sm.destroy();
            // Use after destroy (late network chunk):
            sm.beginTextStream();
            sm.receiveTextChunk(updateChunk(sid, 0));
            sm.endTextStream();
            sm.invalidateFunctionCallValues();
            // Double destroy (retry/cleanup):
            sm.destroy();
        }
        Log.i(TAG, "RISK67-03 survived all iterations");
    }
}
