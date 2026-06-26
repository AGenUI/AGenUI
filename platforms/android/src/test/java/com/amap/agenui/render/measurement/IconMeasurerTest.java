package com.amap.agenui.render.measurement;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;

/**
 * Unit tests for {@link IconMeasurer}.
 *
 * <p>Pure logic — exercises the package-private helpers
 * {@code parseIconSizeA2ui} and {@code resolveSyncResult} as well as the
 * public {@code measure(String, ...)} entry point. Mode constants come from
 * {@link MeasurementSupport}: 0 = UNDEFINED, 1 = EXACTLY, 2 = AT_MOST.
 */
public class IconMeasurerTest {

    private static final int MODE_UNDEFINED = 0;
    private static final int MODE_EXACTLY = 1;
    private static final int MODE_AT_MOST = 2;
    private static final float DEFAULT_ICON_SIZE_A2UI = 48f;
    private static final float EPS = 0.001f;

    // ──────────────────────────────────────────────────────────────
    // parseIconSizeA2ui — number / string / null / negative
    // ──────────────────────────────────────────────────────────────

    @Test
    public void parseIconSizeA2ui_null_returnsNull() {
        assertNull(IconMeasurer.parseIconSizeA2ui(null));
    }

    @Test
    public void parseIconSizeA2ui_jsonNull_returnsNull() {
        assertNull(IconMeasurer.parseIconSizeA2ui(JSONObject.NULL));
    }

    @Test
    public void parseIconSizeA2ui_positiveInt_doublesToA2uiUnit() {
        // 24 dp icon → 48 a2ui units (×2 conversion)
        Float result = IconMeasurer.parseIconSizeA2ui(24);
        assertNotNull(result);
        assertEquals(48f, result, EPS);
    }

    @Test
    public void parseIconSizeA2ui_positiveLong_doublesToA2uiUnit() {
        Float result = IconMeasurer.parseIconSizeA2ui(16L);
        assertNotNull(result);
        assertEquals(32f, result, EPS);
    }

    @Test
    public void parseIconSizeA2ui_positiveFloatTruncatesToInt() {
        // 24.7 → intValue() = 24 → 48
        Float result = IconMeasurer.parseIconSizeA2ui(24.7);
        assertNotNull(result);
        assertEquals(48f, result, EPS);
    }

    @Test
    public void parseIconSizeA2ui_zero_returnsNull() {
        // dpValue > 0 guard rejects zero
        assertNull(IconMeasurer.parseIconSizeA2ui(0));
    }

    @Test
    public void parseIconSizeA2ui_negative_returnsNull() {
        assertNull(IconMeasurer.parseIconSizeA2ui(-8));
    }

    @Test
    public void parseIconSizeA2ui_numericString_doubles() {
        Float result = IconMeasurer.parseIconSizeA2ui("32");
        assertNotNull(result);
        assertEquals(64f, result, EPS);
    }

    @Test
    public void parseIconSizeA2ui_nonNumericString_returnsNull() {
        assertNull(IconMeasurer.parseIconSizeA2ui("not-a-number"));
    }

    @Test
    public void parseIconSizeA2ui_emptyString_returnsNull() {
        assertNull(IconMeasurer.parseIconSizeA2ui(""));
    }

    // ──────────────────────────────────────────────────────────────
    // resolveSyncResult — preference order: size > explicit > default
    // ──────────────────────────────────────────────────────────────

    @Test
    public void resolveSyncResult_sizeOverridesEverything() {
        // size=100 wins over explicit width=50/height=70 and default=48
        MeasureResult r = IconMeasurer.resolveSyncResult(
                50f, 70f, /*size=*/100f, DEFAULT_ICON_SIZE_A2UI,
                /*maxW=*/0f, MODE_UNDEFINED, /*maxH=*/0f, MODE_UNDEFINED);

        assertEquals(100f, r.width, EPS);
        assertEquals(100f, r.height, EPS);
    }

    @Test
    public void resolveSyncResult_explicitFallbackWhenNoSize() {
        MeasureResult r = IconMeasurer.resolveSyncResult(
                /*explicitW=*/60f, /*explicitH=*/80f, /*size=*/null, DEFAULT_ICON_SIZE_A2UI,
                0f, MODE_UNDEFINED, 0f, MODE_UNDEFINED);

        assertEquals(60f, r.width, EPS);
        assertEquals(80f, r.height, EPS);
    }

    @Test
    public void resolveSyncResult_defaultWhenAllAbsent() {
        MeasureResult r = IconMeasurer.resolveSyncResult(
                null, null, null, DEFAULT_ICON_SIZE_A2UI,
                0f, MODE_UNDEFINED, 0f, MODE_UNDEFINED);

        assertEquals(DEFAULT_ICON_SIZE_A2UI, r.width, EPS);
        assertEquals(DEFAULT_ICON_SIZE_A2UI, r.height, EPS);
    }

    @Test
    public void resolveSyncResult_oneAxisExplicitOtherDefault() {
        MeasureResult r = IconMeasurer.resolveSyncResult(
                /*explicitW=*/100f, /*explicitH=*/null, null, DEFAULT_ICON_SIZE_A2UI,
                0f, MODE_UNDEFINED, 0f, MODE_UNDEFINED);

        assertEquals(100f, r.width, EPS);
        assertEquals(DEFAULT_ICON_SIZE_A2UI, r.height, EPS);
    }

    // ──────────────────────────────────────────────────────────────
    // resolveSyncResult — Yoga mode constraints clamp the result
    // ──────────────────────────────────────────────────────────────

    @Test
    public void resolveSyncResult_modeExactly_overridesWithMaxWidth() {
        MeasureResult r = IconMeasurer.resolveSyncResult(
                null, null, /*size=*/100f, DEFAULT_ICON_SIZE_A2UI,
                /*maxW=*/30f, MODE_EXACTLY, 0f, MODE_UNDEFINED);

        // EXACTLY forces width = maxWidth regardless of desired
        assertEquals(30f, r.width, EPS);
    }

    @Test
    public void resolveSyncResult_modeAtMost_clampsWhenDesiredExceeds() {
        MeasureResult r = IconMeasurer.resolveSyncResult(
                null, null, /*size=*/200f, DEFAULT_ICON_SIZE_A2UI,
                /*maxW=*/120f, MODE_AT_MOST, 0f, MODE_UNDEFINED);

        assertEquals(120f, r.width, EPS);
    }

    @Test
    public void resolveSyncResult_modeAtMost_keepsDesiredWhenSmaller() {
        MeasureResult r = IconMeasurer.resolveSyncResult(
                null, null, /*size=*/40f, DEFAULT_ICON_SIZE_A2UI,
                /*maxW=*/120f, MODE_AT_MOST, 0f, MODE_UNDEFINED);

        assertEquals(40f, r.width, EPS);
    }

    @Test
    public void resolveSyncResult_modeUndefined_returnsDesired() {
        MeasureResult r = IconMeasurer.resolveSyncResult(
                null, null, /*size=*/77f, DEFAULT_ICON_SIZE_A2UI,
                /*maxW=*/10f, MODE_UNDEFINED, 0f, MODE_UNDEFINED);

        assertEquals(77f, r.width, EPS);
    }

    // ──────────────────────────────────────────────────────────────
    // measure(String) — entry point, integration of helpers
    // ──────────────────────────────────────────────────────────────

    @Test
    public void measure_nullJson_returnsZero() {
        MeasureResult r = IconMeasurer.measure(null,
                0f, MODE_UNDEFINED, 0f, MODE_UNDEFINED);

        assertEquals(0f, r.width, EPS);
        assertEquals(0f, r.height, EPS);
    }

    @Test
    public void measure_invalidJson_returnsZero() {
        MeasureResult r = IconMeasurer.measure("not-json-{",
                0f, MODE_UNDEFINED, 0f, MODE_UNDEFINED);

        assertEquals(0f, r.width, EPS);
        assertEquals(0f, r.height, EPS);
    }

    @Test
    public void measure_emptyJson_usesDefaultSize() {
        MeasureResult r = IconMeasurer.measure("{}",
                0f, MODE_UNDEFINED, 0f, MODE_UNDEFINED);

        assertEquals(DEFAULT_ICON_SIZE_A2UI, r.width, EPS);
        assertEquals(DEFAULT_ICON_SIZE_A2UI, r.height, EPS);
    }

    @Test
    public void measure_sizeProperty_overridesDefault() throws JSONException {
        JSONObject root = new JSONObject();
        root.put("size", 32);

        MeasureResult r = IconMeasurer.measure(root.toString(),
                0f, MODE_UNDEFINED, 0f, MODE_UNDEFINED);

        // 32 dp → 64 a2ui
        assertEquals(64f, r.width, EPS);
        assertEquals(64f, r.height, EPS);
    }

    @Test
    public void measure_explicitStyleWidthHeight_overridesDefault() throws JSONException {
        JSONObject styles = new JSONObject();
        styles.put("width", 100);
        styles.put("height", 80);
        JSONObject root = new JSONObject();
        root.put("styles", styles);

        MeasureResult r = IconMeasurer.measure(root.toString(),
                0f, MODE_UNDEFINED, 0f, MODE_UNDEFINED);

        assertEquals(100f, r.width, EPS);
        assertEquals(80f, r.height, EPS);
    }

    @Test
    public void measure_sizeWinsOverExplicitStyle() throws JSONException {
        JSONObject styles = new JSONObject();
        styles.put("width", 30);
        styles.put("height", 30);
        JSONObject root = new JSONObject();
        root.put("size", 50);
        root.put("styles", styles);

        MeasureResult r = IconMeasurer.measure(root.toString(),
                0f, MODE_UNDEFINED, 0f, MODE_UNDEFINED);

        // 50 dp → 100 a2ui, beats explicit width/height
        assertEquals(100f, r.width, EPS);
        assertEquals(100f, r.height, EPS);
    }

    @Test
    public void measure_modeExactly_clampsToMaxWidth() throws JSONException {
        JSONObject root = new JSONObject();
        root.put("size", 100);

        MeasureResult r = IconMeasurer.measure(root.toString(),
                /*maxW=*/40f, MODE_EXACTLY, 0f, MODE_UNDEFINED);

        assertEquals(40f, r.width, EPS);
    }
}
