/**
 * InitializationTest.ets
 *
 * AGenUI Harmony SDK 初始化集成测试
 *
 * 测试用例：
 *   INIT-01  SDK 版本号非空
 *   INIT-02  重复创建 SurfaceManager 不崩溃
 *   INIT-03  SurfaceManager 创建成功（createSurface 触发回调）
 */

import { describe, it, expect, beforeEach, afterEach } from '@ohos/hypium';
import { common } from '@kit.AbilityKit';
import { SurfaceManager, AGenUI } from '@agenui/agenui';
import {
  sendAndWaitForSurface,
  TIMEOUT_MS
} from '../base/AGenUIBaseTest';
import { loadFixture, getMessagesAsString, getSurfaceId } from '../base/TestFixtureLoader';

export default function initializationTest() {
  describe('InitializationTest', () => {
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
     * INIT-01: SDK 版本号非空
     * 验证 AGenUI.getVersion() 返回非空字符串。
     */
    it('INIT-01_SDK_version_is_not_empty', 0, () => {
      const version = AGenUI.getVersion();
      expect(version).not().assertNull();
      expect(version.length).assertLarger(0);
    });

    /**
     * INIT-02: 重复创建 SurfaceManager 不崩溃
     * 验证多次创建 SurfaceManager 不会引发崩溃或异常。
     */
    it('INIT-02_multiple_SurfaceManager_no_crash', 0, () => {
      const sm2 = new SurfaceManager(context);
      const sm3 = new SurfaceManager(context);
      // 若无异常则测试通过
      expect(sm2).not().assertNull();
      expect(sm3).not().assertNull();
      sm2.destroy();
      sm3.destroy();
    });

    /**
     * INIT-03: 创建 Surface 成功（回调触发）
     * 发送 createSurface JSON，验证 onCreateSurface 回调被调用。
     */
    it('INIT-03_createSurface_callback_triggered', 0, async (done: Function) => {
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
  });
}
