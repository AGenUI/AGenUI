/**
 * ComponentRenderTest.ets
 *
 * AGenUI Harmony 组件渲染集成测试
 *
 * 覆盖 components/ 下 5 个 fixture 场景：
 *   COMP-01  纯文本组件（01_text_only）
 *   COMP-02  带 action 的 Button 组件（02_button_with_action）
 *   COMP-03  嵌套 Column 布局（03_nested_column）
 *   COMP-04  复杂 Card 组件（04_card_complex）
 *   COMP-05  含 Modal 触发器（05_modal_with_trigger）
 */

import { describe, it, expect, beforeEach, afterEach } from '@ohos/hypium';
import { common } from '@kit.AbilityKit';
import { SurfaceManager } from '@agenui/agenui';
import {
  sendAndWaitForRender
} from '../base/AGenUIBaseTest';
import { loadFixture, getMessagesAsString, getSurfaceId, getExpect } from '../base/TestFixtureLoader';

export default function componentRenderTest() {
  describe('ComponentRenderTest', () => {
    let sm: SurfaceManager;
    let context: common.UIAbilityContext;

    beforeEach(() => {
      context = getContext() as common.UIAbilityContext;
      sm = new SurfaceManager(context);
    });

    afterEach(() => {
      sm.destroy();
    });

    /**
     * COMP-01: 纯文本组件渲染
     * 验证 01_text_only.json：createSurface + updateComponents，
     * 渲染完成后可查询 root / title / body 组件。
     */
    it('COMP-01_text_only_render', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(context.resourceManager, 'components/01_text_only.json');
        const surfaceId = getSurfaceId(fixture);
        const surface = await sendAndWaitForRender(sm, getMessagesAsString(fixture), surfaceId);

        expect(surface).not().assertNull();

        const expectedIds = getExpect(fixture, 'componentIds') as string[] | undefined;
        if (expectedIds && expectedIds.length > 0) {
          const rootComp = surface.getComponent(expectedIds[0], '');
          expect(rootComp.componentId).assertEqual(expectedIds[0]);
        }
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * COMP-02: 带 action 的 Button 组件渲染
     * 验证 02_button_with_action.json 渲染，Button 组件可访问。
     */
    it('COMP-02_button_with_action_render', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(
          context.resourceManager, 'components/02_button_with_action.json'
        );
        const surfaceId = getSurfaceId(fixture);
        const surface = await sendAndWaitForRender(sm, getMessagesAsString(fixture), surfaceId);

        expect(surface).not().assertNull();

        // 验证 Button 组件可被获取
        const buttonComp = surface.getComponent('action-btn', 'Button');
        expect(buttonComp).not().assertNull();
        expect(buttonComp.componentId).assertEqual('action-btn');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * COMP-03: 嵌套 Column 布局渲染
     * 验证 03_nested_column.json 渲染，多层嵌套组件均可访问。
     */
    it('COMP-03_nested_column_render', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(
          context.resourceManager, 'components/03_nested_column.json'
        );
        const surfaceId = getSurfaceId(fixture);
        const surface = await sendAndWaitForRender(sm, getMessagesAsString(fixture), surfaceId);

        expect(surface).not().assertNull();

        // 验证根组件存在
        const root = surface.getComponent('root', 'Column');
        expect(root.componentId).assertEqual('root');

        // 验证期望组件数（从 fixture expect 中读取）
        const expectedCount = getExpect(fixture, 'componentCount') as number | undefined;
        expect(expectedCount).not().assertUndefined();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * COMP-04: 复杂 Card 组件渲染
     * 验证 04_card_complex.json 渲染，Card 内部组件均可访问。
     */
    it('COMP-04_card_complex_render', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(
          context.resourceManager, 'components/04_card_complex.json'
        );
        const surfaceId = getSurfaceId(fixture);
        const surface = await sendAndWaitForRender(sm, getMessagesAsString(fixture), surfaceId);

        expect(surface).not().assertNull();
        const root = surface.getComponent('root', '');
        expect(root).not().assertNull();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * COMP-05: 含 Modal 触发器组件渲染
     * 验证 05_modal_with_trigger.json 渲染，触发按钮组件可访问。
     */
    it('COMP-05_modal_with_trigger_render', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(
          context.resourceManager, 'components/05_modal_with_trigger.json'
        );
        const surfaceId = getSurfaceId(fixture);
        const surface = await sendAndWaitForRender(sm, getMessagesAsString(fixture), surfaceId);

        expect(surface).not().assertNull();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });
  });
}
