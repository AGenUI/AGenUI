/**
 * AGenUI Engine SurfaceManager Bridge (multi-instance)
 *
 * Each SurfaceManager owns one bridge instance with its own
 * C++ ISurfaceManager. The bridge handles per-instance streaming data,
 * surface event listeners, and interaction callbacks.
 *
 * Usage:
 * 1. new AGenUIEngineSurfaceManagerBridge() creates a C++ ISurfaceManager automatically.
 * 2. Manage native listeners with addSurfaceListener() and removeSurfaceListener().
 * 3. Call teardown() when the instance is no longer needed.
 */

import { ISurfaceListener,
  registerA2UISurfaceListener,
  unregisterA2UISurfaceListener,
  submitUIAction,
  submitUIDataModel,
} from 'liba2ui-capi.so';

import internal_a2ui from 'liba2ui-capi.so';

import { AGenUIEngineBridge } from './AGenUIEngineBridge';
import { hilog } from '@kit.PerformanceAnalysisKit';

const nativeBeginTextStream: Function = (internal_a2ui as Record<string, Function>).beginTextStream;
const nativeEndTextStream: Function = (internal_a2ui as Record<string, Function>).endTextStream;
const nativeReceiveTextChunk: Function = (internal_a2ui as Record<string, Function>).receiveTextChunk;

/**
 * AGenUI per-instance SurfaceManager bridge.
 *
 * - Owns an independent C++ ISurfaceManager identified by instanceId
 * - Handles beginTextStream / receiveTextChunk / endTextStream
 * - Forwards triggerAction / syncState callbacks
 * - Manages native surface listeners
 */
export class AGenUIEngineSurfaceManagerBridge {

  /** C++ ISurfaceManager identifier. */
  public readonly instanceId: number;

  /** Registered native listeners removed during teardown. */
  private _registeredListeners: ISurfaceListener[] = [];

  /**
   * Creates the C++ ISurfaceManager during construction.
   * @throws Error if the native layer fails to create a SurfaceManager (instanceId === 0).
   */
  constructor() {
    const instanceId = AGenUIEngineBridge.getInstance().createSurfaceManager();
    if (instanceId === 0) {
      hilog.error(0x0000, 'SurfaceManager', 'AGenUIEngineSurfaceManagerBridge: Failed to create SurfaceManager, native returned instanceId=0');
      throw new Error('AGenUIEngineSurfaceManagerBridge: Failed to create SurfaceManager, native returned instanceId=0');
    }
    this.instanceId = instanceId;
  }

  /**
   * Tears down listeners and destroys the C++ ISurfaceManager.
   */
  teardown(): void {
    for (const listener of this._registeredListeners) {
      unregisterA2UISurfaceListener(this.instanceId, listener);
    }
    this._registeredListeners = [];
    AGenUIEngineBridge.getInstance().destroySurfaceManager(this.instanceId);
  }

  // MARK: - Data Transport

  /**
   * Begins a streaming data session
   */
  beginTextStream(): void {
    nativeBeginTextStream(this.instanceId);
  }

  /**
   * Ends a streaming data session
   */
  endTextStream(): void {
    nativeEndTextStream(this.instanceId);
  }

  /**
   * Receives an A2UI protocol chunk
   */
  receiveTextChunk(data: string): void {
    nativeReceiveTextChunk(this.instanceId, data);
  }

  /**
   * Dispatches an interaction event.
   */
  triggerAction(surfaceId: string, componentId: string, contextJson: string | null): void {
    submitUIAction(this.instanceId, surfaceId, componentId, contextJson ?? '');
  }

  /**
   * Synchronizes UI state to the data model.
   */
  syncState(surfaceId: string, componentId: string, contextJson: string | null): void {
    submitUIDataModel(this.instanceId, surfaceId, componentId, contextJson ?? '');
  }

  // MARK: - Native Listener Management

  /**
   * Registers a native surface listener.
   */
  addSurfaceListener(listener: ISurfaceListener): void {
    this._registeredListeners.push(listener);
    registerA2UISurfaceListener(this.instanceId, listener);
  }

  /**
   * Unregisters a native surface listener.
   */
  removeSurfaceListener(listener: ISurfaceListener): void {
    const idx = this._registeredListeners.indexOf(listener);
    if (idx >= 0) {
      this._registeredListeners.splice(idx, 1);
    }
    unregisterA2UISurfaceListener(this.instanceId, listener);
  }
}
