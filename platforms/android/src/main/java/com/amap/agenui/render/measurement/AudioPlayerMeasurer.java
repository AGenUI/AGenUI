package com.amap.agenui.render.measurement;

import android.content.Context;

import java.util.Map;

/**
 * Intrinsic measurement for the leaf AudioPlayer component.
 *
 * Yoga only calls into this measurer after a measureFunc has been registered and the node still
 * needs content-based sizing. Typical trigger cases:
 * 1. Component styles leave width / height as `auto`, so the default square player size is needed.
 * 2. Parent layout passes AT_MOST / EXACT constraints and the default size must be clamped.
 *
 * If both axes are already fully determined by style + parent constraints, Yoga may skip the
 * callback entirely even though the measurement implementation is registered.
 */
final class AudioPlayerMeasurer {

    private static final String COMPONENT_NAME = "AudioPlayer";

    private AudioPlayerMeasurer() {
    }

    static MeasureResult measure(Context context,
                                 String paramJson,
                                 float maxWidth,
                                 int widthMode,
                                 float maxHeight,
                                 int heightMode) {
        Map<String, String> styleConfig = MeasurementSupport.getComponentStyle(context, COMPONENT_NAME);
        float size = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "size", 80f);
        return resolveSyncResult(size, maxWidth, widthMode, maxHeight, heightMode);
    }

    static MeasureResult resolveSyncResult(float size,
                                           float maxWidth,
                                           int widthMode,
                                           float maxHeight,
                                           int heightMode) {
        return MeasurementSupport.resolveSize(
                size,
                size,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }
}
