/**
 * FunctionCallTest.ets
 *
 * AGenUI Harmony FunctionCall 集成测试
 *
 * 通过 fixture 的 messages 回放验证引擎 FunctionCall 处理链路：
 * - Surface 创建成功（onCreateSurface 回调触发）
 * - 组件 ID 可访问
 *
 * 注意：Harmony SDK 的 Component 类不暴露渲染后的文本内容（无 getProperties/getText），
 * 因此 textContent 断言在 Harmony 端暂不可用，仅验证 Surface 创建和组件 ID。
 * 完整的 textContent 验证由 Android 和 iOS 端覆盖。
 *
 * 覆盖用例：
 *   SKILL-01~11：11 个内置函数
 *   SCENE-01~04：4 个混合场景
 */

import { describe, it, expect, beforeEach, afterEach } from '@ohos/hypium';
import { common } from '@kit.AbilityKit';
import { SurfaceManager } from '@agenui/agenui';
import { sendAndWaitForRender, sleep } from '../base/AGenUIBaseTest';
import {
  loadFixture,
  getMessagesAsString,
  getSurfaceId,
  getExpect
} from '../base/TestFixtureLoader';

/**
 * 通用验证辅助：加载 fixture，发送消息，等待渲染，验证组件 ID
 * 提取为模块级函数以避免 ArkTS arkts-no-nested-funcs 限制
 */
async function runFixtureTest(
  sm: SurfaceManager,
  context: common.UIAbilityContext,
  fixturePath: string
): Promise<void> {
  const fixture = await loadFixture(context.resourceManager, fixturePath);
  const surfaceId = getSurfaceId(fixture);
  const json = getMessagesAsString(fixture);

  const surface = await sendAndWaitForRender(sm, json, surfaceId);
  expect(surface).not().assertNull();
  expect(surface.surfaceId).assertEqual(surfaceId);

  // 额外等待，确保 FunctionCall 求值和组件渲染完成
  await sleep(200);

  // 验证所有期望组件 ID 可访问
  const expectedIds = getExpect(fixture, 'componentIds') as string[] | undefined;
  if (expectedIds) {
    for (const id of expectedIds) {
      const comp = surface.getComponent(id, '');
      expect(comp.componentId).assertEqual(id);
    }
  }
}

export default function functionCallTest() {
  describe('FunctionCallTest', () => {
    let sm: SurfaceManager;
    let context: common.UIAbilityContext;

    beforeEach(() => {
      context = getContext() as common.UIAbilityContext;
      sm = new SurfaceManager(context);
    });

    afterEach(() => {
      sm.destroy();
    });

    // ==================== SKILL 系列：单函数验证 ====================

    it('SKILL-01_formatDate', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/action_formatDate.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SKILL-02_formatString', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/action_formatString.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SKILL-03_formatNumber', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/format_formatNumber.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SKILL-04_formatCurrency', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/format_formatCurrency.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SKILL-05_pluralize', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/format_pluralize.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SKILL-06_formatToken', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/format_token.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SKILL-07_validateRequired', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/validate_required.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SKILL-08_validateNumeric', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/validate_numeric.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SKILL-09_validateLength', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/validate_length.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SKILL-10_validateRegex', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/validate_regex.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SKILL-11_validateEmail', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/validate_email.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    // ==================== SCENE 系列：混合场景验证 ====================

    it('SCENE-01_formatStringNested', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/mixed_formatString_nested.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SCENE-02_databindingWithFunc', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/mixed_databinding_with_func.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SCENE-03_logicExpression', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/mixed_logic_expression.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    it('SCENE-04_funcInAction_textContent', 0, async (done: Function) => {
      try {
        await runFixtureTest(sm, context, 'function_call/mixed_func_in_action.json');
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });
  });
}
