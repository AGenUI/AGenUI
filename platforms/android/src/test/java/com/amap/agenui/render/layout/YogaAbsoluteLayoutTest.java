package com.amap.agenui.render.layout;

import static org.junit.Assert.*;

import org.junit.Test;

import java.lang.reflect.Method;

/**
 * Unit tests for {@link YogaAbsoluteLayout}.
 * Tests the platform-independent logic: LayoutState data class + resolveMeasuredDimension.
 */
public class YogaAbsoluteLayoutTest {

    // ═══════════════════════════════════════════════════════════════════════════
    // LayoutState equals
    // ═══════════════════════════════════════════════════════════════════════════

    @Test
    public void layoutState_equals_reflexive() {
        YogaAbsoluteLayout.LayoutState s = new YogaAbsoluteLayout.LayoutState(10, 20, 100, 200, 0);
        assertEquals(s, s);
    }

    @Test
    public void layoutState_equals_symmetric() {
        YogaAbsoluteLayout.LayoutState a = new YogaAbsoluteLayout.LayoutState(1, 2, 3, 4, 5);
        YogaAbsoluteLayout.LayoutState b = new YogaAbsoluteLayout.LayoutState(1, 2, 3, 4, 5);
        assertEquals(a, b);
        assertEquals(b, a);
    }

    @Test
    public void layoutState_equals_differentX_notEqual() {
        YogaAbsoluteLayout.LayoutState a = new YogaAbsoluteLayout.LayoutState(1, 2, 3, 4, 5);
        YogaAbsoluteLayout.LayoutState b = new YogaAbsoluteLayout.LayoutState(99, 2, 3, 4, 5);
        assertNotEquals(a, b);
    }

    @Test
    public void layoutState_equals_differentY_notEqual() {
        YogaAbsoluteLayout.LayoutState a = new YogaAbsoluteLayout.LayoutState(1, 2, 3, 4, 5);
        YogaAbsoluteLayout.LayoutState b = new YogaAbsoluteLayout.LayoutState(1, 99, 3, 4, 5);
        assertNotEquals(a, b);
    }

    @Test
    public void layoutState_equals_differentWidth_notEqual() {
        YogaAbsoluteLayout.LayoutState a = new YogaAbsoluteLayout.LayoutState(1, 2, 3, 4, 5);
        YogaAbsoluteLayout.LayoutState b = new YogaAbsoluteLayout.LayoutState(1, 2, 99, 4, 5);
        assertNotEquals(a, b);
    }

    @Test
    public void layoutState_equals_differentHeight_notEqual() {
        YogaAbsoluteLayout.LayoutState a = new YogaAbsoluteLayout.LayoutState(1, 2, 3, 4, 5);
        YogaAbsoluteLayout.LayoutState b = new YogaAbsoluteLayout.LayoutState(1, 2, 3, 99, 5);
        assertNotEquals(a, b);
    }

    @Test
    public void layoutState_equals_differentZIndex_notEqual() {
        YogaAbsoluteLayout.LayoutState a = new YogaAbsoluteLayout.LayoutState(1, 2, 3, 4, 5);
        YogaAbsoluteLayout.LayoutState b = new YogaAbsoluteLayout.LayoutState(1, 2, 3, 4, 99);
        assertNotEquals(a, b);
    }

    @Test
    public void layoutState_equals_null_notEqual() {
        YogaAbsoluteLayout.LayoutState s = new YogaAbsoluteLayout.LayoutState(0, 0, 0, 0, 0);
        assertNotEquals(s, null);
    }

    @Test
    public void layoutState_equals_differentType_notEqual() {
        YogaAbsoluteLayout.LayoutState s = new YogaAbsoluteLayout.LayoutState(0, 0, 0, 0, 0);
        assertNotEquals(s, "not a LayoutState");
    }

    @Test
    public void layoutState_equals_zeros() {
        YogaAbsoluteLayout.LayoutState a = new YogaAbsoluteLayout.LayoutState(0, 0, 0, 0, 0);
        YogaAbsoluteLayout.LayoutState b = new YogaAbsoluteLayout.LayoutState(0, 0, 0, 0, 0);
        assertEquals(a, b);
    }

    @Test
    public void layoutState_equals_negativeValues() {
        YogaAbsoluteLayout.LayoutState a = new YogaAbsoluteLayout.LayoutState(-1, -2, -3, -4, -5);
        YogaAbsoluteLayout.LayoutState b = new YogaAbsoluteLayout.LayoutState(-1, -2, -3, -4, -5);
        assertEquals(a, b);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // LayoutState hashCode
    // ═══════════════════════════════════════════════════════════════════════════

    @Test
    public void layoutState_hashCode_equalObjectsSameHash() {
        YogaAbsoluteLayout.LayoutState a = new YogaAbsoluteLayout.LayoutState(10, 20, 300, 400, 2);
        YogaAbsoluteLayout.LayoutState b = new YogaAbsoluteLayout.LayoutState(10, 20, 300, 400, 2);
        assertEquals(a.hashCode(), b.hashCode());
    }

    @Test
    public void layoutState_hashCode_differentObjectsDifferentHash() {
        YogaAbsoluteLayout.LayoutState a = new YogaAbsoluteLayout.LayoutState(1, 2, 3, 4, 5);
        YogaAbsoluteLayout.LayoutState b = new YogaAbsoluteLayout.LayoutState(5, 4, 3, 2, 1);
        // Not guaranteed but very likely for well-implemented hash
        assertNotEquals(a.hashCode(), b.hashCode());
    }

    @Test
    public void layoutState_hashCode_consistent() {
        YogaAbsoluteLayout.LayoutState s = new YogaAbsoluteLayout.LayoutState(42, 43, 44, 45, 1);
        int hash1 = s.hashCode();
        int hash2 = s.hashCode();
        assertEquals(hash1, hash2);
    }

    @Test
    public void layoutState_hashCode_zeroValues() {
        YogaAbsoluteLayout.LayoutState s = new YogaAbsoluteLayout.LayoutState(0, 0, 0, 0, 0);
        // Just verify it doesn't throw; hash of all zeros = 0 by the formula
        assertEquals(0, s.hashCode());
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // LayoutState fields
    // ═══════════════════════════════════════════════════════════════════════════

    @Test
    public void layoutState_fields_accessible() {
        YogaAbsoluteLayout.LayoutState s = new YogaAbsoluteLayout.LayoutState(11, 22, 33, 44, 55);
        assertEquals(11, s.xPx);
        assertEquals(22, s.yPx);
        assertEquals(33, s.widthPx);
        assertEquals(44, s.heightPx);
        assertEquals(55, s.zIndex);
    }

    @Test
    public void layoutState_fields_maxInt() {
        YogaAbsoluteLayout.LayoutState s = new YogaAbsoluteLayout.LayoutState(
                Integer.MAX_VALUE, Integer.MAX_VALUE, Integer.MAX_VALUE,
                Integer.MAX_VALUE, Integer.MAX_VALUE);
        assertEquals(Integer.MAX_VALUE, s.xPx);
        assertEquals(Integer.MAX_VALUE, s.widthPx);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // resolveMeasuredDimension (private, tested via reflection)
    // ═══════════════════════════════════════════════════════════════════════════

    @Test
    public void resolveMeasuredDimension_exactly_returnsSize() throws Exception {
        // MeasureSpec.EXACTLY = 1 << 30
        int exactly = 1 << 30;
        int result = invokeResolveMeasuredDimension(50, exactly, 200);
        assertEquals(200, result);
    }

    @Test
    public void resolveMeasuredDimension_atMost_lessThanSize_returnsDesired() throws Exception {
        // MeasureSpec.AT_MOST = -2147483648 (Integer.MIN_VALUE)
        int atMost = Integer.MIN_VALUE;
        int result = invokeResolveMeasuredDimension(150, atMost, 200);
        assertEquals(150, result);
    }

    @Test
    public void resolveMeasuredDimension_atMost_greaterThanSize_returnsSize() throws Exception {
        int atMost = Integer.MIN_VALUE;
        int result = invokeResolveMeasuredDimension(300, atMost, 200);
        assertEquals(200, result);
    }

    @Test
    public void resolveMeasuredDimension_unspecified_returnsDesired() throws Exception {
        int unspecified = 0;
        int result = invokeResolveMeasuredDimension(500, unspecified, 200);
        assertEquals(500, result);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // YogaLayoutParams
    // ═══════════════════════════════════════════════════════════════════════════

    @Test
    public void yogaLayoutParams_constructor_setsYogaDimensions() {
        YogaAbsoluteLayout.YogaLayoutParams params = new YogaAbsoluteLayout.YogaLayoutParams(100, 200);
        assertEquals(100, params.yogaWidth);
        assertEquals(200, params.yogaHeight);
    }

    @Test
    public void yogaLayoutParams_defaultFields() {
        YogaAbsoluteLayout.YogaLayoutParams params = new YogaAbsoluteLayout.YogaLayoutParams(0, 0);
        assertEquals(0, params.yogaX);
        assertEquals(0, params.yogaY);
        assertEquals(0, params.zIndex);
        assertFalse(params.measureWrapContentHeightWhenZero);
    }

    @Test
    public void yogaLayoutParams_zeroSize() {
        YogaAbsoluteLayout.YogaLayoutParams params = new YogaAbsoluteLayout.YogaLayoutParams(0, 0);
        assertEquals(0, params.yogaWidth);
        assertEquals(0, params.yogaHeight);
    }

    // ─── Helper ──────────────────────────────────────────────────────────────

    private int invokeResolveMeasuredDimension(int desiredSize, int mode, int size) throws Exception {
        Method method = YogaAbsoluteLayout.class.getDeclaredMethod(
                "resolveMeasuredDimension", int.class, int.class, int.class);
        method.setAccessible(true);
        // Need an instance; use constructor via reflection (needs Context which will be null)
        // Since resolveMeasuredDimension is a private instance method, we need to work around Context
        // Actually we can use null context with returnDefaultValues = true
        YogaAbsoluteLayout layout = new YogaAbsoluteLayout(null);
        return (int) method.invoke(layout, desiredSize, mode, size);
    }
}
