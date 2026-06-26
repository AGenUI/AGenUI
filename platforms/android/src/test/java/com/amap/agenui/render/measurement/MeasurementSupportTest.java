package com.amap.agenui.render.measurement;

import org.json.JSONObject;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

public class MeasurementSupportTest {

    // ========================================================================
    // parseA2uiDimension
    // ========================================================================

    @Test
    public void parseA2uiDimension_numericValue_returnsFloat() {
        assertEquals(42f, MeasurementSupport.parseA2uiDimension(42), 0.001f);
    }

    @Test
    public void parseA2uiDimension_floatValue_returnsFloat() {
        assertEquals(3.14f, MeasurementSupport.parseA2uiDimension(3.14), 0.001f);
    }

    @Test
    public void parseA2uiDimension_pxString_stripsAndReturns() {
        assertEquals(100f, MeasurementSupport.parseA2uiDimension("100px"), 0.001f);
    }

    @Test
    public void parseA2uiDimension_pxStringUpperCase_stripsAndReturns() {
        assertEquals(50f, MeasurementSupport.parseA2uiDimension("50PX"), 0.001f);
    }

    @Test
    public void parseA2uiDimension_plainNumericString_returns() {
        assertEquals(200f, MeasurementSupport.parseA2uiDimension("200"), 0.001f);
    }

    @Test
    public void parseA2uiDimension_auto_returnsNull() {
        assertNull(MeasurementSupport.parseA2uiDimension("auto"));
    }

    @Test
    public void parseA2uiDimension_wrapContent_returnsNull() {
        assertNull(MeasurementSupport.parseA2uiDimension("wrap_content"));
    }

    @Test
    public void parseA2uiDimension_matchParent_returnsNull() {
        assertNull(MeasurementSupport.parseA2uiDimension("match_parent"));
    }

    @Test
    public void parseA2uiDimension_percentage_returnsNull() {
        assertNull(MeasurementSupport.parseA2uiDimension("50%"));
    }

    @Test
    public void parseA2uiDimension_emptyString_returnsNull() {
        assertNull(MeasurementSupport.parseA2uiDimension(""));
    }

    @Test
    public void parseA2uiDimension_null_returnsNull() {
        assertNull(MeasurementSupport.parseA2uiDimension(null));
    }

    @Test
    public void parseA2uiDimension_nonNumericString_returnsNull() {
        assertNull(MeasurementSupport.parseA2uiDimension("abc"));
    }

    @Test
    public void parseA2uiDimension_negativeNumber_returnsNegative() {
        assertEquals(-10f, MeasurementSupport.parseA2uiDimension(-10), 0.001f);
    }

    @Test
    public void parseA2uiDimension_zero_returnsZero() {
        assertEquals(0f, MeasurementSupport.parseA2uiDimension(0), 0.001f);
    }

    // ========================================================================
    // extractString
    // ========================================================================

    @Test
    public void extractString_plainString_returnsIt() {
        assertEquals("hello", MeasurementSupport.extractString("hello"));
    }

    @Test
    public void extractString_null_returnsEmpty() {
        assertEquals("", MeasurementSupport.extractString(null));
    }

    @Test
    public void extractString_jsonNull_returnsEmpty() {
        assertEquals("", MeasurementSupport.extractString(JSONObject.NULL));
    }

    @Test
    public void extractString_jsonWithLiteralString_returnsLiteral() throws Exception {
        JSONObject obj = new JSONObject();
        obj.put("literalString", "hello literal");
        assertEquals("hello literal", MeasurementSupport.extractString(obj));
    }

    @Test
    public void extractString_jsonWithPath_returnsPath() throws Exception {
        JSONObject obj = new JSONObject();
        obj.put("path", "/data/name");
        assertEquals("/data/name", MeasurementSupport.extractString(obj));
    }

    @Test
    public void extractString_jsonWithBothLiteralAndPath_prefersLiteral() throws Exception {
        JSONObject obj = new JSONObject();
        obj.put("literalString", "literal");
        obj.put("path", "/data/name");
        assertEquals("literal", MeasurementSupport.extractString(obj));
    }

    @Test
    public void extractString_numberValue_returnsStringRepresentation() {
        assertEquals("42", MeasurementSupport.extractString(42));
    }

    // ========================================================================
    // extractBoolean
    // ========================================================================

    @Test
    public void extractBoolean_trueValue_returnsTrue() {
        assertTrue(MeasurementSupport.extractBoolean(true, false));
    }

    @Test
    public void extractBoolean_falseValue_returnsFalse() {
        assertFalse(MeasurementSupport.extractBoolean(false, true));
    }

    @Test
    public void extractBoolean_null_returnsDefault() {
        assertTrue(MeasurementSupport.extractBoolean(null, true));
        assertFalse(MeasurementSupport.extractBoolean(null, false));
    }

    @Test
    public void extractBoolean_jsonNull_returnsDefault() {
        assertTrue(MeasurementSupport.extractBoolean(JSONObject.NULL, true));
    }

    @Test
    public void extractBoolean_jsonWithLiteralBoolean_returnsIt() throws Exception {
        JSONObject obj = new JSONObject();
        obj.put("literalBoolean", true);
        assertTrue(MeasurementSupport.extractBoolean(obj, false));
    }

    @Test
    public void extractBoolean_nonZeroNumber_returnsTrue() {
        assertTrue(MeasurementSupport.extractBoolean(1, false));
    }

    @Test
    public void extractBoolean_zeroNumber_returnsFalse() {
        assertFalse(MeasurementSupport.extractBoolean(0, true));
    }

    @Test
    public void extractBoolean_stringTrue_returnsTrue() {
        assertTrue(MeasurementSupport.extractBoolean("true", false));
    }

    @Test
    public void extractBoolean_stringFalse_returnsFalse() {
        assertFalse(MeasurementSupport.extractBoolean("false", true));
    }

    // ========================================================================
    // resolveSize
    // ========================================================================

    @Test
    public void resolveSize_undefined_returnsDesired() {
        MeasureResult r = MeasurementSupport.resolveSize(100f, 50f,
                0f, MeasurementSupport.MODE_UNDEFINED,
                0f, MeasurementSupport.MODE_UNDEFINED);
        assertEquals(100f, r.width, 0.001f);
        assertEquals(50f, r.height, 0.001f);
    }

    @Test
    public void resolveSize_exactly_returnsConstraint() {
        MeasureResult r = MeasurementSupport.resolveSize(100f, 50f,
                200f, MeasurementSupport.MODE_EXACTLY,
                80f, MeasurementSupport.MODE_EXACTLY);
        assertEquals(200f, r.width, 0.001f);
        assertEquals(80f, r.height, 0.001f);
    }

    @Test
    public void resolveSize_atMost_capsToMaximum() {
        MeasureResult r = MeasurementSupport.resolveSize(300f, 200f,
                200f, MeasurementSupport.MODE_AT_MOST,
                150f, MeasurementSupport.MODE_AT_MOST);
        assertEquals(200f, r.width, 0.001f);
        assertEquals(150f, r.height, 0.001f);
    }

    @Test
    public void resolveSize_atMost_desiredSmaller_returnsDesired() {
        MeasureResult r = MeasurementSupport.resolveSize(100f, 50f,
                200f, MeasurementSupport.MODE_AT_MOST,
                150f, MeasurementSupport.MODE_AT_MOST);
        assertEquals(100f, r.width, 0.001f);
        assertEquals(50f, r.height, 0.001f);
    }

    @Test
    public void resolveSize_negativeDesired_clampsToZero() {
        MeasureResult r = MeasurementSupport.resolveSize(-10f, -20f,
                0f, MeasurementSupport.MODE_UNDEFINED,
                0f, MeasurementSupport.MODE_UNDEFINED);
        assertEquals(0f, r.width, 0.001f);
        assertEquals(0f, r.height, 0.001f);
    }

    @Test
    public void resolveSize_resultIsSyncType() {
        MeasureResult r = MeasurementSupport.resolveSize(100f, 50f,
                0f, MeasurementSupport.MODE_UNDEFINED,
                0f, MeasurementSupport.MODE_UNDEFINED);
        assertEquals(MeasureResult.CALC_TYPE_SYNC, r.calcType);
    }

    // ========================================================================
    // resolveTextMaxWidth
    // ========================================================================

    @Test
    public void resolveTextMaxWidth_atMost_subtractsReserved() {
        float result = MeasurementSupport.resolveTextMaxWidth(320f, MeasurementSupport.MODE_AT_MOST, 40f);
        assertEquals(280f, result, 0.001f);
    }

    @Test
    public void resolveTextMaxWidth_exactly_subtractsReserved() {
        float result = MeasurementSupport.resolveTextMaxWidth(320f, MeasurementSupport.MODE_EXACTLY, 40f);
        assertEquals(280f, result, 0.001f);
    }

    @Test
    public void resolveTextMaxWidth_undefined_returnsZero() {
        float result = MeasurementSupport.resolveTextMaxWidth(320f, MeasurementSupport.MODE_UNDEFINED, 40f);
        assertEquals(0f, result, 0.001f);
    }

    @Test
    public void resolveTextMaxWidth_reservedExceedsMax_returnsZero() {
        float result = MeasurementSupport.resolveTextMaxWidth(40f, MeasurementSupport.MODE_AT_MOST, 100f);
        assertEquals(0f, result, 0.001f);
    }

    // ========================================================================
    // parseRoot
    // ========================================================================

    @Test
    public void parseRoot_validJson_returnsObject() {
        JSONObject root = MeasurementSupport.parseRoot("{\"key\":\"value\"}");
        assertNotNull(root);
        assertEquals("value", root.optString("key"));
    }

    @Test
    public void parseRoot_invalidJson_returnsNull() {
        assertNull(MeasurementSupport.parseRoot("not json"));
    }

    @Test
    public void parseRoot_emptyString_returnsNull() {
        assertNull(MeasurementSupport.parseRoot(""));
    }

    @Test
    public void parseRoot_null_returnsNull() {
        assertNull(MeasurementSupport.parseRoot(null));
    }

    // ========================================================================
    // optStyles
    // ========================================================================

    @Test
    public void optStyles_withStylesField_returnsIt() throws Exception {
        JSONObject root = new JSONObject();
        JSONObject styles = new JSONObject();
        styles.put("color", "red");
        root.put("styles", styles);
        JSONObject result = MeasurementSupport.optStyles(root);
        assertNotNull(result);
        assertEquals("red", result.optString("color"));
    }

    @Test
    public void optStyles_noStylesField_returnsNull() {
        JSONObject root = new JSONObject();
        assertNull(MeasurementSupport.optStyles(root));
    }

    @Test
    public void optStyles_null_returnsNull() {
        assertNull(MeasurementSupport.optStyles(null));
    }

    // ========================================================================
    // MeasureResult factory methods
    // ========================================================================

    @Test
    public void measureResult_sync_hasSyncType() {
        MeasureResult r = MeasureResult.sync(100f, 50f);
        assertEquals(MeasureResult.CALC_TYPE_SYNC, r.calcType);
        assertEquals(100f, r.width, 0.001f);
        assertEquals(50f, r.height, 0.001f);
        assertEquals(0, r.lineCount);
    }

    @Test
    public void measureResult_syncWithLineCount_preservesCount() {
        MeasureResult r = MeasureResult.sync(100f, 50f, 3);
        assertEquals(3, r.lineCount);
    }

    @Test
    public void measureResult_async_hasAsyncType() {
        MeasureResult r = MeasureResult.async();
        assertEquals(MeasureResult.CALC_TYPE_ASYNC, r.calcType);
        assertEquals(0f, r.width, 0.001f);
        assertEquals(0f, r.height, 0.001f);
    }

    @Test
    public void measureResult_zero_hasZeroSize() {
        MeasureResult r = MeasureResult.zero();
        assertEquals(MeasureResult.CALC_TYPE_SYNC, r.calcType);
        assertEquals(0f, r.width, 0.001f);
        assertEquals(0f, r.height, 0.001f);
    }

    @Test
    public void measureResult_toString_includesFields() {
        MeasureResult r = MeasureResult.sync(100f, 50f, 2);
        String s = r.toString();
        assertTrue(s.contains("100"));
        assertTrue(s.contains("50"));
        assertTrue(s.contains("lineCount=2"));
    }
}
