package com.amap.agenui.render.component.impl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import java.util.HashMap;
import java.util.Map;

public class ImageComponentLogicTest {

    @Test
    public void classifyPercentWidthAsFlexibleConstraint() {
        assertEquals(
                ImageComponent.DimensionConstraint.FLEXIBLE,
                ImageComponent.classifyDimensionConstraint("100%"));
    }

    @Test
    public void classifyFixedPixelHeightAsFixedConstraint() {
        assertEquals(
                ImageComponent.DimensionConstraint.FIXED,
                ImageComponent.classifyDimensionConstraint("200px"));
    }

    @Test
    public void skipAsyncReportWhenWidthPercentAndHeightFixed() {
        Map<String, Object> styleInfo = new HashMap<>();
        styleInfo.put("width", "100%");
        styleInfo.put("height", "200px");

        assertFalse(ImageComponent.shouldReportAsyncImageSizeForStyleInfo(styleInfo));
    }

    @Test
    public void skipAsyncReportWhenWidthPercentAndAspectRatioProvided() {
        Map<String, Object> styleInfo = new HashMap<>();
        styleInfo.put("width", "100%");
        styleInfo.put("aspect-ratio", "16 / 9");

        assertFalse(ImageComponent.shouldReportAsyncImageSizeForStyleInfo(styleInfo));
    }

    @Test
    public void allowAsyncReportWhenImageIsFullyAutoSized() {
        Map<String, Object> styleInfo = new HashMap<>();

        assertTrue(ImageComponent.shouldReportAsyncImageSizeForStyleInfo(styleInfo));
    }

    @Test
    public void allowAsyncReportWhenOnlyFixedWidthNeedsIntrinsicHeight() {
        Map<String, Object> styleInfo = new HashMap<>();
        styleInfo.put("width", "200px");
        styleInfo.put("height", "auto");

        assertTrue(ImageComponent.shouldReportAsyncImageSizeForStyleInfo(styleInfo));
    }
}
