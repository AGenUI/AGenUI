package com.amap.agenui.render.component.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.widget.TextView;

/**
 * Custom {@link TextView} used by AGenUI's Text component.
 *
 * <p><b>Problem.</b> Stock {@link TextView#onDraw(Canvas)} clips the canvas to
 * the compound-padding content box, so any glyph outside that box is silently
 * clipped. AGenUI's CSS model requires:
 * <ul>
 *   <li>Text starts drawing at {@code paddingTop} from the view top.</li>
 *   <li>Text extends downward without being clipped by {@code paddingBottom}
 *       (W3C {@code overflow: visible}).</li>
 *   <li>{@code text-align} only controls horizontal alignment (W3C); vertical
 *       is always top-aligned.</li>
 * </ul>
 *
 * <p><b>Solution.</b> We override {@link #getCompoundPaddingBottom()} to
 * return 0 only while {@link #onDraw} is executing, so that during the draw
 * phase the inner {@link android.text.StaticLayout} extends past the
 * content-box bottom edge. In {@link #onDraw(Canvas)}, we wrap the canvas in
 * a {@link TextDrawCanvas} that intercepts the first {@code clipRect} inside
 * {@code super.onDraw} to set its bottom to {@code Integer.MAX_VALUE},
 * removing the bottom clip while keeping the top (paddingTop) intact —
 * TextView already draws text starting at paddingTop by default.
 * All other operations flow through {@code super.onDraw} unchanged.
 *
 * <p>Extends {@link android.widget.TextView} (NOT AppCompatTextView) on
 * purpose: AGenUI consumers do not require AppCompat theming.
 */
public class A2UITextView extends TextView {

    /**
     * Set to true while onDraw is executing; guards getCompoundPaddingBottom.
     */
    private boolean mInDraw = false;
    private final TextDrawCanvas mCanvas = new TextDrawCanvas();


    public A2UITextView(Context context) {
        super(context);
    }

    public A2UITextView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public A2UITextView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    /**
     * Returns 0 during {@link #onDraw} so that the StaticLayout extends past
     * the content-box bottom edge, allowing overflow text to be drawn.
     * Outside of onDraw, delegates to the real {@code super} value so that
     * measurement is unaffected.
     */
    @Override
    public int getCompoundPaddingBottom() {
        if (mInDraw) return 0;
        return super.getCompoundPaddingBottom();
    }

    /**
     * Wrap canvas so that {@code super.onDraw}'s internal clipRect
     * has its bottom removed (overflow visible).
     */
    @Override
    protected void onDraw(Canvas canvas) {
        mInDraw = true;
        try {
            if (canvas.isHardwareAccelerated()) {
                // HW canvas: delegate directly — the wrapper's native peer (from
                // the Canvas() no-arg constructor) is a software canvas, so using
                // it as a proxy for a hardware canvas causes a SIGSEGV in
                // libhwui.so. HW rendering handles clipRect internally through
                // the GPU pipeline; bottom-clip interception is not needed here.
                super.onDraw(canvas);
            } else {
                // SW canvas: wrap to intercept clipRect bottom → overflow:visible.
                super.onDraw(mCanvas.setDelegate(canvas));
            }
        } finally {
            mInDraw = false;
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    //  TextDrawCanvas — intercepts clipRect bottom only, delegates the rest
    // ─────────────────────────────────────────────────────────────────────

    /**
     * A thin Canvas wrapper that intercepts the first {@code clipRect} call
     * inside {@link TextView#onDraw} to set its bottom to
     * {@code Integer.MAX_VALUE}, removing the content-box bottom clip.
     * All other operations are forwarded verbatim.
     */
    private static class TextDrawCanvas extends Canvas {

        private Canvas mDelegate;
        /**
         * true once we've intercepted the content-box clipRect.
         */
        private boolean mClipIntercepted = false;

        public TextDrawCanvas setDelegate(Canvas delegate) {
            mDelegate = delegate;
            mClipIntercepted = false;
            return this;
        }

        // ── clipRect: remove bottom ─────────────────────────────────────

        @Override
        public boolean clipRect(int left, int top, int right, int bottom) {
            if (!mClipIntercepted) {
                mClipIntercepted = true;
                return mDelegate.clipRect(left, top, right, Integer.MAX_VALUE);
            }
            return mDelegate.clipRect(left, top, right, bottom);
        }

        @Override
        public boolean clipRect(float left, float top, float right, float bottom) {
            if (!mClipIntercepted) {
                mClipIntercepted = true;
                return mDelegate.clipRect(left, top, right, Float.MAX_VALUE);
            }
            return mDelegate.clipRect(left, top, right, bottom);
        }

        @Override
        public boolean clipRect(Rect rect) {
            if (!mClipIntercepted) {
                mClipIntercepted = true;
                return mDelegate.clipRect(rect.left, rect.top, rect.right, Integer.MAX_VALUE);
            }
            return mDelegate.clipRect(rect);
        }

        @Override
        public boolean clipRect(RectF rect) {
            if (!mClipIntercepted) {
                mClipIntercepted = true;
                return mDelegate.clipRect(new RectF(rect.left, rect.top, rect.right, Float.MAX_VALUE));
            }
            return mDelegate.clipRect(rect);
        }

        // ── everything else: forward to delegate ───────────────────────

        @Override
        public int save() {
            return mDelegate.save();
        }

        @Override
        public void restore() {
            mDelegate.restore();
        }

        @Override
        public void restoreToCount(int saveCount) {
            mDelegate.restoreToCount(saveCount);
        }

        @Override
        public int getSaveCount() {
            return mDelegate.getSaveCount();
        }

        @Override
        public int getWidth() {
            return mDelegate.getWidth();
        }

        @Override
        public int getHeight() {
            return mDelegate.getHeight();
        }

        @Override
        public boolean isHardwareAccelerated() {
            return false;
        }

        @Override
        public void scale(float sx, float sy) {
            mDelegate.scale(sx, sy);
        }

        @Override
        public void rotate(float degrees) {
            mDelegate.rotate(degrees);
        }

        @Override
        public void translate(float dx, float dy) {
            mDelegate.translate(dx, dy);
        }

        @Override
        public void skew(float sx, float sy) {
            mDelegate.skew(sx, sy);
        }

        @Override
        public void concat(Matrix matrix) {
            mDelegate.concat(matrix);
        }

        @Override
        public void setMatrix(Matrix matrix) {
            mDelegate.setMatrix(matrix);
        }

        @Override
        public void getMatrix(Matrix ctm) {
            mDelegate.getMatrix(ctm);
        }

        @Override
        public boolean clipPath(Path path) {
            return mDelegate.clipPath(path);
        }

        @Override
        public android.graphics.DrawFilter getDrawFilter() {
            return mDelegate.getDrawFilter();
        }

        @Override
        public void setDrawFilter(android.graphics.DrawFilter filter) {
            mDelegate.setDrawFilter(filter);
        }

        @Override
        public boolean getClipBounds(Rect bounds) {
            return mDelegate.getClipBounds(bounds);
        }

        @Override
        public void drawARGB(int a, int r, int g, int b) {
            mDelegate.drawARGB(a, r, g, b);
        }

        @Override
        public void drawRGB(int r, int g, int b) {
            mDelegate.drawRGB(r, g, b);
        }

        @Override
        public void drawColor(int color) {
            mDelegate.drawColor(color);
        }

        @Override
        public void drawColor(int color, android.graphics.PorterDuff.Mode mode) {
            mDelegate.drawColor(color, mode);
        }

        @Override
        public void drawPaint(Paint paint) {
            mDelegate.drawPaint(paint);
        }

        @Override
        public void drawLine(float startX, float startY, float stopX, float stopY, Paint paint) {
            mDelegate.drawLine(startX, startY, stopX, stopY, paint);
        }

        @Override
        public void drawLines(float[] pts, Paint paint) {
            mDelegate.drawLines(pts, paint);
        }

        @Override
        public void drawLines(float[] pts, int offset, int count, Paint paint) {
            mDelegate.drawLines(pts, offset, count, paint);
        }

        @Override
        public void drawPoint(float x, float y, Paint paint) {
            mDelegate.drawPoint(x, y, paint);
        }

        @Override
        public void drawPoints(float[] pts, Paint paint) {
            mDelegate.drawPoints(pts, paint);
        }

        @Override
        public void drawPoints(float[] pts, int offset, int count, Paint paint) {
            mDelegate.drawPoints(pts, offset, count, paint);
        }

        @Override
        public void drawRect(float left, float top, float right, float bottom, Paint paint) {
            mDelegate.drawRect(left, top, right, bottom, paint);
        }

        @Override
        public void drawRect(Rect r, Paint paint) {
            mDelegate.drawRect(r, paint);
        }

        @Override
        public void drawRect(RectF rect, Paint paint) {
            mDelegate.drawRect(rect, paint);
        }

        @Override
        public void drawOval(RectF oval, Paint paint) {
            mDelegate.drawOval(oval, paint);
        }

        @Override
        public void drawOval(float left, float top, float right, float bottom, Paint paint) {
            mDelegate.drawOval(left, top, right, bottom, paint);
        }

        @Override
        public void drawCircle(float cx, float cy, float radius, Paint paint) {
            mDelegate.drawCircle(cx, cy, radius, paint);
        }

        @Override
        public void drawArc(RectF oval, float startAngle, float sweepAngle, boolean useCenter, Paint paint) {
            mDelegate.drawArc(oval, startAngle, sweepAngle, useCenter, paint);
        }

        @Override
        public void drawArc(float left, float top, float right, float bottom, float startAngle, float sweepAngle, boolean useCenter, Paint paint) {
            mDelegate.drawArc(left, top, right, bottom, startAngle, sweepAngle, useCenter, paint);
        }

        @Override
        public void drawRoundRect(RectF rect, float rx, float ry, Paint paint) {
            mDelegate.drawRoundRect(rect, rx, ry, paint);
        }

        @Override
        public void drawRoundRect(float left, float top, float right, float bottom, float rx, float ry, Paint paint) {
            mDelegate.drawRoundRect(left, top, right, bottom, rx, ry, paint);
        }

        @Override
        public void drawPath(Path path, Paint paint) {
            mDelegate.drawPath(path, paint);
        }

        @Override
        public void drawBitmap(android.graphics.Bitmap bitmap, float left, float top, Paint paint) {
            mDelegate.drawBitmap(bitmap, left, top, paint);
        }

        @Override
        public void drawBitmap(android.graphics.Bitmap bitmap, Rect src, Rect dst, Paint paint) {
            mDelegate.drawBitmap(bitmap, src, dst, paint);
        }

        @Override
        public void drawBitmap(android.graphics.Bitmap bitmap, Rect src, RectF dst, Paint paint) {
            mDelegate.drawBitmap(bitmap, src, dst, paint);
        }

        @Override
        public void drawBitmap(android.graphics.Bitmap bitmap, Matrix matrix, Paint paint) {
            mDelegate.drawBitmap(bitmap, matrix, paint);
        }

        @Override
        public void drawBitmap(int[] colors, int offset, int stride, float x, float y, int width, int height, boolean hasAlpha, Paint paint) {
            mDelegate.drawBitmap(colors, offset, stride, x, y, width, height, hasAlpha, paint);
        }

        @Override
        public void drawBitmap(int[] colors, int offset, int stride, int x, int y, int width, int height, boolean hasAlpha, Paint paint) {
            mDelegate.drawBitmap(colors, offset, stride, x, y, width, height, hasAlpha, paint);
        }

        @Override
        public void drawText(char[] text, int index, int count, float x, float y, Paint paint) {
            mDelegate.drawText(text, index, count, x, y, paint);
        }

        @Override
        public void drawText(CharSequence text, int start, int end, float x, float y, Paint paint) {
            mDelegate.drawText(text, start, end, x, y, paint);
        }

        @Override
        public void drawText(String text, float x, float y, Paint paint) {
            mDelegate.drawText(text, x, y, paint);
        }

        @Override
        public void drawText(String text, int start, int end, float x, float y, Paint paint) {
            mDelegate.drawText(text, start, end, x, y, paint);
        }

        @Override
        public void drawTextOnPath(char[] text, int index, int count, Path path, float hOffset, float vOffset, Paint paint) {
            mDelegate.drawTextOnPath(text, index, count, path, hOffset, vOffset, paint);
        }

        @Override
        public void drawTextOnPath(String text, Path path, float hOffset, float vOffset, Paint paint) {
            mDelegate.drawTextOnPath(text, path, hOffset, vOffset, paint);
        }

        @Override
        public void drawPicture(android.graphics.Picture picture) {
            mDelegate.drawPicture(picture);
        }

        @Override
        public void drawPicture(android.graphics.Picture picture, Rect dst) {
            mDelegate.drawPicture(picture, dst);
        }

        @Override
        public void drawPicture(android.graphics.Picture picture, RectF dst) {
            mDelegate.drawPicture(picture, dst);
        }

        @Override
        public void drawVertices(VertexMode mode, int vertexCount, float[] verts, int vertOffset, float[] texs, int texOffset, int[] colors, int colorOffset, short[] indices, int indexOffset, int indexCount, Paint paint) {
            mDelegate.drawVertices(mode, vertexCount, verts, vertOffset, texs, texOffset, colors, colorOffset, indices, indexOffset, indexCount, paint);
        }

        // API 23+: used by TextView for RTL / complex-script shaping.
        // Without these overrides the calls fall through to the null-bitmap peer
        // of Canvas() and the text is silently discarded.
        @Override
        public void drawTextRun(char[] text, int index, int count, int contextIndex, int contextCount, float x, float y, boolean isRtl, Paint paint) {
            mDelegate.drawTextRun(text, index, count, contextIndex, contextCount, x, y, isRtl, paint);
        }

        @Override
        public void drawTextRun(CharSequence text, int start, int end, int contextStart, int contextEnd, float x, float y, boolean isRtl, Paint paint) {
            mDelegate.drawTextRun(text, start, end, contextStart, contextEnd, x, y, isRtl, paint);
        }

        // saveLayer / saveLayerAlpha: used by TextView for selection highlight compositing.
        @Override
        public int saveLayer(RectF bounds, Paint paint) {
            return mDelegate.saveLayer(bounds, paint);
        }

        @Override
        public int saveLayer(float left, float top, float right, float bottom, Paint paint) {
            return mDelegate.saveLayer(left, top, right, bottom, paint);
        }

        @Override
        public int saveLayerAlpha(RectF bounds, int alpha) {
            return mDelegate.saveLayerAlpha(bounds, alpha);
        }

        @Override
        public int saveLayerAlpha(float left, float top, float right, float bottom, int alpha) {
            return mDelegate.saveLayerAlpha(left, top, right, bottom, alpha);
        }
    }
}
