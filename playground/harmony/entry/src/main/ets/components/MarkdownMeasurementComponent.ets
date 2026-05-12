import { MeasurementComponent } from './MeasurementComponent';
import { MeasureResult } from '@agenui/agenui';

/**
 * Markdown component measurement class
 *
 * Markdown is an async rendering component; the actual height is determined by the Markdown library's render completion callback.
 * - calcType=1 (Async): Returns {0, 0} first, triggers re-layout via reportContentHeight -> markDirty after rendering completes.
 * - Width prioritizes Yoga constraint values (AtMost/Exactly).
 *
 * Corresponds to buildCustomMarkdown:
 *   .width(getViewWidth()  <= 0 ? '100%'     : getViewWidth())
 *   .height(getViewHeight() <= 0 ? undefined  : getViewHeight())
 *   onSizeChange -> reportContentHeight(reportH, reportW)  // Trigger re-layout
 */
export class MarkdownMeasurementComponent extends MeasurementComponent {
  measure(
    _paramJson: string,
    widthMode: number,
    maxWidth: number,
    _heightMode: number,
    _maxHeight: number
  ): MeasureResult {
    // Width: use constraint value when available, otherwise placeholder 0 (Yoga will assign '100%')
    const w = (widthMode === 1 || widthMode === 2) && maxWidth > 0 ? maxWidth : 0;

    // Height: return 0 first, wait for onSizeChange -> reportContentHeight to report actual height, then markDirty
    const result: MeasureResult = { width: w, height: 0, calcType: 1 };
    return result;
  }
}
