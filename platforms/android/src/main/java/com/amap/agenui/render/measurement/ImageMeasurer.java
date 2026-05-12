package com.amap.agenui.render.measurement;

import org.json.JSONObject;

/**
 * Safe synchronous image measurement fallback.
 *
 * Yoga only invokes this measurer after an Image node has registered measureFunc and the final
 * size still cannot be resolved from Yoga styles alone. Typical trigger cases:
 * 1. `width` is fixed while `height` is `auto` — returns 0x0 and waits for async render-finish.
 * 2. `height` is fixed while `width` is `auto` — same as above.
 * 3. Both axes are `auto`, so Yoga asks for intrinsic content size; returns 0x0 first and waits
 *    for ImageComponent's async render-finish callback.
 * 4. Both axes become EXACT from the parent; Yoga may still callback, and this measurer must
 *    simply echo the incoming constraints.
 *
 * A registered Image measurer does not mean every Image story will hit this code path. If Yoga
 * already knows the final width and height from style + parent constraints, it can skip measure.
 */
final class ImageMeasurer {

    private static final int MODE_EXACTLY = 1;

    private ImageMeasurer() {
    }

    static MeasureResult measure(String paramJson,
                                 float maxWidth,
                                 int widthMode,
                                 float maxHeight,
                                 int heightMode) {
        // Fast path: if Yoga already provided exact constraints, avoid JSON parsing entirely.
        MeasureResult exactResult = resolveSyncResult(
                null,
                null,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
        if (exactResult != null) {
            return exactResult;
        }

        try {
            JSONObject root = MeasurementSupport.parseRoot(paramJson);
            JSONObject styles = MeasurementSupport.optStyles(root);

            Float explicitWidth = MeasurementSupport.parseA2uiDimension(styles == null ? null : styles.opt("width"));
            Float explicitHeight = MeasurementSupport.parseA2uiDimension(styles == null ? null : styles.opt("height"));

            MeasureResult resolved = resolveSyncResult(
                    explicitWidth,
                    explicitHeight,
                    maxWidth,
                    widthMode,
                    maxHeight,
                    heightMode);
            if (resolved != null) {
                return resolved;
            }
        } catch (Exception ignored) {
        }
        return MeasureResult.zero();
    }

    static MeasureResult resolveSyncResult(Float explicitWidth,
                                           Float explicitHeight,
                                           float maxWidth,
                                           int widthMode,
                                           float maxHeight,
                                           int heightMode) {
        // This method only handles fully synchronous cases. If no branch matches, the caller
        // must fall back to async runtime measurement and report the final size later.
        if (widthMode == MODE_EXACTLY && heightMode == MODE_EXACTLY
                && maxWidth > 0f && maxHeight > 0f) {
            return MeasureResult.sync(maxWidth, maxHeight);
        }
        if (explicitWidth != null && explicitHeight != null
                && explicitWidth > 0f && explicitHeight > 0f) {
            return MeasureResult.sync(explicitWidth, explicitHeight);
        }
        return null;
    }
}
