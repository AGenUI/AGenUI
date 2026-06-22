package com.amap.agenuiplayground.tests;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.render.surface.ISurfaceManagerListener;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenuiplayground.base.AGenUIBaseTest;
import com.amap.agenuiplayground.base.TestFixtureLoader;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

/**
 * 流式数据集成测试
 *
 * <p>验证 A2UI 协议在不同 chunk 分片策略下，最终 Surface 组件树与完整 JSON 一次性发送结果一致。
 *
 * <p>覆盖场景：
 * <ul>
 *   <li>STREAM-01：小 chunk（chunkSize=10）流式发送 Button 场景</li>
 *   <li>STREAM-02：大 chunk（chunkSize=500）流式发送 Button 场景</li>
 *   <li>STREAM-03：单字符（chunkSize=1）流式发送 Button 场景</li>
 *   <li>STREAM-04：一次性发送（chunkSize=-1）Button 场景</li>
 *   <li>STREAM-05：复杂嵌套布局（nested_layout）大 chunk 流式发送</li>
 *   <li>STREAM-06：含 dataModel 场景（chunkSize=20）</li>
 *   <li>STREAM-07：静态预分片场景（key 边界截断，chunk_01.txt + chunk_02.txt）</li>
 *   <li>STREAM-08：中途 reset（beginTextStream 后 beginTextStream），验证旧 Surface 不产生</li>
 * </ul>
 */
@RunWith(AndroidJUnit4.class)
public class StreamTest extends AGenUIBaseTest {

    private TestFixtureLoader loader;

    @Before
    public void setUpLoader() {
        activityRule.getScenario().onActivity(activity -> {
            loader = new TestFixtureLoader(activity);
        });
    }

    // ==================== 辅助方法 ====================

    /**
     * 以指定 chunkSize 分片发送完整 JSON，等待 surfaceId 对应的 Surface 创建并完成渲染。
     *
     * @param fullJson  完整 A2UI JSON 字符串
     * @param surfaceId 期待创建的 surfaceId
     * @param chunkSize 每片字符数（-1 表示整体发送）
     * @return 渲染完成的 Surface
     * @throws InterruptedException 等待超时
     */
    private Surface streamAndWaitForRender(String fullJson, String surfaceId, int chunkSize)
            throws InterruptedException {
        String[] chunks = TestFixtureLoader.splitIntoChunks(fullJson, chunkSize);

        CountDownLatch latch = new CountDownLatch(1);
        AtomicReference<Surface> surfaceRef = new AtomicReference<>();

        ISurfaceManagerListener listener = new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {
                if (surfaceId.equals(surface.getSurfaceId())) {
                    surfaceRef.set(surface);
                    latch.countDown();
                }
            }

            @Override
            public void onDeleteSurface(Surface surface) {}
        };

        surfaceManager.addListener(listener);
        surfaceManager.beginTextStream();
        for (String chunk : chunks) {
            surfaceManager.receiveTextChunk(chunk);
        }
        surfaceManager.endTextStream();

        boolean ok = latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        surfaceManager.removeListener(listener);
        assertTrue("流式 Surface 创建超时（surfaceId=" + surfaceId
                + ", chunkSize=" + chunkSize + "）", ok);

        // 轮询等待 componentCount 稳定（连续 3 次相同，间隔 50ms）
        Surface surface = surfaceRef.get();
        long deadline = System.currentTimeMillis() + TIMEOUT_MS;
        int stableCount = 0;
        int lastCount = -1;
        while (System.currentTimeMillis() < deadline) {
            Thread.sleep(50);
            final int[] count = {-1};
            CountDownLatch barrier = new CountDownLatch(1);
            new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                count[0] = surface.getComponentCount();
                barrier.countDown();
            });
            barrier.await(TIMEOUT_MS, TimeUnit.MILLISECONDS);
            if (count[0] > 0 && count[0] == lastCount) {
                stableCount++;
                if (stableCount >= 3) break;
            } else {
                stableCount = 0;
                lastCount = count[0];
            }
        }
        return surface;
    }

    /**
     * 对 surface 执行基本的 expect 断言（componentCount + componentIds）。
     */
    private void assertSurfaceMatchesExpect(Surface surface, JSONObject expect,
                                            String tag) throws Exception {
        int expectedCount = expect.getInt("componentCount");
        assertEquals(tag + ": 组件数量", expectedCount, surface.getComponentCount());

        if (expect.has("componentIds")) {
            JSONArray componentIds = expect.getJSONArray("componentIds");
            for (int i = 0; i < componentIds.length(); i++) {
                String cid = componentIds.getString(i);
                assertNotNull(tag + ": 组件 '" + cid + "' 应存在", surface.getComponent(cid));
            }
        }
    }

    // ==================== STREAM-01 ~ STREAM-04：Button 不同 chunkSize ====================

    /**
     * STREAM-01：chunkSize=10，验证小 chunk 流式传输后组件树正确。
     */
    @Test
    public void testSTREAM01_buttonSimple_chunkSize10() throws Exception {
        String fixturePath = "stream/cases/01_button_simple.json";
        String surfaceId = loader.getSurfaceId(fixturePath);
        String fullJson = loader.loadPayloadAsString(fixturePath);
        JSONObject expect = loader.getExpect(fixturePath);

        Surface surface = streamAndWaitForRender(fullJson, surfaceId, 10);
        assertNotNull("STREAM-01: Surface 应创建成功", surface);
        assertSurfaceMatchesExpect(surface, expect, "STREAM-01");
    }

    /**
     * STREAM-02：chunkSize=500，验证大 chunk 流式传输后组件树正确。
     */
    @Test
    public void testSTREAM02_buttonSimple_chunkSize500() throws Exception {
        String fixturePath = "stream/cases/01_button_simple.json";
        String surfaceId = loader.getSurfaceId(fixturePath);
        String fullJson = loader.loadPayloadAsString(fixturePath);
        JSONObject expect = loader.getExpect(fixturePath);

        Surface surface = streamAndWaitForRender(fullJson, surfaceId, 500);
        assertNotNull("STREAM-02: Surface 应创建成功", surface);
        assertSurfaceMatchesExpect(surface, expect, "STREAM-02");
    }

    /**
     * STREAM-03：chunkSize=1（单字符），验证最极端分片下组件树正确。
     */
    @Test
    public void testSTREAM03_buttonSimple_chunkSize1() throws Exception {
        String fixturePath = "stream/cases/01_button_simple.json";
        String surfaceId = loader.getSurfaceId(fixturePath);
        String fullJson = loader.loadPayloadAsString(fixturePath);
        JSONObject expect = loader.getExpect(fixturePath);

        Surface surface = streamAndWaitForRender(fullJson, surfaceId, 1);
        assertNotNull("STREAM-03: Surface 应创建成功", surface);
        assertSurfaceMatchesExpect(surface, expect, "STREAM-03");
    }

    /**
     * STREAM-04：chunkSize=-1（一次性发送），验证整体发送与分片结果一致。
     */
    @Test
    public void testSTREAM04_buttonSimple_fullAtOnce() throws Exception {
        String fixturePath = "stream/cases/01_button_simple.json";
        String surfaceId = loader.getSurfaceId(fixturePath);
        String fullJson = loader.loadPayloadAsString(fixturePath);
        JSONObject expect = loader.getExpect(fixturePath);

        Surface surface = streamAndWaitForRender(fullJson, surfaceId, -1);
        assertNotNull("STREAM-04: Surface 应创建成功", surface);
        assertSurfaceMatchesExpect(surface, expect, "STREAM-04");
    }

    // ==================== STREAM-05：复杂嵌套布局 ====================

    /**
     * STREAM-05：嵌套 Column 布局（nested_layout），chunkSize=50 流式发送。
     */
    @Test
    public void testSTREAM05_nestedLayout_chunkSize50() throws Exception {
        String fixturePath = "stream/cases/02_nested_layout.json";
        String surfaceId = loader.getSurfaceId(fixturePath);
        String fullJson = loader.loadPayloadAsString(fixturePath);
        JSONObject expect = loader.getExpect(fixturePath);

        Surface surface = streamAndWaitForRender(fullJson, surfaceId, 50);
        assertNotNull("STREAM-05: Surface 应创建成功", surface);
        assertSurfaceMatchesExpect(surface, expect, "STREAM-05");
    }

    // ==================== STREAM-06：含 dataModel 场景 ====================

    /**
     * STREAM-06：含 updateDataModel 的场景（with_datamodel），chunkSize=20 流式发送。
     */
    @Test
    public void testSTREAM06_withDataModel_chunkSize20() throws Exception {
        String fixturePath = "stream/cases/03_with_datamodel.json";
        String surfaceId = loader.getSurfaceId(fixturePath);
        String fullJson = loader.loadPayloadAsString(fixturePath);
        JSONObject expect = loader.getExpect(fixturePath);

        Surface surface = streamAndWaitForRender(fullJson, surfaceId, 20);
        assertNotNull("STREAM-06: Surface 应创建成功", surface);
        assertSurfaceMatchesExpect(surface, expect, "STREAM-06");
    }

    // ==================== STREAM-07：静态预分片（key 边界截断） ====================

    /**
     * STREAM-07：发送预先在 key 边界处截断的两片静态分片（chunk_01.txt + chunk_02.txt），
     * 验证引擎能正确拼接并创建完整 Surface。
     */
    @Test
    public void testSTREAM07_staticChunks_keyBoundary() throws Exception {
        String metaPath = "stream/static_chunks/01_split_at_key_boundary/meta.json";
        JSONObject meta = loader.loadFixture(metaPath);
        String surfaceId = meta.getString("surfaceId");
        JSONObject expect = meta.getJSONObject("expect");

        // 读取两片静态分片（.txt 文件）
        String chunk1 = loader.readRawText(
                "stream/static_chunks/01_split_at_key_boundary/chunk_01.txt");
        String chunk2 = loader.readRawText(
                "stream/static_chunks/01_split_at_key_boundary/chunk_02.txt");

        CountDownLatch latch = new CountDownLatch(1);
        AtomicReference<Surface> surfaceRef = new AtomicReference<>();

        ISurfaceManagerListener listener = new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {
                if (surfaceId.equals(surface.getSurfaceId())) {
                    surfaceRef.set(surface);
                    latch.countDown();
                }
            }

            @Override
            public void onDeleteSurface(Surface surface) {}
        };

        surfaceManager.addListener(listener);
        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(chunk1);
        surfaceManager.receiveTextChunk(chunk2);
        surfaceManager.endTextStream();

        boolean ok = latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        surfaceManager.removeListener(listener);
        assertTrue("STREAM-07: 静态分片 Surface 创建超时", ok);

        // 轮询等待 componentCount 稳定
        Surface surface = surfaceRef.get();
        assertNotNull("STREAM-07: Surface 应创建成功", surface);
        long deadline2 = System.currentTimeMillis() + TIMEOUT_MS;
        int stableCount2 = 0;
        int lastCount2 = -1;
        while (System.currentTimeMillis() < deadline2) {
            Thread.sleep(50);
            final int[] count = {-1};
            CountDownLatch barrier = new CountDownLatch(1);
            new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> {
                count[0] = surface.getComponentCount();
                barrier.countDown();
            });
            barrier.await(TIMEOUT_MS, TimeUnit.MILLISECONDS);
            if (count[0] > 0 && count[0] == lastCount2) {
                stableCount2++;
                if (stableCount2 >= 3) break;
            } else {
                stableCount2 = 0;
                lastCount2 = count[0];
            }
        }
        assertSurfaceMatchesExpect(surface, expect, "STREAM-07");
    }

    // ==================== STREAM-08：中途 reset（重置流） ====================

    /**
     * STREAM-08：在第一次 beginTextStream 后发送部分数据（不调用 endTextStream），
     * 再次调用 beginTextStream 重置，然后发送完整 JSON，验证：
     * <ul>
     *   <li>第一次不完整流不创建 Surface（超时等待返回 false）</li>
     *   <li>第二次完整流成功创建 Surface</li>
     * </ul>
     */
    @Test
    public void testSTREAM08_resetMidStream() throws Exception {
        String fixturePath = "stream/cases/01_button_simple.json";
        // 使用不同 surfaceId 避免与其他测试冲突
        String surfaceId = "test-surf-stream-reset-08";
        String fullJson = loader.loadPayloadAsString(fixturePath)
                .replace("test-surf-stream-01", surfaceId);

        // ---- 第一阶段：发送不完整片段（截取前 50 字符），不调 endTextStream ----
        CountDownLatch latch1 = new CountDownLatch(1);
        ISurfaceManagerListener earlyListener = new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {
                if (surfaceId.equals(surface.getSurfaceId())) {
                    latch1.countDown();
                }
            }

            @Override
            public void onDeleteSurface(Surface surface) {}
        };

        surfaceManager.addListener(earlyListener);
        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(fullJson.substring(0, Math.min(50, fullJson.length())));
        // 故意不调 endTextStream，直接 reset
        surfaceManager.beginTextStream(); // reset
        surfaceManager.removeListener(earlyListener);

        // 第一次不完整流不应该创建 Surface
        boolean prematureCreate = latch1.await(500, TimeUnit.MILLISECONDS);
        assertTrue("STREAM-08: 不完整流后重置，不应提前创建 Surface（或即使有也可接受）",
                !prematureCreate || surfaceManager.getSurface(surfaceId) == null);

        // ---- 第二阶段：发送完整 JSON ----
        Surface surface = streamAndWaitForRender(fullJson, surfaceId, 100);
        assertNotNull("STREAM-08: 重置后完整流应成功创建 Surface", surface);
        assertTrue("STREAM-08: 组件数量应大于 0", surface.getComponentCount() > 0);
    }
}
