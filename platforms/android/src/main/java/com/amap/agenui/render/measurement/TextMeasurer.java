package com.amap.agenui.render.measurement;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.os.Build;
import android.text.Html;
import android.text.Layout;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.StaticLayout;
import android.text.TextPaint;
import android.text.TextUtils;
import android.util.TypedValue;

import com.amap.agenui.render.style.StyleHelper;

import org.json.JSONObject;

import java.util.Locale;

/**
 * Synchronous text / rich-text measurement backed by thread-safe text primitives.
 *
 * Yoga reaches this measurer for Text / RichText leaf nodes when at least one axis still depends
 * on content after style resolution. Typical trigger cases:
 * 1. `width:auto` + `height:auto`: both sides come from the text's intrinsic layout.
 * 2. Parent passes AT_MOST / EXACT width and text wrapping determines the final height.
 * 3. Height is constrained by EXACT / AT_MOST and the text layout must be clamped accordingly.
 * 4. Both axes become EXACT from the parent; Yoga may still callback, and this measurer must
 *    honor the incoming constraints instead of recomputing arbitrary free size.
 *
 * If both width and height are already fully resolved by Yoga without consulting content, the
 * callback can be skipped even though a text measurer is registered.
 *
 * The synchronous measurement path runs on the engine's worker thread, so it must stay out of the
 * Android View measure/layout contract. This implementation mirrors the metric-affecting subset of
 * `StyleHelper.applyTextStyles()` onto `TextPaint + StaticLayout`, keeping measure and render
 * close enough while avoiding off-screen `TextView` work on a non-UI thread.
 */
public final class TextMeasurer {

    private static final float DEFAULT_TEXT_SIZE_SP = 14f;
    private static final float DEFAULT_RICH_TEXT_SIZE_SP = 16f;
    private static final ThreadLocal<TextPaint> THREAD_LOCAL_PAINT = new ThreadLocal<TextPaint>() {
        @Override
        protected TextPaint initialValue() {
            return new TextPaint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
        }
    };

    /**
     * Metric-affecting text style snapshot shared by the layout builder.
     */
    private static final class TextLayoutStyle {
        final TextPaint paint;
        final float lineSpacingAddPx;
        final float lineSpacingMultiplier;
        final int maxLines;
        final TextUtils.TruncateAt ellipsize;
        /**
         * Target line-box height in px, &gt;0 when a CSS {@code line-height} is declared.
         * Rendered via a {@link StyleHelper.CenteredLineHeightSpan} applied to the text so
         * that the glyphs sit vertically centered inside the line box (matches Harmony/iOS).
         * When 0, no line-height adjustment is performed and the TextView's natural metrics
         * are used.
         */
        final int lineHeightPx;

        TextLayoutStyle(TextPaint paint,
                        float lineSpacingAddPx,
                        float lineSpacingMultiplier,
                        int maxLines,
                        TextUtils.TruncateAt ellipsize,
                        int lineHeightPx) {
            this.paint = paint;
            this.lineSpacingAddPx = lineSpacingAddPx;
            this.lineSpacingMultiplier = lineSpacingMultiplier;
            this.maxLines = maxLines;
            this.ellipsize = ellipsize;
            this.lineHeightPx = lineHeightPx;
        }
    }

    private TextMeasurer() {
    }

    public static MeasureResult measureText(Context context,
                                            String paramJson,
                                            float maxWidth,
                                            int widthMode,
                                            float maxHeight,
                                            int heightMode) {
        return measureInternal(
                context,
                paramJson,
                false,
                DEFAULT_TEXT_SIZE_SP,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }

    public static MeasureResult measureRichText(Context context,
                                                String paramJson,
                                                float maxWidth,
                                                int widthMode,
                                                float maxHeight,
                                                int heightMode) {
        return measureInternal(
                context,
                paramJson,
                true,
                DEFAULT_RICH_TEXT_SIZE_SP,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode);
    }

    private static MeasureResult measureInternal(Context context,
                                                 String paramJson,
                                                 boolean richText,
                                                 float defaultTextSizeSp,
                                                 float maxWidth,
                                                 int widthMode,
                                                 float maxHeight,
                                                 int heightMode) {
        try {
            JSONObject root = new JSONObject(paramJson);
            JSONObject styles = root.optJSONObject("styles");
            CharSequence text = richText
                    ? extractRichText(root)
                    : extractPlainText(root);
            return measureTextValue(
                    context,
                    text,
                    styles,
                    maxWidth,
                    widthMode,
                    maxHeight,
                    heightMode,
                    defaultTextSizeSp);
        } catch (Exception ignored) {
            return MeasureResult.zero();
        }
    }

    static MeasureResult measureTextValue(Context context,
                                          CharSequence text,
                                          JSONObject styles,
                                          float maxWidth,
                                          int widthMode,
                                          float maxHeight,
                                          int heightMode) {
        return measureTextValue(
                context,
                text,
                styles,
                maxWidth,
                widthMode,
                maxHeight,
                heightMode,
                DEFAULT_TEXT_SIZE_SP);
    }

    private static MeasureResult measureTextValue(Context context,
                                                  CharSequence text,
                                                  JSONObject styles,
                                                  float maxWidth,
                                                  int widthMode,
                                                  float maxHeight,
                                                  int heightMode,
                                                  float defaultTextSizeSp) {
        // Shared text primitive used by Text/RichText and by hybrid measurers that need to
        // measure inner labels with the same metric-affecting style subset as Android rendering.
        if (context == null || TextUtils.isEmpty(text)) {
            return MeasureResult.zero();
        }

        try {
            TextLayoutStyle layoutStyle = buildTextLayoutStyle(context, styles, defaultTextSizeSp);
            int constrainedWidthPx = resolveConstraintPx(context, maxWidth);
            int constrainedHeightPx = resolveConstraintPx(context, maxHeight);
            CharSequence displayText = maybeEllipsizeForLegacySingleLine(
                    text,
                    layoutStyle,
                    constrainedWidthPx,
                    widthMode);
            int layoutWidthPx = resolveLayoutWidthPx(
                    displayText,
                    layoutStyle.paint,
                    constrainedWidthPx,
                    widthMode);

            int desiredWidthPx = 0;
            int desiredHeightPx = 0;
            int lineCount = 0;
            if (layoutWidthPx > 0) {
                CharSequence layoutText = applyCenteredLineHeightIfNeeded(displayText, layoutStyle);
                Layout layout = buildLayout(layoutText, layoutStyle, layoutWidthPx);
                lineCount = resolveVisibleLineCount(layout, layoutStyle.maxLines);
                desiredWidthPx = resolveDesiredWidthPx(layout, lineCount);
                desiredHeightPx = resolveDesiredHeightPx(layout, lineCount);
            }

            int measuredWidthPx = resolveMeasuredSizePx(desiredWidthPx, constrainedWidthPx, widthMode);
            int measuredHeightPx = resolveMeasuredSizePx(desiredHeightPx, constrainedHeightPx, heightMode);

            // On certain devices (e.g. Mi 11, density=2.625), when a Text component has padding,
            // a single character may be shown with an ellipsis even though the text could actually fit. This is because properties like padding
            // lose precision during pixel conversion: Yoga computes precisely in A2UI's floating-point space,
            // borderBox = contentWidth + paddingLeft + paddingRight, but each value is converted independently to
            // integer pixels, so the content area shrinks: round(borderBoxPx) - round(padPx) - round(padPx) < contentPx.
            // The +1 corrects this precision loss and ensures the content area is not under-allocated.
            int ceilW = (int) Math.ceil(StyleHelper.pxToA2ui(context, measuredWidthPx)) + 1;
            int ceilH = (int) Math.ceil(StyleHelper.pxToA2ui(context, measuredHeightPx)) + 1;

            return MeasureResult.sync(
                    ceilW,
                    ceilH,
                    lineCount);
        } catch (Exception ignored) {
            return MeasureResult.zero();
        }
    }

    private static CharSequence extractPlainText(JSONObject root) {
        return extractTextValue(root.opt("text"), root.opt("label"));
    }

    private static CharSequence extractRichText(JSONObject root) {
        CharSequence raw = extractTextValue(root.opt("text"), root.opt("label"));
        if (TextUtils.isEmpty(raw)) {
            return "";
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            Spanned spanned = Html.fromHtml(raw.toString(), Html.FROM_HTML_MODE_LEGACY);
            return spanned == null ? "" : spanned;
        }
        return Html.fromHtml(raw.toString());
    }

    private static CharSequence extractTextValue(Object primary, Object fallback) {
        CharSequence value = resolveText(primary);
        if (!TextUtils.isEmpty(value)) {
            return value;
        }
        return resolveText(fallback);
    }

    private static CharSequence resolveText(Object value) {
        if (value == null || value == JSONObject.NULL) {
            return "";
        }
        if (value instanceof JSONObject) {
            JSONObject object = (JSONObject) value;
            String literal = object.optString("literalString", "");
            if (!TextUtils.isEmpty(literal)) {
                return literal;
            }
            return object.optString("path", "");
        }
        return String.valueOf(value);
    }

    /**
     * Resolves the metric-affecting text styles that must stay aligned with
     * `StyleHelper.applyTextStyles()`: typeface, text size, line spacing, max lines and ellipsis.
     */
    private static TextLayoutStyle buildTextLayoutStyle(Context context,
                                                        JSONObject styles,
                                                        float defaultTextSizeSp) {
        TextPaint paint = obtainThreadLocalPaint(context, defaultTextSizeSp);
        float lineSpacingAddPx = 0f;
        float lineSpacingMultiplier = 1f;
        int lineHeightPx = 0;
        int maxLines = Integer.MAX_VALUE;
        TextUtils.TruncateAt ellipsize = null;

        if (styles != null) {
            paint.setTypeface(resolveTypeface(context, styles));
            paint.setTextSize(resolveTextSizePx(context, styles, paint.getTextSize()));

            Object lineHeightValue = firstStyleValue(styles, "line-height", "lineHeight");
            if (lineHeightValue != null) {
                String lineHeightStr = String.valueOf(lineHeightValue)
                        .trim()
                        .toLowerCase(Locale.ROOT);
                if (lineHeightStr.matches("^\\d+(\\.\\d+)?$")) {
                    // CSS semantics: final line-box height = multiplier * font-size. See the
                    // StyleHelper.applyTextStyles() counterpart for the full rationale. The
                    // target value is applied via CenteredLineHeightSpan at layout time rather
                    // than through setLineSpacing so that glyphs are centered inside the line
                    // box (matching Harmony/iOS).
                    Float parsedMultiplier = tryParseFloat(lineHeightStr);
                    if (parsedMultiplier != null && parsedMultiplier > 0f) {
                        lineHeightPx = Math.round(parsedMultiplier * paint.getTextSize());
                    }
                } else if (lineHeightStr.endsWith("px")) {
                    int parsedPx = StyleHelper.parseDimension(lineHeightValue, context);
                    if (parsedPx > 0) {
                        lineHeightPx = parsedPx;
                    }
                }
            }

            Object lineClampValue = firstStyleValue(styles, "line-clamp", "lineClamp");
            int parsedMaxLines = parseInteger(lineClampValue);
            if (parsedMaxLines > 0) {
                maxLines = parsedMaxLines;
            }

            Object textOverflowValue = firstStyleValue(styles, "text-overflow", "textOverflow");
            String textOverflow = textOverflowValue == null
                    ? ""
                    : String.valueOf(textOverflowValue).trim().toLowerCase(Locale.ROOT);
            switch (textOverflow) {
                case "ellipsis":
                    // Android supports TruncateAt.END for any maxLines > 0
                    if (maxLines > 0 && maxLines < Integer.MAX_VALUE) {
                        ellipsize = TextUtils.TruncateAt.END;
                    }
                    break;
                case "head":
                    if (maxLines == 1) {
                        ellipsize = TextUtils.TruncateAt.START;
                    }
                    break;
                case "middle":
                    if (maxLines == 1) {
                        ellipsize = TextUtils.TruncateAt.MIDDLE;
                    }
                    break;
                case "clip":
                default:
                    ellipsize = null;
                    break;
            }
        }

        return new TextLayoutStyle(
                paint,
                lineSpacingAddPx,
                lineSpacingMultiplier,
                maxLines,
                ellipsize,
                lineHeightPx);
    }

    private static TextPaint obtainThreadLocalPaint(Context context, float defaultTextSizeSp) {
        TextPaint paint = THREAD_LOCAL_PAINT.get();
        paint.reset();
        paint.setFlags(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
        paint.setLinearText(true);
        paint.density = context.getResources().getDisplayMetrics().density;
        paint.setTypeface(Typeface.DEFAULT);
        paint.setTextSize(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_SP,
                defaultTextSizeSp,
                context.getResources().getDisplayMetrics()));
        return paint;
    }

    private static Typeface resolveTypeface(Context context, JSONObject styles) {
        Typeface baseTypeface = StyleHelper.parseFontFamily(
                firstStyleValue(styles, "font-family", "fontFamily"),
                context);
        String fontWeightStr = String.valueOf(firstStyleValue(styles, "font-weight", "fontWeight")).trim().toLowerCase();
        boolean bold = StyleHelper.isBoldWeight(fontWeightStr);
        return bold ? Typeface.create(baseTypeface, Typeface.BOLD) : baseTypeface;
    }

    private static float resolveTextSizePx(Context context,
                                           JSONObject styles,
                                           float defaultTextSizePx) {
        Object fontSizeValue = firstStyleValue(styles, "font-size", "fontSize");
        if (fontSizeValue == null) {
            return defaultTextSizePx;
        }

        String sizeStr = String.valueOf(fontSizeValue).trim().toLowerCase(Locale.ROOT);
        float size = 0f;
        if (sizeStr.endsWith("px")) {
            Float parsed = tryParseFloat(sizeStr.substring(0, sizeStr.length() - 2));
            size = parsed == null ? 0f : parsed;
        } else if (sizeStr.matches("^\\d+(\\.\\d+)?$")) {
            Float parsed = tryParseFloat(sizeStr);
            size = parsed == null ? 0f : parsed;
        }
        if (size <= 0f) {
            return defaultTextSizePx;
        }
        return StyleHelper.standardUnitToPx(context, size);
    }

    private static CharSequence maybeEllipsizeForLegacySingleLine(CharSequence text,
                                                                  TextLayoutStyle style,
                                                                  int constrainedWidthPx,
                                                                  int widthMode) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                || widthMode == MeasurementSupport.MODE_UNDEFINED
                || constrainedWidthPx <= 0
                || style.maxLines != 1
                || style.ellipsize == null) {
            return text;
        }
        return TextUtils.ellipsize(text, style.paint, constrainedWidthPx, style.ellipsize);
    }

    /**
     * When the text style carries a {@code line-height} value, wrap the text in a
     * {@link SpannableString} and apply a {@link StyleHelper.CenteredLineHeightSpan}. The
     * measured layout then reports a height that matches what {@code StyleHelper.applyTextStyles}
     * applies at render time, keeping Yoga's measurement and the final TextView drawing in sync.
     */
    private static CharSequence applyCenteredLineHeightIfNeeded(CharSequence text, TextLayoutStyle style) {
        if (style.lineHeightPx <= 0 || text == null || text.length() == 0) {
            return text;
        }
        SpannableString ss = (text instanceof SpannableString)
                ? (SpannableString) text
                : new SpannableString(text);
        StyleHelper.CenteredLineHeightSpan[] existing =
                ss.getSpans(0, ss.length(), StyleHelper.CenteredLineHeightSpan.class);
        for (StyleHelper.CenteredLineHeightSpan span : existing) {
            ss.removeSpan(span);
        }
        ss.setSpan(new StyleHelper.CenteredLineHeightSpan(style.lineHeightPx),
                0, ss.length(),
                Spanned.SPAN_INCLUSIVE_INCLUSIVE);
        return ss;
    }

    @SuppressLint("WrongConstant")
    private static Layout buildLayout(CharSequence text, TextLayoutStyle style, int layoutWidthPx) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            StaticLayout.Builder builder = StaticLayout.Builder.obtain(
                    text,
                    0,
                    text.length(),
                    style.paint,
                    layoutWidthPx);
            builder.setAlignment(Layout.Alignment.ALIGN_NORMAL);
            builder.setIncludePad(true);
            builder.setLineSpacing(style.lineSpacingAddPx, style.lineSpacingMultiplier);
            builder.setBreakStrategy(Layout.BREAK_STRATEGY_SIMPLE);
            builder.setHyphenationFrequency(Layout.HYPHENATION_FREQUENCY_NONE);
            if (style.maxLines < Integer.MAX_VALUE) {
                builder.setMaxLines(style.maxLines);
            }
            if (style.ellipsize != null) {
                builder.setEllipsize(style.ellipsize);
            }
            return builder.build();
        }

        return new StaticLayout(
                text,
                style.paint,
                layoutWidthPx,
                Layout.Alignment.ALIGN_NORMAL,
                style.lineSpacingMultiplier,
                style.lineSpacingAddPx,
                true);
    }

    private static int resolveLayoutWidthPx(CharSequence text,
                                            TextPaint paint,
                                            int constrainedWidthPx,
                                            int widthMode) {
        if (widthMode == MeasurementSupport.MODE_EXACTLY
                || widthMode == MeasurementSupport.MODE_AT_MOST) {
            return Math.max(constrainedWidthPx, 0);
        }

        float desiredWidth = Layout.getDesiredWidth(text, paint);
        if (Float.isNaN(desiredWidth) || Float.isInfinite(desiredWidth)) {
            return 0;
        }
        return Math.max((int) Math.ceil(desiredWidth), 1);
    }

    private static int resolveDesiredWidthPx(Layout layout, int lineCount) {
        int desiredWidthPx = 0;
        for (int i = 0; i < lineCount; i++) {
            desiredWidthPx = Math.max(
                    desiredWidthPx,
                    (int) Math.ceil(layout.getLineRight(i) - layout.getLineLeft(i)));
            if (layout.getEllipsisCount(i) > 0) {
                desiredWidthPx = Math.max(desiredWidthPx, layout.getWidth());
            }
        }
        return desiredWidthPx;
    }

    private static int resolveDesiredHeightPx(Layout layout, int lineCount) {
        if (lineCount <= 0) {
            return 0;
        }
        return Math.max(layout.getLineBottom(lineCount - 1), 0);
    }

    private static int resolveVisibleLineCount(Layout layout, int maxLines) {
        if (layout == null) {
            return 0;
        }
        int lineCount = layout.getLineCount();
        if (maxLines < Integer.MAX_VALUE) {
            lineCount = Math.min(lineCount, maxLines);
        }
        return Math.max(lineCount, 0);
    }

    private static int resolveMeasuredSizePx(int desiredSizePx, int constrainedSizePx, int mode) {
        switch (mode) {
            case MeasurementSupport.MODE_EXACTLY:
                return Math.max(constrainedSizePx, 0);
            case MeasurementSupport.MODE_AT_MOST:
                return constrainedSizePx > 0
                        ? Math.min(Math.max(desiredSizePx, 0), constrainedSizePx)
                        : 0;
            case MeasurementSupport.MODE_UNDEFINED:
            default:
                return Math.max(desiredSizePx, 0);
        }
    }

    private static int resolveConstraintPx(Context context, float maxSize) {
        if (Float.isNaN(maxSize) || Float.isInfinite(maxSize)) {
            return 0;
        }
        return Math.max(Math.round(StyleHelper.standardUnitToPx(context, maxSize)), 0);
    }

    private static Object firstStyleValue(JSONObject styles, String... keys) {
        if (styles == null || keys == null) {
            return null;
        }
        for (String key : keys) {
            Object value = styles.opt(key);
            if (value != null && value != JSONObject.NULL) {
                return value;
            }
        }
        return null;
    }

    private static int parseInteger(Object value) {
        if (value == null || value == JSONObject.NULL) {
            return 0;
        }
        try {
            if (value instanceof Number) {
                return ((Number) value).intValue();
            }
            return Integer.parseInt(String.valueOf(value).trim());
        } catch (NumberFormatException ignored) {
            return 0;
        }
    }

    private static Float tryParseFloat(String value) {
        try {
            return Float.parseFloat(value);
        } catch (NumberFormatException ignored) {
            return null;
        }
    }
}
