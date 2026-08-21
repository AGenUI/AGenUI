package com.amap.agenui.render.style;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Outline;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.os.Build;
import android.text.Layout;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.LineHeightSpan;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.amap.a2ui_sdk.R;
import com.amap.agenui.AGenUI;
import com.amap.agenui.ColorValue;
import com.amap.agenui.EdgeInsetsValue;
import com.amap.agenui.render.drawable.ShadowPainter;
import com.amap.agenui.render.image.ImageCallback;
import com.amap.agenui.render.image.ImageLoadOptionsKey;
import com.amap.agenui.render.image.ImageLoadResult;
import com.amap.agenui.render.image.ImageLoaderConfig;
import com.amap.agenui.render.image.ImageLoaderError;
import com.amap.agenui.render.utils.AGenUILogger;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A2UI style helper utility class.
 *
 * Responsible for parsing and applying W3C CSS style properties to Android Views.
 * Supported styles include: dimensions, spacing, display, background, border, shadow, filter, etc.
 *
 */
public class StyleHelper {

    private static final String TAG = "StyleHelper";

    /**
     * Centralized style default values. All style properties reference these
     * constants so a default change is a one-line edit.
     */
    public static final class StyleDefaults {
        // Visual
        static final String DISPLAY = "flex";
        static final String VISIBILITY = "visible";
        static final float OPACITY = 1.0f;
        static final int BACKGROUND_COLOR = Color.TRANSPARENT;
        static final int BORDER_COLOR = Color.TRANSPARENT;
        static final int BORDER_RADIUS = 0;
        static final int BORDER_WIDTH = 0;
        static final String BORDER_STYLE = "solid";

        // Text
        public static final float FONT_SIZE_A2UI = 32f;   // 32 a2ui -> 16dp/16pt default text font size
        static final Typeface FONT_FAMILY = Typeface.DEFAULT;
        static final int FONT_WEIGHT = 400;
        static final int COLOR = Color.BLACK;
        static final String TEXT_ALIGN = "left";
        static final int LINE_CLAMP = 0;
        static final String TEXT_OVERFLOW = "ellipsis";
    }

    /**
     * {@link LineHeightSpan} implementation that redistributes the extra space evenly between
     * ascent and descent, so every line's glyph content-area is vertically centered inside the
     * target line box. This matches the W3C `line-height` semantics and the behavior of
     * Harmony ArkUI (`NODE_TEXT_LINE_HEIGHT`) and iOS (`paragraphStyle` with baseline offset).
     */
    public static final class CenteredLineHeightSpan implements LineHeightSpan {
        private final int lineHeightPx;

        public CenteredLineHeightSpan(int lineHeightPx) {
            this.lineHeightPx = lineHeightPx;
        }

        public int getLineHeightPx() {
            return lineHeightPx;
        }

        @Override
        public void chooseHeight(CharSequence text, int start, int end,
                                 int spanstartv, int lineHeight,
                                 Paint.FontMetricsInt fm) {
            int originHeight = fm.descent - fm.ascent;
            if (originHeight <= 0 || lineHeightPx <= 0) {
                return;
            }
            int extra = lineHeightPx - originHeight;
            int halfBefore = extra / 2;
            int halfAfter = extra - halfBefore;
            fm.ascent  -= halfBefore;
            fm.top      = fm.ascent;
            fm.descent += halfAfter;
            fm.bottom   = fm.descent;
        }
    }

    /**
     * Wrap the TextView's current text in a {@link SpannableString} and apply a
     * {@link CenteredLineHeightSpan} sized to {@code targetLineHeightPx}. Any previously
     * applied {@link CenteredLineHeightSpan} is removed first so repeated style updates are
     * idempotent. Keep this in sync with the equivalent logic in {@code TextMeasurer} so the
     * Yoga-measured height matches the rendered height exactly.
     */
    public static void applyCenteredLineHeight(TextView textView, int targetLineHeightPx) {
        if (textView == null || targetLineHeightPx <= 0) {
            return;
        }
        CharSequence current = textView.getText();
        if (current == null || current.length() == 0) {
            return;
        }
        SpannableString ss = (current instanceof SpannableString)
                ? (SpannableString) current
                : new SpannableString(current);
        // Remove any stale centered-line-height span before re-applying.
        CenteredLineHeightSpan[] existing = ss.getSpans(0, ss.length(), CenteredLineHeightSpan.class);
        for (CenteredLineHeightSpan span : existing) {
            ss.removeSpan(span);
        }
        ss.setSpan(new CenteredLineHeightSpan(targetLineHeightPx),
                0, ss.length(),
                Spanned.SPAN_INCLUSIVE_INCLUSIVE);
        textView.setText(ss);
    }

    /**
     * Parses a dimension value.
     * Supports: px, %, auto, match_parent, wrap_content.
     * Note: the px unit is converted following dp conversion rules.
     *
     * Cached by (density, normalized-string) so that repeated values like "0px" / "16px"
     * are parsed only once and only emit one debug log per unique input.
     */
    private static final ConcurrentHashMap<String, Integer> sDimensionCache = new ConcurrentHashMap<>();

    /**
     * Determine if font-weight value means bold.
     * Supports "bold", "normal", or numeric string (>=500 is bold).
     *
     * @param fontWeight font-weight value string
     * @return true if bold
     */
    public static boolean isBoldWeight(String fontWeight) {
        if (fontWeight == null) return false;
        String value = fontWeight.trim().toLowerCase();
        if ("bold".equals(value)) return true;
        if ("normal".equals(value)) return false;
        try {
            return Double.parseDouble(value) >= 500;
        } catch (NumberFormatException ignored) {
            return false;
        }
    }

    /** Numeric weight (Number or numeric String) vs keyword such as "bold" / "normal". */
    public static boolean isNumericFontWeight(Object fontWeight) {
        return parseInteger(fontWeight) != 0;
    }

    /**
     * Parses font-weight to a numeric CSS weight (100-900), one-to-one with iOS/AJX.
     * Keywords: "normal"->400, "medium"->500, "bold"->700; numeric 100-900 map to themselves;
     * anything else -> 400 (regular).
     *
     * <p>Accepts a raw value so a Number-boxed weight (e.g. Gson Double 500.0) is handled by parseInteger.
     *
     * @param fontWeight raw font-weight value (Number, String, or null)
     * @return numeric CSS weight (100-900)
     */
    public static int parseFontWeightValue(Object fontWeight) {
        if (fontWeight == null) return StyleDefaults.FONT_WEIGHT;
        String value = String.valueOf(fontWeight).trim().toLowerCase();
        switch (value) {
            case "normal": return 400;
            case "medium": return 500;
            case "bold":   return 700;
            default:       break;
        }
        switch (parseInteger(fontWeight)) {
            case 100: return 100;
            case 200: return 200;
            case 300: return 300;
            case 400: return 400;
            case 500: return 500;
            case 600: return 600;
            case 700: return 700;
            case 800: return 800;
            case 900: return 900;
            default:  return StyleDefaults.FONT_WEIGHT;
        }
    }

    /**
     * Creates a Typeface for the given numeric CSS weight (100-900).
     * On API 28+ uses Typeface.create(family, weight, false), which automatically matches the
     * nearest available weight, so medium/500 renders as true medium when the font provides it.
     * On older APIs only NORMAL/BOLD exist, so weight >= 600 degrades to bold, otherwise normal
     * (aligned with AJX TextMeasurement.createWeightedTypeface).
     *
     * <p>Italic is not applied: AGenUI has no font-style support and the base is always non-italic.
     *
     * @param base   base typeface (usually carries font-family), may be null
     * @param weight numeric CSS weight (100-900)
     * @return weighted Typeface
     */
    public static Typeface createWeightedTypeface(Typeface base, int weight) {
        Typeface family = (base != null) ? base : StyleDefaults.FONT_FAMILY;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return Typeface.create(family, weight, false);
        }
        int style = (weight >= 600) ? Typeface.BOLD : Typeface.NORMAL;
        return Typeface.create(family, style);
    }

    /**
     * Protocol-aligned weight resolution (fixes string font-weight not taking effect):
     * numeric font-weight (Number / numeric String) renders the real CSS weight via
     * {@link #createWeightedTypeface(Typeface, int)}; string keywords ("bold" / "normal")
     * fall back to the binary Typeface.BOLD/NORMAL path, since the A2UI catalog enum is
     * binary on Android (agent-context/architecture/a2ui-protocol.md).
     *
     * @param base    base typeface (usually carries font-family), may be null
     * @param weight  parsed CSS weight (keyword values arrive as 400/500/700)
     * @param numeric true if the raw font-weight was numeric, false for keyword strings
     */
    public static Typeface createWeightedTypeface(Typeface base, int weight, boolean numeric) {
        if (numeric) return createWeightedTypeface(base, weight);
        Typeface family = (base != null) ? base : StyleDefaults.FONT_FAMILY;
        int style = (weight >= 600) ? Typeface.BOLD : Typeface.NORMAL;
        return Typeface.create(family, style);
    }

    public static int parseDimension(Object value, Context context) {
        if (value == null) {
            return ViewGroup.LayoutParams.WRAP_CONTENT;
        }

        String strValue = String.valueOf(value).trim().toLowerCase();

        // Cache key includes density to avoid cross-device hits when context changes.
        float density = (context != null && context.getResources() != null)
                ? context.getResources().getDisplayMetrics().density
                : 1f;
        String cacheKey = density + ":" + strValue;
        Integer cached = sDimensionCache.get(cacheKey);
        if (cached != null) {
            return cached;
        }

        int result;
        if (strValue.equals("auto") || strValue.equals("wrap_content")) {
            result = ViewGroup.LayoutParams.WRAP_CONTENT;
        } else if (strValue.equals("match_parent") || strValue.equals("100%")) {
            result = ViewGroup.LayoutParams.MATCH_PARENT;
        } else {
            try {
                float numeric;
                if (strValue.endsWith("px")) {
                    numeric = Float.parseFloat(strValue.replace("px", ""));
                } else {
                    numeric = Float.parseFloat(strValue);
                }
                result = standardUnitToPx(context, numeric);
            } catch (NumberFormatException e) {
                AGenUILogger.w(TAG, "Failed to parse dimension: " + value, e);
                return ViewGroup.LayoutParams.WRAP_CONTENT;  // do not cache failures
            }
        }

        sDimensionCache.put(cacheKey, result);
        // Emit a single debug log per unique input. Repeated values reuse the cache without logging.
        AGenUILogger.d(TAG, "parseDimension '" + strValue + "' -> " + result + "px");
        return result;
    }


    /**
     * Applies display styles.
     * Supports: display, visibility, opacity.
     */
    public static void applyDisplay(View view, Map<String, Object> properties) {
        if (view == null || properties == null) return;

        // display (default: flex)
        String display = resolveString(properties, "display", StyleDefaults.DISPLAY);
        switch (display) {
            case "none":
                view.setVisibility(View.GONE);
                break;
            default:
                view.setVisibility(View.VISIBLE);
                break;
        }

        // visibility (default: visible)
        String visibility = resolveString(properties, "visibility", StyleDefaults.VISIBILITY);
        switch (visibility) {
            case "hidden":
                view.setVisibility(View.INVISIBLE);
                break;
            default:
                view.setVisibility(View.VISIBLE);
                break;
        }

        // opacity (default: 1.0f)
        view.setAlpha(resolveFloat(properties, "opacity", StyleDefaults.OPACITY));
    }


    /**
     * Applies background fill: solid color, gradient, or async image. Goes into the View's single
     * background slot ({@link View#setBackground}), drawn at the bottom of the View. Rounding is
     * handled separately by {@link #applyBorder} via outline clip — both can be called in any
     * order; clipping happens at draw time.
     *
     * <p>Supports: background-color, background, background-image.
     */
    public static void applyBackground(View view, Map<String, Object> styles) {
        if (view == null || styles == null) {
            return;
        }
        boolean hasAsyncBg = styles.containsKey("background-image");
        Drawable syncBg = parseSyncBackgroundDrawable(styles, view.getContext());
        view.setBackground(syncBg);
        if (hasAsyncBg) {
            loadBackgroundImageAsync(view, styles, syncBg);
        }
    }

    /**
     * Returns a Drawable for the {@code background-color} / {@code background} value.
     * Returns a transparent {@link ColorDrawable} when the key is absent or the value
     * is unparseable (aligned with iOS default .color(.clear)).
     */
    private static Drawable parseSyncBackgroundDrawable(Map<String, Object> styles, Context ctx) {
        Object raw = styles.containsKey("background-color")
                ? styles.get("background-color")
                : (styles.containsKey("background") ? styles.get("background") : null);
        if (raw == null) {
            return new ColorDrawable(StyleDefaults.BACKGROUND_COLOR);
        }
        String css = String.valueOf(raw).trim();
        if (css.isEmpty()) {
            return new ColorDrawable(StyleDefaults.BACKGROUND_COLOR);
        }
        ColorValue cv = AGenUI.nativeParseColor(css);
        if (cv == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "applyBackground: native parse failed for: " + raw);
            }
            return new ColorDrawable(StyleDefaults.BACKGROUND_COLOR);
        }
        if (cv.type == ColorValue.TYPE_GRADIENT && cv.gradient != null) {
            return GradientDrawableFactory.build(cv.gradient, ctx);
        }
        return new ColorDrawable(cv.solidColor);
    }

    private static void loadBackgroundImageAsync(View view, Map<String, Object> styles,
                                                   Drawable colorBg) {
        String imgUrl = extractUrlsFromCss(String.valueOf(styles.get("background-image")));
        if (imgUrl == null) {
            return;
        }
        view.post(() -> {
            int width = view.getWidth();
            int height = view.getHeight();
            ImageLoaderConfig.getInstance().getLoader().loadImage(imgUrl, buildOptions(width, height),
                    new ImageCallback() {
                        @Override
                        public void onSuccess(@NonNull ImageLoadResult result) {
                            if (colorBg != null) {
                                view.setBackground(new LayerDrawable(
                                        new Drawable[]{colorBg, result.drawable}));
                            } else {
                                view.setBackground(result.drawable);
                            }
                        }

                        @Override
                        public void onFailure(@NonNull ImageLoaderError error) {
                            if (AGenUILogger.isLoggingEnabled()) {
                                AGenUILogger.w(TAG, "background-image load failed, url=" + imgUrl, error);
                            }
                        }
                    });
        });
    }

    /**
     * Builds background image load options including target width and height for downsampling.
     */
    private static Map<String, Object> buildOptions(int width, int height) {
        if (width <= 0 && height <= 0) return null;
        Map<String, Object> options = new HashMap<>();
        if (width > 0) options.put(ImageLoadOptionsKey.WIDTH, (float) width);
        if (height > 0) options.put(ImageLoadOptionsKey.HEIGHT, (float) height);
        return options;
    }

    /**
     * Applies border styles. Two independent sub-mechanisms triggered by different keys:
     *
     * <pre>
     *   border-radius                         → outline + clipToOutline
     *                                           (rounds the View; clips bg AND content drawing)
     *   border-width (+ border-color)         → stroke-only Drawable in ViewOverlay
     *                                           (drawn ABOVE content, stays visible over an
     *                                           ImageView's bitmap)
     * </pre>
     *
     * The two share only the radius value — when both are present the overlay stroke is rounded
     * to the same shape as the outline. {@code border-radius} alone produces a rounded box with
     * no stroke; {@code border-width} alone produces a rectangular stroke.
     *
     * <p>Supports: border-radius, border-width, border-color.
     */
    public static void applyBorder(View view, Map<String, Object> styles) {
        if (view == null || styles == null) {
            return;
        }
        Context ctx = view.getContext();
        int radiusPx = resolveDimension(styles, "border-radius", StyleDefaults.BORDER_RADIUS, ctx);
        int borderWidth = resolveDimension(styles, "border-width", StyleDefaults.BORDER_WIDTH, ctx);
        int borderColor = resolveColor(styles, "border-color", StyleDefaults.BORDER_COLOR);

        applyOutlineRadiusClip(view, radiusPx);
        applyBorderOverlay(view, borderWidth, borderColor, radiusPx);
    }

    /**
     * {@link #parseDimension} returns MATCH_PARENT/WRAP_CONTENT (negative) for non-numeric tokens
     * and 0 for missing values. Callers that only care about a positive pixel count want both
     * normalized to 0.
     */
    private static int parseDimensionOrZero(Object value, Context ctx) {
        if (value == null) return 0;
        return Math.max(0, parseDimension(value, ctx));
    }

    // ------------------------------------------------------------------
    // Resolve helpers: absent → defaultValue, parse-fail → defaultValue.
    // Mirrors iOS `ifPresent(key, defaultValue)` — both absent AND parse
    // failure fall back to defaultValue. Does NOT call parseColor/parseFloat
    // (which return zero-value fallbacks on failure); inlines parse logic so
    // failure is detected and defaultValue is returned.
    // ------------------------------------------------------------------

    /** Resolve a string style property with a default value (lowercased). */
    private static String resolveString(Map<String, Object> styles, String key, String defaultValue) {
        if (!styles.containsKey(key)) return defaultValue;
        Object value = styles.get(key);
        if (value == null) return defaultValue;
        return String.valueOf(value).trim().toLowerCase();
    }

    /** Resolve a float style property with a default value. */
    private static float resolveFloat(Map<String, Object> styles, String key, float defaultValue) {
        if (!styles.containsKey(key)) return defaultValue;
        Object value = styles.get(key);
        if (value == null) return defaultValue;
        try {
            if (value instanceof Number) return ((Number) value).floatValue();
            return Float.parseFloat(String.valueOf(value));
        } catch (NumberFormatException e) {
            return defaultValue;
        }
    }

    /** Resolve a color style property with a default value. */
    private static int resolveColor(Map<String, Object> styles, String key, int defaultValue) {
        if (!styles.containsKey(key)) return defaultValue;
        Object value = styles.get(key);
        if (value == null) return defaultValue;
        String css = String.valueOf(value).trim();
        if (css.isEmpty()) return defaultValue;
        ColorValue cv = AGenUI.nativeParseColor(css);
        if (cv == null || cv.type == ColorValue.TYPE_GRADIENT) return defaultValue;
        return cv.solidColor;
    }

    /** Resolve a dimension style property with a default value (px, clamped >= 0). */
    private static int resolveDimension(Map<String, Object> styles, String key, int defaultValue, Context ctx) {
        if (!styles.containsKey(key)) return defaultValue;
        Object value = styles.get(key);
        if (value == null) return defaultValue;
        int result = parseDimension(value, ctx);
        return result >= 0 ? result : defaultValue;
    }

    /** Resolve an integer style property with a default value (missing key, null, empty, or parse failure → default). */
    private static int resolveInteger(Map<String, Object> styles, String key, int defaultValue) {
        if (!styles.containsKey(key)) return defaultValue;
        Object value = styles.get(key);
        if (value == null) return defaultValue;
        if (value instanceof Number) {
            return ((Number) value).intValue();
        }
        String s = String.valueOf(value).trim();
        if (s.isEmpty()) return defaultValue;
        try {
            return Integer.parseInt(s);
        } catch (NumberFormatException e) {
            return defaultValue;
        }
    }

    /**
     * Installs a {@link ViewOutlineProvider} that rounds the view to {@code radiusPx} and turns
     * on outline clipping, so the background drawable AND any child views are masked to the
     * rounded shape. Resets to the default provider when {@code radiusPx <= 0}, which is
     * required when an update removes a previous border-radius — otherwise stale rounding
     * persists.
     *
     * <p>Also publishes the radius via the {@code agenui_corner_radius} tag: clipToOutline is a
     * RenderNode-only property that silently does nothing on software canvases (screenshots via
     * {@code view.draw(bitmapCanvas)}), so parent containers read this tag in their
     * {@code drawChild} to apply an equivalent clipPath — see
     * {@link com.amap.agenui.render.drawable.SoftwareCornerClip}.
     */
    private static void applyOutlineRadiusClip(View view, int radiusPx) {
        if (radiusPx <= 0) {
            view.setClipToOutline(false);
            view.setOutlineProvider(ViewOutlineProvider.BACKGROUND);
            view.setTag(R.id.agenui_corner_radius, null);
            return;
        }
        final float r = radiusPx;
        view.setOutlineProvider(new ViewOutlineProvider() {
            @Override
            public void getOutline(View v, Outline outline) {
                outline.setRoundRect(0, 0, v.getWidth(), v.getHeight(), r);
            }
        });
        view.setClipToOutline(true);
        view.setTag(R.id.agenui_corner_radius, radiusPx);
    }

    /**
     * Adds (or removes) a stroke-only Drawable to the View's overlay so the border draws on top
     * of the View's content — including an ImageView's bitmap. The drawable's bounds are kept in
     * sync with the View via an {@link View.OnLayoutChangeListener}; both the drawable and the
     * listener are stored on the View as a tag so a subsequent update can dispose of them
     * cleanly (idempotent).
     *
     * <p>Pairs with {@link #applyOutlineRadiusClip}: the outline clip is what actually rounds
     * the corners of this overlay drawable.
     */
    private static void applyBorderOverlay(View view, int borderWidth, int borderColor, int radiusPx) {
        BorderOverlayState prev = (BorderOverlayState) view.getTag(R.id.agenui_border_overlay);
        if (prev != null) {
            view.getOverlay().remove(prev.drawable);
            view.removeOnLayoutChangeListener(prev.listener);
            view.setTag(R.id.agenui_border_overlay, null);
        }
        if (borderWidth <= 0) {
            return;
        }

        // Respect color override if set (e.g. error state)
        Object overrideTag = view.getTag(R.id.agenui_border_color_override);
        int effectiveColor = (overrideTag instanceof Integer) ? (int) overrideTag : borderColor;

        final GradientDrawable stroke = new GradientDrawable();
        stroke.setShape(GradientDrawable.RECTANGLE);
        stroke.setColor(Color.TRANSPARENT);
        stroke.setStroke(borderWidth, effectiveColor);
        if (radiusPx > 0) {
            // GradientDrawable applies the corner radius to the stroke's CENTRE line, not to its
            // outer edge: draw() insets the rect by strokeWidth/2 and then passes the radius
            // through unchanged (AOSP GradientDrawable.draw, "inset = strokeWidth * 0.5f" then
            // drawRoundRect(mRect, rad, rad, mStrokePaint)). So a raw radius R renders an outer
            // edge of R + W/2 and an inner edge of R - W/2, while applyOutlineRadiusClip applies
            // R to the OUTER edge. On a straight edge the two agree; at the corners they differ
            // by W/2, which shows up as the clip shaving the border's outer arc and the fill
            // bulging into the border band.
            //
            // Subtracting W/2 lines them up: outer edge lands on R (matching the clip) and the
            // inner edge on R - W, which is the concentric inner radius CSS specifies. Clamped at
            // 0 for the W/2 > R case, where the corner degenerates to square anyway.
            stroke.setCornerRadius(Math.max(0f, radiusPx - borderWidth / 2f));
        }
        stroke.setBounds(0, 0, view.getWidth(), view.getHeight());
        view.getOverlay().add(stroke);

        View.OnLayoutChangeListener listener = (v, l, t, r, b, ol, ot, or, ob) ->
                stroke.setBounds(0, 0, r - l, b - t);
        view.addOnLayoutChangeListener(listener);

        view.setTag(R.id.agenui_border_overlay, new BorderOverlayState(stroke, borderWidth, radiusPx, listener));
    }

    /**
     * Sets a border color override on the view. The override takes precedence over the
     * style-defined border-color whenever applyBorder is called, and also immediately
     * updates the current overlay if one exists.
     */
    public static void setBorderColorOverride(View view, int color) {
        if (view == null) return;
        view.setTag(R.id.agenui_border_color_override, color);
        // Immediately update existing overlay
        BorderOverlayState state = (BorderOverlayState) view.getTag(R.id.agenui_border_overlay);
        if (state != null && state.drawable instanceof GradientDrawable) {
            ((GradientDrawable) state.drawable).setStroke(state.borderWidth, color);
        }
    }

    /**
     * Clears the border color override, restoring the style-defined border-color.
     * Immediately updates the existing overlay if one exists.
     */
    public static void clearBorderColorOverride(View view) {
        if (view == null) return;
        view.setTag(R.id.agenui_border_color_override, null);
        // Re-apply border from scratch to restore original color
        BorderOverlayState state = (BorderOverlayState) view.getTag(R.id.agenui_border_overlay);
        if (state != null && state.drawable instanceof GradientDrawable) {
            // We don't store the original color here; trigger a full re-apply via the stored dimensions.
            // The caller should call applyBorder after clearing if they want the original color back.
            // For immediate visual feedback, we leave the overlay as-is; the next applyBorder cycle
            // (from updateProperties) will restore it.
        }
    }

    /** Pairs the overlay drawable with its layout listener for clean teardown on re-apply. */
    private static final class BorderOverlayState {
        final Drawable drawable;
        final int borderWidth;
        final int radiusPx;
        final View.OnLayoutChangeListener listener;

        BorderOverlayState(Drawable drawable, int borderWidth, int radiusPx,
                           View.OnLayoutChangeListener listener) {
            this.drawable = drawable;
            this.borderWidth = borderWidth;
            this.radiusPx = radiusPx;
            this.listener = listener;
        }
    }


    /**
     * Applies filter styles: currently only drop-shadow is supported.
     *
     * <p>The shadow config is attached to the view via {@link ShadowPainter} and painted
     * by the parent container's {@code drawChild}, so it never changes the Z-order.
     *
     * <p>For {@code TextComponent}, {@link #applyTextStyles} subsequently overrides this
     * with a per-glyph text shadow and clears the box shadow config. Other TextView-based
     * components (e.g. RichText) keep the box shadow set here.
     */
    public static void applyFilter(View view, Map<String, Object> styles) {
        if (view == null || styles == null) {
            return;
        }
        ShadowPainter.ShadowConfig config = parseDropShadowConfig(view.getContext(), styles);
        ShadowPainter.setConfig(view, config);
    }

    /**
     * Parses {@code filter: drop-shadow(offsetX offsetY blur color)} combined with
     * {@code border-radius} into a complete {@link ShadowPainter.ShadowConfig}.
     *
     * @return config or {@code null} if no drop-shadow filter is present.
     */
    public static ShadowPainter.ShadowConfig parseDropShadowConfig(Context context,
                                                             Map<String, Object> styles) {
        if (!styles.containsKey("filter")) {
            return null;
        }
        String filter = String.valueOf(styles.get("filter")).trim();
        if (!filter.startsWith("drop-shadow(") || !filter.endsWith(")")) {
            return null;
        }
        String params = filter.substring(12, filter.length() - 1).trim();
        if (params.isEmpty()) {
            return null;
        }

        String[] rawParts = params.split("\\s+");
        List<String> lengthTokens = new ArrayList<>(3);
        StringBuilder colorBuilder = new StringBuilder();
        for (String part : rawParts) {
            if (lengthTokens.size() < 3
                    && (part.endsWith("px") || part.matches("-?\\d+(\\.\\d+)?"))) {
                lengthTokens.add(part);
            } else {
                if (colorBuilder.length() > 0) colorBuilder.append(' ');
                colorBuilder.append(part);
            }
        }
        if (lengthTokens.size() < 3) {
            return null;
        }

        try {
            int offsetX = Math.round(parseDimensionFloat(lengthTokens.get(0), context));
            int offsetY = Math.round(parseDimensionFloat(lengthTokens.get(1), context));
            int blurRadius = Math.max(0, Math.round(parseDimensionFloat(lengthTokens.get(2), context)));
            String colorStr = colorBuilder.toString().trim();
            if (colorStr.isEmpty()) {
                return null;
            }
            int color = parseColor(colorStr);
            // Fold the element's declared opacity into the shadow alpha here, at config-build time,
            // instead of reading View.getAlpha() at draw time. During an opacity animation getAlpha()
            // is a transient interpolated value, and the shadow's draw pass (parent drawChild) is not
            // guaranteed to re-run when alpha changes via the RenderNode fast-path — which left the
            // shadow stuck/invisible. The declared opacity from the style payload is stable.
            if (styles.containsKey("opacity")) {
                float opacity = Math.max(0f, Math.min(1f, parseFloat(styles.get("opacity"))));
                int alpha = Math.round(((color >>> 24) & 0xFF) * opacity);
                color = (alpha << 24) | (color & 0x00FFFFFF);
            }
            // Fully transparent (raw color or after applying opacity) → no shadow.
            if ((color >>> 24) == 0) {
                return null;
            }
            int cornerRadius = parseDimensionOrZero(styles.get("border-radius"), context);
            return new ShadowPainter.ShadowConfig(color, offsetX, offsetY, blurRadius, cornerRadius);
        } catch (Exception e) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "Failed to parse drop-shadow: " + filter, e);
            }
            return null;
        }
    }

    /**
     * Parses a dimension value as a float (used for shadow offsets, etc.).
     * Note: the px unit is converted following dp conversion rules.
     */
    public static float parseDimensionFloat(String value, Context context) {
        if (value == null || value.isEmpty()) return 0f;

        value = value.trim().toLowerCase();
        try {
            if (value.endsWith("px")) {
                // px unit is converted following dp conversion rules
                float value_num = Float.parseFloat(value.replace("px", ""));
                return standardUnitToPx(context, value_num);
            } else {
                // Treat as standard unit by default
                return standardUnitToPx(context, Float.parseFloat(value));
            }
        } catch (NumberFormatException e) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "Failed to parse dimension float: " + value, e);
            }
            return 0f;
        }
    }


    /**
     * Applies overflow styles. Supports: overflow.
     *
     * <p>Mechanism: {@link View#setClipBounds} self-clip on the DECLARING view. This is the only
     * primitive whose flag lives on the declarer itself — {@code setClipChildren} on self is
     * off-by-one-level (a parent's flag clips each child to its own rect, AOSP
     * ViewGroup.setClipChildren → child.renderNode.setClipToBounds), and setting it on the
     * parent leaks a shared flag onto siblings. {@code setClipBounds} is orthogonal to
     * {@code setClipToOutline}: rect ∩ rounded outline = rounded clip, so border-radius handled
     * by {@link #applyBorder} needs no coordination.
     *
     * <p>The clip box is the BORDER box (the view's own bounds), chosen deliberately for
     * cross-platform consistency: iOS clips with {@code UIView.clipsToBounds} and HarmonyOS with
     * ArkUI {@code NODE_CLIP}, both of which clip at the node's own boundary, and neither
     * platform ever applies the CSS padding to its platform view (padding is consumed by Yoga to
     * offset child frames). Clipping at the padding box here would leave Android as the only
     * platform that cuts a padding's width earlier than the other two for the same payload.
     *
     * <p>This is a KNOWN, DELIBERATE DEVIATION from CSS Overflow L3, which specifies the padding
     * box. It was verified on device that iOS and HarmonyOS both let content bleed into the
     * padding area on the right/bottom edges; aligning all three on the border box was preferred
     * over one platform being correct and two being wrong. If CSS conformance is prioritised
     * later, all three platforms have to change together — on Android that means taking the
     * insets from the styles map ({@code View.getPadding*()} is always 0 on Yoga-driven
     * containers, since Yoga consumes the padding) and skipping the clip when the padding
     * exceeds the view size, which would otherwise produce an inverted, content-erasing rect.
     *
     * <p>{@code border-radius} and {@code overflow} are two inputs to ONE clip decision, and a
     * positive radius is a peer of the clipping keywords rather than a fallback consulted only when
     * {@code overflow} is absent. See {@link #resolveClipDecision} for the exact table; the
     * authority is HarmonyOS {@code a2ui_component.cpp}, which drives its single {@code NODE_CLIP}
     * flag from both keys in the same block.
     *
     * <p>Because the caller passes the component's full accumulated styles, whichever branch wins
     * must also maintain the {@code agenui_overflow_hidden} tag: a stale tag permanently
     * short-circuits ShadowPainter's clip-guard. The tag therefore tracks the resolved decision,
     * not the literal {@code overflow} keyword — a container clipped only because it is rounded
     * counts as clipping, which is what that guard needs to know.
     */
    public static void applyOverflow(ViewGroup viewGroup, Map<String, Object> styles) {
        if (viewGroup == null || styles == null) return;

        switch (resolveClipDecision(styles, viewGroup.getContext())) {
            case CLIP_ON:
                enableSelfClip(viewGroup);
                viewGroup.setTag(R.id.agenui_overflow_hidden, Boolean.TRUE);
                break;
            case CLIP_OFF:
                disableSelfClip(viewGroup);
                viewGroup.setTag(R.id.agenui_overflow_hidden, null);
                break;
            default:
                // CLIP_UNSPECIFIED: keep the clip state that is already in effect.
                break;
        }
    }

    /** Leave the current clip state alone. */
    private static final int CLIP_UNSPECIFIED = 0;
    /** Establish a clipping viewport. */
    private static final int CLIP_ON = 1;
    /** Explicitly remove any clip. */
    private static final int CLIP_OFF = 2;

    /**
     * Resolves {@code border-radius} + {@code overflow} into one of three outcomes, mirroring the
     * {@code NODE_CLIP} block in HarmonyOS {@code a2ui_component.cpp} condition for condition:
     *
     * <pre>
     *   radius &gt; 0 || overflow == hidden || overflow == scroll   -&gt; CLIP_ON
     *   hasRadiusKey || overflow == visible                       -&gt; CLIP_OFF
     *   otherwise                                                 -&gt; CLIP_UNSPECIFIED
     * </pre>
     *
     * <p>Three things about this table are easy to get wrong:
     *
     * <ul>
     *   <li><b>A positive radius is a peer of the clipping keywords, not a fallback.</b> It is
     *       OR-ed with them, so it clips even against an explicit {@code overflow: visible} — the
     *       same precedence Android already had via {@code setClipToOutline}, which stays on
     *       regardless of {@code overflow}. Reading the radius only when {@code overflow} is absent
     *       would let {@code visible} silently un-round a rounded container's clip.</li>
     *   <li><b>{@code scroll} clips like {@code hidden}</b>, and {@code visible} is the only
     *       keyword that turns the clip off.</li>
     *   <li><b>{@code auto}, and any unknown keyword, is CLIP_UNSPECIFIED</b> — a no-op that
     *       preserves the current state, not a reset to {@code visible}. Both other platforms
     *       leave such values unhandled (iOS only logs in its {@code default} branch), so resetting
     *       here would un-clip a container that was already clipping.</li>
     * </ul>
     *
     * <p>A {@code border-radius} key that is present but resolves to zero falls into CLIP_OFF: that
     * is the reset path for a radius being removed or zeroed. A present-but-null {@code overflow}
     * counts as absent.
     */
    private static int resolveClipDecision(Map<String, Object> styles, Context ctx) {
        String overflow = "";
        Object declared = styles.get("overflow");
        if (declared != null) {
            overflow = String.valueOf(declared).trim().toLowerCase();
        }

        boolean hasRadiusKey = styles.containsKey("border-radius");
        int radiusPx = hasRadiusKey ? parseDimensionOrZero(styles.get("border-radius"), ctx) : 0;

        if (radiusPx > 0 || "hidden".equals(overflow) || "scroll".equals(overflow)) {
            return CLIP_ON;
        }
        if (hasRadiusKey || "visible".equals(overflow)) {
            return CLIP_OFF;
        }
        return CLIP_OFF;
    }

    /**
     * Turns on border-box self-clipping. The clip rect must track the view's size, so an
     * {@link View.OnLayoutChangeListener} re-applies it on every layout. The listener is
     * installed only once (idempotence guard via {@code agenui_overflow_clip_listener}) and is
     * removed again by {@link #disableSelfClip}, so no separate "clip enabled" marker is needed:
     * nothing calls {@link #applySelfClipRect} once the clip has been turned off.
     */
    private static void enableSelfClip(View view) {
        applySelfClipRect(view);
        if (view.getTag(R.id.agenui_overflow_clip_listener) == null) {
            View.OnLayoutChangeListener listener = new View.OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int l, int t, int r, int b,
                                           int ol, int ot, int or, int ob) {
                    applySelfClipRect(v);
                }
            };
            view.addOnLayoutChangeListener(listener);
            view.setTag(R.id.agenui_overflow_clip_listener, listener);
        }
    }

    private static void disableSelfClip(View view) {
        view.setClipBounds(null);
        Object listener = view.getTag(R.id.agenui_overflow_clip_listener);
        if (listener instanceof View.OnLayoutChangeListener) {
            view.removeOnLayoutChangeListener((View.OnLayoutChangeListener) listener);
        }
        view.setTag(R.id.agenui_overflow_clip_listener, null);
    }

    /**
     * Applies the border-box clip rect for the current size. Skips when the view has no size yet
     * (creation time, before the first layout) — an empty clip rect would blank the first frame;
     * the layout listener installed by {@link #enableSelfClip} applies it right after the first
     * layout pass. {@code setClipBounds} is idempotent (AOSP early-returns on equal rects), so
     * re-applying on every layout is cheap.
     *
     * <p>Clipping to the full bounds is not a no-op: without an explicit clip rect a child can
     * still paint outside this view when an ancestor does not clip (YogaAbsoluteLayout sets
     * {@code clipChildren=false}), which is exactly the overflow this method exists to cut.
     */
    private static void applySelfClipRect(View view) {
        int w = view.getWidth();
        int h = view.getHeight();
        if (w <= 0 || h <= 0) {
            return;
        }
        view.setClipBounds(new Rect(0, 0, w, h));
    }


    /**
     * Parsed text style properties with defaults (mirrors iOS ParsedTextStyles).
     * Fields carry their default values; from() only overwrites when the key is
     * present and parsing succeeds — absent or parse-fail keeps the default.
     */
    public static class ParsedTextStyles {
        public float fontSizeA2ui = StyleDefaults.FONT_SIZE_A2UI;
        public boolean hasFontSize = false;
        public int fontWeight = StyleDefaults.FONT_WEIGHT;
        /** True when the raw font-weight was numeric (Number / numeric String); keyword
         *  strings keep the binary bold/normal path (see createWeightedTypeface(base,w,numeric)). */
        public boolean fontWeightIsNumeric = true;
        public Typeface fontFamily = StyleDefaults.FONT_FAMILY;
        public int color = StyleDefaults.COLOR;
        public String textAlign = StyleDefaults.TEXT_ALIGN;
        public int lineClamp = StyleDefaults.LINE_CLAMP;
        public String textOverflow = StyleDefaults.TEXT_OVERFLOW;
        public float lineHeightMultiplier = 0f;
        public int lineHeightAbsPx = 0;

        public static ParsedTextStyles from(Map<String, Object> styles, Context context) {
            ParsedTextStyles p = new ParsedTextStyles();
            if (styles == null || styles.isEmpty()) return p;

            if (styles.containsKey("font-family")) {
                p.fontFamily = parseFontFamily(styles.get("font-family"), context);
            }
            if (styles.containsKey("font-weight")) {
                Object fontWeightValue = styles.get("font-weight");
                p.fontWeight = parseFontWeightValue(fontWeightValue);
                p.fontWeightIsNumeric = isNumericFontWeight(fontWeightValue);
            }
            Object fontSize = styles.get("font-size");
            if (fontSize != null) {
                float sizeA2ui = parseFontSizeA2ui(fontSize);
                if (sizeA2ui > 0) {
                    p.fontSizeA2ui = sizeA2ui;
                    p.hasFontSize = true;
                }
            }
            p.color = resolveColor(styles, "color", StyleDefaults.COLOR);

            Object textAlign = styles.get("text-align");
            if (textAlign != null) {
                String s = String.valueOf(textAlign).trim().toLowerCase();
                if (!s.isEmpty()) p.textAlign = s;
            }
            p.lineClamp = resolveInteger(styles, "line-clamp", StyleDefaults.LINE_CLAMP);
            Object textOverflow = styles.get("text-overflow");
            if (textOverflow != null) {
                String s = String.valueOf(textOverflow).trim().toLowerCase();
                if (!s.isEmpty()) p.textOverflow = s;
            }
            Object lineHeight = styles.get("line-height");
            if (lineHeight != null) {
                String s = String.valueOf(lineHeight).trim().toLowerCase();
                if (s.matches("^\\d+(\\.\\d+)?$")) {
                    float multiplier = Float.parseFloat(s);
                    if (multiplier > 0f) p.lineHeightMultiplier = multiplier;
                } else if (s.endsWith("px")) {
                    int px = parseDimension(lineHeight, context);
                    if (px > 0) p.lineHeightAbsPx = px;
                }
            }

            return p;
        }

        /** Resolve line-height to px given the effective text size in px. Shared by render/measure. */
        public int resolveLineHeightPx(float textSizePx) {
            if (lineHeightMultiplier > 0f) return Math.round(lineHeightMultiplier * textSizePx);
            if (lineHeightAbsPx > 0) return lineHeightAbsPx;
            return 0;
        }

        /** Map text-overflow + maxLines to an ellipsize mode. Shared by render/measure. */
        public static TextUtils.TruncateAt resolveEllipsize(String textOverflow, int maxLines) {
            switch (textOverflow) {
                case "ellipsis":
                    return (maxLines > 0 && maxLines < Integer.MAX_VALUE) ? TextUtils.TruncateAt.END : null;
                case "head":
                    return (maxLines == 1) ? TextUtils.TruncateAt.START : null;
                case "middle":
                    return (maxLines == 1) ? TextUtils.TruncateAt.MIDDLE : null;
                case "clip":
                default:
                    return null;
            }
        }
    }

    /** Parse font-size string into a2ui value. Returns 0 on parse failure. */
    private static float parseFontSizeA2ui(Object value) {
        if (value == null) return 0;
        String sizeStr = String.valueOf(value).trim().toLowerCase();
        try {
            if (sizeStr.endsWith("px")) {
                return Float.parseFloat(sizeStr.replace("px", ""));
            } else if (sizeStr.matches("^\\d+(\\.\\d+)?$")) {
                return Float.parseFloat(sizeStr);
            }
        } catch (NumberFormatException e) {
            AGenUILogger.w(TAG, "Failed to parse font-size: " + value, e);
        }
        return 0;
    }

    /**
     * Applies text styles to a TextView.
     * Supports all style properties of TextComponent.
     *
     * @param textView TextView to apply styles to
     * @param styles   Style property map
     * @param context  Android Context
     */
    @SuppressLint("WrongConstant")
    public static void applyTextStyles(TextView textView, Map<String, Object> styles, Context context) {
        if (textView == null || styles == null) return;

        ParsedTextStyles p = ParsedTextStyles.from(styles, context);

        // 1. Font: family + weight + size (composed together)
        // Numeric weight -> real CSS weight; string keyword -> binary bold/normal path.
        textView.setTypeface(createWeightedTypeface(p.fontFamily, p.fontWeight, p.fontWeightIsNumeric));
        textView.getPaint().setFakeBoldText(false);
        textView.setTextSize(TypedValue.COMPLEX_UNIT_PX, standardUnitToPx(context, p.fontSizeA2ui));

        // 2. Color
        textView.setTextColor(p.color);

        // 3. Line-height (only override when explicitly set)
        // W3C semantics: line box = multiplier * font-size (or px value).
        // CenteredLineHeightSpan redistributes extra space evenly, matching iOS/Harmony.
        // Parsed in ParsedTextStyles (shared with TextMeasurer).
        int targetLineHeightPx = p.resolveLineHeightPx(textView.getTextSize());
        if (targetLineHeightPx > 0) {
            textView.setLineSpacing(0f, 1.0f);
            applyCenteredLineHeight(textView, targetLineHeightPx);
        }

        // 4. Line-clamp
        if (p.lineClamp > 0) {
            textView.setMaxLines(p.lineClamp);
        } else {
            textView.setMaxLines(Integer.MAX_VALUE);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            textView.setBreakStrategy(Layout.BREAK_STRATEGY_SIMPLE);
            textView.setHyphenationFrequency(Layout.HYPHENATION_FREQUENCY_NONE);
        }

        // 5. Text-overflow (shared resolver with TextMeasurer)
        int currentMaxLines = textView.getMaxLines();
        textView.setEllipsize(ParsedTextStyles.resolveEllipsize(p.textOverflow, currentMaxLines));

        // 6. Text-align
        int gravity = parseTextAlign(p.textAlign);
        if (gravity != -1) textView.setGravity(gravity);

        // 7. Text decoration
        applyTextDecoration(textView, styles, context);

        // 8. CSS padding -> TextView.setPadding
        applyTextPadding(textView, styles, context);

        // 9. Filter: drop-shadow -> per-glyph text shadow
        ShadowPainter.setConfig(textView, null);
        ShadowPainter.ShadowConfig shadowConfig = parseDropShadowConfig(context, styles);
        if (shadowConfig != null) {
            textView.setShadowLayer(shadowConfig.blurRadius, shadowConfig.offsetX,
                    shadowConfig.offsetY, shadowConfig.shadowColor);
            textView.setLayerType(View.LAYER_TYPE_SOFTWARE, null);
        } else {
            textView.setShadowLayer(0, 0, 0, Color.TRANSPARENT);
            textView.setLayerType(View.LAYER_TYPE_NONE, null);
        }
    }

    /**
     * Applies CSS `padding` (and the four physical sub-properties) to a TextView.
     *
     * Supports the same shorthand grammar as W3C CSS:
     *   - 1 value : all four sides
     *   - 2 values: vertical | horizontal
     *   - 3 values: top | horizontal | bottom
     *   - 4 values: top | right | bottom | left
     *
     * Per-side overrides (`padding-top` / `padding-right` / `padding-bottom` /
     * `padding-left`) take precedence over the shorthand value.  Each token is
     * parsed via {@link #parseDimension(Object, Context)} so dp conversion
     * matches the rest of the engine.
     *
     * NOTE: Yoga's `padding` already shapes the leaf TextView's borderBox; this
     * method only narrows the glyph area inside that borderBox so the rendered
     * text stops at the contentBox.  See applyTextStyles step 8b for context.
     *
     * @param textView Target TextView
     * @param styles   Style map
     * @param context  Android Context (required for dp conversion)
     */
    private static void applyTextPadding(TextView textView, Map<String, Object> styles, Context context) {
        applyCSSPadding(textView, styles, context);
    }

    /**
     * Apply CSS `padding` (and `padding-top/right/bottom/left` overrides) to
     * any leaf-style {@link View} via {@link View#setPadding(int, int, int, int)}.
     *
     * <p>Used by Text/Image/Button (and any other leaf component whose native
     * draw area defaults to filling the entire frame). The C++ Yoga engine
     * has already accounted for padding when sizing the leaf's borderBox, so
     * this call is what actually shrinks the rendered content (glyph /
     * bitmap / Stack-centered child) into the contentBox. setPadding does
     * NOT change the view's outer size, so this is not a double-count with
     * Yoga.
     *
     * <p>Supports W3C 1/2/3/4-component shorthand and the four single-edge
     * overrides. When no padding key is present in {@code styles}, the
     * existing padding on the view is left untouched.
     */
    public static void applyCSSPadding(View view, Map<String, Object> styles, Context context) {
        if (view == null) {
            return;
        }
        Rect padding = resolveCSSPaddingPx(styles, context);
        view.setPadding(padding.left, padding.top, padding.right, padding.bottom);
    }

    /**
     * Resolve CSS {@code padding} (and per-edge overrides) from a styles map
     * into a {@link Rect} carrying ({@code left}, {@code top}, {@code right},
     * {@code bottom}) values in px. Mirrors {@link #applyCSSPadding}'s parsing
     * rules but does not touch any view; intended for callers that need the
     * numeric padding values themselves (e.g. scroll-content sizing for a
     * RecyclerView-backed list whose right/bottom padding gutter must extend
     * the scrollable range).
     *
     * <p>Returns an all-zero {@code Rect} when no {@code padding*} key is present
     * (aligned with the "absent → default value" spec).
     */
    public static Rect resolveCSSPaddingPx(@Nullable Map<String, Object> styles,
                                           @NonNull Context context) {
        Rect out = new Rect(0, 0, 0, 0);
        if (styles == null || styles.isEmpty()) {
            return out;
        }

        int topPx = 0, rightPx = 0, bottomPx = 0, leftPx = 0;

        if (styles.containsKey("padding")) {
            String shorthand = String.valueOf(styles.get("padding")).trim();
            if (!shorthand.isEmpty() && !shorthand.equalsIgnoreCase("null")) {
                EdgeInsetsValue insets = AGenUI.nativeParseEdgeInsets(shorthand);
                if (insets != null) {
                    topPx    = resolveSidePx(insets.top,    context);
                    rightPx  = resolveSidePx(insets.right,  context);
                    bottomPx = resolveSidePx(insets.bottom, context);
                    leftPx   = resolveSidePx(insets.left,   context);
                }
            }
        }

        if (styles.containsKey("padding-top")) {
            topPx = parseDimension(styles.get("padding-top"), context);
        }
        if (styles.containsKey("padding-right")) {
            rightPx = parseDimension(styles.get("padding-right"), context);
        }
        if (styles.containsKey("padding-bottom")) {
            bottomPx = parseDimension(styles.get("padding-bottom"), context);
        }
        if (styles.containsKey("padding-left")) {
            leftPx = parseDimension(styles.get("padding-left"), context);
        }

        // Guard against parseDimension returning negative sentinel values such as
        // ViewGroup.LayoutParams.WRAP_CONTENT (-2) when the source value is
        // "auto" or otherwise unparsable; treat those as zero to avoid setting
        // a negative padding which Android silently clamps to 0 anyway.
        if (topPx    < 0) topPx    = 0;
        if (rightPx  < 0) rightPx  = 0;
        if (bottomPx < 0) bottomPx = 0;
        if (leftPx   < 0) leftPx   = 0;
        out.set(leftPx, topPx, rightPx, bottomPx);
        return out;
    }

    /**
     * Resolve a single edge value parsed by {@link AGenUI#nativeParseEdgeInsets}
     * into absolute pixels. Only {@code px} (and unitless, which the C++ parser
     * normalizes to px) is honored — every other unit collapses to 0. The
     * cross-platform render layer intentionally only consumes px so that the
     * three platforms stay byte-for-byte aligned without dragging viewport,
     * font-size, or physical-unit machinery into platform code.
     */
    private static int resolveSidePx(EdgeInsetsValue.EdgeInsetSide side, Context ctx) {
        if (side == null || side.isCalc) {
            return 0;
        }
        if (side.unit == EdgeInsetsValue.EdgeInsetSide.UNIT_PX) {
            return standardUnitToPx(ctx, side.value);
        }
        return 0;
    }

    /**
     * Applies text decoration properties.
     *
     * @param textView TextView
     * @param styles   Style map
     * @param context  Android Context
     */
    private static void applyTextDecoration(TextView textView, Map<String, Object> styles, Context context) {
        // Decoration properties
        String decorationLine = null;      // underline or line-through
        String decorationStyle = "solid";  // solid, dashed, dotted, double, wavy
        String decorationColor = null;     // color value
        String decorationThickness = "1px"; // thickness

        // 1. Parse shorthand property text-decoration (lower priority)
        if (styles.containsKey("text-decoration")) {
            Object textDecorationValue = styles.get("text-decoration");
            String textDecoration = String.valueOf(textDecorationValue).trim();

            // Parse format: line style color (e.g. "underline dashed #FF0000")
            String[] parts = textDecoration.split("\\s+");
            if (parts.length >= 1) {
                decorationLine = parts[0].toLowerCase();
            }
            if (parts.length >= 2) {
                decorationStyle = parts[1].toLowerCase();
            }
            if (parts.length >= 3) {
                decorationColor = parts[2];
            }
        }

        // 2. Parse individual properties (higher priority, overrides shorthand)
        //    The longhand overrides the shorthand only when it carries a valid value; an
        //    invalid value (e.g. "xxx") is a parse error and dropped per CSS spec, keeping
        //    the shorthand's value. Aligned with iOS (TextDecorationConfig.from only overrides
        //    on a valid enum match).
        if (styles.containsKey("text-decoration-line")) {
            String lineValue = String.valueOf(styles.get("text-decoration-line")).trim().toLowerCase();
            if (lineValue.equals("none") || lineValue.equals("underline") || lineValue.equals("line-through")) {
                decorationLine = lineValue;
            }
        }
        if (styles.containsKey("text-decoration-style")) {
            String styleValue = String.valueOf(styles.get("text-decoration-style")).trim().toLowerCase();
            if (styleValue.equals("solid") || styleValue.equals("double") || styleValue.equals("dotted")
                    || styleValue.equals("dashed") || styleValue.equals("wavy")) {
                decorationStyle = styleValue;
            }
        }
        if (styles.containsKey("text-decoration-color")) {
            decorationColor = String.valueOf(styles.get("text-decoration-color")).trim();
        }
        if (styles.containsKey("text-decoration-thickness")) {
            decorationThickness = String.valueOf(styles.get("text-decoration-thickness")).trim();
        }

        // 3. If no decoration line type is set, return early
        if (decorationLine == null || decorationLine.isEmpty() || decorationLine.equals("none")) {
            return;
        }

        // 4. Parse decoration line parameters
        // When text-decoration-color is not specified, fall back to the text color so
        // the decoration line is always visible (Color.TRANSPARENT would be invisible).
        int color = (decorationColor != null) ? parseColor(decorationColor) : textView.getCurrentTextColor();
        int thickness = parseDimension(decorationThickness, context);
        if (thickness <= 0) {
            thickness = StyleHelper.standardUnitToPx(context, 2); // Default: 2 a2ui unit
        }

        // 5. Get the TextView's gravity
        int gravity = textView.getGravity();

        // 6. Create a SpannableString and apply decoration
        CharSequence text = textView.getText();
        if (text == null || text.length() == 0) {
            return;
        }

        SpannableString spannableString = new SpannableString(text);

        // Create the appropriate Span based on decoration line type and style (passing gravity)
        if (decorationLine.equals("underline")) {
            com.amap.agenui.render.component.impl.span.CustomUnderlineSpan.Style style = parseUnderlineStyle(decorationStyle);
            com.amap.agenui.render.component.impl.span.CustomUnderlineSpan span =
                    new com.amap.agenui.render.component.impl.span.CustomUnderlineSpan(
                            color, thickness, style, gravity, context);
            spannableString.setSpan(span, 0, text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        } else if (decorationLine.equals("line-through")) {
            com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan.Style style = parseStrikethroughStyle(decorationStyle);
            com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan span =
                    new com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan(
                            color, thickness, style, gravity, context);
            spannableString.setSpan(span, 0, text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        // 7. Apply the SpannableString
        textView.setText(spannableString);
    }

    /**
     * Parses the decoration line style (for underline).
     *
     * @param styleStr Style string
     * @return CustomUnderlineSpan.Style
     */
    private static com.amap.agenui.render.component.impl.span.CustomUnderlineSpan.Style parseUnderlineStyle(String styleStr) {
        if (styleStr == null) {
            return com.amap.agenui.render.component.impl.span.CustomUnderlineSpan.Style.SOLID;
        }

        switch (styleStr.toLowerCase()) {
            case "dashed":
                return com.amap.agenui.render.component.impl.span.CustomUnderlineSpan.Style.DASHED;
            case "dotted":
                return com.amap.agenui.render.component.impl.span.CustomUnderlineSpan.Style.DOTTED;
            case "double":
                return com.amap.agenui.render.component.impl.span.CustomUnderlineSpan.Style.DOUBLE;
            case "wavy":
                return com.amap.agenui.render.component.impl.span.CustomUnderlineSpan.Style.WAVY;
            case "solid":
            default:
                return com.amap.agenui.render.component.impl.span.CustomUnderlineSpan.Style.SOLID;
        }
    }

    /**
     * Parses the decoration line style (for strikethrough).
     *
     * @param styleStr Style string
     * @return CustomStrikethroughSpan.Style
     */
    private static com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan.Style parseStrikethroughStyle(String styleStr) {
        if (styleStr == null) {
            return com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan.Style.SOLID;
        }

        switch (styleStr.toLowerCase()) {
            case "dashed":
                return com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan.Style.DASHED;
            case "dotted":
                return com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan.Style.DOTTED;
            case "double":
                return com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan.Style.DOUBLE;
            case "wavy":
                return com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan.Style.WAVY;
            case "solid":
            default:
                return com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan.Style.SOLID;
        }
    }

    /**
     * Parses a font family (supports system fonts and custom fonts).
     *
     * @param value   Font family name
     * @param context Android Context
     * @return Typeface
     */
    public static Typeface parseFontFamily(Object value, Context context) {
        if (value == null) {
            return StyleDefaults.FONT_FAMILY;
        }

        String raw = String.valueOf(value).trim();
        if (raw.isEmpty()) {
            return StyleDefaults.FONT_FAMILY;
        }

        // CSS fallback list: "CustomFont, monospace, sans-serif"
        String[] candidates = raw.split(",");
        for (String candidate : candidates) {
            String name = stripFontQuotes(candidate.trim());
            if (name.isEmpty()) {
                continue;
            }

            // Generic family names
            Typeface generic = resolveGenericFamily(name);
            if (generic != null) {
                return generic;
            }

            // Custom font from FontRegistry
            Typeface registered = com.amap.agenui.render.font.FontRegistry.getInstance().resolve(name);
            if (registered != null) {
                return registered;
            }
        }

        return StyleDefaults.FONT_FAMILY;
    }

    private static String stripFontQuotes(String value) {
        if (value.length() >= 2) {
            char first = value.charAt(0);
            char last = value.charAt(value.length() - 1);
            if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
                return value.substring(1, value.length() - 1);
            }
        }
        return value;
    }

    private static Typeface resolveGenericFamily(String name) {
        switch (name.toLowerCase(java.util.Locale.ROOT)) {
            case "system":
            case "sans-serif":
                return Typeface.SANS_SERIF;
            case "serif":
                return Typeface.SERIF;
            case "monospace":
                return Typeface.MONOSPACE;
            default:
                return null;
        }
    }

    /**
     * Parses a CSS text-align value to Android Gravity (horizontal only).
     *
     * <p>W3C {@code text-align} only controls horizontal alignment. If the
     * input contains a second vertical token (A2UI two-axis extension,
     * e.g. "center bottom"), it is silently dropped — consistent with the
     * iOS and HarmonyOS implementations.
     *
     * @param textAlign Alignment value (e.g. "left", "center", "right bottom")
     * @return Gravity value with horizontal alignment and TOP vertical;
     *         returns -1 if textAlign is null
     */
    private static int parseTextAlign(String textAlign) {
        if (textAlign == null) {
            return -1;
        }

        // W3C text-align only controls horizontal alignment. The vertical
        // token from the A2UI two-axis extension (e.g. "center bottom") is
        // intentionally dropped here, matching iOS and HarmonyOS behaviour.
        // Vertical positioning is always TOP so that text starts at
        // paddingTop and extends downward, consistent with HTML rendering.
        String[] parts = textAlign.toLowerCase().trim().split("\\s+");
        int horizontal = Gravity.START;
        String h = parts[0];
        if (h.equals("left") || h.equals("start")) {
            horizontal = Gravity.START;
        } else if (h.equals("center")) {
            horizontal = Gravity.CENTER_HORIZONTAL;
        } else if (h.equals("right") || h.equals("end")) {
            horizontal = Gravity.END;
        }

        return horizontal | Gravity.TOP;
    }

    /**
     * Parses an integer value.
     *
     * @param value Integer value
     * @return Integer; returns 0 if parsing fails
     */
    private static int parseInteger(Object value) {
        if (value == null) {
            return 0;
        }

        try {
            if (value instanceof Number) {
                return ((Number) value).intValue();
            }
            return Integer.parseInt(String.valueOf(value).trim());
        } catch (NumberFormatException e) {
            return 0;
        }
    }


    /**
     * Converts a standard unit value to pixels.
     * Divides the value by 2 then converts using dp rules.
     *
     * @param context Android Context
     * @param value   Value in standard units
     * @return Converted pixel value
     */
    public static int standardUnitToPx(Context context, float value) {
        if (context == null) {
            return (int)value;
        }
        // Standard unit must be divided by 2 before converting to dp
        float dipValue = value / 2;
        float density = context.getResources().getDisplayMetrics().density;

        try {
            float pixelFloat = dipValue * density;
            // Special case: if value > 0 but the converted result < 1, return 1
            if (dipValue > 0 && pixelFloat < 1) {
                return 1;
            }
            // Round to nearest integer
            return (int) (pixelFloat + 0.5f);
        } catch (Exception ignored) {
        }

        return (int) dipValue;
    }

    public static float pxToA2ui(Context context, float value) {
        if (context == null) {
            return value;
        }
        float density = context.getResources().getDisplayMetrics().density;
        if (density <= 0f) {
            return value;
        }
        return value / density * 2f;
    }

    /**
     * Parses a CSS color string into an ARGB int. Delegates to the shared native
     * ColorParser ({@link AGenUI#nativeParseColor}) so Android matches iOS / Harmony
     * for the full CSS grammar (named colors, #RGB shorthand, hsl/hsla, etc.).
     *
     * <p>Returns {@link Color#TRANSPARENT} for null/empty input, parse failure,
     * or values that can't be expressed as a single int (gradients, currentColor).
     */
    public static int parseColor(Object value) {
        if (value == null) {
            return Color.TRANSPARENT;
        }
        String css = String.valueOf(value).trim();
        if (css.isEmpty()) {
            return Color.TRANSPARENT;
        }

        ColorValue cv = AGenUI.nativeParseColor(css);
        if (cv == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "parseColor: native parse failed for: " + css);
            }
            return Color.TRANSPARENT;
        }
        if (cv.type == ColorValue.TYPE_GRADIENT) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "parseColor: gradient not representable as int: " + css);
            }
            return Color.TRANSPARENT;
        }
        return cv.solidColor;
    }

    /**
     * Parses a float value.
     */
    private static float parseFloat(Object value) {
        if (value == null) return 0f;

        try {
            if (value instanceof Number) {
                return ((Number) value).floatValue();
            }
            return Float.parseFloat(String.valueOf(value));
        } catch (NumberFormatException e) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "Failed to parse float: " + value, e);
            }
            return 0f;
        }
    }

    /**
     * Extracts all URLs from CSS url() functions in the given text.
     * Supports quoted and unquoted forms, for example:
     * url("http://example.com/img.png")
     * url('http://example.com/img.png')
     * url(http://example.com/img.png)
     *
     * @param text Text containing CSS url() functions
     * @return The first extracted URL (without quotes), or null if not found
     */
    public static String extractUrlsFromCss(String text) {
        if (text == null || text.isEmpty()) {
            return null;
        }

        // Regex explanation:
        // url\\(          : matches literal "url("
        // ['"]?           : matches an optional single or double quote
        // (               : start of capture group — the content we want to extract
        // [^)]*           : matches any character except ")" (non-greedy via exclusion)
        // )               : end of capture group
        // ['"]?           : matches an optional closing single or double quote
        // \\)             : matches literal ")"
        String regex = "url\\(['\"]?([^)'\"]*)['\"]?\\)";

        Pattern pattern = Pattern.compile(regex);
        Matcher matcher = pattern.matcher(text);

        while (matcher.find()) {
            // group(1) is the first capture group — the clean URL inside the parentheses without quotes
            String url = matcher.group(1);
            if (url != null && !url.isEmpty()) {
                return url.trim();
            }
        }

        return null;
    }

    /**
     * Resolve CSS margin from a styles map into pixel values.
     * Handles both the shorthand "margin" (1~4 values) and individual
     * "margin-top" / "margin-right" / "margin-bottom" / "margin-left"
     * properties. Individual properties override the shorthand, matching
     * CSS cascade / Yoga semantics (CSSStyleConverter::applyStyles).
     *
     * Mirrors {@link #resolveCSSPaddingPx} in naming convention and structure.
     *
     * @param styles CSS styles map
     * @param context Android context for unit conversion
     * @return Rect(left, top, right, bottom) in pixels, or null if no margin is present
     */
    @Nullable
    public static Rect resolveCSSMarginPx(@Nullable Map<String, Object> styles,
                                          @NonNull Context context) {
        if (styles == null || styles.isEmpty()) {
            return null;
        }

        int topPx = 0, rightPx = 0, bottomPx = 0, leftPx = 0;
        boolean anyMarginPresent = false;

        // 1. Parse shorthand "margin" via native edge-insets parser
        if (styles.containsKey("margin")) {
            String shorthand = String.valueOf(styles.get("margin")).trim();
            if (!shorthand.isEmpty() && !shorthand.equalsIgnoreCase("null")) {
                EdgeInsetsValue insets = AGenUI.nativeParseEdgeInsets(shorthand);
                if (insets != null) {
                    topPx    = resolveSidePx(insets.top,    context);
                    rightPx  = resolveSidePx(insets.right,  context);
                    bottomPx = resolveSidePx(insets.bottom, context);
                    leftPx   = resolveSidePx(insets.left,   context);
                    anyMarginPresent = true;
                }
            }
        }

        // 2. Individual properties override shorthand
        if (styles.containsKey("margin-top")) {
            topPx = parseDimension(styles.get("margin-top"), context);
            anyMarginPresent = true;
        }
        if (styles.containsKey("margin-right")) {
            rightPx = parseDimension(styles.get("margin-right"), context);
            anyMarginPresent = true;
        }
        if (styles.containsKey("margin-bottom")) {
            bottomPx = parseDimension(styles.get("margin-bottom"), context);
            anyMarginPresent = true;
        }
        if (styles.containsKey("margin-left")) {
            leftPx = parseDimension(styles.get("margin-left"), context);
            anyMarginPresent = true;
        }

        if (!anyMarginPresent) {
            return null;
        }

        if (topPx    < 0) topPx    = 0;
        if (rightPx  < 0) rightPx  = 0;
        if (bottomPx < 0) bottomPx = 0;
        if (leftPx   < 0) leftPx   = 0;

        Rect out = new Rect(leftPx, topPx, rightPx, bottomPx);
        return out;
    }
}
