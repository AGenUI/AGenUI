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
import com.amap.agenui.render.image.ImageCallback;
import com.amap.agenui.render.image.ImageLoadOptionsKey;
import com.amap.agenui.render.image.ImageLoadResult;
import com.amap.agenui.render.image.ImageLoaderConfig;
import com.amap.agenui.render.image.ImageLoaderError;
import com.amap.agenui.render.utils.AGenUILogger;

import java.util.HashMap;
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
            fm.top     -= halfBefore;
            fm.descent += halfAfter;
            fm.bottom  += halfAfter;
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

        // display
        if (properties.containsKey("display")) {
            String display = String.valueOf(properties.get("display")).toLowerCase();
            switch (display) {
                case "none":
                    view.setVisibility(View.GONE);
                    break;
                case "flex":
                case "block":
                case "inline-block":
                default:
                    view.setVisibility(View.VISIBLE);
                    break;
            }
        }

        // visibility
        if (properties.containsKey("visibility")) {
            String visibility = String.valueOf(properties.get("visibility")).toLowerCase();
            switch (visibility) {
                case "hidden":
                    view.setVisibility(View.INVISIBLE);
                    break;
                case "visible":
                default:
                    view.setVisibility(View.VISIBLE);
                    break;
            }
        }

        // opacity
        if (properties.containsKey("opacity")) {
            float opacity = parseFloat(properties.get("opacity"));
            view.setAlpha(opacity);
        }
    }


    /**
     * Applies background fill: solid color, gradient, or async image. Goes into the View's single
     * background slot ({@link View#setBackground}), drawn at the bottom of the View. Rounding is
     * handled separately by {@link #applyBorder} via outline clip — both can be called in any
     * order; clipping happens at draw time.
     *
     * <p>Supports: background-color, background, background-image. No-op when none are present.
     */
    public static void applyBackground(View view, Map<String, Object> styles) {
        if (view == null || styles == null) {
            return;
        }
        boolean hasSyncBg = styles.containsKey("background-color") || styles.containsKey("background");
        boolean hasAsyncBg = styles.containsKey("background-image");
        if (!hasSyncBg && !hasAsyncBg) {
            return;
        }
        Drawable syncBg = hasSyncBg ? parseSyncBackgroundDrawable(styles, view.getContext()) : null;
        if (hasAsyncBg) {
            if (syncBg != null) {
                view.setBackground(syncBg);
            }
            loadBackgroundImageAsync(view, styles, syncBg);
        } else {
            view.setBackground(syncBg);
        }
    }

    /**
     * Returns a Drawable for the {@code background-color} / {@code background} value, a
     * transparent {@link ColorDrawable} when the value is present but unparseable (matches the
     * legacy parse-failure fallback), or {@code null} when neither key is present.
     */
    private static Drawable parseSyncBackgroundDrawable(Map<String, Object> styles, Context ctx) {
        Object raw = styles.containsKey("background-color")
                ? styles.get("background-color")
                : (styles.containsKey("background") ? styles.get("background") : null);
        if (raw == null) {
            return null;
        }
        String css = String.valueOf(raw).trim();
        if (css.isEmpty()) {
            return null;
        }
        ColorValue cv = AGenUI.nativeParseColor(css);
        if (cv == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "applyBackground: native parse failed for: " + raw);
            }
            return new ColorDrawable(Color.TRANSPARENT);
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
     * <p>Supports: border-radius, border-width, border-color. No-op when none are present.
     */
    public static void applyBorder(View view, Map<String, Object> styles) {
        if (view == null || styles == null) {
            return;
        }
        boolean hasRadius = styles.containsKey("border-radius");
        boolean hasStroke = styles.containsKey("border-width") || styles.containsKey("border-color");
        if (!hasRadius && !hasStroke) {
            return;
        }

        Context ctx = view.getContext();
        int radiusPx = parseDimensionOrZero(styles.get("border-radius"), ctx);
        int borderWidth = parseDimensionOrZero(styles.get("border-width"), ctx);
        int borderColor = styles.containsKey("border-color")
                ? parseColor(styles.get("border-color")) : Color.BLACK;

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

    /**
     * Installs a {@link ViewOutlineProvider} that rounds the view to {@code radiusPx} and turns
     * on outline clipping, so the background drawable AND any child views are masked to the
     * rounded shape. Resets to the default provider when {@code radiusPx <= 0}, which is
     * required when an update removes a previous border-radius — otherwise stale rounding
     * persists.
     */
    private static void applyOutlineRadiusClip(View view, int radiusPx) {
        if (radiusPx <= 0) {
            view.setClipToOutline(false);
            view.setOutlineProvider(ViewOutlineProvider.BACKGROUND);
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
            stroke.setCornerRadius(radiusPx);
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
     * Supports: filter (drop-shadow).
     */
    public static void applyFilter(View view, Map<String, Object> properties) {
        if (view == null || properties == null) return;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            if (properties.containsKey("filter")) {
                String filter = String.valueOf(properties.get("filter")).trim();

                // Parse drop-shadow
                if (filter.startsWith("drop-shadow(")) {
                    Context context = view.getContext();
                    try {
                        // Format: drop-shadow(offset-x offset-y blur-radius color)
                        // Example: drop-shadow(2px 2px 4px rgba(0,0,0,0.3))
                        String params = filter.substring(12, filter.length() - 1).trim();
                        String[] parts = params.split("\\s+");

                        if (parts.length >= 3) {
                            // Parse offset and blur radius
                            float dx = parseDimensionFloat(parts[0], context);
                            float dy = parseDimensionFloat(parts[1], context);
                            float radius = parseDimensionFloat(parts[2], context);

                            // Note: Android's View.setElevation() does not support custom shadow colors;
                            // shadow color is determined by the system theme, so the color parameter is
                            // not parsed or used here. To use a custom shadow color, a custom Drawable
                            // or Canvas drawing approach is needed.

                            // Apply shadow (use elevation to approximate blur-radius)
                            // elevation should be the blur-radius value, not radius directly
                            view.setElevation(radius / 4f);

                            // Use translation to simulate shadow offset.
                            // Note: translation affects the actual position of the View and may not be ideal.
                            // A better approach is to reserve space for the shadow via padding or margin.
                            view.setTranslationX(dx);
                            view.setTranslationY(dy);

                            // To prevent the shadow from being clipped, ensure the parent container
                            // does not clip child Views. This typically requires setting
                            // clipChildren=false and clipToPadding=false on the parent.
                            if (view.getParent() instanceof ViewGroup) {
                                ViewGroup parent = (ViewGroup) view.getParent();
                                parent.setClipChildren(false);
                                parent.setClipToPadding(false);
                            }

                        }
                    } catch (Exception e) {
                        if (AGenUILogger.isLoggingEnabled()) {
                            AGenUILogger.w(TAG, "Failed to parse drop-shadow: " + filter, e);
                        }
                    }
                }
            }
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
     * Applies overflow styles.
     * Supports: overflow.
     */
    public static void applyOverflow(ViewGroup viewGroup, Map<String, Object> properties) {
        if (viewGroup == null || properties == null) return;

        if (properties.containsKey("overflow")) {
            String overflow = String.valueOf(properties.get("overflow")).toLowerCase();
            switch (overflow) {
                case "hidden":
                    viewGroup.setClipChildren(true);
                    viewGroup.setClipToPadding(true);
                    if (properties.containsKey("border-radius")) {
                        final int radiusPx = parseDimension(properties.get("border-radius"),
                                viewGroup.getContext());
                        if (radiusPx > 0) {
                            viewGroup.setOutlineProvider(new ViewOutlineProvider() {
                                @Override
                                public void getOutline(View v, Outline outline) {
                                    outline.setRoundRect(0, 0, v.getWidth(), v.getHeight(), radiusPx);
                                }
                            });
                            viewGroup.setClipToOutline(true);
                        }
                    }
                    break;
                case "visible":
                    viewGroup.setClipChildren(false);
                    viewGroup.setClipToPadding(false);
                    break;
                case "scroll":
                case "auto":
                    // Requires wrapping in a ScrollView; not yet implemented
                    break;
            }
        }
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
        if (textView == null || styles == null || styles.isEmpty()) {
            return;
        }

        // 1. Handle font-related properties (Typeface must be composed together)
        Typeface currentTypeface = textView.getTypeface();
        Typeface baseTypeface = currentTypeface != null ? currentTypeface : Typeface.DEFAULT;

        // font-family: font family
        if (styles.containsKey("font-family")) {
            Object fontFamilyValue = styles.get("font-family");
            baseTypeface = parseFontFamily(fontFamilyValue, context);
        }

        // font-weight: supports "bold", "normal", or numeric (>=500 is bold)
        if (styles.containsKey("font-weight")) {
            Object fontWeightValue = styles.get("font-weight");
            String fontWeight = String.valueOf(fontWeightValue).trim().toLowerCase();
            textView.setTypeface(baseTypeface, isBoldWeight(fontWeight) ? Typeface.BOLD : Typeface.NORMAL);
        } else if (styles.containsKey("font-family")) {
            // Only font-family is set, no weight
            textView.setTypeface(baseTypeface);
        }

        // 2. font-size: font size (only px is supported)
        if (styles.containsKey("font-size")) {
            Object fontSizeValue = styles.get("font-size");
            String sizeStr = String.valueOf(fontSizeValue).trim().toLowerCase();

            float size = 0;
            if (sizeStr.endsWith("px")) {
                size = Float.parseFloat(sizeStr.replace("px", ""));
            } else if (sizeStr.matches("^\\d+(\\.\\d+)?$")) {
                size = Float.parseFloat(sizeStr);
            }

            if (size > 0) {
                textView.setTextSize(TypedValue.COMPLEX_UNIT_PX, standardUnitToPx(context, size));
            }
        }

        // 3. color: text color
        if (styles.containsKey("color")) {
            Object colorValue = styles.get("color");
            int color = parseColor(colorValue);
            if (color != 0) {
                textView.setTextColor(color);
            } else {
                textView.setTextColor(Color.BLACK);
            }
        }

        // 4. line-height: line height (supports multiplier or pixel value)
        //
        // W3C semantics: the line box height equals `multiplier * font-size` (or the px value
        // directly). The glyph "content area" is centered in the line box, so the first line
        // has a half-leading gap above and the last line has a half-leading gap below.
        //
        // `TextView.setLineSpacing(add, mult)` does NOT match this semantics — `add` is piled
        // on top of every non-first line, leaving the glyph flush with the line-box top and the
        // first/last lines without any padding. Visually this makes Android's line gap look
        // larger than Harmony (ArkUI's NODE_TEXT_LINE_HEIGHT is W3C-compliant) and iOS.
        //
        // Instead we apply a `LineHeightSpan` that redistributes the extra space evenly between
        // ascent and descent on EVERY line, producing the same centered layout as Harmony/iOS.
        if (styles.containsKey("line-height")) {
            Object lineHeightValue = styles.get("line-height");
            String lineHeightStr = String.valueOf(lineHeightValue).trim().toLowerCase();

            int targetLineHeightPx = 0;
            if (lineHeightStr.matches("^\\d+(\\.\\d+)?$")) {
                // Syntax 1: line-height:2.0; — multiplier of font-size
                float multiplier = Float.parseFloat(lineHeightStr);
                if (multiplier > 0f) {
                    targetLineHeightPx = Math.round(multiplier * textView.getTextSize());
                }
            } else if (lineHeightStr.endsWith("px")) {
                // Syntax 2: line-height:10px; — explicit line height value
                int parsedPx = parseDimension(lineHeightValue, context);
                if (parsedPx > 0) {
                    targetLineHeightPx = parsedPx;
                }
            }

            if (targetLineHeightPx > 0) {
                // Reset any legacy line-spacing tweaks so the span is the sole source of truth.
                textView.setLineSpacing(0f, 1.0f);
                applyCenteredLineHeight(textView, targetLineHeightPx);
            }
        }

        // 5. line-clamp: maximum number of lines (<=0 means unlimited)
        if (styles.containsKey("line-clamp")) {
            Object lineClampValue = styles.get("line-clamp");
            int maxLines = parseInteger(lineClampValue);
            if (maxLines > 0) {
                textView.setMaxLines(maxLines);
            } else {
                // <=0 means unlimited
                textView.setMaxLines(Integer.MAX_VALUE);
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Keep Android wrapping closer to iOS/Harmony by using greedy line breaking
            // instead of the platform's balanced/high-quality strategy.
            textView.setBreakStrategy(Layout.BREAK_STRATEGY_SIMPLE);
            textView.setHyphenationFrequency(Layout.HYPHENATION_FREQUENCY_NONE);
        }

        // 6. text-overflow: text overflow handling
        if (styles.containsKey("text-overflow")) {
            Object textOverflowValue = styles.get("text-overflow");
            String textOverflow = String.valueOf(textOverflowValue).toLowerCase();

            // Get the current line-clamp setting
            int currentMaxLines = textView.getMaxLines();

            switch (textOverflow) {
                case "ellipsis":
                    // Android TextView supports TruncateAt.END for any maxLines > 0
                    if (currentMaxLines > 0 && currentMaxLines < Integer.MAX_VALUE) {
                        textView.setEllipsize(TextUtils.TruncateAt.END);
                    } else {
                        textView.setEllipsize(null);
                    }
                    break;
                case "head":
                    // head requires line-clamp=1 to take effect
                    if (currentMaxLines == 1) {
                        textView.setEllipsize(TextUtils.TruncateAt.START);
                    }
                    break;
                case "middle":
                    // middle requires line-clamp=1 to take effect
                    if (currentMaxLines == 1) {
                        textView.setEllipsize(TextUtils.TruncateAt.MIDDLE);
                    }
                    break;
                case "clip":
                default:
                    textView.setEllipsize(null);
                    break;
            }
        }

        // 7. text-align: text alignment
        if (styles.containsKey("text-align")) {
            Object textAlignValue = styles.get("text-align");
            String textAlign = String.valueOf(textAlignValue).toLowerCase();
            int gravity = parseTextAlign(textAlign);
            if (gravity != -1) {
                textView.setGravity(gravity);
            }
        }

        // 8. Text decoration properties (text-decoration series)
        applyTextDecoration(textView, styles, context);

        // 8b. CSS padding -> TextView.setPadding
        // Yoga has already accounted for `padding` in the leaf node's layout box
        // (the TextView's final width/height is the borderBox), but TextView by
        // default renders glyphs across the entire box. Translating the CSS
        // padding to `TextView.setPadding(...)` lets the glyph area shrink to
        // the contentBox so it lines up with what Yoga gave to the measureFunc.
        // setPadding does NOT change the view's outer size, so this is not a
        // double-count with Yoga's padding.
        applyTextPadding(textView, styles, context);

        // 9. filter: drop-shadow -> TextView.setShadowLayer
        // setShadowLayer is the correct API for text shadow on Android;
        // it requires a software layer to render properly.
        if (styles.containsKey("filter")) {
            String filter = String.valueOf(styles.get("filter")).trim();
            if (filter.startsWith("drop-shadow(")) {
                try {
                    String params = filter.substring(12, filter.length() - 1).trim();
                    // Split on whitespace but keep rgba(...) intact by using a smarter split.
                    // Strategy: tokenise by spaces, then re-join rgba tokens.
                    String[] rawParts = params.split("\\s+");
                    // Collect numeric length tokens (offsetX, offsetY, blur) then color.
                    java.util.List<String> lengthTokens = new java.util.ArrayList<>();
                    StringBuilder colorBuilder = new StringBuilder();
                    for (String part : rawParts) {
                        if (part.endsWith("px") || part.matches("-?\\d+(\\.\\d+)?")) {
                            lengthTokens.add(part);
                        } else {
                            if (colorBuilder.length() > 0) colorBuilder.append(' ');
                            colorBuilder.append(part);
                        }
                    }
                    if (lengthTokens.size() >= 3) {
                        float dx = parseDimensionFloat(lengthTokens.get(0), context);
                        float dy = parseDimensionFloat(lengthTokens.get(1), context);
                        float radius = parseDimensionFloat(lengthTokens.get(2), context);
                        String colorStr = colorBuilder.toString().trim();
                        int shadowColor = colorStr.isEmpty() ? Color.BLACK : parseColor(colorStr);
                        textView.setShadowLayer(radius, dx, dy, shadowColor);
                        textView.setLayerType(View.LAYER_TYPE_SOFTWARE, null);
                    }
                } catch (Exception e) {
                    if (AGenUILogger.isLoggingEnabled()) {
                        AGenUILogger.w(TAG, "Failed to parse drop-shadow for TextView: " + filter, e);
                    }
                }
            }
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
        if (view == null || styles == null || styles.isEmpty()) {
            return;
        }
        Rect padding = resolveCSSPaddingPx(styles, context);
        if (padding == null) {
            return;
        }
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
     * <p>Returns {@code null} when no {@code padding*} key is present,
     * so callers can distinguish "not specified" from "explicitly zero".
     */
    @Nullable
    public static Rect resolveCSSPaddingPx(@Nullable Map<String, Object> styles,
                                           @NonNull Context context) {
        Rect out = new Rect(0, 0, 0, 0);
        if (styles == null || styles.isEmpty()) {
            return out;
        }

        int topPx = 0, rightPx = 0, bottomPx = 0, leftPx = 0;
        boolean anyPaddingPresent = false;

        if (styles.containsKey("padding")) {
            String shorthand = String.valueOf(styles.get("padding")).trim();
            if (!shorthand.isEmpty() && !shorthand.equalsIgnoreCase("null")) {
                EdgeInsetsValue insets = AGenUI.nativeParseEdgeInsets(shorthand);
                if (insets != null) {
                    topPx    = resolveSidePx(insets.top,    context);
                    rightPx  = resolveSidePx(insets.right,  context);
                    bottomPx = resolveSidePx(insets.bottom, context);
                    leftPx   = resolveSidePx(insets.left,   context);
                    anyPaddingPresent = true;
                }
            }
        }

        if (styles.containsKey("padding-top")) {
            topPx = parseDimension(styles.get("padding-top"), context);
            anyPaddingPresent = true;
        }
        if (styles.containsKey("padding-right")) {
            rightPx = parseDimension(styles.get("padding-right"), context);
            anyPaddingPresent = true;
        }
        if (styles.containsKey("padding-bottom")) {
            bottomPx = parseDimension(styles.get("padding-bottom"), context);
            anyPaddingPresent = true;
        }
        if (styles.containsKey("padding-left")) {
            leftPx = parseDimension(styles.get("padding-left"), context);
            anyPaddingPresent = true;
        }

        if (!anyPaddingPresent) {
            return null;
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
        if (styles.containsKey("text-decoration-line")) {
            decorationLine = String.valueOf(styles.get("text-decoration-line")).trim().toLowerCase();
        }
        if (styles.containsKey("text-decoration-style")) {
            decorationStyle = String.valueOf(styles.get("text-decoration-style")).trim().toLowerCase();
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
            thickness = 1; // Default: 1px
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
                    new com.amap.agenui.render.component.impl.span.CustomUnderlineSpan(color, thickness, style, gravity);
            spannableString.setSpan(span, 0, text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        } else if (decorationLine.equals("line-through")) {
            com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan.Style style = parseStrikethroughStyle(decorationStyle);
            com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan span =
                    new com.amap.agenui.render.component.impl.span.CustomStrikethroughSpan(color, thickness, style, gravity);
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
            return Typeface.DEFAULT;
        }

        // Custom font loading is not yet supported
        return Typeface.DEFAULT;
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
}
