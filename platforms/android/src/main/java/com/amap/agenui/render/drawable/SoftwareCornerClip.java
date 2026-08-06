package com.amap.agenui.render.drawable;

import android.graphics.Canvas;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.View;

import com.amap.a2ui_sdk.R;

/**
 * Software-canvas fallback for rounded-corner clipping.
 *
 * Rounded corners are normally clipped by {@code setClipToOutline}, which is a RenderNode-only
 * property: it silently does nothing when a view is drawn onto a bitmap-backed (software) canvas
 * — e.g. AJX node snapshots ({@code SnapshotUtil.snapShotByView} → {@code view.draw(swCanvas)}),
 * blank-screen bitmap sampling, or any manual {@code View.draw} call. Without this fallback,
 * screenshots render rounded containers with square corners.
 *
 * Mirrors AJX's {@code SoftwareRoundCornerRender} (canvas.clipPath), but hooks into the PARENT's
 * {@code drawChild} instead of the view's own draw — A2UI components are stock platform views
 * with no unified self-draw hook. Coverage therefore equals the {@code drawChild} overrides that
 * call it (YogaAbsoluteLayout and the Surface root container), the same coverage ShadowPainter
 * has.
 *
 * The corner radius is published by {@code StyleHelper.applyOutlineRadiusClip} via the
 * {@code agenui_corner_radius} tag whenever it installs a rounded outline, and cleared when the
 * radius is reset — keeping this fallback in exact sync with the hardware outline clip.
 *
 * Coverage has two independent conditions, both of which must hold:
 *   1. the parent overrides drawChild and calls this (YogaAbsoluteLayout, Surface root);
 *   2. the child published agenui_corner_radius.
 * TODO: CarouselComponent and ModalComponent call setClipToOutline(true) directly with a
 * hardcoded radius and never publish the tag, so their rounded corners still come out square in
 * snapshots. Not wired up yet because neither component is registered in
 * ComponentRegistry.registerBuiltInComponents, i.e. unreachable in the current product.
 *
 * Zero overhead in normal rendering: the first check short-circuits on hardware canvases.
 * No anti-aliasing on the clipPath edge (same trade-off AJX ships with; acceptable for
 * screenshots). Main-thread only, like all View drawing.
 */
public final class SoftwareCornerClip {

    private static final Path sPath = new Path();
    private static final RectF sRect = new RectF();

    private SoftwareCornerClip() {
    }

    /**
     * If {@code canvas} is a software canvas and {@code child} declares a rounded outline clip,
     * saves the canvas and clips it to the child's round rect (in the parent's coordinate
     * space). Returns the canvas save count to restore after {@code super.drawChild}, or -1
     * when nothing was clipped. Call from the parent's {@code drawChild}.
     *
     * Reusing static Path/RectF is safe: drawing is single-threaded and nested drawChild calls
     * only happen after the outer clipPath has already been consumed by the canvas.
     */
    public static int clipIfNeeded(Canvas canvas, View child) {
        if (canvas.isHardwareAccelerated() || !child.getClipToOutline()) {
            return -1;
        }
        Object tag = child.getTag(R.id.agenui_corner_radius);
        if (!(tag instanceof Integer)) {
            return -1;
        }
        int radius = (Integer) tag;
        if (radius <= 0) {
            return -1;
        }
        int saveCount = canvas.save();
        // Child position in the parent's canvas is plain getLeft()/getTop(). A scrolling parent
        // has already had its own scroll folded into the canvas by the time dispatchDraw runs:
        // its parent drew it with canvas.translate(mLeft - mScrollX, mTop - mScrollY), so this
        // canvas is in the parent's *content* space (which is also why dispatchDraw's
        // clipToPadding rect starts at mScrollX). Subtracting parent.getScrollX() here would
        // double-count it. Same convention as ShadowPainter.drawIfNeeded a few lines above the
        // call site.
        float dx = child.getLeft() + child.getTranslationX();
        float dy = child.getTop() + child.getTranslationY();
        sRect.set(dx, dy, dx + child.getWidth(), dy + child.getHeight());
        sPath.rewind();
        sPath.addRoundRect(sRect, radius, radius, Path.Direction.CW);
        canvas.clipPath(sPath);
        return saveCount;
    }
}
