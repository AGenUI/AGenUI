package com.amap.agenui.render.measurement;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.Map;

/**
 * Intrinsic measurement for ChoicePicker.
 *
 * Yoga calls this measurer when the picker size still depends on option content after style
 * resolution. Typical trigger cases:
 * 1. Width / height stay `auto` and must be derived from option labels plus paddings.
 * 2. Vertical mode needs the widest option for width and the accumulated option heights.
 * 3. Horizontal mode needs the summed option widths and the tallest option height.
 * 4. Parent width becomes AT_MOST / EXACT and individual option text may wrap differently.
 *
 * If a future ChoicePicker variant becomes fully fixed-size, Yoga can skip this callback.
 */
final class ChoicePickerMeasurer {

    private static final String COMPONENT_NAME = "ChoicePicker";
    private static final float ITEM_PADDING_HORIZONTAL_A2UI = 24f;
    private static final float ITEM_PADDING_VERTICAL_A2UI = 22f;

    private ChoicePickerMeasurer() {
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

        JSONArray options = root.optJSONArray("options");
        if (options == null || options.length() == 0) {
            return MeasureResult.zero();
        }

        JSONObject styles = MeasurementSupport.optStyles(root);
        String orientation = styles == null ? "vertical" : styles.optString("orientation", "vertical");
        boolean horizontal = "horizontal".equalsIgnoreCase(orientation);

        boolean filterable = MeasurementSupport.extractBoolean(root.opt("filterable"), false);

        Map<String, String> styleConfig = MeasurementSupport.getComponentStyle(context, COMPONENT_NAME);
        float checkboxSize = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "checkbox-size", 32f);
        float textMargin = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "text-margin", 16f);
        float textSize = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "text-size", 32f);
        float choiceGap = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "choice-gap", 40f);

        float[] itemWidths = new float[options.length()];
        float[] itemHeights = new float[options.length()];
        JSONObject textStyles = MeasurementSupport.buildTextStyles(null, textSize, false, "text-size");

        for (int i = 0; i < options.length(); i++) {
            JSONObject option = options.optJSONObject(i);
            String label = option == null ? "" : MeasurementSupport.extractString(option.opt("label"));
            float reservedWidth = ITEM_PADDING_HORIZONTAL_A2UI * 2f + checkboxSize
                    + (label.isEmpty() ? 0f : textMargin);
            float labelMaxWidth = !horizontal
                    ? MeasurementSupport.resolveTextMaxWidth(maxWidth, widthMode, reservedWidth)
                    : 0f;
            int labelWidthMode = labelMaxWidth > 0f ? MeasurementSupport.MODE_AT_MOST : MeasurementSupport.MODE_UNDEFINED;
            MeasureResult labelResult = label.isEmpty()
                    ? MeasureResult.zero()
                    : TextMeasurer.measureTextValue(
                            context,
                            label,
                            textStyles,
                            labelMaxWidth,
                            labelWidthMode,
                            0f,
                            MeasurementSupport.MODE_UNDEFINED);

            itemWidths[i] = reservedWidth + labelResult.width;
            itemHeights[i] = ITEM_PADDING_VERTICAL_A2UI * 2f
                    + Math.max(checkboxSize, labelResult.height);
        }

        // Reserve vertical space for the search input when filterable=true.
        // EditText height ≈ filter-padding-v * 2 + filter-text-size (line height factor),
        // plus filter-margin-bottom that separates it from the option list.
        float filterExtraHeight = 0f;
        if (filterable) {
            float filterPaddingV = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "filter-padding-v", 16f);
            float filterTextSize = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "filter-text-size", 28f);
            float filterMarginBottom = MeasurementSupport.readStyleDimensionA2ui(styleConfig, "filter-margin-bottom", 24f);
            // 1.4f line-height multiplier covers EditText's intrinsic ascent/descent margins.
            filterExtraHeight = filterPaddingV * 2f + filterTextSize * 1.4f + filterMarginBottom;
        }

        return resolveSyncResult(
                horizontal,
                itemWidths,
                itemHeights,
                choiceGap,
                filterExtraHeight,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }

    static MeasureResult resolveSyncResult(boolean horizontal,
                                           float[] itemWidths,
                                           float[] itemHeights,
                                           float gap,
                                           float extraHeight,
                                           float maxWidth,
                                           int widthMode,
                                           float maxHeight,
                                           int heightMode) {
        if (itemWidths == null || itemHeights == null || itemWidths.length == 0
                || itemWidths.length != itemHeights.length) {
            return MeasureResult.zero();
        }

        float desiredWidth = 0f;
        float desiredHeight = 0f;

        for (int i = 0; i < itemWidths.length; i++) {
            if (horizontal) {
                desiredWidth += itemWidths[i];
                if (i > 0) {
                    desiredWidth += gap;
                }
                desiredHeight = Math.max(desiredHeight, itemHeights[i]);
            } else {
                desiredWidth = Math.max(desiredWidth, itemWidths[i]);
                desiredHeight += itemHeights[i];
                if (i > 0) {
                    desiredHeight += gap;
                }
            }
        }

        // Account for sibling content above the option list (e.g. the filter input
        // when filterable=true). Width is unaffected since the filter input matches parent.
        desiredHeight += extraHeight;

        return MeasurementSupport.resolveSize(
                desiredWidth,
                desiredHeight,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }
}
