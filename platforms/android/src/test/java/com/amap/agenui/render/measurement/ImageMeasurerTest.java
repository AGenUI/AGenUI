package com.amap.agenui.render.measurement;

import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

public class ImageMeasurerTest {

    @Test
    public void returnsExactConstraintsWhenBothSidesExactly() {
        MeasureResult result = ImageMeasurer.measure("{}", 320f, 1, 180f, 1);

        assertEquals(MeasureResult.CALC_TYPE_SYNC, result.calcType);
        assertEquals(320f, result.width, 0.001f);
        assertEquals(180f, result.height, 0.001f);
    }

    @Test
    public void returnsExplicitWidthAndHeightFromStyleInfo() {
        MeasureResult result = ImageMeasurer.resolveSyncResult(200f, 120f, 0f, 0, 0f, 0);

        assertEquals(200f, result.width, 0.001f);
        assertEquals(120f, result.height, 0.001f);
    }

    @Test
    public void fallsBackToNullForFullyAutoImage() {
        MeasureResult result = ImageMeasurer.resolveSyncResult(null, null, 0f, 0, 0f, 0);

        assertNull(result);
    }
}
