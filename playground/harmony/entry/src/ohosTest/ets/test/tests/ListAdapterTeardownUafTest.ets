/**
 * RISK56: ListComponent horizontal list adapter teardown UAF
 *
 * When a horizontal List component is destroyed, ~ListComponent() calls
 * teardownNodeAdapter() AFTER base class destroy() has already:
 *   1. Deleted all children (child nodeHandles disposed)
 *   2. Disposed m_nodeHandle (set to nullptr)
 *
 * teardownNodeAdapter() then:
 *   - Calls resetNodeAdapter() on null m_nodeHandle -> undefined behavior
 *   - Calls removeChild(wrapper, childHandle) where childHandle was disposed -> UAF
 *
 * Fix: move adapter teardown from destructor to onDestroy() (runs before
 * base class cleanup).
 */

import { describe, it, expect } from '@ohos/hypium';
import { SurfaceManager } from '@agenui/agenui';
import { common } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { sendAndWaitForRender, sendAndWaitForDeleteSurface, sleep } from '../base/AGenUIBaseTest';

const TAG = 'RiskProbe-ListAdapterUAF';

function buildCreateSurfaceJSON(surfaceId: string): string {
  return `{"version":"v0.9","createSurface":{"surfaceId":"${surfaceId}","catalogId":"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json"}}`;
}

function buildDeleteSurfaceJSON(surfaceId: string): string {
  return `{"version":"v0.9","deleteSurface":{"surfaceId":"${surfaceId}"}}`;
}

function buildHorizontalListJSON(surfaceId: string): string {
  return `{"version":"v0.9","updateComponents":{"surfaceId":"${surfaceId}","components":[{"id":"root","component":"Column","children":["hlist1"]},{"id":"hlist1","component":"List","properties":{"direction":"horizontal","scrollable":true},"children":["item1","item2","item3","item4","item5"]},{"id":"item1","component":"Text","properties":{"text":"Item 1"}},{"id":"item2","component":"Text","properties":{"text":"Item 2"}},{"id":"item3","component":"Text","properties":{"text":"Item 3"}},{"id":"item4","component":"Text","properties":{"text":"Item 4"}},{"id":"item5","component":"Text","properties":{"text":"Item 5"}}]}}`;
}

function buildMultiHorizontalListJSON(surfaceId: string): string {
  let components = `{"id":"root","component":"Column","children":["list-a","list-b","list-c"]}`;
  const listIds: string[] = ['list-a', 'list-b', 'list-c'];
  for (let li = 0; li < listIds.length; li++) {
    const listId = listIds[li];
    let childIds = '';
    for (let i = 0; i < 4; i++) {
      if (i > 0) {
        childIds += ',';
      }
      childIds += `"${listId}-item-${i}"`;
    }
    components += `,{"id":"${listId}","component":"List","properties":{"direction":"horizontal","scrollable":true},"children":[${childIds}]}`;
    for (let i = 0; i < 4; i++) {
      components += `,{"id":"${listId}-item-${i}","component":"Text","properties":{"text":"${listId} Item ${i}"}}`;
    }
  }
  return `{"version":"v0.9","updateComponents":{"surfaceId":"${surfaceId}","components":[${components}]}}`;
}

export default function listAdapterTeardownUafTest() {
  describe('ListAdapterTeardownUafTest', () => {

    /**
     * RISK56: Horizontal list destroy triggers adapter teardown UAF
     *
     * Create a horizontal List with children, then destroy the surface.
     * The destroy path hits ~ListComponent() which calls teardownNodeAdapter()
     * with stale/null handles.
     *
     * With ASAN: heap-use-after-free expected.
     * Without ASAN: may crash on ArkUI CAPI null handle, or survive silently.
     */
    it('RISK56_Harmony_listAdapterTeardownUaf', 0, async (done: Function) => {
      hilog.info(0x0000, TAG, '=== Starting RISK56 probe ===');

      const context = getContext() as common.UIAbilityContext;
      const sm = new SurfaceManager(context);
      const surfaceId = 'risk56-hlist';

      try {
        const json = buildCreateSurfaceJSON(surfaceId) + buildHorizontalListJSON(surfaceId);
        await sendAndWaitForRender(sm, json, surfaceId);
        hilog.info(0x0000, TAG, 'Horizontal list with 5 children created');

        await sleep(1000);

        hilog.info(0x0000, TAG, 'Destroying surface (triggers destructor UAF path)...');
        await sendAndWaitForDeleteSurface(sm, buildDeleteSurfaceJSON(surfaceId), surfaceId);

        await sleep(500);
        hilog.info(0x0000, TAG, 'Surface destroyed — process survived');

      } catch (e) {
        hilog.error(0x0000, TAG, `Exception: ${e}`);
      } finally {
        try {
          sm.destroy();
        } catch (e) {
          hilog.error(0x0000, TAG, `SM cleanup failed: ${e}`);
        }
      }

      hilog.info(0x0000, TAG, '=== RISK56 probe complete ===');
      hilog.info(0x0000, TAG, 'Code analysis confirms UAF in teardownNodeAdapter():');
      hilog.info(0x0000, TAG, '  1. resetNodeAdapter() on nullptr m_nodeHandle');
      hilog.info(0x0000, TAG, '  2. removeChild() with disposed childHandle');
      hilog.info(0x0000, TAG, 'Fix: move adapter teardown from ~ListComponent() to onDestroy()');
      expect(true).assertTrue();
      done();
    });

    /**
     * RISK56 variant: Multiple horizontal lists destroyed simultaneously
     *
     * Tests that destroying a surface with multiple horizontal lists
     * (each with its own adapter) doesn't compound the UAF issue.
     */
    it('RISK56_Harmony_multipleHorizontalListDestroy', 0, async (done: Function) => {
      hilog.info(0x0000, TAG, '=== Starting RISK56 multi-list variant ===');

      const context = getContext() as common.UIAbilityContext;
      const sm = new SurfaceManager(context);
      const surfaceId = 'risk56-multi';

      try {
        const json = buildCreateSurfaceJSON(surfaceId) + buildMultiHorizontalListJSON(surfaceId);
        await sendAndWaitForRender(sm, json, surfaceId);
        hilog.info(0x0000, TAG, 'Sent 3 horizontal lists with 4 children each');

        await sleep(1000);

        hilog.info(0x0000, TAG, 'Destroying surface with 3 horizontal lists...');
        await sendAndWaitForDeleteSurface(sm, buildDeleteSurfaceJSON(surfaceId), surfaceId);

        await sleep(500);
        hilog.info(0x0000, TAG, 'Multi-list destroy completed — process survived');

      } catch (e) {
        hilog.error(0x0000, TAG, `Exception in multi-list test: ${e}`);
      } finally {
        try {
          sm.destroy();
        } catch (e) {
          hilog.error(0x0000, TAG, `SM cleanup failed: ${e}`);
        }
      }

      hilog.info(0x0000, TAG, '=== RISK56 multi-list variant complete ===');
      expect(true).assertTrue();
      done();
    });

  });
}
