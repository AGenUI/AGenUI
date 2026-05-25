package com.amap.agenui;

import androidx.annotation.Keep;

/**
 * Parsed CSS edge insets value returned from native EdgeInsetsParser.
 * Represents a four-directional value (top, right, bottom, left) parsed from
 * CSS shorthand strings like "10px 20% auto 5px".
 */
@Keep
public final class EdgeInsetsValue {

    @Keep public EdgeInsetSide top;
    @Keep public EdgeInsetSide right;
    @Keep public EdgeInsetSide bottom;
    @Keep public EdgeInsetSide left;

    /**
     * A single edge value with unit and optional calc() expression.
     */
    @Keep
    public static final class EdgeInsetSide {
        // EdgeInsetUnit constants (must match C++ enum order)
        public static final int UNIT_PX = 0;
        public static final int UNIT_PERCENT = 1;
        public static final int UNIT_EM = 2;
        public static final int UNIT_REM = 3;
        public static final int UNIT_VW = 4;
        public static final int UNIT_VH = 5;
        public static final int UNIT_VMIN = 6;
        public static final int UNIT_VMAX = 7;
        public static final int UNIT_CM = 8;
        public static final int UNIT_MM = 9;
        public static final int UNIT_IN = 10;
        public static final int UNIT_PT = 11;
        public static final int UNIT_PC = 12;
        public static final int UNIT_AUTO = 13;

        @Keep public float value;
        @Keep public int unit;
        @Keep public boolean isCalc;
        @Keep public String calcExpr;
    }
}
