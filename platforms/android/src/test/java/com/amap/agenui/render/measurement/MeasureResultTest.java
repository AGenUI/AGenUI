package com.amap.agenui.render.measurement;

import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

/**
 * Unit tests for MeasureResult factory methods and data integrity.
 * All methods are pure logic with no Android framework dependencies.
 */
public class MeasureResultTest {

    // ========================================================================
    // sync(width, height, lineCount) — Full Factory
    // ========================================================================

    @Test
    public void sync_fullArgs_setsAllFields() {
        MeasureResult result = MeasureResult.sync(100f, 200f, 5);
        assertEquals(MeasureResult.CALC_TYPE_SYNC, result.calcType);
        assertEquals(100f, result.width, 0.001f);
        assertEquals(200f, result.height, 0.001f);
        assertEquals(5, result.lineCount);
    }

    @Test
    public void sync_zeroLineCount_setsLineCountToZero() {
        MeasureResult result = MeasureResult.sync(50f, 80f, 0);
        assertEquals(0, result.lineCount);
    }

    @Test
    public void sync_largeValues_handlesCorrectly() {
        MeasureResult result = MeasureResult.sync(9999.99f, 8888.88f, 100);
        assertEquals(9999.99f, result.width, 0.01f);
        assertEquals(8888.88f, result.height, 0.01f);
        assertEquals(100, result.lineCount);
    }

    // ========================================================================
    // sync(width, height) — Convenience Factory (lineCount defaults to 0)
    // ========================================================================

    @Test
    public void sync_twoArgs_lineCountDefaultsToZero() {
        MeasureResult result = MeasureResult.sync(120f, 60f);
        assertEquals(MeasureResult.CALC_TYPE_SYNC, result.calcType);
        assertEquals(120f, result.width, 0.001f);
        assertEquals(60f, result.height, 0.001f);
        assertEquals(0, result.lineCount);
    }

    @Test
    public void sync_negativeValues_preserved() {
        // Although unusual, negative dimensions shouldn't crash
        MeasureResult result = MeasureResult.sync(-1f, -2f);
        assertEquals(-1f, result.width, 0.001f);
        assertEquals(-2f, result.height, 0.001f);
    }

    // ========================================================================
    // async() — Async Factory
    // ========================================================================

    @Test
    public void async_returnsAsyncType() {
        MeasureResult result = MeasureResult.async();
        assertEquals(MeasureResult.CALC_TYPE_ASYNC, result.calcType);
    }

    @Test
    public void async_dimensionsAreZero() {
        MeasureResult result = MeasureResult.async();
        assertEquals(0f, result.width, 0.001f);
        assertEquals(0f, result.height, 0.001f);
    }

    @Test
    public void async_lineCountIsZero() {
        MeasureResult result = MeasureResult.async();
        assertEquals(0, result.lineCount);
    }

    // ========================================================================
    // zero() — Zero Factory
    // ========================================================================

    @Test
    public void zero_returnsSyncType() {
        MeasureResult result = MeasureResult.zero();
        assertEquals(MeasureResult.CALC_TYPE_SYNC, result.calcType);
    }

    @Test
    public void zero_allValuesAreZero() {
        MeasureResult result = MeasureResult.zero();
        assertEquals(0f, result.width, 0.001f);
        assertEquals(0f, result.height, 0.001f);
        assertEquals(0, result.lineCount);
    }

    // ========================================================================
    // Constants Verification
    // ========================================================================

    @Test
    public void calcTypeSync_isZero() {
        assertEquals(0, MeasureResult.CALC_TYPE_SYNC);
    }

    @Test
    public void calcTypeAsync_isOne() {
        assertEquals(1, MeasureResult.CALC_TYPE_ASYNC);
    }

    // ========================================================================
    // toString
    // ========================================================================

    @Test
    public void toString_syncResult_containsAllFields() {
        MeasureResult result = MeasureResult.sync(42f, 24f, 3);
        String str = result.toString();
        assertNotNull(str);
        assertTrue(str.contains("42.0"));
        assertTrue(str.contains("24.0"));
        assertTrue(str.contains("3"));
        assertTrue(str.contains("calcType=0"));
    }

    @Test
    public void toString_asyncResult_showsAsyncType() {
        MeasureResult result = MeasureResult.async();
        String str = result.toString();
        assertTrue(str.contains("calcType=1"));
    }

    // ========================================================================
    // Constructor — Direct Usage
    // ========================================================================

    @Test
    public void constructor_setsAllFieldsCorrectly() {
        MeasureResult result = new MeasureResult(0, 55.5f, 77.7f, 12);
        assertEquals(0, result.calcType);
        assertEquals(55.5f, result.width, 0.001f);
        assertEquals(77.7f, result.height, 0.001f);
        assertEquals(12, result.lineCount);
    }

    // ========================================================================
    // Boundary: Float edge values
    // ========================================================================

    @Test
    public void sync_zeroWidthZeroHeight_accepted() {
        MeasureResult result = MeasureResult.sync(0f, 0f, 0);
        assertEquals(0f, result.width, 0.001f);
        assertEquals(0f, result.height, 0.001f);
    }

    @Test
    public void sync_verySmallFloat_preserved() {
        MeasureResult result = MeasureResult.sync(0.001f, 0.002f, 1);
        assertEquals(0.001f, result.width, 0.0001f);
        assertEquals(0.002f, result.height, 0.0001f);
    }
}
