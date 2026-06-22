/**
 * MultiSurfaceTest.ets
 *
 * AGenUI Harmony 多 Surface 隔离集成测试
 *
 * 测试用例：
 *   MULTI-01  同一 SurfaceManager 并发创建两个 Surface
 *   MULTI-02  两个 Surface 的组件树互不干扰（隔离验证）
 *   MULTI-03  销毁一个 Surface 不影响另一个 Surface
 */

import { describe, it, expect, beforeEach, afterEach } from '@ohos/hypium';
import { common } from '@kit.AbilityKit';
import { SurfaceManager } from '@agenui/agenui';
import {
  sendAndWaitForSurface,
  sendAndWaitForDeleteSurface,
  sendAndWaitForRender
} from '../base/AGenUIBaseTest';
import { loadFixture, getMessagesAsString, getSurfaceId } from '../base/TestFixtureLoader';

export default function multiSurfaceTest() {
  describe('MultiSurfaceTest', () => {
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
     * MULTI-01: 同一 SurfaceManager 并发创建两个 Surface
     * 同时发送 surface_a.json 和 surface_b.json，验证两个 Surface 均被创建。
     */
    it('MULTI-01_concurrent_create_two_surfaces', 0, async (done: Function) => {
      try {
        const fixtures = await Promise.all([
          loadFixture(context.resourceManager, 'multi_surface/surface_a.json'),
          loadFixture(context.resourceManager, 'multi_surface/surface_b.json')
        ]);
        const fixtureA = fixtures[0];
        const fixtureB = fixtures[1];

        const jsonA = getMessagesAsString(fixtureA);
        const jsonB = getMessagesAsString(fixtureB);
        const surfaceIdA = getSurfaceId(fixtureA);
        const surfaceIdB = getSurfaceId(fixtureB);

        // 并发等待两个 Surface 创建
        const surfaces = await Promise.all([
          sendAndWaitForRender(sm, jsonA, surfaceIdA),
          sendAndWaitForRender(sm, jsonB, surfaceIdB)
        ]);
        const surfaceA = surfaces[0];
        const surfaceB = surfaces[1];

        expect(surfaceA).not().assertNull();
        expect(surfaceB).not().assertNull();
        expect(surfaceA.surfaceId).assertEqual(surfaceIdA);
        expect(surfaceB.surfaceId).assertEqual(surfaceIdB);
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * MULTI-02: 两个 Surface 的组件树互不干扰
     * Surface A 包含 btn-a，Surface B 不应能通过 getSurface(surfaceIdA) 访问 btn-a。
     * 验证 getSurface(surfaceIdB).getComponent('btn-a') 不在 B 的树中。
     */
    it('MULTI-02_surface_component_isolation', 0, async (done: Function) => {
      try {
        const fixtures2 = await Promise.all([
          loadFixture(context.resourceManager, 'multi_surface/surface_a.json'),
          loadFixture(context.resourceManager, 'multi_surface/surface_b.json')
        ]);
        const fixtureA2 = fixtures2[0];
        const fixtureB2 = fixtures2[1];

        const surfaces2 = await Promise.all([
          sendAndWaitForRender(sm, getMessagesAsString(fixtureA2), getSurfaceId(fixtureA2)),
          sendAndWaitForRender(sm, getMessagesAsString(fixtureB2), getSurfaceId(fixtureB2))
        ]);
        const surfaceA2 = surfaces2[0];
        const surfaceB2 = surfaces2[1];

        // Surface A 拥有 btn-a，Surface B 不拥有 btn-a
        const compAInA = surfaceA2.getComponent('btn-a', 'Button');
        expect(compAInA).not().assertNull();
        expect(compAInA.componentId).assertEqual('btn-a');

        // Surface B 只有 text-b1 / text-b2，不应有 btn-a
        // Harmony Surface.getComponent 总会返回/创建对象，
        // 但组件类型为空串表示该组件未被 updateComponents 注册
        const compAInB = surfaceB2.getComponent('btn-a', '');
        // 组件在 B 的 context 中未被 updateComponents 写入，componentType 为空
        expect(compAInB.componentType).assertEqual('');

        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * MULTI-03: 销毁 Surface A 不影响 Surface B
     * 删除 Surface A 后，Surface B 仍可通过 getSurface 访问。
     */
    it('MULTI-03_delete_surfaceA_does_not_affect_surfaceB', 0, async (done: Function) => {
      try {
        const fixtures3 = await Promise.all([
          loadFixture(context.resourceManager, 'multi_surface/surface_a.json'),
          loadFixture(context.resourceManager, 'multi_surface/surface_b.json')
        ]);
        const fixtureA3 = fixtures3[0];
        const fixtureB3 = fixtures3[1];
        const surfaceIdA3 = getSurfaceId(fixtureA3);
        const surfaceIdB3 = getSurfaceId(fixtureB3);

        // 创建两个 Surface
        await Promise.all([
          sendAndWaitForRender(sm, getMessagesAsString(fixtureA3), surfaceIdA3),
          sendAndWaitForRender(sm, getMessagesAsString(fixtureB3), surfaceIdB3)
        ]);

        // 从 fixture 加载 deleteSurface JSON
        const deleteFixture = await loadFixture(
          context.resourceManager,
          'multi_surface/delete_surface_a.json'
        );
        const deleteJsonA = getMessagesAsString(deleteFixture);
        await sendAndWaitForDeleteSurface(sm, deleteJsonA, surfaceIdA3);

        // Surface A 已销毁
        expect(sm.getSurface(surfaceIdA3)).assertNull();

        // Surface B 仍存在
        const surfaceB3 = sm.getSurface(surfaceIdB3);
        expect(surfaceB3).not().assertNull();
        expect(surfaceB3!.surfaceId).assertEqual(surfaceIdB3);
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });
  });
}
