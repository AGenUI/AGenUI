/**
 * SurfaceLifecycleTest.ets
 *
 * AGenUI Harmony Surface 生命周期集成测试
 *
 * 测试用例：
 *   SURFACE-01  onCreateSurface 回调触发（验证 surfaceId 正确）
 *   SURFACE-02  onDeleteSurface 回调触发（发送 deleteSurface 消息后触发）
 *   SURFACE-03  createSurface + updateComponents 后组件可查询
 */

import { describe, it, expect, beforeEach, afterEach } from '@ohos/hypium';
import { common } from '@kit.AbilityKit';
import { SurfaceManager, ISurfaceManagerListener, Surface } from '@agenui/agenui';
import {
  sendAndWaitForSurface,
  sendAndWaitForDeleteSurface,
  sendAndWaitForRender,
  sleep
} from '../base/AGenUIBaseTest';
import { loadFixture, getMessagesAsString, getSurfaceId } from '../base/TestFixtureLoader';

export default function surfaceLifecycleTest() {
  describe('SurfaceLifecycleTest', () => {
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
     * SURFACE-01: onCreateSurface 回调触发
     * 发送 createSurface 消息，验证回调携带正确的 surfaceId。
     */
    it('SURFACE-01_onCreateSurface_callback', 0, async (done: Function) => {
      const fixture = await loadFixture(
        context.resourceManager,
        'init/create_surface_simple.json'
      );
      const json = getMessagesAsString(fixture);
      const surfaceId = getSurfaceId(fixture);

      const surface = await sendAndWaitForSurface(sm, json, surfaceId);
      expect(surface).not().assertNull();
      expect(surface.surfaceId).assertEqual(surfaceId);
      done();
    });

    /**
     * SURFACE-02: onDeleteSurface 回调触发
     * 先创建 Surface，再发送 deleteSurface 消息，验证 onDeleteSurface 回调被触发。
     */
    it('SURFACE-02_onDeleteSurface_callback', 0, async (done: Function) => {
      const fixture = await loadFixture(
        context.resourceManager,
        'init/create_surface_simple.json'
      );
      const surfaceId = getSurfaceId(fixture);
      const createJson = getMessagesAsString(fixture);

      await sendAndWaitForSurface(sm, createJson, surfaceId);

      const deleteFixture = await loadFixture(
        context.resourceManager,
        'init/delete_surface_simple.json'
      );
      const deleteJson = getMessagesAsString(deleteFixture);
      await sendAndWaitForDeleteSurface(sm, deleteJson, surfaceId);

      const deletedSurface = sm.getSurface(surfaceId);
      expect(deletedSurface).assertNull();
      done();
    });

    /**
     * SURFACE-03: createSurface + updateComponents 后组件可查询
     * 使用包含 updateComponents 的 fixture，验证 Surface 创建后组件可访问。
     */
    it('SURFACE-03_components_accessible_after_render', 0, async (done: Function) => {
      const fixture = await loadFixture(
        context.resourceManager,
        'components/01_text_only.json'
      );
      const json = getMessagesAsString(fixture);
      const surfaceId = getSurfaceId(fixture);

      const surface = await sendAndWaitForRender(sm, json, surfaceId);
      expect(surface).not().assertNull();
      expect(surface.surfaceId).assertEqual(surfaceId);

      const rootComp = surface.getComponent('root', 'Column');
      expect(rootComp).not().assertNull();
      expect(rootComp.componentId).assertEqual('root');
      done();
    });
  });
}
