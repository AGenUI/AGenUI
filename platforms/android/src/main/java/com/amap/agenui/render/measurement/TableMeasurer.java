package com.amap.agenui.render.measurement;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.Map;

/**
 * Intrinsic measurement for Table.
 *
 * Yoga reaches this measurer when table width / height still depend on headers, rows, and
 * available width after style resolution. Typical trigger cases:
 * 1. Width is `auto` and must be estimated from column content.
 * 2. Parent passes AT_MOST / EXACT width and cell text wrapping changes row heights.
 * 3. Horizontal-scroll mode needs to distinguish between intrinsic content width and viewport
 *    width returned back to Yoga.
 *
 * If a future table variant becomes fully fixed-size, Yoga can bypass this callback.
 */
final class TableMeasurer {

    private static final String COMPONENT_NAME = "Table";
    private static final int MAX_ROWS_TO_MEASURE = 10;

    private TableMeasurer() {
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

        JSONArray headerRow = MeasurementSupport.optArray(root, "columns", "headers");
        JSONArray rows = root.optJSONArray("rows");
        int columnCount = resolveColumnCount(headerRow, rows);
        if (columnCount <= 0) {
            return MeasureResult.zero();
        }

        Map<String, String> styleConfig = MeasurementSupport.getComponentStyle(context, COMPONENT_NAME);
        TableStyle tableStyle = TableStyle.from(styleConfig);

        float availableWidth = (widthMode == MeasurementSupport.MODE_EXACTLY
                || widthMode == MeasurementSupport.MODE_AT_MOST) && maxWidth > 0f
                ? maxWidth
                : 0f;

        float[] columnWidths = estimateColumnWidths(
                context,
                headerRow,
                rows,
                columnCount,
                tableStyle,
                availableWidth);

        float contentWidth = sum(columnWidths);
        if (tableStyle.horizontalScroll && availableWidth > 0f && contentWidth < availableWidth) {
            scaleColumnWidths(columnWidths, availableWidth, tableStyle.minColumnWidth);
            contentWidth = sum(columnWidths);
        }

        float contentHeight = 0f;
        if (headerRow != null && headerRow.length() > 0) {
            contentHeight += measureRowHeight(context, headerRow, columnWidths, tableStyle, true);
        }

        if (rows != null) {
            for (int i = 0; i < rows.length(); i++) {
                JSONArray row = rows.optJSONArray(i);
                if (row != null) {
                    contentHeight += measureRowHeight(context, row, columnWidths, tableStyle, false);
                }
            }
        }

        return resolveSyncResult(
                contentWidth,
                contentHeight,
                tableStyle.horizontalScroll,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }

    static MeasureResult resolveSyncResult(float contentWidth,
                                           float contentHeight,
                                           boolean horizontalScroll,
                                           float maxWidth,
                                           int widthMode,
                                           float maxHeight,
                                           int heightMode) {
        float desiredWidth = contentWidth;
        if (horizontalScroll
                && (widthMode == MeasurementSupport.MODE_EXACTLY
                || widthMode == MeasurementSupport.MODE_AT_MOST)
                && maxWidth > 0f) {
            desiredWidth = maxWidth;
        }

        return MeasurementSupport.resolveSize(
                desiredWidth,
                contentHeight,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }

    private static int resolveColumnCount(JSONArray headerRow, JSONArray rows) {
        if (headerRow != null && headerRow.length() > 0) {
            return headerRow.length();
        }
        if (rows != null && rows.length() > 0) {
            JSONArray firstRow = rows.optJSONArray(0);
            if (firstRow != null) {
                return firstRow.length();
            }
        }
        return 0;
    }

    private static float[] estimateColumnWidths(Context context,
                                                JSONArray headerRow,
                                                JSONArray rows,
                                                int columnCount,
                                                TableStyle style,
                                                float availableWidth) {
        float[] columnWidths = new float[columnCount];
        int rowsToMeasure = rows == null ? 0 : Math.min(MAX_ROWS_TO_MEASURE, rows.length());

        if (!style.horizontalScroll && availableWidth > 0f) {
            float sharedWidth = availableWidth / columnCount;
            for (int i = 0; i < columnCount; i++) {
                columnWidths[i] = sharedWidth;
            }
            return columnWidths;
        }

        for (int column = 0; column < columnCount; column++) {
            float maxWidth = 0f;

            if (headerRow != null && column < headerRow.length()) {
                maxWidth = Math.max(maxWidth, measureIntrinsicCellWidth(
                        context,
                        headerRow.opt(column),
                        style.headerPaddingHorizontal,
                        style.headerTextStyle));
            }

            for (int rowIndex = 0; rowIndex < rowsToMeasure; rowIndex++) {
                JSONArray row = rows.optJSONArray(rowIndex);
                if (row == null || column >= row.length()) {
                    continue;
                }
                maxWidth = Math.max(maxWidth, measureIntrinsicCellWidth(
                        context,
                        row.opt(column),
                        style.bodyPaddingHorizontal,
                        style.bodyTextStyle));
            }

            columnWidths[column] = Math.max(
                    style.minColumnWidth,
                    Math.min(maxWidth, style.maxColumnWidth));
        }

        return columnWidths;
    }

    private static float measureIntrinsicCellWidth(Context context,
                                                   Object rawValue,
                                                   float horizontalPadding,
                                                   JSONObject textStyle) {
        String text = MeasurementSupport.extractString(rawValue);
        MeasureResult textResult = TextMeasurer.measureTextValue(
                context,
                text,
                textStyle,
                0f,
                MeasurementSupport.MODE_UNDEFINED,
                0f,
                MeasurementSupport.MODE_UNDEFINED);
        return textResult.width + horizontalPadding * 2f;
    }

    private static float measureRowHeight(Context context,
                                          JSONArray row,
                                          float[] columnWidths,
                                          TableStyle style,
                                          boolean header) {
        float rowHeight = 0f;
        float horizontalPadding = header
                ? style.headerPaddingHorizontal
                : style.bodyPaddingHorizontal;
        float verticalPadding = header
                ? style.headerPaddingVertical
                : style.bodyPaddingVertical;
        JSONObject textStyle = header ? style.headerTextStyle : style.bodyTextStyle;

        for (int column = 0; column < columnWidths.length; column++) {
            String text = column < row.length() ? MeasurementSupport.extractString(row.opt(column)) : "";
            float innerWidth = Math.max(0f, columnWidths[column] - horizontalPadding * 2f);
            int textWidthMode = innerWidth > 0f ? MeasurementSupport.MODE_AT_MOST : MeasurementSupport.MODE_UNDEFINED;
            MeasureResult textMeasure = TextMeasurer.measureTextValue(
                    context,
                    text,
                    textStyle,
                    innerWidth,
                    textWidthMode,
                    0f,
                    MeasurementSupport.MODE_UNDEFINED);
            rowHeight = Math.max(rowHeight, verticalPadding * 2f + textMeasure.height);
        }

        return rowHeight;
    }

    private static void scaleColumnWidths(float[] columnWidths,
                                          float availableWidth,
                                          float minColumnWidth) {
        float totalWidth = sum(columnWidths);
        if (totalWidth <= 0f) {
            return;
        }

        float scale = availableWidth / totalWidth;
        for (int i = 0; i < columnWidths.length; i++) {
            columnWidths[i] = Math.max(minColumnWidth, columnWidths[i] * scale);
        }
    }

    private static float sum(float[] values) {
        float result = 0f;
        for (float value : values) {
            result += value;
        }
        return result;
    }

    private static final class TableStyle {
        final float minColumnWidth;
        final float maxColumnWidth;
        final float headerPaddingVertical;
        final float headerPaddingHorizontal;
        final float bodyPaddingVertical;
        final float bodyPaddingHorizontal;
        final boolean horizontalScroll;
        final JSONObject headerTextStyle;
        final JSONObject bodyTextStyle;

        TableStyle(float minColumnWidth,
                   float maxColumnWidth,
                   float headerPaddingVertical,
                   float headerPaddingHorizontal,
                   float bodyPaddingVertical,
                   float bodyPaddingHorizontal,
                   boolean horizontalScroll,
                   JSONObject headerTextStyle,
                   JSONObject bodyTextStyle) {
            this.minColumnWidth = minColumnWidth;
            this.maxColumnWidth = maxColumnWidth;
            this.headerPaddingVertical = headerPaddingVertical;
            this.headerPaddingHorizontal = headerPaddingHorizontal;
            this.bodyPaddingVertical = bodyPaddingVertical;
            this.bodyPaddingHorizontal = bodyPaddingHorizontal;
            this.horizontalScroll = horizontalScroll;
            this.headerTextStyle = headerTextStyle;
            this.bodyTextStyle = bodyTextStyle;
        }

        static TableStyle from(Map<String, String> styleConfig) {
            float headerFontSize = MeasurementSupport.readStyleDimensionA2ui(
                    styleConfig, "header-font-size", 28f);
            float bodyFontSize = MeasurementSupport.readStyleDimensionA2ui(
                    styleConfig, "body-font-size", 28f);

            return new TableStyle(
                    MeasurementSupport.readStyleDimensionA2ui(styleConfig, "min-column-width", 100f),
                    MeasurementSupport.readStyleDimensionA2ui(styleConfig, "max-column-width", 600f),
                    MeasurementSupport.readStyleDimensionA2ui(styleConfig, "header-padding-vertical", 26f),
                    MeasurementSupport.readStyleDimensionA2ui(styleConfig, "header-padding-horizontal", 32f),
                    MeasurementSupport.readStyleDimensionA2ui(styleConfig, "body-padding-vertical", 25f),
                    MeasurementSupport.readStyleDimensionA2ui(styleConfig, "body-padding-horizontal", 32f),
                    styleConfig == null || !styleConfig.containsKey("horizontal-scroll")
                            || Boolean.parseBoolean(styleConfig.get("horizontal-scroll")),
                    MeasurementSupport.buildTextStyles(null, headerFontSize, true, "font-size"),
                    MeasurementSupport.buildTextStyles(null, bodyFontSize, false, "font-size"));
        }
    }
}
