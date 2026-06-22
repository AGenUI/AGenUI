/**
 * TestFixtureLoader.ets
 *
 * Harmony 集成测试数据加载工具
 *
 * 从 rawfile 资源目录读取 test_fixtures 下的 JSON 文件。
 * Harmony ohosTest 中通过 getContext().resourceManager 访问 rawfile。
 *
 * 目录结构（rawfile/test_fixtures/）：
 *   init/create_surface_simple.json
 *   multi_surface/surface_a.json
 *   multi_surface/surface_b.json
 *   components/01_text_only.json
 *   components/02_button_with_action.json
 *   components/03_nested_column.json
 *   components/04_card_complex.json
 *   components/05_modal_with_trigger.json
 *   function_call/action_toast.json
 *   function_call/action_formatString.json
 *   function_call/action_formatDate.json
 *   stream/cases/01_button_simple.json
 *   stream/cases/02_nested_layout.json
 *   stream/cases/03_with_datamodel.json
 *   stream/static_chunks/01_split_at_key_boundary/meta.json
 *   stream/static_chunks/01_split_at_key_boundary/chunk_01.txt
 *   stream/static_chunks/01_split_at_key_boundary/chunk_02.txt
 */

import resourceManager from '@ohos.resourceManager';
import util from '@ohos.util';

/** 测试 Fixture JSON 根对象 */
export interface FixtureData {
  description?: string;
  surfaceId?: string;
  messages?: object[];
  payload?: object[];
  expect?: Record<string, Object>;
  streamCases?: StreamCase[];
  skillName?: string;
  type?: string;
}

/** 流式测试用例描述 */
export interface StreamCase {
  id: string;
  description: string;
  chunkSize: number;
}

/**
 * 从 rawfile 中读取 test_fixtures JSON 文件
 *
 * @param resMgr  ResourceManager（从 getContext().resourceManager 获取）
 * @param relPath test_fixtures 下的相对路径，例如 "init/create_surface_simple.json"
 * @returns 解析后的 FixtureData 对象
 * @throws 文件不存在或解析失败时抛出错误
 */
export async function loadFixture(
  resMgr: resourceManager.ResourceManager,
  relPath: string
): Promise<FixtureData> {
  const rawPath = `test_fixtures/${relPath}`;
  const bytes = await resMgr.getRawFileContent(rawPath);
  const text = new util.TextDecoder('utf-8').decodeWithStream(bytes);
  return JSON.parse(text) as FixtureData;
}

/**
 * 将 fixture 的 messages 数组序列化为单条 JSON 字符串（逐帧合并发送）
 *
 * 每条消息包装为独立字符串并换行拼接（与 A2UI 流式协议格式一致）。
 * 也可以用 getMessagesAsString 一次性生成整体字符串。
 *
 * @param fixture 已加载的 FixtureData
 * @returns 每条消息序列化后的字符串数组
 */
export function getMessagesAsStrings(fixture: FixtureData): string[] {
  const msgs = fixture.messages ?? fixture.payload ?? [];
  return msgs.map(m => JSON.stringify(m));
}

/**
 * 将 fixture 的所有消息拼接为单个 JSON 字符串（用于单次 receiveTextChunk）
 *
 * @param fixture 已加载的 FixtureData
 * @returns 拼接后的 JSON 字符串
 */
export function getMessagesAsString(fixture: FixtureData): string {
  return getMessagesAsStrings(fixture).join('');
}

/**
 * 获取 fixture 期望的 surfaceId
 *
 * @param fixture 已加载的 FixtureData
 * @returns surfaceId 字符串
 * @throws fixture.surfaceId 未定义时抛出错误
 */
export function getSurfaceId(fixture: FixtureData): string {
  if (!fixture.surfaceId) {
    throw new Error('fixture.surfaceId is undefined');
  }
  return fixture.surfaceId;
}

/**
 * 获取 fixture 期望字段
 *
 * @param fixture 已加载的 FixtureData
 * @param key     期望字段的 key
 * @returns 对应值
 */
export function getExpect(fixture: FixtureData, key: string): Object | undefined {
  return fixture.expect?.[key];
}

/**
 * 从 rawfile 中读取 test_fixtures 下的任意文本文件（不做 JSON 解析）
 *
 * @param resMgr  ResourceManager
 * @param relPath test_fixtures 下的相对路径，例如 "stream/static_chunks/01_split_at_key_boundary/chunk_01.txt"
 * @returns 文件原始文本内容
 */
export async function readRawText(
  resMgr: resourceManager.ResourceManager,
  relPath: string
): Promise<string> {
  const rawPath = `test_fixtures/${relPath}`;
  const bytes = await resMgr.getRawFileContent(rawPath);
  return new util.TextDecoder('utf-8').decodeWithStream(bytes);
}

/**
 * 将字符串分割为指定大小的 chunk 数组
 *
 * @param text      原始字符串
 * @param chunkSize 每块字符数（<= 0 表示整体不分片）
 * @returns chunk 字符串数组
 */
export function splitIntoChunks(text: string, chunkSize: number): string[] {
  if (chunkSize <= 0) {
    return [text];
  }
  const chunks: string[] = [];
  let i = 0;
  while (i < text.length) {
    chunks.push(text.substring(i, Math.min(i + chunkSize, text.length)));
    i += chunkSize;
  }
  return chunks;
}


