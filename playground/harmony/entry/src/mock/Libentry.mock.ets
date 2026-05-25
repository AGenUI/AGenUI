/**
 * Mock implementation of liba2ui-capi.so for ohosTest.
 *
 * Provides stub exports that mirror the real native library so that the
 * @agenui/agenui SDK modules can be loaded in the test harness without
 * requiring the C++ engine binary.
 *
 * Key mock behaviors:
 * - registerA2UISurfaceListener: stores listeners per engineId
 * - beginTextStream / receiveTextChunk / endTextStream: accumulates data
 *   and dispatches onCreateSurface / onDeleteSurface callbacks by parsing
 *   A2UI protocol messages from the buffer.
 */

// ---- Interfaces ----

interface ISurfaceListener {
  onCreateSurface(surfaceId: string, messageId: string, rawProtocolContent: string): void;
  onDeleteSurface(surfaceId: string): void;
  onActionEventRouted?: (content: string) => void;
}

interface MockCreateSurface {
  surfaceId: string;
}

interface MockDeleteSurface {
  surfaceId: string;
}

interface MockA2UIMessage {
  createSurface?: MockCreateSurface;
  deleteSurface?: MockDeleteSurface;
}

// ---- Module-level mock state ----

let engineIdCounter: number = 0;

/** Registered listeners per engineId */
const mockListeners: Map<number, ISurfaceListener[]> = new Map();

/** Accumulated text buffer per engineId */
const mockBuffers: Map<number, string> = new Map();

// ---- Helpers ----

/**
 * Extracts top-level JSON object strings from a concatenated buffer.
 * Handles strings with escaped characters correctly.
 */
function extractJsonStrings(buffer: string): string[] {
  const results: string[] = [];
  let depth = 0;
  let start = -1;
  let inString = false;
  let escaped = false;

  for (let i = 0; i < buffer.length; i++) {
    const ch = buffer.charAt(i);

    if (escaped) {
      escaped = false;
      continue;
    }

    if (ch === '\\' && inString) {
      escaped = true;
      continue;
    }

    if (ch === '"') {
      inString = !inString;
      continue;
    }

    if (inString) {
      continue;
    }

    if (ch === '{') {
      if (depth === 0) {
        start = i;
      }
      depth++;
    } else if (ch === '}') {
      depth--;
      if (depth === 0 && start >= 0) {
        results.push(buffer.substring(start, i + 1));
        start = -1;
      }
    }
  }
  return results;
}

/**
 * Dispatches mock callbacks by parsing A2UI protocol messages.
 */
function dispatchMockMessages(engineId: number, buffer: string): void {
  const listeners = mockListeners.get(engineId);
  if (!listeners || listeners.length === 0) {
    return;
  }

  const jsonStrings = extractJsonStrings(buffer);
  for (const jsonStr of jsonStrings) {
    try {
      const msg: MockA2UIMessage = JSON.parse(jsonStr) as MockA2UIMessage;
      if (msg.createSurface && msg.createSurface.surfaceId) {
        const sid = msg.createSurface.surfaceId;
        const snapshot = listeners.slice();
        for (const l of snapshot) {
          l.onCreateSurface(sid, '', jsonStr);
        }
      }
      if (msg.deleteSurface && msg.deleteSurface.surfaceId) {
        const sid = msg.deleteSurface.surfaceId;
        const snapshot = listeners.slice();
        for (const l of snapshot) {
          l.onDeleteSurface(sid);
        }
      }
    } catch (_e) {
      // skip unparseable fragments
    }
  }
}

// ---- Named exports (used by SDK via `import { xxx } from 'liba2ui-capi.so'`) ----

export function start(): void {}

export function stop(): void {}

export function setMinLogLevel(level: number): void {}

export function registerDefaultTheme(theme: string, designToken: string): boolean {
  return true;
}

export function setDayNightMode(mode: string): void {}

export function registerComponent(type: string, creator: Function): void {}

export function getVersion(): string {
  return '0.0.0-mock';
}

export function createSurfaceManager(): number {
  engineIdCounter++;
  return engineIdCounter;
}

export function destroySurfaceManager(engineId: number): void {
  mockListeners.delete(engineId);
  mockBuffers.delete(engineId);
}

export function sendMockData(mockData: string): void {}

export function removeEventListener(listener: Object): void {}

export function requestSurface(engineId: number, requestContent: string): void {}

export function registerA2UISurfaceListener(engineId: number, listener: ISurfaceListener): void {
  let listeners = mockListeners.get(engineId);
  if (!listeners) {
    listeners = [];
    mockListeners.set(engineId, listeners);
  }
  listeners.push(listener);
}

export function unregisterA2UISurfaceListener(engineId: number, listener: ISurfaceListener): void {
  const listeners = mockListeners.get(engineId);
  if (listeners) {
    const idx = listeners.indexOf(listener);
    if (idx >= 0) {
      listeners.splice(idx, 1);
    }
  }
}

export function bindSurface(engineId: number, surfaceId: string, nodeContent: Object): boolean {
  return true;
}

export function unbindSurface(engineId: number, surfaceId: string): boolean {
  return true;
}

export function clearA2UiContainer(engineId: number): void {}

export function registerOpenUrlCallback(callback: Function): void {}

export function registerSkillInvokerCallback(callback: Function): void {}

export function registerEtsFunction(name: string, f: Function): void {}

export function setDeviceInfo(width: number, height: number, density: number): void {}

export function hybridFactoryGetAttribute(ptr: bigint, key: string): string {
  return '';
}

export function hybridFactoryGetPropertiesJson(ptr: bigint): string {
  return '{}';
}

export function markdownUpdateHeight(
  surfaceId: string, nodeId: string, type: string,
  height: number, width: number, ptr: bigint
): void {}

export function reportComponentRenderSize(
  surfaceId: string, nodeId: string, type: string,
  height: number, width: number, ptr: bigint
): void {}

export function onSurfaceSizeChanged(surfaceId: string, width: number, height: number): void {}

export function setThemeConfig(config: string): boolean {
  return true;
}

export function setDesignTokenConfig(config: string): boolean {
  return true;
}

export function registerFunction(name: string, config: string, callback: Function): void {}

export function unregisterFunction(name: string): void {}

export function setThemeMode(mode: string): void {}

export function submitUIAction(
  engineId: number, surfaceId: string,
  sourceComponentId: string, contextJson: string
): void {}

export function submitUIDataModel(
  engineId: number, surfaceId: string,
  componentId: string, change: string
): void {}

export function destroySurface(engineId: number, surfaceId: string): void {}

export function receiveTextChunk(engineId: number, data: string): void {
  const current = mockBuffers.get(engineId) ?? '';
  mockBuffers.set(engineId, current + data);
}

export function beginTextStream(engineId: number): void {
  mockBuffers.set(engineId, '');
}

export function endTextStream(engineId: number): void {
  const buffer = mockBuffers.get(engineId) ?? '';
  mockBuffers.delete(engineId);
  if (buffer.length > 0) {
    dispatchMockMessages(engineId, buffer);
  }
}

export function registerImageLoader(loader: Object): void {}

export function setImagePixelMap(
  requestId: string, buffer: ArrayBuffer,
  width: number, height: number,
  pixelFormat: number, alphaType: number
): void {}

export function onImageLoadFailed(requestId: string, isCancelled: boolean): void {}

interface MeasureResult {
  width: number;
  height: number;
  calcType?: number;
}

export function registerMeasurement(instanceId: number, type: string, callback: Function): void {}

export function unregisterMeasurement(instanceId: number, type: string): void {}

export function findInstanceIdBySurfaceId(surfaceId: string): number {
  return 0;
}

// ---- Default export (used by SDK via `import internal_a2ui from 'liba2ui-capi.so'`) ----

const NativeMock: Record<string, Object> = {
  'start': start as Object,
  'stop': stop as Object,
  'setMinLogLevel': setMinLogLevel as Object,
  'registerDefaultTheme': registerDefaultTheme as Object,
  'setDayNightMode': setDayNightMode as Object,
  'registerComponent': registerComponent as Object,
  'getVersion': getVersion as Object,
  'createSurfaceManager': createSurfaceManager as Object,
  'destroySurfaceManager': destroySurfaceManager as Object,
  'sendMockData': sendMockData as Object,
  'removeEventListener': removeEventListener as Object,
  'requestSurface': requestSurface as Object,
  'registerA2UISurfaceListener': registerA2UISurfaceListener as Object,
  'unregisterA2UISurfaceListener': unregisterA2UISurfaceListener as Object,
  'bindSurface': bindSurface as Object,
  'unbindSurface': unbindSurface as Object,
  'clearA2UiContainer': clearA2UiContainer as Object,
  'registerOpenUrlCallback': registerOpenUrlCallback as Object,
  'registerSkillInvokerCallback': registerSkillInvokerCallback as Object,
  'registerEtsFunction': registerEtsFunction as Object,
  'setDeviceInfo': setDeviceInfo as Object,
  'hybridFactoryGetAttribute': hybridFactoryGetAttribute as Object,
  'hybridFactoryGetPropertiesJson': hybridFactoryGetPropertiesJson as Object,
  'markdownUpdateHeight': markdownUpdateHeight as Object,
  'onSurfaceSizeChanged': onSurfaceSizeChanged as Object,
  'setThemeConfig': setThemeConfig as Object,
  'setDesignTokenConfig': setDesignTokenConfig as Object,
  'registerFunction': registerFunction as Object,
  'unregisterFunction': unregisterFunction as Object,
  'setThemeMode': setThemeMode as Object,
  'submitUIAction': submitUIAction as Object,
  'submitUIDataModel': submitUIDataModel as Object,
  'destroySurface': destroySurface as Object,
  'receiveTextChunk': receiveTextChunk as Object,
  'beginTextStream': beginTextStream as Object,
  'endTextStream': endTextStream as Object,
  'registerImageLoader': registerImageLoader as Object,
  'setImagePixelMap': setImagePixelMap as Object,
  'onImageLoadFailed': onImageLoadFailed as Object,
  'reportComponentRenderSize': reportComponentRenderSize as Object,
  'registerMeasurement': registerMeasurement as Object,
  'unregisterMeasurement': unregisterMeasurement as Object,
  'findInstanceIdBySurfaceId': findInstanceIdBySurfaceId as Object,
};

export default NativeMock;
