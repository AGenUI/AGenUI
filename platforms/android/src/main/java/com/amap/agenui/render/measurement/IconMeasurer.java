package com.amap.agenui.render.measurement;

import org.json.JSONObject;

/**
 * Intrinsic measurement for Icon.
 *
 * Yoga reaches this measurer for icon leaf nodes when width / height are not both finalized by
 * explicit styles. Typical trigger cases:
 * 1. `size` is provided and Yoga needs it expanded into width + height.
 * 2. Only one axis is explicit and the other axis should follow the icon's square size.
 * 3. Both axes are `auto` and the default icon size is required.
 *
 * If both width and height are already fixed, Yoga may skip the callback.
 */
final class IconMeasurer {

    private static final float DEFAULT_ICON_SIZE_A2UI = 48f;

    private IconMeasurer() {
    }

    static MeasureResult measure(String paramJson,
                                 float maxWidth,
                                 int widthMode,
                                 float maxHeight,
                                 int heightMode) {
        JSONObject root = MeasurementSupport.parseRoot(paramJson);
        if (root == null) {
            return MeasureResult.zero();
        }

        JSONObject styles = MeasurementSupport.optStyles(root);
        Float size = parseIconSizeA2ui(root.opt("size"));
        Float explicitWidth = MeasurementSupport.parseA2uiDimension(styles == null ? null : styles.opt("width"));
        Float explicitHeight = MeasurementSupport.parseA2uiDimension(styles == null ? null : styles.opt("height"));

        return resolveSyncResult(
                explicitWidth,
                explicitHeight,
                size,
                DEFAULT_ICON_SIZE_A2UI,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }

    static MeasureResult resolveSyncResult(Float explicitWidth,
                                           Float explicitHeight,
                                           Float sizeProperty,
                                           float defaultSize,
                                           float maxWidth,
                                           int widthMode,
                                           float maxHeight,
                                           int heightMode) {
        float resolvedWidth = sizeProperty != null
                ? sizeProperty
                : explicitWidth != null ? explicitWidth : defaultSize;
        float resolvedHeight = sizeProperty != null
                ? sizeProperty
                : explicitHeight != null ? explicitHeight : defaultSize;
        return MeasurementSupport.resolveSize(
                resolvedWidth,
                resolvedHeight,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }

    static Float parseIconSizeA2ui(Object value) {
        if (value == null || value == JSONObject.NULL) {
            return null;
        }
        int dpValue;
        if (value instanceof Number) {
            dpValue = ((Number) value).intValue();
        } else {
            try {
                dpValue = Integer.parseInt(String.valueOf(value));
            } catch (NumberFormatException ignored) {
                return null;
            }
        }
        return dpValue > 0 ? dpValue * 2f : null;
    }
}
