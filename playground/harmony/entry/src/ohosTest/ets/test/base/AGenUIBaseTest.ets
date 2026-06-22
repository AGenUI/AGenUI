/**
 * AGenUIBaseTest.ets
 *
 * AGenUI Harmony 集成测试基类
 *
 * 封装通用能力：
 * - SurfaceManager 创建与销毁
 * - 异步等待：sendAndWaitForSurface + waitForNextTick
 * - Surface 销毁等待：sendAndWaitForDeleteSurface
 * - 流式分片发送：streamJson
 *
 * 异步机制说明：
 * receiveTextChunk() 立即返回，但 Surface 创建与组件填充通过
 * ISurfaceManagerListener 回调异步完成。测试必须使用 Promise 等待回调后再断言。
 */

import { describe, beforeEach, afterEach } from '@ohos/hypium';
import { SurfaceManager, ISurfaceManagerListener, Surface } from '@agenui/agenui';
import { common } from '@kit.AbilityKit';

/** 测试超时时间（毫秒） */
export const TIMEOUT_MS = 5000;

/**
 * 等待工具函数：等待指定超时时间
 */
export function sleep(ms: number): Promise<void> {
  return new Promise<void>((resolve) => {
    setTimeout(resolve, ms);
  });
}

/**
 * 发送完整 JSON 并等待指定 surfaceId 的 Surface 创建
 *
 * @param sm - SurfaceManager 实例
 * @param json - A2UI 协议 JSON 字符串
 * @param surfaceId - 期待创建的 surfaceId
 * @returns Surface 实例（超时则抛出错误）
 */
export function sendAndWaitForSurface(
  sm: SurfaceManager,
  json: string,
  surfaceId: string
): Promise<Surface> {
  return new Promise<Surface>((resolve, reject) => {
    let resolved = false;
    const timer = setTimeout(() => {
      if (!resolved) {
        resolved = true;
        sm.removeListener(listener);
        reject(new Error(`Surface 创建超时（surfaceId=${surfaceId}）`));
      }
    }, TIMEOUT_MS);

    const listener: ISurfaceManagerListener = {
      onCreateSurface: (surface: Surface) => {
        if (surface.surfaceId === surfaceId && !resolved) {
          resolved = true;
          clearTimeout(timer);
          sm.removeListener(listener);
          resolve(surface);
        }
      }
    };
    sm.addListener(listener);

    sm.beginTextStream();
    sm.receiveTextChunk(json);
    sm.endTextStream();
  });
}

/**
 * 发送含 deleteSurface 消息的 JSON，并等待 onDeleteSurface 回调
 *
 * @param sm - SurfaceManager 实例
 * @param json - 含 deleteSurface 的 JSON 字符串
 * @param surfaceId - 期待销毁的 surfaceId
 */
export function sendAndWaitForDeleteSurface(
  sm: SurfaceManager,
  json: string,
  surfaceId: string
): Promise<void> {
  return new Promise<void>((resolve, reject) => {
    let resolved = false;
    const timer = setTimeout(() => {
      if (!resolved) {
        resolved = true;
        sm.removeListener(listener);
        reject(new Error(`Surface 销毁超时（surfaceId=${surfaceId}）`));
      }
    }, TIMEOUT_MS);

    const listener: ISurfaceManagerListener = {
      onDeleteSurface: (surface: Surface) => {
        if (surface.surfaceId === surfaceId && !resolved) {
          resolved = true;
          clearTimeout(timer);
          sm.removeListener(listener);
          resolve();
        }
      }
    };
    sm.addListener(listener);

    sm.beginTextStream();
    sm.receiveTextChunk(json);
    sm.endTextStream();
  });
}

/**
 * 组合发送 JSON + 等待 Surface 创建 + 等待下一个 tick（确保组件渲染完成）
 *
 * @param sm - SurfaceManager 实例
 * @param json - A2UI 协议 JSON 字符串
 * @param surfaceId - 期待创建的 surfaceId
 * @returns 已完成组件渲染的 Surface
 */
export async function sendAndWaitForRender(
  sm: SurfaceManager,
  json: string,
  surfaceId: string
): Promise<Surface> {
  const surface = await sendAndWaitForSurface(sm, json, surfaceId);
  // 等待组件渲染完成（给 TaskDispatcher 一些时间处理 onUpdateComponents）
  await sleep(100);
  return surface;
}

/**
 * 将完整 JSON 按 chunkSize 分片发送
 *
 * @param sm - SurfaceManager 实例
 * @param json - 完整 JSON 字符串
 * @param chunkSize - 每片字符数（0 表示整体发送）
 */
export function streamJson(sm: SurfaceManager, json: string, chunkSize: number): void {
  sm.beginTextStream();
  if (chunkSize <= 0) {
    sm.receiveTextChunk(json);
  } else {
    let i = 0;
    while (i < json.length) {
      const end = Math.min(i + chunkSize, json.length);
      sm.receiveTextChunk(json.substring(i, end));
      i = end;
    }
  }
  sm.endTextStream();
}

/**
 * 流式分片发送并等待 Surface 创建
 *
 * @param sm - SurfaceManager 实例
 * @param fullJson - 完整 JSON 字符串
 * @param surfaceId - 期待创建的 surfaceId
 * @param chunkSize - 每片字符数
 * @returns Surface 实例
 */
export function streamAndWaitForRender(
  sm: SurfaceManager,
  fullJson: string,
  surfaceId: string,
  chunkSize: number
): Promise<Surface> {
  return new Promise<Surface>((resolve, reject) => {
    let resolved = false;
    const timer = setTimeout(() => {
      if (!resolved) {
        resolved = true;
        sm.removeListener(listener);
        reject(new Error(`流式 Surface 创建超时（surfaceId=${surfaceId}, chunkSize=${chunkSize}）`));
      }
    }, TIMEOUT_MS);

    const listener: ISurfaceManagerListener = {
      onCreateSurface: (surface: Surface) => {
        if (surface.surfaceId === surfaceId && !resolved) {
          resolved = true;
          clearTimeout(timer);
          sm.removeListener(listener);
          // 等待 100ms 让组件渲染完成
          setTimeout(() => resolve(surface), 100);
        }
      }
    };
    sm.addListener(listener);
    streamJson(sm, fullJson, chunkSize);
  });
}
