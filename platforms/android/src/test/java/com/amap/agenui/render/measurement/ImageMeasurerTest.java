package com.amap.agenui.render.measurement;

import org.junit.Test;

import static org.junit.Assert.assertEquals;

public class ImageMeasurerTest {

    @Test
    public void returnsExactConstraintsWhenBothSidesExactly() {
        MeasureResult result = ImageMeasurer.measure("{}", 320f, 1, 180f, 1);

        assertEquals(MeasureResult.CALC_TYPE_SYNC, result.calcType);
        assertEquals(320f, result.width, 0.001f);
        assertEquals(180f, result.height, 0.001f);
    }
}
