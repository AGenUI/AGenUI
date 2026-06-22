package com.amap.agenuiplayground.tests;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.surface.SurfaceManager;
import com.amap.agenuiplayground.base.AGenUIBaseTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

/**
 * INIT 模块集成测试
 *
 * <p>覆盖用例：
 * <ul>
 *   <li>INIT-01：正常初始化 - {@code isInitialized()} 为 true</li>
 *   <li>INIT-02：重复初始化不崩溃，状态仍为 true</li>
 *   <li>INIT-03：SurfaceManager 可正常创建（引擎已就绪）</li>
 * </ul>
 *
 * <p>注意：不在此处调用 {@code destroy()} 后重建，因为 destroy 会影响其他测试用例的全局状态。
 * destroy 后重建的验证由独立隔离用例处理（需在单独进程中运行）。
 */
@RunWith(AndroidJUnit4.class)
public class InitializationTest extends AGenUIBaseTest {

    /**
     * INIT-01：正常初始化
     * <p>
     * Playground Activity 在 {@code onCreate()} 中完成 AGenUI 初始化，
     * {@code AGenUIBaseTest.setUp()} 启动 Activity 后验证 {@code isInitialized()} 为 true。
     */
    @Test
    public void testINIT01_normalInitialization() {
        assertTrue("INIT-01: AGenUI 应已初始化", AGenUI.getInstance().isInitialized());
    }

    /**
     * INIT-02：重复初始化不崩溃，状态仍为 true
     * <p>
     * 连续调用两次 {@code initialize()}，第二次应静默跳过，不抛出异常，状态仍为 initialized。
     */
    @Test
    public void testINIT02_duplicateInitializationNotCrash() {
        activityRule.getScenario().onActivity(activity -> {
            // 第一次（已在 setUp 中完成）
            assertTrue("前置条件：AGenUI 已初始化", AGenUI.getInstance().isInitialized());

            // 第二次重复初始化，不应抛出异常
            try {
                AGenUI.getInstance().initialize(activity.getApplicationContext());
            } catch (Exception e) {
                throw new AssertionError("INIT-02: 重复初始化抛出异常: " + e.getMessage(), e);
            }

            // 状态仍为 true
            assertTrue("INIT-02: 重复初始化后 isInitialized() 仍应为 true",
                    AGenUI.getInstance().isInitialized());
        });
    }

    /**
     * INIT-03：SurfaceManager 可在初始化后正常创建
     * <p>
     * 验证 {@link AGenUIBaseTest#setUp()} 创建的 SurfaceManager 非 null，
     * 说明引擎已就绪，SurfaceManager 正常构造成功。
     */
    @Test
    public void testINIT03_surfaceManagerCreatedSuccessfully() {
        assertNotNull("INIT-03: SurfaceManager 应创建成功（非 null）", surfaceManager);
    }
}
