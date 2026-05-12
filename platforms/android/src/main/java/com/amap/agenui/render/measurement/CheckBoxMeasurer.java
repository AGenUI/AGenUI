package com.amap.agenui.render.measurement;

import android.content.Context;

import org.json.JSONObject;

import java.util.Map;

/**
 * Intrinsic measurement for CheckBox.
 *
 * Yoga reaches this measurer when the checkbox cannot be fully sized from explicit width / height
 * alone. Typical trigger cases:
 * 1. Width is `auto` and must expand to `checkbox-size + label text`.
 * 2. Parent passes an AT_MOST / EXACT width and label wrapping changes the final height.
 * 3. Height is `auto` and must follow the larger of the indicator box or the measured label.
 *
 * If both axes are already fixed by layout styles, Yoga may resolve the node without invoking
 * this callback.
 */
final class CheckBoxMeasurer {

    private static final String COMPONENT_NAME = "CheckBox";

    private CheckBoxMeasurer() {
    }

    static MeasureResult measure(Context context,
                                 String paramJson,
                                 float maxWidth,
                                 int widthMode,
                                 float maxHeight,
                                 int heightMode) {
        JSONObject root = MeasurementSupport.parseRoot(paramJson);
        if (root == null) {
            return MeasureResult.zero();
        }

        JSONObject styles = MeasurementSupport.optStyles(root);
        Map<String, String> styleConfig = MeasurementSupport.getComponentStyle(context, COMPONENT_NAME);

        float checkboxSize = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "checkbox-size", 32f);
        float textMargin = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "text-margin", 16f);
        float textSize = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "text-size", 32f);

        Float customCheckboxSize = MeasurementSupport.firstDimension(styles, "checkbox-size");
        if (customCheckboxSize != null) {
            checkboxSize = customCheckboxSize;
        }
        Float customTextMargin = MeasurementSupport.firstDimension(styles, "text-margin");
        if (customTextMargin != null) {
            textMargin = customTextMargin;
        }
        Float customTextSize = MeasurementSupport.firstDimension(styles, "text-size", "font-size", "fontSize");
        if (customTextSize != null) {
            textSize = customTextSize;
        }

        String label = MeasurementSupport.extractString(root.opt("label"));
        MeasureResult labelResult = MeasureResult.zero();
        if (!label.isEmpty()) {
            float reservedWidth = checkboxSize + textMargin;
            float labelMaxWidth = MeasurementSupport.resolveTextMaxWidth(maxWidth, widthMode, reservedWidth);
            int labelWidthMode = labelMaxWidth > 0f ? MeasurementSupport.MODE_AT_MOST : MeasurementSupport.MODE_UNDEFINED;
            JSONObject textStyles = MeasurementSupport.buildTextStyles(styles, textSize, false, "text-size");
            labelResult = TextMeasurer.measureTextValue(
                    context,
                    label,
                    textStyles,
                    labelMaxWidth,
                    labelWidthMode,
                    0f,
                    MeasurementSupport.MODE_UNDEFINED);
        }

        return resolveSyncResult(
                checkboxSize,
                label.isEmpty() ? 0f : textMargin,
                labelResult.width,
                labelResult.height,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }

    static MeasureResult resolveSyncResult(float checkboxSize,
                                           float textMargin,
                                           float labelWidth,
                                           float labelHeight,
                                           float maxWidth,
                                           int widthMode,
                                           float maxHeight,
                                           int heightMode) {
        float desiredWidth = checkboxSize + textMargin + Math.max(0f, labelWidth);
        float desiredHeight = Math.max(checkboxSize, Math.max(0f, labelHeight));
        return MeasurementSupport.resolveSize(
                desiredWidth,
                desiredHeight,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }
}
