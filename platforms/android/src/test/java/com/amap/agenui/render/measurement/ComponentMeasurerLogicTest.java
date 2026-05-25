package com.amap.agenui.render.measurement;

import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

public class ComponentMeasurerLogicTest {

    @Test
    public void checkBoxMeasurementAddsCheckboxAndLabelWidth() {
        MeasureResult result = CheckBoxMeasurer.resolveSyncResult(
                32f,
                16f,
                120f,
                40f,
                0f,
                MeasurementSupport.MODE_UNDEFINED,
                0f,
                MeasurementSupport.MODE_UNDEFINED);

        assertEquals(168f, result.width, 0.001f);
        assertEquals(40f, result.height, 0.001f);
    }

    @Test
    public void choicePickerVerticalStacksHeightsWithGap() {
        MeasureResult result = ChoicePickerMeasurer.resolveSyncResult(
                false,
                new float[]{180f, 140f},
                new float[]{64f, 72f},
                40f,
                0f, // extraHeight - no filter
                0f,
                MeasurementSupport.MODE_UNDEFINED,
                0f,
                MeasurementSupport.MODE_UNDEFINED);

        assertEquals(180f, result.width, 0.001f);
        assertEquals(176f, result.height, 0.001f);
    }

    @Test
    public void choicePickerHorizontalStacksWidthsWithGap() {
        MeasureResult result = ChoicePickerMeasurer.resolveSyncResult(
                true,
                new float[]{180f, 140f},
                new float[]{64f, 72f},
                40f,
                0f, // extraHeight - no filter
                0f,
                MeasurementSupport.MODE_UNDEFINED,
                0f,
                MeasurementSupport.MODE_UNDEFINED);

        assertEquals(360f, result.width, 0.001f);
        assertEquals(72f, result.height, 0.001f);
    }

    @Test
    public void dateTimeInputUsesPlaceholderWhenValueInvalid() {
        DateTimeInputMeasurer.DisplayState state = DateTimeInputMeasurer.resolveDisplayState(
                true,
                false,
                "invalid-date",
                "Select Date");

        assertEquals("Select Date", state.text);
        assertFalse(state.selected);
        assertTrue(state.showIcon);
    }

    @Test
    public void dateTimeInputHidesIconForSelectedValue() {
        DateTimeInputMeasurer.DisplayState state = DateTimeInputMeasurer.resolveDisplayState(
                true,
                false,
                "2026-05-07",
                "Select Date");

        assertEquals("2026-05-07", state.text);
        assertTrue(state.selected);
        assertFalse(state.showIcon);
    }

    @Test
    public void dateTimeInputAddsIconWidthOnlyWhenVisible() {
        MeasureResult result = DateTimeInputMeasurer.resolveSyncResult(
                56f,
                24f,
                100f,
                24f,
                6f,
                true,
                0f,
                MeasurementSupport.MODE_UNDEFINED,
                0f,
                MeasurementSupport.MODE_UNDEFINED);

        assertEquals(178f, result.width, 0.001f);
        assertEquals(56f, result.height, 0.001f);
    }

    @Test
    public void iconSizePropertyUsesDpSemantics() {
        assertEquals(48f, IconMeasurer.parseIconSizeA2ui(24), 0.001f);
        assertNull(IconMeasurer.parseIconSizeA2ui("24px"));
    }

    @Test
    public void iconSizePropertyOverridesExplicitStyles() {
        MeasureResult result = IconMeasurer.resolveSyncResult(
                24f,
                24f,
                60f,
                48f,
                0f,
                MeasurementSupport.MODE_UNDEFINED,
                0f,
                MeasurementSupport.MODE_UNDEFINED);

        assertEquals(60f, result.width, 0.001f);
        assertEquals(60f, result.height, 0.001f);
    }

    @Test
    public void tableHorizontalScrollUsesViewportWidthWhenConstrained() {
        MeasureResult result = TableMeasurer.resolveSyncResult(
                280f,
                160f,
                true,
                320f,
                MeasurementSupport.MODE_AT_MOST,
                0f,
                MeasurementSupport.MODE_UNDEFINED);

        assertEquals(320f, result.width, 0.001f);
        assertEquals(160f, result.height, 0.001f);
    }

    @Test
    public void tableWithoutConstraintKeepsContentWidth() {
        MeasureResult result = TableMeasurer.resolveSyncResult(
                420f,
                160f,
                true,
                0f,
                MeasurementSupport.MODE_UNDEFINED,
                0f,
                MeasurementSupport.MODE_UNDEFINED);

        assertEquals(420f, result.width, 0.001f);
        assertEquals(160f, result.height, 0.001f);
    }
}
