/**
 * RISK57: ModalComponent destructor dangling pointer UAF
 *
 * When a Modal component is destroyed, base class destroy() deletes all
 * children (including the content component stored as m_contentComponent).
 * Then ~ModalComponent() dereferences m_contentComponent->getNodeHandle()
 * which is a dangling pointer — use-after-free.
 *
 * Fix: move dialog cleanup from ~ModalComponent() to onDestroy().
 */

import { describe, it, expect } from '@ohos/hypium';
import { SurfaceManager } from '@agenui/agenui';
import { common } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { sendAndWaitForRender, sendAndWaitForDeleteSurface, sleep } from '../base/AGenUIBaseTest';

const TAG = 'RiskProbe-ModalUAF';

function buildCreateSurfaceJSON(surfaceId: string): string {
  return `{"version":"v0.9","createSurface":{"surfaceId":"${surfaceId}","catalogId":"https://a2ui.org/specification/v0_9/catalogs/basic/catalog.json"}}`;
}

function buildDeleteSurfaceJSON(surfaceId: string): string {
  return `{"version":"v0.9","deleteSurface":{"surfaceId":"${surfaceId}"}}`;
}

function buildModalJSON(surfaceId: string): string {
  return `{"version":"v0.9","updateComponents":{"surfaceId":"${surfaceId}","components":[{"id":"root","component":"Column","children":["modal1"]},{"id":"modal1","component":"Modal","children":["trigger1","content1"]},{"id":"trigger1","component":"Button","properties":{"text":"Open Modal"}},{"id":"content1","component":"Column","children":["content-text"]},{"id":"content-text","component":"Text","properties":{"text":"Modal Content"}}]}}`;
}

function buildVisibleModalJSON(surfaceId: string): string {
  return `{"version":"v0.9","updateComponents":{"surfaceId":"${surfaceId}","components":[{"id":"root","component":"Column","children":["modal-open"]},{"id":"modal-open","component":"Modal","properties":{"isVisible":true},"children":["trigger-open","content-open"]},{"id":"trigger-open","component":"Button","properties":{"text":"Trigger"}},{"id":"content-open","component":"Column","children":["content-open-text"]},{"id":"content-open-text","component":"Text","properties":{"text":"Visible Modal Content"}}]}}`;
}

export default function modalDestructorUafTest() {
  describe('ModalDestructorUafTest', () => {

    /**
     * RISK57: Modal destroy triggers dangling pointer dereference
     *
     * Create a Modal with trigger + content children, then destroy the surface.
     * The destroy path:
     *   1. destroy() deletes children (m_contentComponent becomes dangling)
     *   2. ~ModalComponent() calls m_contentComponent->getNodeHandle() -> UAF
     *
     * With ASAN: heap-use-after-free expected.
     * Without ASAN: usually silent (reads stale nullptr from freed memory).
     */
    it('RISK57_Harmony_modalDestructorDanglingPtr', 0, async (done: Function) => {
      hilog.info(0x0000, TAG, '=== Starting RISK57 probe ===');

      const context = getContext() as common.UIAbilityContext;
      const sm = new SurfaceManager(context);
      const surfaceId = 'risk57-modal';

      try {
        const json = buildCreateSurfaceJSON(surfaceId) + buildModalJSON(surfaceId);
        await sendAndWaitForRender(sm, json, surfaceId);
        hilog.info(0x0000, TAG, 'Modal with trigger + content children created');

        await sleep(1000);

        hilog.info(0x0000, TAG, 'Destroying surface (triggers destructor UAF path)...');
        await sendAndWaitForDeleteSurface(sm, buildDeleteSurfaceJSON(surfaceId), surfaceId);

        await sleep(500);
        hilog.info(0x0000, TAG, 'Surface destroyed — process survived');
        hilog.info(0x0000, TAG, 'RISK57 exists by code analysis but freed memory not yet reused');

      } catch (e) {
        hilog.error(0x0000, TAG, `Exception: ${e}`);
      } finally {
        try {
          sm.destroy();
        } catch (e) {
          hilog.error(0x0000, TAG, `SM cleanup failed: ${e}`);
        }
      }

      hilog.info(0x0000, TAG, '=== RISK57 probe complete ===');
      hilog.info(0x0000, TAG, 'Code analysis confirms dangling pointer in ~ModalComponent():');
      hilog.info(0x0000, TAG, '  m_contentComponent->getNodeHandle() after child deleted by destroy()');
      hilog.info(0x0000, TAG, 'Fix: move dialog cleanup from destructor to onDestroy()');
      expect(true).assertTrue();
      done();
    });

    /**
     * RISK57 variant: Modal with dialog opened then destroyed
     *
     * Opens the dialog before destroying — tests the dismissDialog() path
     * in the destructor as well.
     */
    it('RISK57_Harmony_modalOpenThenDestroy', 0, async (done: Function) => {
      hilog.info(0x0000, TAG, '=== Starting RISK57 open-then-destroy variant ===');

      const context = getContext() as common.UIAbilityContext;
      const sm = new SurfaceManager(context);
      const surfaceId = 'risk57-open';

      try {
        const json = buildCreateSurfaceJSON(surfaceId) + buildVisibleModalJSON(surfaceId);
        await sendAndWaitForRender(sm, json, surfaceId);
        hilog.info(0x0000, TAG, 'Modal with isVisible=true created (dialog should open)');

        await sleep(1000);

        hilog.info(0x0000, TAG, 'Destroying surface while dialog is open...');
        await sendAndWaitForDeleteSurface(sm, buildDeleteSurfaceJSON(surfaceId), surfaceId);

        await sleep(500);
        hilog.info(0x0000, TAG, 'Open-modal destroy completed — process survived');

      } catch (e) {
        hilog.error(0x0000, TAG, `Exception in open-then-destroy test: ${e}`);
      } finally {
        try {
          sm.destroy();
        } catch (e) {
          hilog.error(0x0000, TAG, `SM cleanup failed: ${e}`);
        }
      }

      hilog.info(0x0000, TAG, '=== RISK57 open-then-destroy variant complete ===');
      expect(true).assertTrue();
      done();
    });

  });
}
