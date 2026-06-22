package com.amap.agenuiplayground.tests;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.amap.agenui.render.surface.ISurfaceManagerListener;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenuiplayground.base.AGenUIBaseTest;
import com.amap.agenuiplayground.base.TestFixtureLoader;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

/**
 * MULTI 模块集成测试 — 同一 SM 内多 surfaceId 隔离性
 *
 * <p>覆盖用例：
 * <ul>
 *   <li>MULTI-01：同一 SM 依次创建 3 个 Surface，onCreateSurface 触发 3 次，各 surfaceId 独立</li>
 *   <li>MULTI-02：surfaceId-A 和 surfaceId-B 组件树内容互不影响</li>
 *   <li>MULTI-03：销毁其中一个 surfaceId，另一个组件树完整</li>
 * </ul>
 */
@RunWith(AndroidJUnit4.class)
public class MultiSurfaceTest extends AGenUIBaseTest {

    private TestFixtureLoader loader;

    @Override
    public void setUp() {
        super.setUp();
        loader = new TestFixtureLoader(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
    }

    /**
     * MULTI-01：同一 SM 依次创建 3 个 Surface，onCreateSurface 触发 3 次
     */
    @Test
    public void testMULTI01_createThreeSurfacesCallbackFiredThreeTimes() throws Exception {
        // 使用 init 用例生成三个不同 surfaceId
        String[] surfaceIds = {
                "test-surf-multi-01-a",
                "test-surf-multi-01-b",
                "test-surf-multi-01-c"
        };

        AtomicInteger callbackCount = new AtomicInteger(0);
        CountDownLatch latch = new CountDownLatch(3);

        ISurfaceManagerListener listener = new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {
                for (String sid : surfaceIds) {
                    if (sid.equals(surface.getSurfaceId())) {
                        callbackCount.incrementAndGet();
                        latch.countDown();
                        break;
                    }
                }
            }
            @Override
            public void onDeleteSurface(Surface surface) {}
        };
        surfaceManager.addListener(listener);

        // 依次发送 3 个 createSurface 消息
        for (String sid : surfaceIds) {
            String json = "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\""
                    + sid + "\",\"catalogId\":\"https://a2ui.org/specification/v0_9/standard_catalog.json\",\"animated\":false}}";
            surfaceManager.beginTextStream();
            surfaceManager.receiveTextChunk(json);
            surfaceManager.endTextStream();
        }

        boolean ok = latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        surfaceManager.removeListener(listener);

        assertTrue("MULTI-01: 3 个 Surface 创建超时", ok);
        assertEquals("MULTI-01: onCreateSurface 应触发 3 次", 3, callbackCount.get());

        // 每个 surfaceId 的 getSurface() 均非 null
        for (String sid : surfaceIds) {
            assertNotNull("MULTI-01: getSurface(" + sid + ") 应非 null",
                    surfaceManager.getSurface(sid));
        }
    }

    /**
     * MULTI-02：surfaceId-A（Button）与 surfaceId-B（Text）组件树相互独立
     */
    @Test
    public void testMULTI02_twoSurfacesComponentTreesAreIsolated() throws Exception {
        String fixPathA = "multi_surface/surface_a.json";
        String fixPathB = "multi_surface/surface_b.json";

        String surfaceIdA = loader.getSurfaceId(fixPathA);
        String surfaceIdB = loader.getSurfaceId(fixPathB);
        String jsonA = loader.loadMessagesAsString(fixPathA);
        String jsonB = loader.loadMessagesAsString(fixPathB);

        int expectedCountA = loader.getExpect(fixPathA).getInt("componentCount");
        int expectedCountB = loader.getExpect(fixPathB).getInt("componentCount");

        // 依次创建两个 Surface 并渲染
        Surface surfaceA = sendAndWaitForRender(jsonA, surfaceIdA);
        Surface surfaceB = sendAndWaitForRender(jsonB, surfaceIdB);

        assertNotNull("MULTI-02: Surface A 应创建成功", surfaceA);
        assertNotNull("MULTI-02: Surface B 应创建成功", surfaceB);

        // 验证组件数量
        assertEquals("MULTI-02: Surface A 组件数应为 " + expectedCountA,
                expectedCountA, surfaceA.getComponentCount());
        assertEquals("MULTI-02: Surface B 组件数应为 " + expectedCountB,
                expectedCountB, surfaceB.getComponentCount());

        // 验证隔离性：A 中不应包含 B 的组件，反之亦然
        assertNull("MULTI-02: Surface A 不应包含 Surface B 的 text-b1 组件",
                surfaceA.getComponent("text-b1"));
        assertNull("MULTI-02: Surface B 不应包含 Surface A 的 btn-a 组件",
                surfaceB.getComponent("btn-a"));

        // 验证各自的特有组件存在
        assertNotNull("MULTI-02: Surface A 应包含 btn-a 组件",
                surfaceA.getComponent("btn-a"));
        assertNotNull("MULTI-02: Surface B 应包含 text-b1 组件",
                surfaceB.getComponent("text-b1"));
    }

    /**
     * MULTI-03：销毁 surfaceId-A 后，surfaceId-B 组件树完整
     */
    @Test
    public void testMULTI03_deleteSurfaceADoesNotAffectSurfaceB() throws Exception {
        String fixPathA = "multi_surface/surface_a.json";
        String fixPathB = "multi_surface/surface_b.json";

        String surfaceIdA = loader.getSurfaceId(fixPathA);
        String surfaceIdB = loader.getSurfaceId(fixPathB);

        // 1. 创建两个 Surface
        Surface surfaceA = sendAndWaitForRender(loader.loadMessagesAsString(fixPathA), surfaceIdA);
        Surface surfaceB = sendAndWaitForRender(loader.loadMessagesAsString(fixPathB), surfaceIdB);
        assertNotNull("前置条件：Surface A 应已创建", surfaceA);
        assertNotNull("前置条件：Surface B 应已创建", surfaceB);

        int expectedCountB = loader.getExpect(fixPathB).getInt("componentCount");

        // 2. 从 fixture 加载 deleteSurface JSON
        String deleteJsonA = loader.loadMessagesAsString("multi_surface/delete_surface_a.json");        sendAndWaitForDeleteSurface(deleteJsonA, surfaceIdA);
        waitForMainThread();

        // 3. Surface A 应不存在
        assertNull("MULTI-03: Surface A 销毁后 getSurface() 应返回 null",
                surfaceManager.getSurface(surfaceIdA));

        // 4. Surface B 组件树应完整
        Surface surfaceBAfter = surfaceManager.getSurface(surfaceIdB);
        assertNotNull("MULTI-03: Surface B 销毁 A 后仍应存在", surfaceBAfter);
        assertEquals("MULTI-03: Surface B 组件数仍应为 " + expectedCountB,
                expectedCountB, surfaceBAfter.getComponentCount());
        assertNotNull("MULTI-03: Surface B 中 text-b1 组件仍应存在",
                surfaceBAfter.getComponent("text-b1"));
    }
}
