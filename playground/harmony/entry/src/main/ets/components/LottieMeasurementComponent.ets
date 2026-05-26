import { MeasurementComponent } from './MeasurementComponent';
import { MeasureResult } from '@agenui/agenui';

/**
 * Lottie component measurement class
 *
 * Lottie animation has no intrinsic natural size exposed by lottie-turbo before
 * the animation JSON is loaded, and CustomLottieComponent does not call
 * reportContentHeight after load. So this measurer always returns Sync with a
 * deterministic placeholder so Yoga can size the node up-front:
 *   - When Yoga passes Exactly/AtMost constraints, use the constraint value.
 *   - Otherwise fall back to DEFAULT_SIZE (matches buildCustomLottie's 200 default).
 *
 * Why Sync instead of Async:
 *   The engine in core/.../agenui_virtual_dom_node.cpp drops the measurer's
 *   width/height entirely when calcType == Async and returns {0,0} to Yoga,
 *   relying on a later reportContentHeight call to mark the node dirty. Since
 *   no such call is made for Lottie, Async would leave the node at 0x0 forever.
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

    const result: MeasureResult = { width: w, height: h, calcType: 0 };
    return result;
  }
}
