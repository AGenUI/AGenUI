package com.amap.agenui;

import androidx.annotation.Keep;

/**
 * Parsed CSS color value returned from native ColorParser.
 * Represents either a solid color or a gradient.
 */
@Keep
public final class ColorValue {

    // ColorValueType constants
    public static final int TYPE_SOLID = 0;
    public static final int TYPE_GRADIENT = 1;

    @Keep public int type;
    @Keep public int solidColor;
    @Keep public boolean isCurrentColor;
    @Keep public GradientInfo gradient;

    @Keep
    public static final class GradientInfo {
        // GradientType constants
        public static final int GRADIENT_LINEAR = 0;
        public static final int GRADIENT_RADIAL = 1;
        public static final int GRADIENT_CONIC = 2;

        @Keep public int gradientType;
        @Keep public boolean isRepeating;
        @Keep public String colorInterpolationMethod;
        @Keep public ColorStop[] colorStops;
        @Keep public LinearParams linear;
        @Keep public RadialParams radial;
        @Keep public ConicParams conic;
    }

    @Keep
    public static final class ColorStop {
        // StopUnit constants
        public static final int UNIT_PERCENT = 0;
        public static final int UNIT_PX = 1;
        public static final int UNIT_DEG = 2;
        public static final int UNIT_GRAD = 3;
        public static final int UNIT_RAD = 4;
        public static final int UNIT_TURN = 5;

        @Keep public int color;
        @Keep public float position;
        @Keep public float positionEnd;
        @Keep public int unit;
        @Keep public int unitEnd;
        @Keep public boolean hasPosition;
        @Keep public boolean hasPositionEnd;
        @Keep public boolean isHint;
        @Keep public boolean isCurrentColor;
        @Keep public boolean positionIsCalc;
        @Keep public String positionCalcExpr;
    }

    @Keep
    public static final class LinearParams {
        @Keep public float angle;
        @Keep public boolean angleIsCalc;
        @Keep public String angleCalcExpr;
    }

    @Keep
    public static final class RadialParams {
        // RadialShape constants
        public static final int SHAPE_CIRCLE = 0;
        public static final int SHAPE_ELLIPSE = 1;

        // RadialSize constants
        public static final int SIZE_CLOSEST_SIDE = 0;
        public static final int SIZE_CLOSEST_CORNER = 1;
        public static final int SIZE_FARTHEST_SIDE = 2;
        public static final int SIZE_FARTHEST_CORNER = 3;

        @Keep public int shape;
        @Keep public int size;
        @Keep public float centerX;
        @Keep public float centerY;
        @Keep public boolean centerXIsPx;
        @Keep public boolean centerYIsPx;
        @Keep public float radiusX;
        @Keep public float radiusY;
        @Keep public boolean hasExplicitSize;
        @Keep public boolean radiusXIsPercent;
        @Keep public boolean radiusYIsPercent;
        @Keep public boolean radiusXIsCalc;
        @Keep public boolean radiusYIsCalc;
        @Keep public String radiusXCalcExpr;
        @Keep public String radiusYCalcExpr;
    }

    @Keep
    public static final class ConicParams {
        @Keep public float startAngle;
        @Keep public float centerX;
        @Keep public float centerY;
        @Keep public boolean centerXIsPx;
        @Keep public boolean centerYIsPx;
        @Keep public boolean startAngleIsCalc;
        @Keep public String startAngleCalcExpr;
    }
}
