package com.amap.agenui.render.style;

import com.amap.agenui.ColorValue;

import org.junit.Test;

import java.lang.reflect.Method;
import java.lang.reflect.Constructor;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

/**
 * Unit tests for GradientDrawableFactory's pure-logic internal methods.
 *
 * These methods (normalizePosition, collectStops, keywordSize, sidesFromCenter) are private
 * but contain significant math/algorithm logic. We use reflection to test them directly
 * since they have no Android framework dependencies.
 */
public class GradientDrawableFactoryLogicTest {

    // ========================================================================
    // normalizePosition — percent unit
    // ========================================================================

    @Test
    public void normalizePosition_percentUnit_returnsPositionAsIs() throws Exception {
        ColorValue.ColorStop stop = createColorStop(0.5f, ColorValue.ColorStop.UNIT_PERCENT, true);
        float result = invokeNormalizePosition(stop, false);
        assertEquals(0.5f, result, 0.0001f);
    }

    @Test
    public void normalizePosition_percentZero_returnsZero() throws Exception {
        ColorValue.ColorStop stop = createColorStop(0.0f, ColorValue.ColorStop.UNIT_PERCENT, true);
        float result = invokeNormalizePosition(stop, false);
        assertEquals(0.0f, result, 0.0001f);
    }

    @Test
    public void normalizePosition_percentOne_returnsOne() throws Exception {
        ColorValue.ColorStop stop = createColorStop(1.0f, ColorValue.ColorStop.UNIT_PERCENT, true);
        float result = invokeNormalizePosition(stop, false);
        assertEquals(1.0f, result, 0.0001f);
    }

    // ========================================================================
    // normalizePosition — degree unit (for sweep/conic)
    // ========================================================================

    @Test
    public void normalizePosition_degForSweep_normalizesTo360() throws Exception {
        ColorValue.ColorStop stop = createColorStop(180f, ColorValue.ColorStop.UNIT_DEG, true);
        float result = invokeNormalizePosition(stop, true);
        assertEquals(0.5f, result, 0.0001f);
    }

    @Test
    public void normalizePosition_degForSweep_360_returnsOne() throws Exception {
        ColorValue.ColorStop stop = createColorStop(360f, ColorValue.ColorStop.UNIT_DEG, true);
        float result = invokeNormalizePosition(stop, true);
        assertEquals(1.0f, result, 0.0001f);
    }

    @Test
    public void normalizePosition_degNotForSweep_returnsRaw() throws Exception {
        ColorValue.ColorStop stop = createColorStop(180f, ColorValue.ColorStop.UNIT_DEG, true);
        float result = invokeNormalizePosition(stop, false);
        assertEquals(180f, result, 0.0001f);
    }

    // ========================================================================
    // normalizePosition — grad unit
    // ========================================================================

    @Test
    public void normalizePosition_gradForSweep_normalizesTo400() throws Exception {
        ColorValue.ColorStop stop = createColorStop(200f, ColorValue.ColorStop.UNIT_GRAD, true);
        float result = invokeNormalizePosition(stop, true);
        assertEquals(0.5f, result, 0.0001f);
    }

    @Test
    public void normalizePosition_gradForSweep_400_returnsOne() throws Exception {
        ColorValue.ColorStop stop = createColorStop(400f, ColorValue.ColorStop.UNIT_GRAD, true);
        float result = invokeNormalizePosition(stop, true);
        assertEquals(1.0f, result, 0.0001f);
    }

    // ========================================================================
    // normalizePosition — rad unit
    // ========================================================================

    @Test
    public void normalizePosition_radForSweep_normalizesTo2Pi() throws Exception {
        ColorValue.ColorStop stop = createColorStop((float) Math.PI, ColorValue.ColorStop.UNIT_RAD, true);
        float result = invokeNormalizePosition(stop, true);
        assertEquals(0.5f, result, 0.01f);
    }

    // ========================================================================
    // normalizePosition — turn unit
    // ========================================================================

    @Test
    public void normalizePosition_turnForSweep_returnsAsIs() throws Exception {
        ColorValue.ColorStop stop = createColorStop(0.75f, ColorValue.ColorStop.UNIT_TURN, true);
        float result = invokeNormalizePosition(stop, true);
        assertEquals(0.75f, result, 0.0001f);
    }

    // ========================================================================
    // normalizePosition — px unit (fallback)
    // ========================================================================

    @Test
    public void normalizePosition_pxUnit_returnsSentinel() throws Exception {
        ColorValue.ColorStop stop = createColorStop(100f, ColorValue.ColorStop.UNIT_PX, true);
        float result = invokeNormalizePosition(stop, false);
        assertEquals(-1f, result, 0.0001f);
    }

    // ========================================================================
    // sidesFromCenter
    // ========================================================================

    @Test
    public void sidesFromCenter_center_returnsEqualSides() throws Exception {
        float[] result = invokeSidesFromCenter(50f, 50f, 100, 100);
        // left, right, top, bottom
        assertEquals(50f, result[0], 0.01f);
        assertEquals(50f, result[1], 0.01f);
        assertEquals(50f, result[2], 0.01f);
        assertEquals(50f, result[3], 0.01f);
    }

    @Test
    public void sidesFromCenter_topLeft_returnsCorrectDistances() throws Exception {
        float[] result = invokeSidesFromCenter(0f, 0f, 200, 100);
        assertEquals(0f, result[0], 0.01f);   // left
        assertEquals(200f, result[1], 0.01f);  // right
        assertEquals(0f, result[2], 0.01f);    // top
        assertEquals(100f, result[3], 0.01f);  // bottom
    }

    @Test
    public void sidesFromCenter_offsetCenter_correctDistances() throws Exception {
        float[] result = invokeSidesFromCenter(30f, 70f, 100, 100);
        assertEquals(30f, result[0], 0.01f);  // left
        assertEquals(70f, result[1], 0.01f);  // right
        assertEquals(70f, result[2], 0.01f);  // top
        assertEquals(30f, result[3], 0.01f);  // bottom
    }

    // ========================================================================
    // keywordSize — circle closest-side
    // ========================================================================

    @Test
    public void keywordSize_circleClosestSide_returnsMinOfAllSides() throws Exception {
        float[] sides = {30f, 70f, 40f, 60f}; // l, r, t, b
        float[] result = invokeKeywordSize(
                ColorValue.RadialParams.SIZE_CLOSEST_SIDE,
                ColorValue.RadialParams.SHAPE_CIRCLE,
                sides);
        // circle: min(min(l,r), min(t,b)) = min(30,40) = 30
        assertEquals(30f, result[0], 0.01f);
        assertEquals(30f, result[1], 0.01f);
    }

    // ========================================================================
    // keywordSize — ellipse closest-side
    // ========================================================================

    @Test
    public void keywordSize_ellipseClosestSide_returnsMinPerAxis() throws Exception {
        float[] sides = {30f, 70f, 40f, 60f};
        float[] result = invokeKeywordSize(
                ColorValue.RadialParams.SIZE_CLOSEST_SIDE,
                ColorValue.RadialParams.SHAPE_ELLIPSE,
                sides);
        // ellipse: rx = min(l,r)=30, ry = min(t,b)=40
        assertEquals(30f, result[0], 0.01f);
        assertEquals(40f, result[1], 0.01f);
    }

    // ========================================================================
    // keywordSize — circle farthest-side
    // ========================================================================

    @Test
    public void keywordSize_circleFarthestSide_returnsMaxOfAllSides() throws Exception {
        float[] sides = {30f, 70f, 40f, 60f};
        float[] result = invokeKeywordSize(
                ColorValue.RadialParams.SIZE_FARTHEST_SIDE,
                ColorValue.RadialParams.SHAPE_CIRCLE,
                sides);
        // circle: max(max(l,r), max(t,b)) = max(70,60) = 70
        assertEquals(70f, result[0], 0.01f);
        assertEquals(70f, result[1], 0.01f);
    }

    // ========================================================================
    // keywordSize — ellipse farthest-side
    // ========================================================================

    @Test
    public void keywordSize_ellipseFarthestSide_returnsMaxPerAxis() throws Exception {
        float[] sides = {30f, 70f, 40f, 60f};
        float[] result = invokeKeywordSize(
                ColorValue.RadialParams.SIZE_FARTHEST_SIDE,
                ColorValue.RadialParams.SHAPE_ELLIPSE,
                sides);
        assertEquals(70f, result[0], 0.01f);
        assertEquals(60f, result[1], 0.01f);
    }

    // ========================================================================
    // keywordSize — circle closest-corner
    // ========================================================================

    @Test
    public void keywordSize_circleClosestCorner_returnsHypotOfMin() throws Exception {
        float[] sides = {30f, 70f, 40f, 60f};
        float[] result = invokeKeywordSize(
                ColorValue.RadialParams.SIZE_CLOSEST_CORNER,
                ColorValue.RadialParams.SHAPE_CIRCLE,
                sides);
        float expected = (float) Math.hypot(30, 40); // 50
        assertEquals(expected, result[0], 0.01f);
        assertEquals(expected, result[1], 0.01f);
    }

    // ========================================================================
    // keywordSize — circle farthest-corner (default)
    // ========================================================================

    @Test
    public void keywordSize_circleFarthestCorner_returnsHypotOfMax() throws Exception {
        float[] sides = {30f, 70f, 40f, 60f};
        float[] result = invokeKeywordSize(
                ColorValue.RadialParams.SIZE_FARTHEST_CORNER,
                ColorValue.RadialParams.SHAPE_CIRCLE,
                sides);
        float expected = (float) Math.hypot(70, 60); // ~92.2
        assertEquals(expected, result[0], 0.1f);
        assertEquals(expected, result[1], 0.1f);
    }

    // ========================================================================
    // keywordSize — ellipse closest-corner
    // ========================================================================

    @Test
    public void keywordSize_ellipseClosestCorner_scalesByRoot2() throws Exception {
        float[] sides = {30f, 70f, 40f, 60f};
        float[] result = invokeKeywordSize(
                ColorValue.RadialParams.SIZE_CLOSEST_CORNER,
                ColorValue.RadialParams.SHAPE_ELLIPSE,
                sides);
        float k = (float) Math.sqrt(2.0);
        assertEquals(30f * k, result[0], 0.1f);
        assertEquals(40f * k, result[1], 0.1f);
    }

    // ========================================================================
    // keywordSize — ellipse farthest-corner
    // ========================================================================

    @Test
    public void keywordSize_ellipseFarthestCorner_scalesByRoot2() throws Exception {
        float[] sides = {30f, 70f, 40f, 60f};
        float[] result = invokeKeywordSize(
                ColorValue.RadialParams.SIZE_FARTHEST_CORNER,
                ColorValue.RadialParams.SHAPE_ELLIPSE,
                sides);
        float k = (float) Math.sqrt(2.0);
        assertEquals(70f * k, result[0], 0.1f);
        assertEquals(60f * k, result[1], 0.1f);
    }

    // ========================================================================
    // collectStops — even distribution when no positions set
    // ========================================================================

    @Test
    public void collectStops_twoStopsNoPositions_distributesEvenly() throws Exception {
        ColorValue.ColorStop[] stops = new ColorValue.ColorStop[]{
                createColorStop(0xFF0000, false),
                createColorStop(0x0000FF, false)
        };
        Object result = invokeCollectStops(stops, false);
        assertNotNull(result);

        float[] positions = getStopsPositions(result);
        assertEquals(0.0f, positions[0], 0.001f);
        assertEquals(1.0f, positions[1], 0.001f);
    }

    @Test
    public void collectStops_threeStopsNoPositions_distributesEvenly() throws Exception {
        ColorValue.ColorStop[] stops = new ColorValue.ColorStop[]{
                createColorStop(0xFF0000, false),
                createColorStop(0x00FF00, false),
                createColorStop(0x0000FF, false)
        };
        Object result = invokeCollectStops(stops, false);
        assertNotNull(result);

        float[] positions = getStopsPositions(result);
        assertEquals(0.0f, positions[0], 0.001f);
        assertEquals(0.5f, positions[1], 0.001f);
        assertEquals(1.0f, positions[2], 0.001f);
    }

    // ========================================================================
    // collectStops — null/empty input
    // ========================================================================

    @Test
    public void collectStops_nullInput_returnsNull() throws Exception {
        Object result = invokeCollectStops(null, false);
        assertNull(result);
    }

    @Test
    public void collectStops_emptyArray_returnsNull() throws Exception {
        Object result = invokeCollectStops(new ColorValue.ColorStop[0], false);
        assertNull(result);
    }

    @Test
    public void collectStops_singleStop_returnsNull() throws Exception {
        ColorValue.ColorStop[] stops = new ColorValue.ColorStop[]{
                createColorStop(0xFF0000, false)
        };
        Object result = invokeCollectStops(stops, false);
        assertNull(result);
    }

    // ========================================================================
    // collectStops — hint stops filtered out
    // ========================================================================

    @Test
    public void collectStops_hintStopsFiltered_lessThanTwo_returnsNull() throws Exception {
        ColorValue.ColorStop hint = createColorStop(0xFF0000, false);
        hint.isHint = true;
        ColorValue.ColorStop[] stops = new ColorValue.ColorStop[]{hint, createColorStop(0x0000FF, false)};
        // After filtering hints: only 1 stop remains → null
        Object result = invokeCollectStops(stops, false);
        assertNull(result);
    }

    // ========================================================================
    // collectStops — explicit positions with gaps filled
    // ========================================================================

    @Test
    public void collectStops_explicitPositionsWithGap_fillsLinearly() throws Exception {
        ColorValue.ColorStop[] stops = new ColorValue.ColorStop[]{
                createColorStop(0xFF0000, 0.0f, ColorValue.ColorStop.UNIT_PERCENT, true),
                createColorStop(0x00FF00, false), // no position → gap
                createColorStop(0x0000FF, 1.0f, ColorValue.ColorStop.UNIT_PERCENT, true)
        };
        Object result = invokeCollectStops(stops, false);
        assertNotNull(result);

        float[] positions = getStopsPositions(result);
        assertEquals(0.0f, positions[0], 0.001f);
        assertEquals(0.5f, positions[1], 0.001f); // linearly interpolated
        assertEquals(1.0f, positions[2], 0.001f);
    }

    // ========================================================================
    // collectStops — monotonic enforcement
    // ========================================================================

    @Test
    public void collectStops_nonMonotonicPositions_clamped() throws Exception {
        // Position[1] < Position[0] should be clamped
        ColorValue.ColorStop[] stops = new ColorValue.ColorStop[]{
                createColorStop(0xFF0000, 0.8f, ColorValue.ColorStop.UNIT_PERCENT, true),
                createColorStop(0x00FF00, 0.3f, ColorValue.ColorStop.UNIT_PERCENT, true),
                createColorStop(0x0000FF, 1.0f, ColorValue.ColorStop.UNIT_PERCENT, true)
        };
        Object result = invokeCollectStops(stops, false);
        assertNotNull(result);

        float[] positions = getStopsPositions(result);
        // pos[1]=0.3 < pos[0]=0.8 → clamped to 0.8
        assertEquals(0.8f, positions[0], 0.001f);
        assertEquals(0.8f, positions[1], 0.001f);
        assertEquals(1.0f, positions[2], 0.001f);
    }

    // ========================================================================
    // collectStops — colors preserved
    // ========================================================================

    @Test
    public void collectStops_colorsPreserved() throws Exception {
        ColorValue.ColorStop[] stops = new ColorValue.ColorStop[]{
                createColorStop(0xAABBCC, false),
                createColorStop(0x112233, false)
        };
        Object result = invokeCollectStops(stops, false);
        assertNotNull(result);

        int[] colors = getStopsColors(result);
        assertEquals(0xAABBCC, colors[0]);
        assertEquals(0x112233, colors[1]);
    }

    // ========================================================================
    // Reflection helpers
    // ========================================================================

    private float invokeNormalizePosition(ColorValue.ColorStop cs, boolean forSweep) throws Exception {
        Method m = GradientDrawableFactory.class.getDeclaredMethod(
                "normalizePosition", ColorValue.ColorStop.class, boolean.class);
        m.setAccessible(true);
        return (float) m.invoke(null, cs, forSweep);
    }

    private float[] invokeSidesFromCenter(float cx, float cy, int w, int h) throws Exception {
        Method m = GradientDrawableFactory.class.getDeclaredMethod(
                "sidesFromCenter", float.class, float.class, int.class, int.class);
        m.setAccessible(true);
        return (float[]) m.invoke(null, cx, cy, w, h);
    }

    private float[] invokeKeywordSize(int sizeKeyword, int shape, float[] sides) throws Exception {
        Method m = GradientDrawableFactory.class.getDeclaredMethod(
                "keywordSize", int.class, int.class, float[].class);
        m.setAccessible(true);
        return (float[]) m.invoke(null, sizeKeyword, shape, sides);
    }

    private Object invokeCollectStops(ColorValue.ColorStop[] raw, boolean forSweep) throws Exception {
        Method m = GradientDrawableFactory.class.getDeclaredMethod(
                "collectStops", ColorValue.ColorStop[].class, boolean.class);
        m.setAccessible(true);
        return m.invoke(null, raw, forSweep);
    }

    private float[] getStopsPositions(Object stopsObj) throws Exception {
        java.lang.reflect.Field f = stopsObj.getClass().getDeclaredField("positions");
        f.setAccessible(true);
        return (float[]) f.get(stopsObj);
    }

    private int[] getStopsColors(Object stopsObj) throws Exception {
        java.lang.reflect.Field f = stopsObj.getClass().getDeclaredField("colors");
        f.setAccessible(true);
        return (int[]) f.get(stopsObj);
    }

    // ========================================================================
    // ColorStop factory helpers
    // ========================================================================

    private ColorValue.ColorStop createColorStop(float position, int unit, boolean hasPosition) {
        ColorValue.ColorStop cs = new ColorValue.ColorStop();
        cs.position = position;
        cs.unit = unit;
        cs.hasPosition = hasPosition;
        cs.color = 0xFF000000;
        return cs;
    }

    private ColorValue.ColorStop createColorStop(int color, boolean hasPosition) {
        ColorValue.ColorStop cs = new ColorValue.ColorStop();
        cs.color = color;
        cs.hasPosition = hasPosition;
        return cs;
    }

    private ColorValue.ColorStop createColorStop(int color, float position, int unit, boolean hasPosition) {
        ColorValue.ColorStop cs = new ColorValue.ColorStop();
        cs.color = color;
        cs.position = position;
        cs.unit = unit;
        cs.hasPosition = hasPosition;
        return cs;
    }
}
