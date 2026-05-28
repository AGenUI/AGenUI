package com.amap.agenui.render.surface;

import android.content.Context;

import androidx.annotation.Keep;

import com.amap.agenui.AGenUI;
import com.amap.agenui.render.style.StyleHelper;
import com.amap.agenui.render.utils.AGenUILogger;

/**
 * Size of a surface in a2ui virtual-pixel (vp) units.
 *
 * <p>Mirrors the C++ {@code agenui::SurfaceSize} struct. Returned from
 * {@link ISurfaceManagerListener#surfaceSize(String)} and read back by the
 * native engine via JNI field reflection, so the {@link #width} / {@link #height}
 * field layout is part of the binary contract — do not rename or change their
 * types without updating
 * {@code core/src/jni/jni_surface_size_provider_bridge.cpp}.
 *
 * <p>Hosts construct instances by passing raw screen pixels; the constructor
 * internally normalizes them to a2ui (vp) units via the same
 * {@link StyleHelper#pxToA2ui} formula the SDK uses on the push channel
 * ({@code SurfaceLayoutDispatcher.reportSurfaceSize}). Push and pull are
 * therefore guaranteed to agree on units — hosts never deal with vp directly.
 *
 * <p>The Application context required for the px → vp conversion is read
 * from {@link AGenUI#getApplicationContextForSdk()}, so callers do not need
 * to thread one through. The conversion uses the same {@code DisplayMetrics}
 * the SDK pushes to native at {@code AGenUI.initialize()} time, so density
 * source is consistent across the SDK.
 *
 * <p>Non-positive components (zero or negative width/height) are treated by
 * the engine as "not measurable yet": the in-flight layout pass is skipped
 * and replayed once a positive size arrives via either the provider channel
 * or a subsequent {@code notifySurfaceSizeChanged} push.
 */
@Keep
public final class SurfaceSize {

    private static final String TAG = "SurfaceSize";

    /** Width in a2ui (vp) units; populated from the px argument by the constructor. */
    @Keep
    public final float width;

    /** Height in a2ui (vp) units; populated from the px argument by the constructor. */
    @Keep
    public final float height;

    /**
     * Constructs a SurfaceSize from raw screen-pixel values.
     *
     * @param widthPx  Width in screen pixels. Pass 0 (or any non-positive value)
     *                 to signal "no constraint on this axis".
     * @param heightPx Height in screen pixels. Same "no constraint" semantics.
     */
    public SurfaceSize(float widthPx, float heightPx) {
        Context context = AGenUI.getInstance().getApplicationContextForSdk();
        if (context == null) {
            // Engine not initialized yet — should be unreachable for real
            // host integrations because creating a SurfaceManager already
            // requires initialize() to have run. Log loudly and fall through
            // to StyleHelper's null branch (returns the input unchanged), so
            // the caller can spot the misuse rather than receive a silently-
            // wrong vp value scaled with density=1.
            AGenUILogger.w(TAG, "Application context unavailable — AGenUI.initialize() not called?"
                    + " width/height will be stored verbatim and likely produce wrong layout.");
        }
        this.width = StyleHelper.pxToA2ui(context, widthPx);
        this.height = StyleHelper.pxToA2ui(context, heightPx);
    }

    @Override
    public String toString() {
        return "SurfaceSize{width=" + width + ", height=" + height + "}";
    }
}
