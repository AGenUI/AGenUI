/**
 * MeasurementComponent.ets
 *
 * Pure measurement base class, does not depend on A2UIComponent.
 *
 * Subclass and override the measure() method, write calculation logic in measure().
 * Register externally via measurementManager.register(type, comp.measure.bind(comp)).
 */

import { MeasurementHandler } from '@agenui/agenui';
import { MeasureResult } from '@agenui/agenui';

export abstract class MeasurementComponent {
  /**
   * Measurement method, subclasses must override.
   * Signature is consistent with MeasurementHandler, can be used directly as a handler:
   *   measurementManager.register(type, comp.measure.bind(comp))
   *
   * @returns { width, height, calcType? }
   *   calcType: 0=Sync (default) 1=Async (async component returns {0,0} first, markDirty after loading completes)
   */
  abstract measure(
    paramJson: string,
    widthMode: number,
    maxWidth: number,
    heightMode: number,
    maxHeight: number
  ): MeasureResult;

  /**
   * Convenience method: returns a MeasurementHandler bound to this, can be passed directly to register().
   */
  asMeasurementHandler(): MeasurementHandler {
    return this.measure.bind(this);
  }
}
