package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.NonNull;

import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.image.ImageCallback;
import com.amap.agenui.render.image.ImageLoadOptionsKey;
import com.amap.agenui.render.image.ImageLoadResult;
import com.amap.agenui.render.image.ImageLoaderConfig;
import com.amap.agenui.render.image.ImageLoaderError;
import com.amap.agenui.render.style.StyleHelper;
import com.amap.agenui.render.utils.AGenUILogger;
import com.amap.agenui.render.utils.ImageTransition;
import com.amap.agenui.render.utils.ImageTransitionManager;
import com.amap.agenui.render.utils.ShimmerTransition;

import java.util.HashMap;
import java.util.Map;

/**
 * Image component implementation - compliant with A2UI v0.9 protocol
 *
 * Supported properties:
 * - url: image URL (DynamicString)
 * - fit: scale mode (contain, cover, fill, none, scaleDown)
 * - variant: size hint (icon, avatar, smallFeature, mediumFeature, largeFeature, header)
 *
 */
public class ImageComponent extends A2UIComponent {

    private static final String TAG = "ImageComponent";

    enum DimensionConstraint {
        NONE,
        FIXED,
        FLEXIBLE
    }

    private Context context;

    private ImageView imageView;

    // Shimmer animation management
    private ShimmerTransition shimmerTransition;
    private ShimmerTransition.ShimmerView currentShimmerView;

    // Currently loading requestId, used for cancellation and stale callback filtering.
    private String currentRequestId;

    public ImageComponent(Context context, String id, Map<String, Object> properties) {
        super(id, "Image");
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "[ImageComponent] Constructor called - id: " + id);
            AGenUILogger.d(TAG, "[ImageComponent] Constructor - properties: " + properties);
        }

        // Store only the ApplicationContext to avoid holding a strong reference to Activity and causing leaks
        this.context = context.getApplicationContext();
        if (properties != null) {
            this.properties.putAll(properties);
            AGenUILogger.d(TAG, "[ImageComponent] Constructor - properties saved to base class");
        }
    }

    @Override
    protected View onCreateView(Context context) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "[ImageComponent] onCreateView called - id: " + getId());
            AGenUILogger.d(TAG, "[ImageComponent] onCreateView - properties: " + properties);
        }

        // Background, border-radius, and border are applied uniformly by StyleHelper through the
        // base class — this component owns only the image content (url, fit, scale type).
        imageView = new ImageView(context);
        imageView.setScaleType(ImageView.ScaleType.CENTER_CROP);

        if (!properties.isEmpty()) {
            AGenUILogger.d(TAG, "[ImageComponent] onCreateView - applying properties immediately");
            onUpdateProperties(properties);
        } else {
            AGenUILogger.w(TAG, "[ImageComponent] onCreateView - properties is EMPTY!");
        }

        return imageView;
    }

    @Override
    protected void onUpdateProperties(Map<String, Object> properties) {
        if (imageView == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "onUpdateProperties: imageView is null, id=" + getId());
            }
            return;
        }

        // Update image URL (A2UI v0.9 protocol: url)
        if (properties.containsKey("url")) {
            String url = extractStringValue(properties.get("url"));
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.d(TAG, "[ImageComponent] onUpdateProperties - loading image url: " + url);
            }
            loadImage(url);
        } else {
            AGenUILogger.w(TAG, "[ImageComponent] onUpdateProperties - NO 'url' property found!");
        }

        // Update scale mode (A2UI v0.9 protocol: fit)
        if (properties.containsKey("fit")) {
            String fit = String.valueOf(properties.get("fit"));
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.d(TAG, "[ImageComponent] onUpdateProperties - setting fit: " + fit);
            }
            imageView.setScaleType(parseFit(fit));
        }

        // Handle styles (read from styles)
        if (properties.containsKey("styles")) {
            @SuppressWarnings("unchecked")
            Map<String, Object> styles = (Map<String, Object>) properties.get("styles");
            if (styles != null) {
                // CSS padding -> ImageView.setPadding (delegated to the
                // shared StyleHelper.applyCSSPadding so leaf components share
                // a single parsing implementation). Per W3C, <img> is a
                // replaced element that honours `padding`; the bitmap is
                // shrunk into the contentBox.
                StyleHelper.applyCSSPadding(imageView, styles, context);
            }
        }
    }

    /**
     * Load the image.
     * Uses a pluggable ImageLoader to load network images and local resources.
     * Integrates Shimmer loading animation and transition animation.
     */
    private void loadImage(String src) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "[ImageComponent] loadImage called - src: " + src);
        }

        if (src == null || src.isEmpty()) {
            AGenUILogger.w(TAG, "[ImageComponent] loadImage - src is null or empty, clearing image");
            imageView.setImageDrawable(null);
            return;
        }

        // Cancel the previous loading task so a recycled component does not receive stale callbacks.
        if (currentRequestId != null) {
            ImageLoaderConfig.getInstance().getLoader().cancel(currentRequestId);
            currentRequestId = null;
        }

        // Start Shimmer animation based on animation toggle
        if (isAnimationEnabled()) {
            if (shimmerTransition == null) {
                shimmerTransition = new ShimmerTransition();
            }
            currentShimmerView = shimmerTransition.startShimmer(imageView);
        }

        // Delegate to the pluggable Loader, build options
        Map<String, Object> options = buildLoadOptions();
        final String[] requestIdHolder = new String[1];
        String requestId = ImageLoaderConfig.getInstance()
                .getLoader()
                .loadImage(src, options, new ImageCallback() {
            @Override
            public void onSuccess(@NonNull ImageLoadResult result) {
                String activeRequestId = requestIdHolder[0];
                if (activeRequestId == null || !activeRequestId.equals(currentRequestId)) {
                    return;
                }
                currentRequestId = null;
                imageView.setImageDrawable(result.drawable);
                reportImageRenderSizeIfNeeded(result.drawable);
                onImageLoadComplete(result.isFromCache);
            }

            @Override
            public void onFailure(@NonNull ImageLoaderError error) {
                String activeRequestId = requestIdHolder[0];
                if (activeRequestId == null || !activeRequestId.equals(currentRequestId)) {
                    return;
                }
                currentRequestId = null;
                AGenUILogger.e(TAG, "[ImageComponent] Image load failed: " + error.getMessage());
                if (shimmerTransition != null) {
                    shimmerTransition.stopShimmer();
                }
                // Set the global default placeholder
                Drawable placeholder = ImageLoaderConfig.getInstance().getDefaultPlaceholder();
                if (placeholder != null) {
                    imageView.setImageDrawable(placeholder);
                }
                if (shouldReportAsyncImageSize()) {
                    notifyRenderFinish("Image", 0f, 0f);
                }
            }
        });
        requestIdHolder[0] = requestId;
        currentRequestId = requestId;
    }

    /**
     * Build image loading options based on component properties and layout information.
     *
     * <p>When the layout explicitly specifies width and height, pass options to the loader
     * for downsampling optimization.
     *
     * @return options map, or null if no valid options are present
     */
    private Map<String, Object> buildLoadOptions() {
        Map<String, Object> options = new HashMap<>();
        Map<String, Object> styles = extractStyles(properties);

        putNumericOption(options, ImageLoadOptionsKey.WIDTH, styles.get("width"));
        putNumericOption(options, ImageLoadOptionsKey.HEIGHT, styles.get("height"));

        // Component ID
        options.put(ImageLoadOptionsKey.COMPONENT_ID, getId());

        return options.size() > 1 ? options : null; // don't pass options if only componentId is present
    }

    /**
     * Called when image loading is complete.
     *
     * @param isFromCache whether the image came from cache (skip entrance animation on cache hit)
     */
    private void onImageLoadComplete(boolean isFromCache) {
        // Remove the Shimmer overlay
        if (shimmerTransition != null) {
            shimmerTransition.stopShimmer();
        }

        // Execute reveal animation based on animation toggle (skip on cache hit)
        if (isAnimationEnabled() && !isFromCache) {
            executeTransition();
        }
    }

    /**
     * Cancel the loading task when the component is destroyed.
     */
    public void onDestroy() {
        if (currentRequestId != null) {
            ImageLoaderConfig.getInstance().getLoader().cancel(currentRequestId);
            currentRequestId = null;
        }
    }

    /**
     * Execute the transition animation after image loading completes.
     */
    private void executeTransition() {
        ImageTransition transition = ImageTransitionManager.getDefaultTransition();
        long duration = ImageTransitionManager.getDefaultDuration();

        if (transition != null) {
            transition.animate(imageView, duration, null);
        }
    }

    /**
     * Extract string value (supports DynamicString)
     */
    private String extractStringValue(Object value) {
        if (value instanceof Map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> valueMap = (Map<String, Object>) value;
            if (valueMap.containsKey("literalString")) {
                return String.valueOf(valueMap.get("literalString"));
            }
            if (valueMap.containsKey("path")) {
                return "";
            }
        }
        return String.valueOf(value);
    }

    private void reportImageRenderSizeIfNeeded(Drawable drawable) {
        if (!shouldReportAsyncImageSize() || drawable == null) {
            return;
        }
        Map<String, Object> styles = extractStyles(properties);
        int[] reportedSizePx = resolveReportedImageSizePx(drawable, styles);
        if (reportedSizePx[0] > 0 && reportedSizePx[1] > 0) {
            notifyRenderFinishFromPx("Image", reportedSizePx[0], reportedSizePx[1]);
        }
    }

    // TODO: 2026/5/9 Temporarily comment out size notification 
    private boolean shouldReportAsyncImageSize() {
//        Map<String, Object> styles = extractStyles(properties);
//        return shouldReportAsyncImageSizeForStyles(styles);
        return false;
    }

    static boolean shouldReportAsyncImageSizeForStyles(Map<String, Object> styles) {
        if (styles == null || styles.isEmpty()) {
            return true;
        }

        DimensionConstraint widthConstraint = classifyDimensionConstraint(styles.get("width"));
        DimensionConstraint heightConstraint = classifyDimensionConstraint(styles.get("height"));

        if (widthConstraint != DimensionConstraint.NONE
                && heightConstraint != DimensionConstraint.NONE) {
            return false;
        }
        return widthConstraint != DimensionConstraint.FLEXIBLE
                && heightConstraint != DimensionConstraint.FLEXIBLE;
    }

    static DimensionConstraint classifyDimensionConstraint(Object value) {
        if (value instanceof Number) {
            return ((Number) value).floatValue() > 0f
                    ? DimensionConstraint.FIXED
                    : DimensionConstraint.NONE;
        }
        if (value == null) {
            return DimensionConstraint.NONE;
        }
        String raw = String.valueOf(value).trim().toLowerCase();
        if (raw.isEmpty() || "auto".equals(raw)) {
            return DimensionConstraint.NONE;
        }
        if (raw.endsWith("%")) {
            return DimensionConstraint.FLEXIBLE;
        }
        if (raw.endsWith("px")) {
            raw = raw.substring(0, raw.length() - 2);
        }
        try {
            return Float.parseFloat(raw) > 0f
                    ? DimensionConstraint.FIXED
                    : DimensionConstraint.NONE;
        } catch (NumberFormatException ignored) {
            return DimensionConstraint.NONE;
        }
    }

    private int[] resolveReportedImageSizePx(Drawable drawable, Map<String, Object> styles) {
        int intrinsicWidthPx = drawable.getIntrinsicWidth();
        int intrinsicHeightPx = drawable.getIntrinsicHeight();
        if (intrinsicWidthPx <= 0 || intrinsicHeightPx <= 0) {
            intrinsicWidthPx = imageView != null ? imageView.getMeasuredWidth() : 0;
            intrinsicHeightPx = imageView != null ? imageView.getMeasuredHeight() : 0;
        }

        if (intrinsicWidthPx <= 0 || intrinsicHeightPx <= 0) {
            return new int[]{0, 0};
        }

        DimensionConstraint widthConstraint = classifyDimensionConstraint(styles.get("width"));
        DimensionConstraint heightConstraint = classifyDimensionConstraint(styles.get("height"));
        float intrinsicAspectRatio = intrinsicHeightPx > 0
                ? (float) intrinsicWidthPx / intrinsicHeightPx
                : 0f;

        if (widthConstraint == DimensionConstraint.FIXED
                && heightConstraint == DimensionConstraint.NONE
                && intrinsicAspectRatio > 0f) {
            int widthPx = resolveFixedDimensionPx(styles.get("width"));
            if (widthPx > 0) {
                return new int[]{widthPx, Math.max(1, Math.round(widthPx / intrinsicAspectRatio))};
            }
        }

        if (widthConstraint == DimensionConstraint.NONE
                && heightConstraint == DimensionConstraint.FIXED
                && intrinsicAspectRatio > 0f) {
            int heightPx = resolveFixedDimensionPx(styles.get("height"));
            if (heightPx > 0) {
                return new int[]{Math.max(1, Math.round(heightPx * intrinsicAspectRatio)), heightPx};
            }
        }

        return new int[]{intrinsicWidthPx, intrinsicHeightPx};
    }

    private int resolveFixedDimensionPx(Object value) {
        return context == null ? 0 : StyleHelper.parseDimension(value, context);
    }

    /**
     * Resolve {@code value} to physical px via {@link StyleHelper#parseDimension} and write
     * to {@code options}. Negative sentinels (auto / match_parent / 100% / parse-failure)
     * and ≤ 0 results are filtered out by the {@code > 0} guard so the loader can rely on
     * {@link ImageLoadOptionsKey#WIDTH} / {@link ImageLoadOptionsKey#HEIGHT} carrying a
     * positive physical-pixel target whenever the key is present.
     */
    private void putNumericOption(Map<String, Object> options, String key, Object value) {
        int px = StyleHelper.parseDimension(value, context);
        if (px > 0) {
            options.put(key, (float) px);
        }
    }

    /**
     * Parse scale mode.
     * A2UI v0.9 protocol values: contain, cover, fill, none, scaleDown
     */
    private ImageView.ScaleType parseFit(String fit) {
        switch (fit.toLowerCase()) {
            case "contain":
                return ImageView.ScaleType.FIT_CENTER;
            case "cover":
                return ImageView.ScaleType.CENTER_CROP;
            case "fill":
                return ImageView.ScaleType.FIT_XY;
            case "none":
                // none maps to fill: stretch to fill container
                return ImageView.ScaleType.FIT_XY;
            case "scaleDown":
                // scaleDown is similar to contain but does not enlarge the image
                return ImageView.ScaleType.CENTER_INSIDE;
            default:
                return ImageView.ScaleType.FIT_CENTER;
        }
    }

}
