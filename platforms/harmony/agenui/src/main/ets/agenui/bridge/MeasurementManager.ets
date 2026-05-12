/**
 * MeasurementManager.ets
 *
 * ETS-side measurement registration manager.
 *
 * Usage:
 *   1. Create an instance after Surface initialization (after obtaining engineId)
 *   2. Call register(type, handler) to register component measurement implementations
 *   3. Call dispose() to unregister all registrations when the Surface is destroyed
 *
 * Measurement callback signature:
 *   (paramJson: string,
 *    widthMode: number, maxWidth: number,
 *    heightMode: number, maxHeight: number) => { width: number; height: number }
 */

import {
  registerMeasurement as nativeRegisterMeasurement,
  unregisterMeasurement as nativeUnregisterMeasurement,
  MeasureResult,
} from 'liba2ui-capi.so';

/**
 * Component measurement callback type
 *
 * @param paramJson    Component attribute JSON string (contains component/attrs/styles)
 * @param widthMode    Width mode (0=Undefined 1=Exactly 2=AtMost)
 * @param maxWidth     Maximum available width (vp)
 * @param heightMode   Height mode
 * @param maxHeight    Maximum available height (vp)
 * @returns            Measurement result { width, height, calcType? } (unit: vp)
 *                     calcType: 0=Sync (default) 1=Async (async components return {0,0} first, then markDirty after loading)
 */
export type MeasurementHandler = (
  paramJson: string,
  widthMode: number,
  maxWidth: number,
  heightMode: number,
  maxHeight: number
) => MeasureResult;

/**
 * ETS-side measurement manager
 *
 * Lifecycle is tied to SurfaceManager (one instance per engineId).
 */
export class MeasurementManager {
  private readonly _engineId: number;
  private readonly _registeredTypes: Set<string> = new Set();

  constructor(engineId: number) {
    this._engineId = engineId;
  }

  /**
   * Register measurement callback for a specified component type
   *
   * @param type    Component type string (e.g. "Text", "Image", "Table")
   * @param handler Measurement callback function
   */
  register(type: string, handler: MeasurementHandler): void {
    // Adapt handler to the format expected by NAPI side (pass through parameters directly)
    const wrappedHandler = (
      paramJson: string,
      widthMode: number,
      maxWidth: number,
      heightMode: number,
      maxHeight: number
    ): MeasureResult => {
      try {
        return handler(paramJson, widthMode, maxWidth, heightMode, maxHeight);
      } catch (e) {
        console.error(`[MeasurementManager] measure error for type=${type}: ${e}`);
        const fallback: MeasureResult = { width: 0, height: 0 };
        return fallback;
      }
    };

    nativeRegisterMeasurement(this._engineId, type, wrappedHandler);
    this._registeredTypes.add(type);
    console.info(`[MeasurementManager] registered: engineId=${this._engineId} type=${type}`);
  }

  /**
   * Unregister the specified component type
   *
   * @param type Component type string
   */
  unregister(type: string): void {
    nativeUnregisterMeasurement(this._engineId, type);
    this._registeredTypes.delete(type);
    console.info(`[MeasurementManager] unregistered: engineId=${this._engineId} type=${type}`);
  }

  /**
   * Unregister all registered measurement callbacks (called when Surface is destroyed)
   */
  dispose(): void {
    for (const type of this._registeredTypes) {
      nativeUnregisterMeasurement(this._engineId, type);
    }
    this._registeredTypes.clear();
    console.info(`[MeasurementManager] disposed: engineId=${this._engineId}`);
  }

  /**
   * Get the list of registered component types
   */
  get registeredTypes(): ReadonlySet<string> {
    return this._registeredTypes;
  }
}
