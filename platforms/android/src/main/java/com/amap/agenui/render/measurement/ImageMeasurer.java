package com.amap.agenui.render.measurement;

public final class ImageMeasurer {

    private ImageMeasurer() {
    }

    public static MeasureResult measure(String paramJson,
                                        float maxWidth,
                                        int widthMode,
                                        float maxHeight,
                                        int heightMode) {
        float w = (widthMode == MeasurementSupport.MODE_EXACTLY || widthMode == MeasurementSupport.MODE_AT_MOST) && maxWidth > 0f
                ? maxWidth : 0f;
        float h = (heightMode == MeasurementSupport.MODE_EXACTLY || heightMode == MeasurementSupport.MODE_AT_MOST) && maxHeight > 0f
                ? maxHeight : 0f;
        return MeasureResult.sync(w, h);
    }
}
