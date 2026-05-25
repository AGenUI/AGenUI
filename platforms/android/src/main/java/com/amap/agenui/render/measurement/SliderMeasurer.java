package com.amap.agenui.render.measurement;

import android.content.Context;

import java.util.Map;

/**
 * Intrinsic measurement for Slider.
 *
 * Yoga reaches this measurer when the slider still needs its default track height or a parent
 * constrained width applied through measure. Typical trigger cases:
 * 1. Height is `auto` and should fall back to the style-config slider height + vertical padding.
 * 2. Width is unresolved and should follow the parent's EXACT / AT_MOST constraint.
 *
 * If both axes are explicitly fixed by style, Yoga may skip measure.
 */
final class SliderMeasurer {

    private static final String COMPONENT_NAME = "Slider";
    private static final float CONTAINER_VERTICAL_PADDING_A2UI = 16f;

    private SliderMeasurer() {
    }

    static MeasureResult measure(Context context,
                                 String paramJson,
                                 float maxWidth,
                                 int widthMode,
                                 float maxHeight,
                                 int heightMode) {
        Map<String, String> styleConfig = MeasurementSupport.getComponentStyle(context, COMPONENT_NAME);
        float sliderHeight = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "slider-height", 48f);
        return resolveSyncResult(
                widthMode == MeasurementSupport.MODE_UNDEFINED ? 0f : maxWidth,
                sliderHeight + CONTAINER_VERTICAL_PADDING_A2UI,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }

    static MeasureResult resolveSyncResult(float desiredWidth,
                                           float desiredHeight,
                                           float maxWidth,
                                           int widthMode,
                                           float maxHeight,
                                           int heightMode) {
        return MeasurementSupport.resolveSize(
                desiredWidth,
                desiredHeight,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }
}
