package com.amap.agenuiplayground.base;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;

import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.ISurfaceManagerListener;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.A2UIPlaygroundActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

import static org.junit.Assert.assertTrue;

/**
 * AGenUI 集成测试基类
 *
 * <p>封装以下通用能力：
 * <ul>
 *   <li>Activity 生命周期管理（通过 ActivityScenarioRule）</li>
 *   <li>AGenUI 初始化与 SurfaceManager 创建</li>
 *   <li>异步等待：{@link #sendAndWaitForSurface} + {@link #waitForMainThread}</li>
 *   <li>Surface 销毁等待：{@link #sendAndWaitForDeleteSurface}</li>
 *   <li>{@link #After} 中自动 release SurfaceManager</li>
 * </ul>
 *
 * <p>异步机制说明：
 * {@code receiveTextChunk()} 立即返回，但 Surface 创建 / 组件填充均通过
 * {@code mainHandler.post()} 异步切换到主线程执行。测试必须等待回调完成后再断言。
 */
public abstract class AGenUIBaseTest {

    protected static final long TIMEOUT_MS = 5000;

    @Rule
    public ActivityScenarioRule<A2UIPlaygroundActivity> activityRule =
            new ActivityScenarioRule<>(A2UIPlaygroundActivity.class);

    /** 被测 SurfaceManager，每个测试方法独立创建/释放 */
    protected SurfaceManager surfaceManager;

    @Before
    public void setUp() {
        activityRule.getScenario().onActivity(activity -> {
            // 确保 AGenUI 已初始化（Playground Activity 在 onCreate 中初始化）
            if (!AGenUI.getInstance().isInitialized()) {
                AGenUI.getInstance().initialize(activity.getApplicationContext());
            }
            surfaceManager = new SurfaceManager(activity);
        });
    }

    @After
    public void tearDown() {
        if (surfaceManager != null) {
            activityRule.getScenario().onActivity(activity -> {
                surfaceManager.destroy();
                surfaceManager = null;
            });
        }
    }

    // ==================== 异步等待工具 ====================

    /**
     * 以 beginTextStream / receiveTextChunk / endTextStream 发送完整 JSON，
     * 并阻塞等待指定 surfaceId 的 {@code onCreateSurface} 回调触发。
     *
     * <p>注意：Surface 创建完成时组件可能尚未填充，需配合
     * {@link #waitForMainThread()} 使用。
     *
     * @param json      符合 A2UI 协议的完整 JSON 字符串
     * @param surfaceId 期待创建的 surfaceId
     * @return 创建完成的 {@link Surface} 实例
     * @throws InterruptedException 等待被中断
     */
    protected Surface sendAndWaitForSurface(String json, String surfaceId)
            throws InterruptedException {
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
        surfaceManager.receiveTextChunk(json);
        surfaceManager.endTextStream();

        boolean ok = latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        surfaceManager.removeListener(listener);
        assertTrue("Surface 创建超时（surfaceId=" + surfaceId + "）", ok);
        return surfaceRef.get();
    }

    /**
     * 发送含 deleteSurface 消息的 JSON，并等待 {@code onDeleteSurface} 回调触发。
     *
     * @param json      含 deleteSurface 的 JSON 字符串
     * @param surfaceId 期待销毁的 surfaceId
     * @throws InterruptedException 等待被中断
     */
    protected void sendAndWaitForDeleteSurface(String json, String surfaceId)
            throws InterruptedException {
        CountDownLatch latch = new CountDownLatch(1);

        ISurfaceManagerListener listener = new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {}

            @Override
            public void onDeleteSurface(Surface surface) {
                if (surfaceId.equals(surface.getSurfaceId())) {
                    latch.countDown();
                }
            }
        };
        surfaceManager.addListener(listener);

        surfaceManager.beginTextStream();
        surfaceManager.receiveTextChunk(json);
        surfaceManager.endTextStream();

        boolean ok = latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        surfaceManager.removeListener(listener);
        assertTrue("Surface 销毁超时（surfaceId=" + surfaceId + "）", ok);
    }

    /**
     * 向主线程 post 一个 barrier 任务，等待其执行完毕。
     *
     * <p>用于确保 {@code onCreateSurface} 和 {@code onUpdateComponents} 的两次
     * {@code mainHandler.post()} 均已完成，组件已写入 {@code componentTree}。
     *
     * @throws InterruptedException 等待被中断
     */
    protected void waitForMainThread() throws InterruptedException {
        CountDownLatch barrier = new CountDownLatch(1);
        new Handler(Looper.getMainLooper()).post(barrier::countDown);
        boolean ok = barrier.await(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        assertTrue("等待主线程超时", ok);
    }

    /**
     * 组合发送 JSON + 等待 Surface 创建 + 轮询等待组件数量稳定。
     *
     * <p>采用轮询策略：每 50ms 检查一次 componentCount，连续 3 次相同则认为稳定。
     * 最长等待 {@link #TIMEOUT_MS} ms。
     *
     * @param json      A2UI 协议 JSON 字符串
     * @param surfaceId 期待创建的 surfaceId
     * @return 已完成组件渲染的 {@link Surface}
     * @throws InterruptedException 等待被中断
     */
    protected Surface sendAndWaitForRender(String json, String surfaceId)
            throws InterruptedException {
        Surface surface = sendAndWaitForSurface(json, surfaceId);
        // 轮询等待 componentCount 稳定（连续 3 次相同，间隔 50ms）
        long deadline = System.currentTimeMillis() + TIMEOUT_MS;
        int stableCount = 0;
        int lastCount = -1;
        while (System.currentTimeMillis() < deadline) {
            Thread.sleep(50);
            // 通过主线程 barrier 获取当前计数
            final int[] count = {-1};
            CountDownLatch barrier = new CountDownLatch(1);
            new Handler(Looper.getMainLooper()).post(() -> {
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
     * 逐条发送 JSON 消息数组，等待 Surface 创建和所有消息处理完成。
     *
     * <p>背景：引擎的 {@code receiveTextChunk()} 将数据 post 到内部 MessageThread 异步处理；
     * 若多条消息拼接为一个 chunk 一次性发送，流式解析器可能在组件未全部 emit 时就因
     * {@code endTextStream()} 的 resetState() 清空了解析状态，导致后续组件丢失。
     *
     * <p>解决方法：每条消息独立走完 begin → receive → end 三步，保证每条消息被完整处理后再发送下一条，
     * 彻底规避流式解析的截断问题。
     *
     * @param messages  fixture 中的消息 JSONArray（每个元素为完整的协议 JSON 对象）
     * @param surfaceId 期待创建的 surfaceId（createSurface 消息所携带的 id）
     * @return 已创建的 {@link Surface} 实例
     * @throws Exception 超时或解析异常
     */
    protected Surface sendMessagesAndWaitForSurface(
            org.json.JSONArray messages, String surfaceId) throws Exception {
        Surface[] surfaceHolder = {null};
        CountDownLatch surfaceLatch = new CountDownLatch(1);

        ISurfaceManagerListener listener = new ISurfaceManagerListener() {
            @Override
            public void onCreateSurface(Surface surface) {
                if (surfaceId.equals(surface.getSurfaceId())) {
                    surfaceHolder[0] = surface;
                    surfaceLatch.countDown();
                }
            }
            @Override
            public void onDeleteSurface(Surface surface) {}
        };
        surfaceManager.addListener(listener);

        // 逐条发送：每条消息独立走完 begin → receive → end，避免流式解析截断
        for (int i = 0; i < messages.length(); i++) {
            String msgJson = messages.get(i).toString();
            surfaceManager.beginTextStream();
            surfaceManager.receiveTextChunk(msgJson);
            surfaceManager.endTextStream();
        }

        // 等待 Surface 创建（由 createSurface 消息触发）
        boolean ok = surfaceLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS);
        surfaceManager.removeListener(listener);
        assertTrue("Surface 创建超时（surfaceId=" + surfaceId + "）", ok);
        return surfaceHolder[0];
    }

    /**
     * 获取当前 Activity 实例（在主线程回调中使用）。
     * 子类可通过 {@code activityRule.getScenario().onActivity(cb)} 获取 Activity。
     */
    protected void runOnActivity(ActivityScenario.ActivityAction<A2UIPlaygroundActivity> action) {
        activityRule.getScenario().onActivity(action);
    }
}
