import { MeasurementComponent } from './MeasurementComponent';
import { MeasureResult } from '@agenui/agenui';

/**
 * Lottie component measurement class
 *
 * Lottie animation is an async loading component; width and height are determined by Yoga layout constraints.
 * - calcType=1 (Async): Returns placeholder {0, 0} first, triggers re-measurement via markDirty after animation loading completes.
 * - When Yoga passes Exactly/AtMost constraints, use constraint values directly; use DEFAULT_SIZE fallback when unconstrained.
 *
 * Corresponds to buildCustomLottie:
 *   .width(getViewWidth()  <= 0 ? 200 : getViewWidth())
 *   .height(getViewHeight() <= 0 ? 200 : getViewHeight())
 */
export class LottieMeasurementComponent extends MeasurementComponent {
  /** Fallback size when unconstrained (aligned with builder's 200) */
  private static readonly DEFAULT_SIZE: number = 200;

  measure(
    _paramJson: string,
    widthMode: number,
    maxWidth: number,
    heightMode: number,
    maxHeight: number
  ): MeasureResult {
    // widthMode / heightMode: 0=Undefined 1=Exactly 2=AtMost
    const w = (widthMode === 1 || widthMode === 2) && maxWidth > 0
      ? maxWidth
      : LottieMeasurementComponent.DEFAULT_SIZE;

    const h = (heightMode === 1 || heightMode === 2) && maxHeight > 0
      ? maxHeight
      : LottieMeasurementComponent.DEFAULT_SIZE;

    // calcType=1: Async, Yoga uses this size as placeholder first, markDirty triggers re-layout after animation loading completes
    const result: MeasureResult = { width: w, height: h, calcType: 1 };
    return result;
  }
}
