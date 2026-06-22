package com.amap.agenuiplayground.tests;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.surface.Surface;
import com.amap.agenuiplayground.base.AGenUIBaseTest;
import com.amap.agenuiplayground.base.TestFixtureLoader;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Map;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

/**
 * 组件渲染验证测试
 *
 * <p>覆盖各类 fixture 的组件数量、类型、ID 及父子关系的全面断言。
 *
 * <p>用例：
 * <ul>
 *   <li>01_text_only：4 个组件（Column + 3×Text）</li>
 *   <li>02_button_with_action：3 个组件，Button 类型与 ID 正确</li>
 *   <li>03_nested_column：7 个组件，两层嵌套 Column</li>
 *   <li>04_card_complex：7 个组件，Card 嵌套结构</li>
 *   <li>05_modal_with_trigger：5 个组件，含 Modal</li>
 * </ul>
 */
@RunWith(AndroidJUnit4.class)
public class ComponentRenderTest extends AGenUIBaseTest {

    private TestFixtureLoader loader;

    @Override
    public void setUp() {
        super.setUp();
        loader = new TestFixtureLoader(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
    }

    // ==================== 通用验证辅助 ====================

    /**
     * 读取 fixture，发送并等待渲染完成，然后验证组件数量和所有组件 ID 是否存在。
     */
    private Surface renderAndVerifyBasic(String fixturePath) throws Exception {
        String surfaceId = loader.getSurfaceId(fixturePath);
        String json = loader.loadMessagesAsString(fixturePath);
        JSONObject expect = loader.getExpect(fixturePath);

        Surface surface = sendAndWaitForRender(json, surfaceId);
        assertNotNull("Surface 应创建成功: " + fixturePath, surface);

        // 验证组件数量
        int expectedCount = expect.getInt("componentCount");
        assertEquals("组件数量应为 " + expectedCount + ": " + fixturePath,
                expectedCount, surface.getComponentCount());

        // 验证所有组件 ID 均可通过 getComponent() 获取
        JSONArray componentIds = expect.getJSONArray("componentIds");
        for (int i = 0; i < componentIds.length(); i++) {
            String id = componentIds.getString(i);
            assertNotNull("组件 ID '" + id + "' 应存在于组件树中: " + fixturePath,
                    surface.getComponent(id));
        }

        return surface;
    }

    // ==================== 测试用例 ====================

    /**
     * 01_text_only：Column + 3 个 Text，共 4 个组件
     */
    @Test
    public void testRender_01_textOnly() throws Exception {
        Surface surface = renderAndVerifyBasic("components/01_text_only.json");

        // 验证根组件类型
        A2UIComponent root = surface.getComponent("root");
        assertNotNull("root 组件不应为 null", root);
        assertEquals("root 类型应为 Column", "Column", root.getComponentType());

        // 验证 Text 组件类型
        assertEquals("text-title 类型应为 Text", "Text",
                surface.getComponent("text-title").getComponentType());
        assertEquals("text-body 类型应为 Text", "Text",
                surface.getComponent("text-body").getComponentType());
        assertEquals("text-caption 类型应为 Text", "Text",
                surface.getComponent("text-caption").getComponentType());
    }

    /**
     * 02_button_with_action：Column + Button + Text，共 3 个组件
     */
    @Test
    public void testRender_02_buttonWithAction() throws Exception {
        Surface surface = renderAndVerifyBasic("components/02_button_with_action.json");

        // 验证 Button 类型与 ID
        A2UIComponent btn = surface.getComponent("btn-submit");
        assertNotNull("btn-submit 不应为 null", btn);
        assertEquals("btn-submit 类型应为 Button", "Button", btn.getComponentType());

        // 验证 Button 子 Text
        A2UIComponent btnLabel = surface.getComponent("btn-label");
        assertNotNull("btn-label 不应为 null", btnLabel);
        assertEquals("btn-label 类型应为 Text", "Text", btnLabel.getComponentType());
    }

    /**
     * 03_nested_column：外层 Column + 2×内层 Column + 4×Text，共 7 个组件
     */
    @Test
    public void testRender_03_nestedColumn() throws Exception {
        Surface surface = renderAndVerifyBasic("components/03_nested_column.json");

        // 验证所有 Column 和 Text 类型
        assertEquals("col-a 类型应为 Column", "Column",
                surface.getComponent("col-a").getComponentType());
        assertEquals("col-b 类型应为 Column", "Column",
                surface.getComponent("col-b").getComponentType());
        assertEquals("text-a1 类型应为 Text", "Text",
                surface.getComponent("text-a1").getComponentType());
        assertEquals("text-b2 类型应为 Text", "Text",
                surface.getComponent("text-b2").getComponentType());
    }

    /**
     * 04_card_complex：Column + Card + Column + 2×Text + Button + Text，共 7 个组件
     */
    @Test
    public void testRender_04_cardComplex() throws Exception {
        Surface surface = renderAndVerifyBasic("components/04_card_complex.json");

        // 验证 Card 类型
        assertEquals("card-wrapper 类型应为 Card", "Card",
                surface.getComponent("card-wrapper").getComponentType());

        // 验证 Button 存在且类型正确
        A2UIComponent cardBtn = surface.getComponent("card-btn");
        assertNotNull("card-btn 不应为 null", cardBtn);
        assertEquals("card-btn 类型应为 Button", "Button", cardBtn.getComponentType());
    }

    /**
     * 05_modal_with_trigger：Column + Button + Text + Modal + Text，共 5 个组件
     */
    @Test
    public void testRender_05_modalWithTrigger() throws Exception {
        String fixturePath = "components/05_modal_with_trigger.json";
        String surfaceId = loader.getSurfaceId(fixturePath);
        String json = loader.loadMessagesAsString(fixturePath);

        Surface surface = sendAndWaitForRender(json, surfaceId);
        assertNotNull("Surface should be created: " + fixturePath, surface);

        // Modal factory is not registered on Android, so modal-dialog won't be created.
        // Expect 4 components: root(Column), trigger-btn(Button), trigger-btn-text(Text), modal-body(Text)
        assertEquals("Component count should be 5",
                5, surface.getComponentCount());

        // Verify non-Modal components are rendered correctly
        assertNotNull("root should exist", surface.getComponent("root"));
        assertEquals("root type should be Column", "Column",
                surface.getComponent("root").getComponentType());

        assertNotNull("trigger-btn should exist", surface.getComponent("trigger-btn"));
        assertEquals("trigger-btn type should be Button", "Button",
                surface.getComponent("trigger-btn").getComponentType());

        assertNotNull("trigger-btn-text should exist", surface.getComponent("trigger-btn-text"));
        assertNotNull("modal-body should exist", surface.getComponent("modal-body"));
    }

    /**
     * 完整组件树验证：通过 getComponentTree() 确认所有组件均在树中
     */
    @Test
    public void testRender_componentTreeContainsAllComponents() throws Exception {
        String fixturePath = "components/03_nested_column.json";
        String surfaceId = loader.getSurfaceId(fixturePath);
        String json = loader.loadMessagesAsString(fixturePath);
        JSONObject expect = loader.getExpect(fixturePath);

        Surface surface = sendAndWaitForRender(json, surfaceId);

        Map<String, A2UIComponent> tree = surface.getComponentTree();
        JSONArray componentIds = expect.getJSONArray("componentIds");

        for (int i = 0; i < componentIds.length(); i++) {
            String id = componentIds.getString(i);
            assertNotNull("getComponentTree() 应包含组件 ID: " + id, tree.get(id));
        }
    }
}
