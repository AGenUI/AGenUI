package com.amap.agenui.render.drawable;

import android.graphics.Bitmap;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.RectF;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import com.amap.a2ui_sdk.R;
import com.amap.agenui.render.utils.AGenUILogger;

import java.util.Objects;

/**
 * Software-rendered drop-shadow that does not rely on {@link View#setElevation(float)}.
 *
 * <p>Shadow config and cached bitmap are stored on the child view via two tags
 * ({@code R.id.agenui_shadow_config} and {@code R.id.agenui_shadow_bitmap}).
 * The parent container calls {@link #drawIfNeeded} from its {@code drawChild} override
 * to paint the shadow before the child content. This keeps the shadow behind the view
 * and never affects the Z-order of sibling views.
 *
 * <p>The bitmap is generated at 0.5x scale and drawn scaled-up (4x pixel reduction).
 * It is created lazily on first draw when the view has a valid size, and recreated
 * automatically when the view size changes.
 */
public final class ShadowPainter {

    private static final String TAG = "ShadowPainter";
    private static final float SHADOW_SCALE = 0.5f;
    // Fine-tunes the perceived blur to match iOS rendering.
    private static final float BLUR_RECTIFICATION = 0.735f;
    private static final int MAX_BLUR_RADIUS = 128;

    // sPaint: draws the finished shadow bitmap onto the on-screen canvas (drawIfNeeded only).
    // Kept in its original form — constructed with the two flags, never reset, never given a
    // maskfilter/xfermode — so a retained hardware display-list op can't turn the blit into an erase.
    private static final Paint sPaint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.ANTI_ALIAS_FLAG);
    // sBuildPaint: draws into the off-screen shadow bitmap (shadow shape + hollow-out) in
    // createBitmap. Those draws hit a software Canvas and execute immediately, so reset()/CLEAR
    // here is safe and stays fully isolated from the on-screen blit paint above.
    private static final Paint sBuildPaint = new Paint();
    private static final RectF sDstRect = new RectF();
    private static final PorterDuffXfermode CLEAR_XFERMODE = new PorterDuffXfermode(PorterDuff.Mode.CLEAR);

    private ShadowPainter() {
    }

    /**
     * Immutable shadow configuration.
     */
    public static final class ShadowConfig {
        public final int shadowColor;
        public final int offsetX;
        public final int offsetY;
        public final int blurRadius;
        public final int cornerRadius;

        public ShadowConfig(int shadowColor, int offsetX, int offsetY,
                            int blurRadius, int cornerRadius) {
            this.shadowColor = shadowColor;
            this.offsetX = offsetX;
            this.offsetY = offsetY;
            this.blurRadius = blurRadius;
            this.cornerRadius = cornerRadius;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof ShadowConfig)) return false;
            ShadowConfig that = (ShadowConfig) o;
            return shadowColor == that.shadowColor
                    && offsetX == that.offsetX
                    && offsetY == that.offsetY
                    && blurRadius == that.blurRadius
                    && cornerRadius == that.cornerRadius;
        }

        @Override
        public int hashCode() {
            int result = shadowColor;
            result = 31 * result + offsetX;
            result = 31 * result + offsetY;
            result = 31 * result + blurRadius;
            result = 31 * result + cornerRadius;
            return result;
        }
    }

    /**
     * Cached bitmap together with the view dimensions it was created for.
     */
    static final class ShadowCache {
        final Bitmap bitmap;
        final int viewWidth;
        final int viewHeight;

        ShadowCache(Bitmap bitmap, int viewWidth, int viewHeight) {
            this.bitmap = bitmap;
            this.viewWidth = viewWidth;
            this.viewHeight = viewHeight;
        }
    }

    /**
     * Stores the shadow config on the view. The bitmap is NOT created here; it will be
     * created lazily on the first {@link #drawIfNeeded} call when the view has a valid size.
     */
    public static void setConfig(View view, ShadowConfig config) {
        ShadowConfig old = getConfig(view);
        if (Objects.equals(old, config)) {
            return;
        }
        view.setTag(R.id.agenui_shadow_config, config);
        recycleCache(view);
        ViewParent parent = view.getParent();
        if (parent instanceof View) {
            ((View) parent).invalidate();
        }
    }

    private static void disableClipOnAncestors(View view) {
        ViewParent parent = view.getParent();
        if (!(parent instanceof ViewGroup)) {
            return;
        }
        ViewGroup parentGroup = (ViewGroup) parent;
        if (Boolean.TRUE.equals(parentGroup.getTag(R.id.agenui_overflow_hidden))) {
            return;
        }
        parentGroup.setClipChildren(false);
        parentGroup.setClipToPadding(false);

        // 不影响surface_root的父容器
        if (Boolean.TRUE.equals(parentGroup.getTag(R.id.agenui_surface_root))) {
            return;
        }

        ViewParent grandparent = parentGroup.getParent();
        if (!(grandparent instanceof ViewGroup)) {
            return;
        }
        ViewGroup grandparentGroup = (ViewGroup) grandparent;
        if (Boolean.TRUE.equals(grandparentGroup.getTag(R.id.agenui_overflow_hidden))) {
            return;
        }
        grandparentGroup.setClipChildren(false);
        grandparentGroup.setClipToPadding(false);
    }

    /**
     * Recycles the cached shadow bitmap. The shadow config is retained so that
     * the bitmap can be recreated lazily when the view is next drawn.
     */
    public static void clearBitmap(View view) {
        recycleCache(view);
    }

    /**
     * Returns the shadow config currently attached to the view, or {@code null}.
     */
    public static ShadowConfig getConfig(View view) {
        Object tag = view.getTag(R.id.agenui_shadow_config);
        return tag instanceof ShadowConfig ? (ShadowConfig) tag : null;
    }

    /**
     * Draws the shadow for {@code child} onto {@code canvas} if the child has a shadow config.
     *
     * <p>The bitmap is created lazily on first draw rather than in onLayout because child views
     * may not have their final size during layout (getWidth/getHeight can be 0 or stale),
     * which would produce a wasted bitmap that is immediately discarded on the next size change.
     * At 0.5x scale the bitmap is typically tens of KB and creation is sub-millisecond, so
     * the first-frame cost is negligible for normal use. If profiling shows jank on low-end
     * devices with many shadow items entering the viewport simultaneously, consider deferring
     * creation via {@code view.post(() -> ensureBitmap(...))} instead of moving it to onLayout.
     *
     * <p>Parent containers should call this at the top of their {@code drawChild} override,
     * before {@code super.drawChild}.
     */
    public static void drawIfNeeded(Canvas canvas, View child) {
        ShadowConfig config = getConfig(child);
        if (config == null) {
            return;
        }
        ShadowCache cache = getCache(child);
        if (cache == null) {
            disableClipOnAncestors(child);
        }
        Bitmap bitmap = ensureBitmap(child, config);
        if (bitmap == null) {
            return;
        }

        int paddingX = getPaddingX(config);
        int paddingY = getPaddingY(config);
        int dstLeft = child.getLeft() + (int) child.getTranslationX() - paddingX + config.offsetX;
        int dstTop = child.getTop() + (int) child.getTranslationY() - paddingY + config.offsetY;
        int dstRight = dstLeft + (int) (bitmap.getWidth() / SHADOW_SCALE);
        int dstBottom = dstTop + (int) (bitmap.getHeight() / SHADOW_SCALE);

        sDstRect.set(dstLeft, dstTop, dstRight, dstBottom);
        // config.shadowColor already has the element's declared opacity folded into its alpha at
        // build time (see StyleHelper.parseDropShadowConfig). We deliberately do NOT read
        // child.getAlpha() here: it is a transient value during animation and this pass may not
        // re-run when it changes.
        sPaint.setColor(config.shadowColor);
        // The child's own box is already hollowed out of the bitmap (see createBitmap), so the
        // shadow forms only a halo and never shows through a semi-transparent child.
        canvas.drawBitmap(bitmap, null, sDstRect, sPaint);
    }

    private static Bitmap ensureBitmap(View child, ShadowConfig config) {
        int w = child.getWidth();
        int h = child.getHeight();
        if (w <= 0 || h <= 0) {
            return null;
        }

        ShadowCache cache = getCache(child);
        if (cache != null && cache.viewWidth == w && cache.viewHeight == h
                && cache.bitmap != null && !cache.bitmap.isRecycled()) {
            return cache.bitmap;
        }

        recycleCache(child);
        Bitmap bitmap = createBitmap(w, h, config);
        if (bitmap != null) {
            child.setTag(R.id.agenui_shadow_bitmap, new ShadowCache(bitmap, w, h));
        }
        return bitmap;
    }

    private static Bitmap createBitmap(int viewWidth, int viewHeight, ShadowConfig config) {
        if ((config.shadowColor >>> 24) == 0) {
            return null;
        }

        int paddingX = getPaddingX(config);
        int paddingY = getPaddingY(config);
        int clampedBlur = Math.min(config.blurRadius, MAX_BLUR_RADIUS);
        int shadowW = Math.max(1, (int) ((viewWidth + paddingX * 2) * SHADOW_SCALE));
        int shadowH = Math.max(1, (int) ((viewHeight + paddingY * 2) * SHADOW_SCALE));

        // ALPHA_8: 1 byte/pixel (vs ARGB_8888's 4). Shadow is single-color with varying
        // alpha from the blur; the shadow color is applied at draw time via the Paint.
        Bitmap bitmap;
        try {
            bitmap = Bitmap.createBitmap(shadowW, shadowH, Bitmap.Config.ALPHA_8);
        } catch (OutOfMemoryError e) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "OOM creating shadow bitmap " + shadowW + "x" + shadowH);
            }
            return null;
        }

        Canvas canvas = new Canvas(bitmap);
        // ALPHA_8 target: only the paint's alpha is written (RGB is discarded), and reset() already
        // leaves it fully opaque, so we just need FILL. The shadow color is applied at blit time.
        sBuildPaint.reset();
        sBuildPaint.setAntiAlias(true);
        sBuildPaint.setStyle(Paint.Style.FILL);
        if (clampedBlur > 0) {
            float blur = clampedBlur * SHADOW_SCALE * BLUR_RECTIFICATION;
            sBuildPaint.setMaskFilter(new BlurMaskFilter(Math.max(1f, blur), BlurMaskFilter.Blur.NORMAL));
        }

        float contentW = viewWidth * SHADOW_SCALE;
        float contentH = viewHeight * SHADOW_SCALE;
        float left = paddingX * SHADOW_SCALE;
        float top = paddingY * SHADOW_SCALE;
        RectF rect = new RectF(left, top, left + contentW, top + contentH);

        float radius = config.cornerRadius > 0 ? config.cornerRadius * SHADOW_SCALE : 0f;
        if (radius > 0f) {
            canvas.drawRoundRect(rect, radius, radius, sBuildPaint);
        } else {
            canvas.drawRect(rect, sBuildPaint);
        }

        // Hollow out the child's own box so the shadow is only a halo around it (CSS clips outer
        // shadow to outside the border box; opaque content would cover this area anyway). This is
        // what keeps the shadow from showing through a semi-transparent child. The hole sits at the
        // child's real position, i.e. the shadow core (rect) shifted back by the drop offset.
        sBuildPaint.reset();
        sBuildPaint.setAntiAlias(true);
        sBuildPaint.setStyle(Paint.Style.FILL);
        sBuildPaint.setXfermode(CLEAR_XFERMODE);
        float holeLeft = (paddingX - config.offsetX) * SHADOW_SCALE;
        float holeTop = (paddingY - config.offsetY) * SHADOW_SCALE;
        RectF holeRect = new RectF(holeLeft, holeTop, holeLeft + contentW, holeTop + contentH);
        if (radius > 0f) {
            canvas.drawRoundRect(holeRect, radius, radius, sBuildPaint);
        } else {
            canvas.drawRect(holeRect, sBuildPaint);
        }

        return bitmap;
    }

    private static ShadowCache getCache(View view) {
        Object tag = view.getTag(R.id.agenui_shadow_bitmap);
        return tag instanceof ShadowCache ? (ShadowCache) tag : null;
    }

    private static void recycleCache(View view) {
        ShadowCache cache = getCache(view);
        if (cache != null && cache.bitmap != null && !cache.bitmap.isRecycled()) {
            cache.bitmap.recycle();
        }
        view.setTag(R.id.agenui_shadow_bitmap, null);
    }

    static int getPaddingX(ShadowConfig config) {
        return Math.min(config.blurRadius, MAX_BLUR_RADIUS) + Math.abs(config.offsetX);
    }

    static int getPaddingY(ShadowConfig config) {
        return Math.min(config.blurRadius, MAX_BLUR_RADIUS) + Math.abs(config.offsetY);
    }
}
