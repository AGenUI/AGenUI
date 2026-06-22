/**
 * StreamTest.ets
 *
 * AGenUI Harmony 流式数据集成测试
 *
 * 测试用例：
 *   STREAM-01  小 chunk 流式（chunkSize=10）
 *   STREAM-02  大 chunk 流式（chunkSize=500）
 *   STREAM-03  单字符流式（chunkSize=1）
 *   STREAM-04  一次性发送（chunkSize=-1）
 *   STREAM-05  嵌套布局 + 小 chunk
 *   STREAM-06  含 DataModel 的完整三消息流式（chunkSize=20）
 *   STREAM-07  静态分片场景（在 JSON key 边界处截断）
 *   STREAM-08  中途 reset（beginTextStream 重置后重新发送）
 */

import { describe, it, expect, beforeEach, afterEach } from '@ohos/hypium';
import { common } from '@kit.AbilityKit';
import { SurfaceManager, ISurfaceManagerListener, Surface } from '@agenui/agenui';
import {
  streamAndWaitForRender,
  streamJson,
  sendAndWaitForRender,
  sleep,
  TIMEOUT_MS
} from '../base/AGenUIBaseTest';
import { loadFixture, getMessagesAsString, getSurfaceId, getExpect, readRawText } from '../base/TestFixtureLoader';

/**
 * 等待 Surface 创建（用于 reset 场景）
 */
function waitForSurface(sm: SurfaceManager, surfaceId: string): Promise<Surface> {
  return new Promise<Surface>((resolve, reject) => {
    let resolved = false;
    const timer = setTimeout(() => {
      if (!resolved) {
        resolved = true;
        sm.removeListener(listener);
        reject(new Error(`等待 Surface 超时：${surfaceId}`));
      }
    }, TIMEOUT_MS);
    const listener: ISurfaceManagerListener = {
      onCreateSurface: (surface: Surface) => {
        if (surface.surfaceId === surfaceId && !resolved) {
          resolved = true;
          clearTimeout(timer);
          sm.removeListener(listener);
          setTimeout(() => resolve(surface), 100);
        }
      }
    };
    sm.addListener(listener);
  });
}

export default function streamTest() {
  describe('StreamTest', () => {
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
     * STREAM-01: 小 chunk 流式（chunkSize=10）
     * 将 01_button_simple.json 以每 10 字符分片发送，验证 Surface 创建成功。
     */
    it('STREAM-01_small_chunk_10', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(
          context.resourceManager, 'stream/cases/01_button_simple.json'
        );
        const surfaceId = getSurfaceId(fixture);
        const fullJson = getMessagesAsString(fixture);

        const surface = await streamAndWaitForRender(sm, fullJson, surfaceId, 10);
        expect(surface).not().assertNull();
        expect(surface.surfaceId).assertEqual(surfaceId);
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * STREAM-02: 大 chunk 流式（chunkSize=500）
     */
    it('STREAM-02_large_chunk_500', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(
          context.resourceManager, 'stream/cases/01_button_simple.json'
        );
        const surfaceId = getSurfaceId(fixture);
        const fullJson = getMessagesAsString(fixture);

        const surface = await streamAndWaitForRender(sm, fullJson, surfaceId, 500);
        expect(surface).not().assertNull();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * STREAM-03: 单字符流式（chunkSize=1）
     * 极端场景：每次只发 1 个字符。
     */
    it('STREAM-03_single_char_chunk_1', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(
          context.resourceManager, 'stream/cases/01_button_simple.json'
        );
        const surfaceId = getSurfaceId(fixture);
        const fullJson = getMessagesAsString(fixture);

        const surface = await streamAndWaitForRender(sm, fullJson, surfaceId, 1);
        expect(surface).not().assertNull();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * STREAM-04: 一次性发送（chunkSize=-1 即整体不分片）
     */
    it('STREAM-04_full_json_no_split', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(
          context.resourceManager, 'stream/cases/01_button_simple.json'
        );
        const surfaceId = getSurfaceId(fixture);
        const fullJson = getMessagesAsString(fixture);

        const surface = await streamAndWaitForRender(sm, fullJson, surfaceId, -1);
        expect(surface).not().assertNull();
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * STREAM-05: 嵌套布局 + 小 chunk（chunkSize=10）
     * 使用 02_nested_layout.json 验证复杂结构在分片传输后完整性。
     */
    it('STREAM-05_nested_layout_small_chunk', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(
          context.resourceManager, 'stream/cases/02_nested_layout.json'
        );
        const surfaceId = getSurfaceId(fixture);
        const fullJson = getMessagesAsString(fixture);

        const surface = await streamAndWaitForRender(sm, fullJson, surfaceId, 10);
        expect(surface).not().assertNull();
        expect(surface.surfaceId).assertEqual(surfaceId);
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * STREAM-06: 含 DataModel 的完整三消息流式（chunkSize=20）
     * 使用 03_with_datamodel.json（payload 含 createSurface + updateComponents + updateDataModel），
     * 验证流式传输 DataModel 场景下 Surface 创建成功。
     */
    it('STREAM-06_dataModel_small_chunk', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(
          context.resourceManager, 'stream/cases/03_with_datamodel.json'
        );
        const surfaceId = getSurfaceId(fixture);
        const fullJson = getMessagesAsString(fixture);

        const surface = await streamAndWaitForRender(sm, fullJson, surfaceId, 20);
        expect(surface).not().assertNull();
        expect(surface.surfaceId).assertEqual(surfaceId);
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * STREAM-07: 静态分片场景（在 JSON key 边界处截断）
     * 使用预先手动切分的 chunk_01.txt / chunk_02.txt，
     * 验证在 JSON key 名称中间截断后重新拼接仍能正确解析。
     */
    it('STREAM-07_static_chunks_key_boundary', 0, async (done: Function) => {
      try {
        const meta = await loadFixture(
          context.resourceManager,
          'stream/static_chunks/01_split_at_key_boundary/meta.json'
        );
        const surfaceId = getSurfaceId(meta);

        const chunk1 = await readRawText(
          context.resourceManager,
          'stream/static_chunks/01_split_at_key_boundary/chunk_01.txt'
        );
        const chunk2 = await readRawText(
          context.resourceManager,
          'stream/static_chunks/01_split_at_key_boundary/chunk_02.txt'
        );

        // 监听 Surface 创建
        const waitPromise = waitForSurface(sm, surfaceId);

        // 按预定义分片顺序发送
        sm.beginTextStream();
        sm.receiveTextChunk(chunk1);
        sm.receiveTextChunk(chunk2);
        sm.endTextStream();

        const surface = await waitPromise;
        expect(surface).not().assertNull();
        expect(surface.surfaceId).assertEqual(surfaceId);

        // 验证预期组件存在（ArkTS Surface 通过 getComponent 懒加载，逐个验证）
        const expectedIds = getExpect(meta, 'componentIds') as string[];
        if (expectedIds && expectedIds.length > 0) {
          for (const cid of expectedIds) {
            const comp = surface.getComponent(cid);
            expect(comp).not().assertNull();
          }
        }
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });

    /**
     * STREAM-08: 中途 reset 场景
     * 发送 50 字节后调用 beginTextStream()（自动 reset），
     * 然后重新完整发送，验证最终 Surface 正确创建。
     */
    it('STREAM-08_midway_reset_and_resend', 0, async (done: Function) => {
      try {
        const fixture = await loadFixture(
          context.resourceManager, 'stream/cases/02_nested_layout.json'
        );
        const surfaceId = getSurfaceId(fixture);
        const fullJson = getMessagesAsString(fixture);

        // 先监听 Surface 创建
        const waitPromise = waitForSurface(sm, surfaceId);

        // 发送一半数据后 reset（只发前 50 字节）
        sm.beginTextStream();
        sm.receiveTextChunk(fullJson.substring(0, 50));
        // reset：再次调用 beginTextStream() 相当于中止上次流
        // 然后完整重新发送
        streamJson(sm, fullJson, 20);

        const surface = await waitPromise;
        expect(surface).not().assertNull();
        expect(surface.surfaceId).assertEqual(surfaceId);
        done();
      } catch (e) {
        expect().assertFail();
        done();
      }
    });
  });
}
