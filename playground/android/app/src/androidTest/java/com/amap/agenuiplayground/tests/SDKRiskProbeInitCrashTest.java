package com.amap.agenuiplayground.tests;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.base.AGenUIBaseTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.assertEquals;

@RunWith(AndroidJUnit4.class)
public class SDKRiskProbeInitCrashTest extends AGenUIBaseTest {

    private static final int CREATE_COUNT = 200;

    @Test
    public void testSDKRISK01_surfaceManagerInitLoop() throws Exception {
        List<SurfaceManager> managers = new ArrayList<>(CREATE_COUNT);

        runOnActivity(activity -> {
            for (int i = 0; i < CREATE_COUNT; i++) {
                SurfaceManager manager = new SurfaceManager(activity);
                managers.add(manager);

                manager.beginTextStream();
                manager.receiveTextChunk(createSurfaceJson("sdk-risk-android-" + i));
                manager.endTextStream();

                if (i % 10 == 0) {
                    AGenUI.getInstance().setDayNightMode(i % 20 == 0 ? "light" : "dark");
                }
            }
        });

        waitForMainThread();
        Thread.sleep(2000L);

        runOnActivity(activity -> {
            for (SurfaceManager manager : managers) {
                manager.destroy();
            }
        });

        assertEquals(CREATE_COUNT, managers.size());
    }

    private static String createSurfaceJson(String surfaceId) {
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"" + surfaceId
                + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json\"}}";
    }
}
