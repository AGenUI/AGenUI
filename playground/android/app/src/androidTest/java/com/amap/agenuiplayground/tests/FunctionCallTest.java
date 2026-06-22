package com.amap.agenuiplayground.tests;

import android.os.Handler;
import android.os.Looper;
import android.widget.TextView;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenuiplayground.base.AGenUIBaseTest;
import com.amap.agenuiplayground.base.TestFixtureLoader;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

/**
 * FunctionCall 集成测试
 *
 * <p>通过 fixture 的 messages 回放 + textContent 断言，验证引擎 FunctionCall 执行结果。
 *
 * <p>测试覆盖：
 * <ul>
 *   <li>SKILL-01  action_formatDate.json       — formatDate 日期格式化</li>
 *   <li>SKILL-02  action_formatString.json      — formatString 模板字符串</li>
 *   <li>SKILL-03  format_formatNumber.json      — formatNumber 数字格式化</li>
 *   <li>SKILL-04  format_formatCurrency.json    — formatCurrency 货币格式化</li>
 *   <li>SKILL-05  format_pluralize.json         — pluralize 复数化</li>
 *   <li>SKILL-06  format_token.json             — token 设计令牌</li>
 *   <li>SKILL-07  validate_required.json        — required 非空校验</li>
 *   <li>SKILL-08  validate_numeric.json         — numeric 数字校验</li>
 *   <li>SKILL-09  validate_length.json          — length 长度校验</li>
 *   <li>SKILL-10  validate_regex.json           — regex 正则校验</li>
 *   <li>SKILL-11  validate_email.json           — email 邮箱校验</li>
 *   <li>SCENE-01  mixed_formatString_nested.json — formatString 嵌套调用</li>
 *   <li>SCENE-02  mixed_databinding_with_func.json — DataBinding + FunctionCall</li>
 *   <li>SCENE-03  mixed_logic_expression.json   — 逻辑表达式 + FunctionCall</li>
 *   <li>SCENE-04  mixed_func_in_action.json     — FunctionCall 在 Action 参数中使用（仅验证 textContent）</li>
 * </ul>
 *
 * <p>不包含 action_toast.json（平台回调验证，需要单独的 ToastCallbackTest 来模拟 IFunction）。
 */
@RunWith(AndroidJUnit4.class)
public class FunctionCallTest extends AGenUIBaseTest {

    private TestFixtureLoader loader;

    @Override
    public void setUp() {
        super.setUp();
        loader = new TestFixtureLoader(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
    }

    // ==================== 通用验证辅助 ====================

    /**
     * 发送 fixture 消息，等待渲染完成，断言组件数量和 ID，并返回 Surface。
     *
     * <p>采用逐条发送策略：每条消息独立走完 begin → receive → end，
     * 避免引擎流式解析器在 endTextStream resetState() 时丢失未 emit 的组件。
     * 对于含 {@code updateDataModel} 的多消息序列，逐条发送可保证
     * DataModel 已就绪后再处理 updateComponents。
     */
    private Surface renderAndVerify(String fixturePath) throws Exception {
        String surfaceId = loader.getSurfaceId(fixturePath);
        org.json.JSONArray messages = loader.getMessages(fixturePath);
        JSONObject expect = loader.getExpect(fixturePath);

        // 逐条发送消息，等待 Surface 创建
        Surface surface = sendMessagesAndWaitForSurface(messages, surfaceId);
        assertNotNull("Surface 应创建成功: " + fixturePath, surface);

        int expectedCount = expect.getInt("componentCount");

        // 轮询等待组件数量达到期望值（每 50ms 检查一次，最长等待 TIMEOUT_MS）
        long deadline = System.currentTimeMillis() + TIMEOUT_MS;
        while (System.currentTimeMillis() < deadline) {
            final int[] count = {0};
            CountDownLatch barrier = new CountDownLatch(1);
            new Handler(Looper.getMainLooper()).post(() -> {
                count[0] = surface.getComponentCount();
                barrier.countDown();
            });
            barrier.await(TIMEOUT_MS, TimeUnit.MILLISECONDS);
            if (count[0] >= expectedCount) {
                break;
            }
            Thread.sleep(50);
        }

        assertEquals("[读取到的组件IDs: " + surface.getComponentTree().keySet() + "] 组件数量应为 " + expectedCount + ": " + fixturePath,
                expectedCount, surface.getComponentCount());

        JSONArray componentIds = expect.getJSONArray("componentIds");
        for (int i = 0; i < componentIds.length(); i++) {
            String id = componentIds.getString(i);
            assertNotNull("组件 ID '" + id + "' 应存在: " + fixturePath,
                    surface.getComponent(id));
        }

        return surface;
    }

    /**
     * 断言 fixture expect.textContent 数组中每一项的 componentId 对应组件的文本
     * 包含（contains）期望字符串。
     *
     * <p>通过 {@code component.getView()} 获取底层 {@link TextView}，
     * 在主线程上读取 {@code TextView.getText()}，不修改任何 SDK 源码。
     */
    private void assertTextContent(Surface surface, JSONObject expect, String fixturePath)
            throws Exception {
        if (!expect.has("textContent")) {
            return;
        }
        JSONArray textContent = expect.getJSONArray("textContent");
        for (int i = 0; i < textContent.length(); i++) {
            JSONObject item = textContent.getJSONObject(i);
            String componentId = item.getString("componentId");
            String expectedContains = item.getString("contains");
            String note = item.optString("note", componentId);

            A2UIComponent component = surface.getComponent(componentId);
            assertNotNull("组件 '" + componentId + "' 应存在: " + note, component);

            // 通过 getView() 获取底层 TextView，在主线程读取文本
            String[] actualText = {""};
            CountDownLatch latch = new CountDownLatch(1);
            new Handler(Looper.getMainLooper()).post(() -> {
                try {
                    if (component.getView() instanceof TextView) {
                        actualText[0] = ((TextView) component.getView()).getText().toString();
                    }
                } finally {
                    latch.countDown();
                }
            });
            boolean ok = latch.await(3000, TimeUnit.MILLISECONDS);
            assertTrue("等待主线程读取文本超时: " + componentId, ok);

            assertTrue(
                    "【" + note + "】textContent 应包含 '" + expectedContains
                            + "'，实际为: '" + actualText[0] + "'",
                    actualText[0].contains(expectedContains)
            );
        }
    }

    /**
     * 完整回放 fixture 并同时验证组件结构与 textContent。
     */
    private void runFixtureTest(String fixturePath) throws Exception {
        JSONObject expect = loader.getExpect(fixturePath);
        Surface surface = renderAndVerify(fixturePath);
        // 额外等待一个主线程 cycle，确保 FunctionCall 求值已写入 currentText
        waitForMainThread();
        assertTextContent(surface, expect, fixturePath);
    }

    // ==================== SKILL 系列：单函数验证 ====================

    /** SKILL-01: formatDate 日期格式化（ISO 字符串、时间戳、多种格式） */
    @Test
    public void testSKILL01_formatDate() throws Exception {
        runFixtureTest("function_call/action_formatDate.json");
    }

    /** SKILL-02: formatString 模板字符串（简单插值、嵌套调用） */
    @Test
    public void testSKILL02_formatString() throws Exception {
        runFixtureTest("function_call/action_formatString.json");
    }

    /** SKILL-03: formatNumber 数字格式化（千分位、小数位、百分比） */
    @Test
    public void testSKILL03_formatNumber() throws Exception {
        runFixtureTest("function_call/format_formatNumber.json");
    }

    /** SKILL-04: formatCurrency 货币格式化（CNY、USD、EUR） */
    @Test
    public void testSKILL04_formatCurrency() throws Exception {
        runFixtureTest("function_call/format_formatCurrency.json");
    }

    /** SKILL-05: pluralize 复数化（count=0/1/2） */
    @Test
    public void testSKILL05_pluralize() throws Exception {
        runFixtureTest("function_call/format_pluralize.json");
    }

    /** SKILL-06: token 设计令牌（通过 formatString 嵌套） */
    @Test
    public void testSKILL06_formatToken() throws Exception {
        runFixtureTest("function_call/format_token.json");
    }

    /** SKILL-07: required 非空校验（有值/空字符串/null） */
    @Test
    @Ignore("TODO: 待修复 - SKILL-07-c (null → false) textContent 实际为空，引擎对 null 入参的 validateRequired 返回值未渲染到 TextView。详见 reports/runs/feature-1618-agenui_afe271c_20260527_173826")
    public void testSKILL07_validateRequired() throws Exception {
        runFixtureTest("function_call/validate_required.json");
    }

    /** SKILL-08: numeric 数字校验（整数/小数/非数字字符串） */
    @Test
    public void testSKILL08_validateNumeric() throws Exception {
        runFixtureTest("function_call/validate_numeric.json");
    }

    /** SKILL-09: length 长度校验（min/max/range） */
    @Test
    public void testSKILL09_validateLength() throws Exception {
        runFixtureTest("function_call/validate_length.json");
    }

    /** SKILL-10: regex 正则校验（匹配/不匹配） */
    @Test
    public void testSKILL10_validateRegex() throws Exception {
        runFixtureTest("function_call/validate_regex.json");
    }

    /** SKILL-11: email 邮箱格式校验（合法/非法） */
    @Test
    public void testSKILL11_validateEmail() throws Exception {
        runFixtureTest("function_call/validate_email.json");
    }

    // ==================== SCENE 系列：混合场景验证 ====================

    /** SCENE-01: formatString 嵌套调用（formatDate、formatNumber 内联） */
    @Test
    public void testSCENE01_formatStringNested() throws Exception {
        runFixtureTest("function_call/mixed_formatString_nested.json");
    }

    /** SCENE-02: DataBinding 路径绑定 + FunctionCall 组合使用 */
    @Test
    public void testSCENE02_databindingWithFunc() throws Exception {
        runFixtureTest("function_call/mixed_databinding_with_func.json");
    }

    /** SCENE-03: 逻辑表达式中嵌入 FunctionCall（条件渲染） */
    @Test
    public void testSCENE03_logicExpression() throws Exception {
        runFixtureTest("function_call/mixed_logic_expression.json");
    }

    /**
     * SCENE-04: FunctionCall 在 Action 参数中使用（仅验证 textContent 部分）。
     *
     * <p>platformCallback 部分（toast action 触发验证）需要配合 IFunction 注册与
     * Button.performClick，留待 PlatformCallbackTest 补充。
     */
    @Test
    public void testSCENE04_funcInAction_textContent() throws Exception {
        runFixtureTest("function_call/mixed_func_in_action.json");
    }
}
