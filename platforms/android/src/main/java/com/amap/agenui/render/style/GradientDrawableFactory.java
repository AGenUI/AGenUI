package com.amap.agenui.render.style;

import android.content.Context;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.graphics.SweepGradient;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.PaintDrawable;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RectShape;

import com.amap.agenui.ColorValue;
import com.amap.agenui.render.utils.AGenUILogger;

import java.util.ArrayList;
import java.util.List;

/**
 * Builds an Android {@link Drawable} from a parsed CSS gradient
 * ({@link ColorValue.GradientInfo}). Supports linear / radial / conic.
 *
 * <p>The shader is rebuilt every time the host view resizes, via
 * {@link ShapeDrawable.ShaderFactory}, so layout changes are tracked
 * automatically.
 */
public final class GradientDrawableFactory {

    private static final String TAG = "GradientDrawableFactory";

    private GradientDrawableFactory() {
    }

    public static Drawable build(final ColorValue.GradientInfo info, final Context ctx) {
        PaintDrawable drawable = new PaintDrawable();
        drawable.setShape(new RectShape());
        drawable.setShaderFactory(new ShapeDrawable.ShaderFactory() {
            @Override
            public Shader resize(int width, int height) {
                if (width <= 0 || height <= 0) return null;
                switch (info.gradientType) {
                    case ColorValue.GradientInfo.GRADIENT_LINEAR:
                        return buildLinear(info, width, height);
                    case ColorValue.GradientInfo.GRADIENT_RADIAL:
                        return buildRadial(info, width, height, ctx);
                    case ColorValue.GradientInfo.GRADIENT_CONIC:
                        return buildConic(info, width, height);
                    default:
                        return null;
                }
            }
        });
        return drawable;
    }

    private static Shader buildLinear(ColorValue.GradientInfo info, int w, int h) {
        Stops s = collectStops(info.colorStops, /*forSweep=*/ false);
        if (s == null) return null;

        float angle = info.linear != null ? info.linear.angle : 180f;
        if (info.linear != null && info.linear.angleIsCalc) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "linear angle is calc(), using literal: " + info.linear.angleCalcExpr);
            }
        }
        double rad = Math.toRadians(angle - 90.0); // CSS 0deg = to top; Android Y is down
        double dx = Math.cos(rad);
        double dy = Math.sin(rad);

        // CSS gradient line length: |W*sin(a)| + |H*cos(a)|
        double angleRad = Math.toRadians(angle);
        double len = Math.abs(w * Math.sin(angleRad)) + Math.abs(h * Math.cos(angleRad));

        float cx = w / 2f;
        float cy = h / 2f;
        float startX = (float) (cx - dx * len / 2.0);
        float startY = (float) (cy - dy * len / 2.0);
        float endX = (float) (cx + dx * len / 2.0);
        float endY = (float) (cy + dy * len / 2.0);

        Shader.TileMode tile = info.isRepeating ? Shader.TileMode.REPEAT : Shader.TileMode.CLAMP;
        return new LinearGradient(startX, startY, endX, endY, s.colors, s.positions, tile);
    }

    private static Shader buildRadial(ColorValue.GradientInfo info, int w, int h, Context ctx) {
        Stops s = collectStops(info.colorStops, /*forSweep=*/ false);
        if (s == null || info.radial == null) return null;

        ColorValue.RadialParams rp = info.radial;
        float cx = rp.centerXIsPx ? StyleHelper.standardUnitToPx(ctx, rp.centerX) : rp.centerX * w;
        float cy = rp.centerYIsPx ? StyleHelper.standardUnitToPx(ctx, rp.centerY) : rp.centerY * h;

        float rx, ry;
        if (rp.hasExplicitSize) {
            if (rp.radiusXIsCalc || rp.radiusYIsCalc) {
                AGenUILogger.w(TAG, "radial radius calc() not supported, using literal");
            }
            rx = rp.radiusXIsPercent ? rp.radiusX * w : StyleHelper.standardUnitToPx(ctx, rp.radiusX);
            ry = rp.radiusYIsPercent ? rp.radiusY * h : StyleHelper.standardUnitToPx(ctx, rp.radiusY);
        } else {
            float[] sides = sidesFromCenter(cx, cy, w, h);     // l, r, t, b distances
            float[] keyword = keywordSize(rp.size, rp.shape, sides);
            rx = keyword[0];
            ry = keyword[1];
        }
        if (rx <= 0f) rx = 1f;
        if (ry <= 0f) ry = 1f;

        float radius = Math.max(rx, ry);
        RadialGradient shader = new RadialGradient(
                cx, cy, radius, s.colors, s.positions,
                info.isRepeating ? Shader.TileMode.REPEAT : Shader.TileMode.CLAMP);

        if (Math.abs(rx - ry) > 0.5f) {
            // Approximate ellipse via local matrix scale around center.
            Matrix m = new Matrix();
            m.setScale(rx / radius, ry / radius, cx, cy);
            shader.setLocalMatrix(m);
        }
        return shader;
    }

    private static Shader buildConic(ColorValue.GradientInfo info, int w, int h) {
        Stops s = collectStops(info.colorStops, /*forSweep=*/ true);
        if (s == null || info.conic == null) return null;

        ColorValue.ConicParams cp = info.conic;
        float cx = cp.centerXIsPx ? cp.centerX : cp.centerX * w;
        float cy = cp.centerYIsPx ? cp.centerY : cp.centerY * h;
        if (cp.centerXIsPx && cp.centerX == 0f && cp.centerY == 0f) {
            // Sensible default: center if parser left both at 0 px (first paint before layout)
            cx = w / 2f;
            cy = h / 2f;
        } else if (!cp.centerXIsPx && cp.centerX == 0f && cp.centerY == 0f) {
            // 0% means top-left corner — keep as-is
        }

        SweepGradient shader = new SweepGradient(cx, cy, s.colors, s.positions);

        // CSS conic: 0deg = 12 o'clock (up); Android sweep starts at +X (3 o'clock).
        // Rotate by (startAngle - 90) so that startAngle=0 maps the first stop to 12 o'clock.
        float startAngle = cp.startAngleIsCalc ? 0f : cp.startAngle;
        if (cp.startAngleIsCalc) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "conic from <angle> is calc(), defaulting to 0: " + cp.startAngleCalcExpr);
            }
        }
        float rotate = startAngle - 90f;
        if (rotate != 0f) {
            Matrix m = new Matrix();
            m.setRotate(rotate, cx, cy);
            shader.setLocalMatrix(m);
        }
        return shader;
    }

    /**
     * {l, r, t, b} distances from (cx, cy) to view sides.
     */
    private static float[] sidesFromCenter(float cx, float cy, int w, int h) {
        return new float[]{
                cx,
                Math.max(0f, w - cx),
                cy,
                Math.max(0f, h - cy)
        };
    }

    /**
     * Returns {radiusX, radiusY} for a radial size keyword.
     * For a circle, both components are equal; for an ellipse, X uses
     * horizontal sides and Y uses vertical sides.
     */
    private static float[] keywordSize(int sizeKeyword, int shape, float[] sides) {
        float left = sides[0], right = sides[1], top = sides[2], bottom = sides[3];
        boolean circle = shape == ColorValue.RadialParams.SHAPE_CIRCLE;
        switch (sizeKeyword) {
            case ColorValue.RadialParams.SIZE_CLOSEST_SIDE: {
                float rx = Math.min(left, right);
                float ry = Math.min(top, bottom);
                return circle ? new float[]{Math.min(rx, ry), Math.min(rx, ry)} : new float[]{rx, ry};
            }
            case ColorValue.RadialParams.SIZE_FARTHEST_SIDE: {
                float rx = Math.max(left, right);
                float ry = Math.max(top, bottom);
                return circle ? new float[]{Math.max(rx, ry), Math.max(rx, ry)} : new float[]{rx, ry};
            }
            case ColorValue.RadialParams.SIZE_CLOSEST_CORNER: {
                float dx = Math.min(left, right);
                float dy = Math.min(top, bottom);
                float r = (float) Math.hypot(dx, dy);
                if (circle) return new float[]{r, r};
                // Per CSS: keep aspect ratio of "closest side", scaled so the gradient
                // touches the closest corner.
                float rxSide = Math.min(left, right);
                float rySide = Math.min(top, bottom);
                float k = (float) Math.sqrt(2.0);
                return new float[]{rxSide * k, rySide * k};
            }
            case ColorValue.RadialParams.SIZE_FARTHEST_CORNER:
            default: {
                float dx = Math.max(left, right);
                float dy = Math.max(top, bottom);
                float r = (float) Math.hypot(dx, dy);
                if (circle) return new float[]{r, r};
                float rxSide = Math.max(left, right);
                float rySide = Math.max(top, bottom);
                float k = (float) Math.sqrt(2.0);
                return new float[]{rxSide * k, rySide * k};
            }
        }
    }

    private static final class Stops {
        final int[] colors;
        final float[] positions;

        Stops(int[] c, float[] p) {
            this.colors = c;
            this.positions = p;
        }
    }

    /**
     * Filters out hint stops, normalizes positions to 0~1, fills in missing
     * positions evenly, and returns parallel arrays for shader construction.
     * Returns null if fewer than two valid color stops exist.
     */
    private static Stops collectStops(ColorValue.ColorStop[] raw, boolean forSweep) {
        if (raw == null || raw.length == 0) return null;
        List<ColorValue.ColorStop> kept = new ArrayList<>(raw.length);
        for (ColorValue.ColorStop cs : raw) {
            if (cs == null) continue;
            if (cs.isHint) continue;
            kept.add(cs);
        }
        int n = kept.size();
        if (n < 2) return null;

        int[] colors = new int[n];
        float[] positions = new float[n];
        boolean anyExplicit = false;
        for (int i = 0; i < n; i++) {
            ColorValue.ColorStop cs = kept.get(i);
            colors[i] = cs.color;
            if (cs.positionIsCalc) {
                if (AGenUILogger.isLoggingEnabled()) {
                    AGenUILogger.w(TAG, "color-stop position calc() not supported: " + cs.positionCalcExpr);
                }
            }
            if (!cs.hasPosition) {
                positions[i] = -1f; // sentinel for "fill later"
            } else {
                positions[i] = normalizePosition(cs, forSweep);
                anyExplicit = true;
            }
        }

        if (!anyExplicit) {
            for (int i = 0; i < n; i++) positions[i] = (float) i / (n - 1);
        } else {
            // Anchor endpoints if missing.
            if (positions[0] < 0f) positions[0] = 0f;
            if (positions[n - 1] < 0f) positions[n - 1] = 1f;
            // Fill internal gaps by linear interpolation between neighbours.
            int i = 0;
            while (i < n) {
                if (positions[i] >= 0f) {
                    i++;
                    continue;
                }
                int gapStart = i - 1;
                int gapEnd = i;
                while (gapEnd < n && positions[gapEnd] < 0f) gapEnd++;
                float a = positions[gapStart];
                float b = gapEnd < n ? positions[gapEnd] : 1f;
                int slots = gapEnd - gapStart;
                for (int k = 1; k < slots; k++) {
                    positions[gapStart + k] = a + (b - a) * k / (float) slots;
                }
                i = gapEnd;
            }
        }

        // Shader requires monotonic non-decreasing positions in [0,1].
        for (int i = 0; i < n; i++) {
            if (positions[i] < 0f) positions[i] = 0f;
            if (positions[i] > 1f) positions[i] = 1f;
            if (i > 0 && positions[i] < positions[i - 1]) positions[i] = positions[i - 1];
        }
        return new Stops(colors, positions);
    }

    private static float normalizePosition(ColorValue.ColorStop cs, boolean forSweep) {
        switch (cs.unit) {
            case ColorValue.ColorStop.UNIT_PERCENT:
                return cs.position; // already 0~1
            case ColorValue.ColorStop.UNIT_DEG:
                return forSweep ? (cs.position / 360f) : cs.position;
            case ColorValue.ColorStop.UNIT_GRAD:
                return forSweep ? (cs.position / 400f) : cs.position;
            case ColorValue.ColorStop.UNIT_RAD:
                return forSweep ? (float) (cs.position / (2.0 * Math.PI)) : cs.position;
            case ColorValue.ColorStop.UNIT_TURN:
                return forSweep ? cs.position : cs.position;
            case ColorValue.ColorStop.UNIT_PX:
            default:
                // Px positions in linear-gradient are uncommon; without the
                // gradient line length we can't map exactly. Fall back to a
                // neutral midpoint and let the even-distribution path correct.
                return -1f;
        }
    }
}
