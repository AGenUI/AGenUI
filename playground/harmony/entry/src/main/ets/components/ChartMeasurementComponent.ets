import { MeasurementComponent } from './MeasurementComponent';
import { MeasureResult } from '@agenui/agenui';

/**
 * Chart component measurement class
 *
 * Chart is an async rendering component; width and height are determined by Yoga layout.
 * - calcType=1 (Async): Returns placeholder height first, triggers re-layout via markDirty after chart rendering completes.
 * - Width prioritizes Yoga constraint values; height uses reasonable placeholder height based on chartType.
 *
 * Corresponds to buildCustomChart: Column has no explicit width/height, fills the parent container allocated by Yoga.
 */
export class ChartMeasurementComponent extends MeasurementComponent {
  // Placeholder heights for each chart type (unit: vp)
  private static readonly HEIGHT_PIE: number = 300;
  private static readonly HEIGHT_LINE: number = 250;
  private static readonly HEIGHT_BAR: number = 300;
  private static readonly HEIGHT_DEFAULT: number = 300;
  private static readonly DEFAULT_WIDTH: number = 350;

  measure(
    paramJson: string,
    _widthMode: number,
    maxWidth: number,
    heightMode: number,
    maxHeight: number
  ): MeasureResult {
    let chartType = '';
    try {
      const param = JSON.parse(paramJson) as Record<string, Object>;
      const attrs = param['attributes'] as Record<string, Object> | undefined;
      chartType = (attrs?.['chartType'] as string) ?? '';
    } catch (_e) {
      // ignore
    }

    const w = maxWidth > 0 ? maxWidth : ChartMeasurementComponent.DEFAULT_WIDTH;

    let defaultH: number;
    if (chartType === 'donut' || chartType === 'pie') {
      defaultH = ChartMeasurementComponent.HEIGHT_PIE;
    } else if (chartType === 'line') {
      defaultH = ChartMeasurementComponent.HEIGHT_LINE;
    } else if (chartType === 'bar') {
      defaultH = ChartMeasurementComponent.HEIGHT_BAR;
    } else {
      defaultH = ChartMeasurementComponent.HEIGHT_DEFAULT;
    }

    const h = (heightMode === 1 || heightMode === 2) && maxHeight > 0
      ? Math.min(defaultH, maxHeight)
      : defaultH;

    // calcType=1: Async, markDirty triggers re-layout after chart data parsing and rendering complete
    const result: MeasureResult = { width: w, height: h, calcType: 1 };
    return result;
  }
}
