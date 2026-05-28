package com.amap.agenui.render.measurement;

import android.content.Context;

/**
 * Component measurement interface.
 *
 * Implementations provide the intrinsic size of a component type under given Yoga constraints.
 * Returned by {@link com.amap.agenui.render.component.IComponentFactory#getMeasurer()} to
 * bind measurement logic to component registration automatically.
 */
@FunctionalInterface
public interface IMeasurer {

    /**
     * Calculates the intrinsic size of a component.
     *
     * @param context    Android Context
     * @param paramJson  Component attribute JSON string (from C++ ComponentSnapshot.stringify())
     * @param maxWidth   Maximum available width (A2UI logical units)
     * @param widthMode  Width mode (0=Undefined 1=Exactly 2=AtMost)
     * @param maxHeight  Maximum available height (A2UI logical units)
     * @param heightMode Height mode (0=Undefined 1=Exactly 2=AtMost)
     * @return Measurement result in A2UI logical units
     */
    MeasureResult measure(Context context,
                          String paramJson,
                          float maxWidth,
                          int widthMode,
                          float maxHeight,
                          int heightMode);
}
