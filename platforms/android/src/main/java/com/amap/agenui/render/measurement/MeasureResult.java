package com.amap.agenui.render.measurement;

import androidx.annotation.Keep;

/**
 * JNI-visible measurement result.
 *
 * <p>The numeric values are returned in A2UI logical units.</p>
 */
@Keep
public final class MeasureResult {

    public static final int CALC_TYPE_SYNC = 0;
    public static final int CALC_TYPE_ASYNC = 1;

    @Keep
    public final int calcType;
    @Keep
    public final float width;
    @Keep
    public final float height;
    @Keep
    public final int lineCount;

    public MeasureResult(int calcType, float width, float height, int lineCount) {
        this.calcType = calcType;
        this.width = width;
        this.height = height;
        this.lineCount = lineCount;
    }

    public static MeasureResult sync(float width, float height, int lineCount) {
        return new MeasureResult(CALC_TYPE_SYNC, width, height, lineCount);
    }

    public static MeasureResult sync(float width, float height) {
        return sync(width, height, 0);
    }

    public static MeasureResult async() {
        return new MeasureResult(CALC_TYPE_ASYNC, 0f, 0f, 0);
    }

    public static MeasureResult zero() {
        return sync(0f, 0f, 0);
    }

    @Override
    public String toString() {
        return "MeasureResult{"
                + "calcType=" + calcType
                + ", width=" + width
                + ", height=" + height
                + ", lineCount=" + lineCount
                + '}';
    }
}
