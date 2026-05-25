package com.amap.agenui.render.measurement;

import android.content.Context;

import org.json.JSONObject;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.Map;

/**
 * Intrinsic measurement for DateTimeInput.
 *
 * Yoga reaches this measurer when the control width still depends on the display text and icon
 * after style defaults are applied. Typical trigger cases:
 * 1. Width is `auto` and must be derived from placeholder / formatted value text.
 * 2. Parent width becomes AT_MOST / EXACT and the measured width needs clamping.
 * 3. Height is not explicitly fixed by the caller and falls back to the compact style height.
 *
 * If both width and height are explicitly pinned by layout styles, Yoga may not invoke measure.
 */
final class DateTimeInputMeasurer {

    private static final String COMPONENT_NAME = "DateTimeInput";

    private DateTimeInputMeasurer() {
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

        Map<String, String> styleConfig = MeasurementSupport.getComponentStyle(context, COMPONENT_NAME);
        float inputHeight = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "compact.height", 56f);
        float fontSize = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "compact.font-size", 24f);
        float paddingHorizontal = MeasurementSupport.readStyleDimensionA2ui(styleConfig,
                "compact.padding-horizontal", 24f);
        float iconSize = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "compact.icon-size", 24f);
        float iconSpacing = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "compact.icon-spacing", 6f);
        String placeholder = styleConfig.containsKey("compact.placeholder-text")
                ? styleConfig.get("compact.placeholder-text")
                : "Select Date";

        boolean enableDate = MeasurementSupport.extractBoolean(root.opt("enableDate"), true);
        boolean enableTime = MeasurementSupport.extractBoolean(root.opt("enableTime"), false);
        String value = MeasurementSupport.extractString(root.opt("value"));
        DisplayState displayState = resolveDisplayState(enableDate, enableTime, value, placeholder);

        JSONObject textStyles = MeasurementSupport.buildTextStyles(
                null,
                fontSize,
                displayState.selected,
                "font-size");
        MeasureResult textMeasure = TextMeasurer.measureTextValue(
                context,
                displayState.text,
                textStyles,
                0f,
                MeasurementSupport.MODE_UNDEFINED,
                0f,
                MeasurementSupport.MODE_UNDEFINED);

        return resolveSyncResult(
                inputHeight,
                paddingHorizontal,
                textMeasure.width,
                iconSize,
                iconSpacing,
                displayState.showIcon,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }

    static DisplayState resolveDisplayState(boolean enableDate,
                                            boolean enableTime,
                                            String value,
                                            String placeholder) {
        if (value == null || value.isEmpty()) {
            return new DisplayState(placeholder, false, true);
        }

        try {
            SimpleDateFormat inputFormat;
            SimpleDateFormat outputFormat;
            if (enableDate && enableTime) {
                inputFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault());
                outputFormat = inputFormat;
            } else if (enableDate) {
                inputFormat = new SimpleDateFormat("yyyy-MM-dd", Locale.getDefault());
                outputFormat = inputFormat;
            } else if (enableTime) {
                inputFormat = new SimpleDateFormat("HH:mm", Locale.getDefault());
                outputFormat = inputFormat;
            } else {
                return new DisplayState(placeholder, false, true);
            }
            inputFormat.setLenient(false);
            Date parsed = inputFormat.parse(value);
            if (parsed == null) {
                return new DisplayState(placeholder, false, true);
            }
            return new DisplayState(outputFormat.format(parsed), true, false);
        } catch (Exception ignored) {
            return new DisplayState(placeholder, false, true);
        }
    }

    static MeasureResult resolveSyncResult(float inputHeight,
                                           float paddingHorizontal,
                                           float textWidth,
                                           float iconSize,
                                           float iconSpacing,
                                           boolean showIcon,
                                           float maxWidth,
                                           int widthMode,
                                           float maxHeight,
                                           int heightMode) {
        float desiredWidth = paddingHorizontal * 2f + textWidth;
        if (showIcon) {
            desiredWidth += iconSize + iconSpacing;
        }
        return MeasurementSupport.resolveSize(
                desiredWidth,
                inputHeight,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }

    static final class DisplayState {
        final String text;
        final boolean selected;
        final boolean showIcon;

        DisplayState(String text, boolean selected, boolean showIcon) {
            this.text = text;
            this.selected = selected;
            this.showIcon = showIcon;
        }
    }
}
