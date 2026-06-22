package com.amap.agenuiplayground.tests;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.amap.agenui.render.surface.Surface;
import com.amap.agenuiplayground.base.AGenUIBaseTest;
import com.amap.agenuiplayground.base.TestFixtureLoader;

import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

/**
 * SURFACE 模块集成测试 — Surface 生命周期
 *
 * <p>覆盖用例：
 * <ul>
 *   <li>SURFACE-01：创建 Surface — {@code onCreateSurface} 回调触发</li>
 *   <li>SURFACE-02：销毁 Surface — {@code onDeleteSurface} 回调触发，{@code getSurface()} 返回 null</li>
 *   <li>SURFACE-03：渲染组件 — 等待主线程 flush 后 {@code getComponentCount()} 等于预期</li>
 * </ul>
 */
@RunWith(AndroidJUnit4.class)
public class SurfaceLifecycleTest extends AGenUIBaseTest {

    private TestFixtureLoader loader;

    @Override
    public void setUp() {
        super.setUp();
        loader = new TestFixtureLoader(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
    }

    /**
     * SURFACE-01：创建 Surface
     * <p>
     * 发送含 createSurface 消息的 JSON，验证 {@code ISurfaceListener.onCreateSurface()} 被触发，
     * 返回的 Surface 实例非 null 且 surfaceId 匹配。
     */
    @Test
    public void testSURFACE01_createSurfaceCallbackTriggered() throws Exception {
        String surfaceId = loader.getSurfaceId("init/create_surface_simple.json");
        String json = loader.loadMessagesAsString("init/create_surface_simple.json");

        // sendAndWaitForSurface 内部使用 CountDownLatch 等待 onCreateSurface 回调
        Surface surface = sendAndWaitForSurface(json, surfaceId);

        assertNotNull("SURFACE-01: onCreateSurface 回调应触发并返回非 null Surface", surface);
        assertEquals("SURFACE-01: surfaceId 应匹配", surfaceId, surface.getSurfaceId());
    }

    /**
     * SURFACE-02：销毁 Surface
     * <p>
     * 先创建 Surface，再发送 deleteSurface 消息，验证：
     * <ul>
     *   <li>{@code ISurfaceListener.onDeleteSurface()} 回调触发</li>
     *   <li>{@code getSurface(surfaceId)} 返回 null</li>
     * </ul>
     */
    @Test
    public void testSURFACE02_deleteSurfaceCallbackAndGetSurfaceNull() throws Exception {
        String surfaceId = loader.getSurfaceId("init/create_surface_simple.json");
        String createJson = loader.loadMessagesAsString("init/create_surface_simple.json");

        // 1. 先创建 Surface
        Surface surface = sendAndWaitForSurface(createJson, surfaceId);
        assertNotNull("前置条件：Surface 应已创建", surface);

        // 2. 从 fixture 加载 deleteSurface JSON
        String deleteJson = loader.loadMessagesAsString("init/delete_surface_simple.json");
        // 3. 发送并等待 onDeleteSurface 回调
        sendAndWaitForDeleteSurface(deleteJson, surfaceId);

        // 4. 等待主线程处理完成
        waitForMainThread();

        // 5. getSurface() 应返回 null
        assertNull("SURFACE-02: 销毁后 getSurface() 应返回 null",
                surfaceManager.getSurface(surfaceId));
    }

    /**
     * SURFACE-03：渲染组件
     * <p>
     * 发送含 createSurface + updateComponents 的完整 JSON，
     * 等待主线程 flush 后验证 {@code getComponentCount()} 等于预期值。
     */
    @Test
    public void testSURFACE03_componentCountAfterRender() throws Exception {
        String fixturePath = "components/01_text_only.json";
        String surfaceId = loader.getSurfaceId(fixturePath);
        String json = loader.loadMessagesAsString(fixturePath);
        int expectedCount = loader.getExpect(fixturePath).getInt("componentCount");

        // sendAndWaitForRender = sendAndWaitForSurface + waitForMainThread
        Surface surface = sendAndWaitForRender(json, surfaceId);

        assertNotNull("前置条件：Surface 不应为 null", surface);
        assertEquals("SURFACE-03: 组件数量应为 " + expectedCount,
                expectedCount, surface.getComponentCount());
    }

    /**
     * SURFACE-03（扩展）：验证 Button 渲染后组件 ID 可通过 {@code getComponent()} 查到
     */
    @Test
    public void testSURFACE03_buttonComponentAccessibleById() throws Exception {
        String fixturePath = "components/02_button_with_action.json";
        String surfaceId = loader.getSurfaceId(fixturePath);
        String json = loader.loadMessagesAsString(fixturePath);

        Surface surface = sendAndWaitForRender(json, surfaceId);

        assertNotNull("Surface 应创建成功", surface);
        assertNotNull("SURFACE-03: btn-submit 组件应可通过 getComponent() 获取",
                surface.getComponent("btn-submit"));
        assertEquals("SURFACE-03: btn-submit 类型应为 Button",
                "Button",
                surface.getComponent("btn-submit").getComponentType());
    }
}
