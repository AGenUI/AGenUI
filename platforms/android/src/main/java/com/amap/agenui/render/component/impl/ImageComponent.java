package com.amap.agenui.render.component.impl;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.TypedValue;
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

    private Context context;

    private ImageView imageView;

    // Shimmer animation management
    private ShimmerTransition shimmerTransition;
    private ShimmerTransition.ShimmerView currentShimmerView;

    // Currently loading requestId, used for cancellation and stale callback filtering.
    private String currentRequestId;

    private float currentYogaWidth = 0f;
    private float currentYogaHeight = 0f;

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
    protected void onUpdateProperties(Map<String, Object> changedProps) {
        if (imageView == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "onUpdateProperties: imageView is null, id=" + getId());
            }
            return;
        }

        // Update scale mode (A2UI v0.9 protocol: fit)
        if (changedProps.containsKey("fit")) {
            String fit = String.valueOf(changedProps.get("fit"));
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.d(TAG, "[ImageComponent] onUpdateProperties - setting fit: " + fit);
            }
            imageView.setScaleType(parseFit(fit));
        }

        boolean sizeFromReport = false;
        boolean hasSizeChange = false;
        float yogaWidth = 0f;
        float yogaHeight = 0f;

        // Handle styles (read from styles)
        if (changedProps.containsKey("styles")) {
            @SuppressWarnings("unchecked")
            Map<String, Object> styles = (Map<String, Object>) changedProps.get("styles");
            if (styles != null) {
                StyleHelper.applyCSSPadding(imageView, styles, context);
                if (styles != null) {
                    sizeFromReport = Boolean.TRUE.equals(styles.get("sizeFromReport"));
                    hasSizeChange = styles.containsKey("width") || styles.containsKey("height");
                    if (!sizeFromReport) {
                        Object w = styles.get("width");
                        Object h = styles.get("height");
                        if (w instanceof Number) yogaWidth = ((Number) w).floatValue();
                        if (h instanceof Number) yogaHeight = ((Number) h).floatValue();
                        currentYogaWidth = yogaWidth;
                        currentYogaHeight = yogaHeight;
                    }
                }
            }
        }
        // Load image if url changed, or if size changed from CSS layout (not from our own report)
        if (changedProps.containsKey("url") || (hasSizeChange && !sizeFromReport && properties.containsKey("url"))) {
            String url = extractStringValue(changedProps.containsKey("url")
                    ? changedProps.get("url")
                    : properties.get("url"));
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.d(TAG, "[ImageComponent] onUpdateProperties - loading image url: " + url);
            }
            loadImage(url, yogaWidth, yogaHeight);
        }

    }

    /**
     * Load the image.
     * Uses a pluggable ImageLoader to load network images and local resources.
     * Integrates Shimmer loading animation and transition animation.
     */
    private void loadImage(String src, float yogaWidth, float yogaHeight) {
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
        Map<String, Object> options = buildLoadOptions(yogaWidth, yogaHeight);
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
                reportImageRenderSizeIfNeeded(result.drawable, yogaWidth, yogaHeight);
                onImageLoadComplete(result.isFromCache);
            }

            @Override
            public void onFailure(@NonNull ImageLoaderError error) {
                String activeRequestId = requestIdHolder[0];
                if (activeRequestId == null || !activeRequestId.equals(currentRequestId)) {
                    return;
                }
                currentRequestId = null;
                AGenUILogger.e(TAG, "[ImageComponent] Image load failed: " + id + ", error: " + error.getMessage());
                if (shimmerTransition != null) {
                    shimmerTransition.stopShimmer();
                }
                // Set the global default placeholder
                Drawable placeholder = ImageLoaderConfig.getInstance().getDefaultPlaceholder();
                if (placeholder != null) {
                    imageView.setImageDrawable(placeholder);
                }
                reportImageRenderSizeIfNeeded(null, yogaWidth, yogaHeight);
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
    private Map<String, Object> buildLoadOptions(float yogaWidth, float yogaHeight) {
        Map<String, Object> options = new HashMap<>();

        if (yogaWidth > 0) {
            float px = StyleHelper.standardUnitToPx(context, yogaWidth);
            if (px > 0) options.put(ImageLoadOptionsKey.WIDTH, px);
        }
        if (yogaHeight > 0) {
            float px = StyleHelper.standardUnitToPx(context, yogaHeight);
            if (px > 0) options.put(ImageLoadOptionsKey.HEIGHT, px);
        }

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

    private void reportImageRenderSizeIfNeeded(Drawable drawable, float yogaWidth, float yogaHeight) {
        if (yogaWidth != currentYogaWidth || yogaHeight != currentYogaHeight) {
            return;
        }

        if (yogaWidth > 0 && yogaHeight > 0) {
            notifyRenderFinish("Image", yogaWidth, yogaHeight);
            return;
        }

        if (drawable == null) {
            notifyRenderFinish("Image", yogaWidth, yogaHeight);
            return;
        }

        int intrinsicW = drawable.getIntrinsicWidth();
        int intrinsicH = drawable.getIntrinsicHeight();
        if (intrinsicW <= 0 || intrinsicH <= 0) {
            notifyRenderFinish("Image", yogaWidth, yogaHeight);
            return;
        }

        float aspectRatio = (float) intrinsicW / intrinsicH;

        if (yogaWidth > 0) {
            float computedH = yogaWidth / aspectRatio;
            notifyRenderFinish("Image", yogaWidth, computedH);
            return;
        }

        if (yogaHeight > 0) {
            float computedW = yogaHeight * aspectRatio;
            notifyRenderFinish("Image", computedW, yogaHeight);
            return;
        }

        float wPx = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, intrinsicW,
                context.getResources().getDisplayMetrics());
        float hPx = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, intrinsicH,
                context.getResources().getDisplayMetrics());
        if (wPx > 0 && hPx > 0) {
            notifyRenderFinishFromPx("Image", wPx, hPx);
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
