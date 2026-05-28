package com.amap.agenui.render.measurement;

import android.content.Context;
import com.amap.agenui.render.utils.AGenUILogger;

import androidx.annotation.Keep;

import com.amap.agenui.AGenUI;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Java-side measurement entry point called from the native Yoga pipeline.
 *
 * Responsibilities:
 * 1. Acts as the only JNI-visible measurement dispatch method.
 * 2. Maintains a registry of type -> IMeasurer and dispatches via lookup.
 * 3. Guarantees a stable zero result when context, params or type are invalid.
 */
@Keep
public final class MeasurementBridge {

    private static final String TAG = "MeasurementBridge";

    private static final Map<String, IMeasurer> sMeasurers = new ConcurrentHashMap<>();

    private MeasurementBridge() {
    }

    /**
     * Registers a measurer for the given component type.
     * Also registers the type in the native C++ MeasurementManager so Yoga can invoke it.
     *
     * @param type     Component type string (e.g. "Text", "Image")
     * @param measurer IMeasurer implementation
     */
    public static void registerMeasurer(String type, IMeasurer measurer) {
        if (type == null || measurer == null) {
            return;
        }
        sMeasurers.put(type, measurer);
        nativeRegisterMeasurement(type);
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "registerMeasurer: type=" + type);
        }
    }

    /**
     * Unregisters the measurer for the given component type.
     * Also unregisters from the native C++ MeasurementManager.
     *
     * @param type Component type string
     */
    public static void unregisterMeasurer(String type) {
        if (type == null) {
            return;
        }
        sMeasurers.remove(type);
        nativeUnregisterMeasurement(type);
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "unregisterMeasurer: type=" + type);
        }
    }

    /**
     * Executes a synchronous platform measurement for one Yoga leaf/hybrid component.
     *
     * The native side has already converted Yoga's measure request into A2UI-space constraints.
     * This method only dispatches by component type and returns a {@link MeasureResult}; any
     * asynchronous follow-up reflow must be reported later by the component itself through
     * `notifyRenderFinish`.
     */
    @Keep
    public static MeasureResult directMeasure(String type,
                                              String paramJson,
                                              float maxWidth,
                                              int widthMode,
                                              float maxHeight,
                                              int heightMode) {
        Context context = AGenUI.getInstance().getApplicationContextForSdk();
        if (context == null || type == null || paramJson == null) {
            MeasureResult result = MeasureResult.zero();
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "directMeasure skipped: context/type/paramJson invalid, result=" + result);
            }
            return result;
        }

        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.w(TAG, "directMeasure start: type=" + type
                    + ", paramJson=" + paramJson
                    + ", maxWidth=" + maxWidth
                    + ", widthMode=" + widthMode
                    + ", maxHeight=" + maxHeight
                    + ", heightMode=" + heightMode);
        }

        IMeasurer measurer = sMeasurers.get(type);
        MeasureResult result;
        if (measurer != null) {
            result = measurer.measure(context, paramJson, maxWidth, widthMode, maxHeight, heightMode);
        } else {
            result = MeasureResult.zero();
        }

        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.w(TAG, "directMeasure result: type=" + type + ", result=" + result);
        }
        return result;
    }

    private static native void nativeRegisterMeasurement(String type);

    private static native void nativeUnregisterMeasurement(String type);
}
