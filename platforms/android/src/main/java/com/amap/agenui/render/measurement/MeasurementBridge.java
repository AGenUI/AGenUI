package com.amap.agenui.render.measurement;

import android.content.Context;
import com.amap.agenui.render.utils.AGenUILogger;

import androidx.annotation.Keep;

import com.amap.agenui.AGenUI;

/**
 * Java-side measurement entry point called from the native Yoga pipeline.
 *
 * Responsibilities:
 * 1. Acts as the only JNI-visible measurement dispatch method.
 * 2. Maps component type -> concrete measurer implementation.
 * 3. Guarantees a stable zero result when context, params or type are invalid.
 */
@Keep
public final class MeasurementBridge {

    private static final String TAG = "MeasurementBridge";

    private MeasurementBridge() {
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

        MeasureResult result;
        switch (type) {
            case "Text":
                result = TextMeasurer.measureText(context, paramJson, maxWidth, widthMode, maxHeight, heightMode);
                break;
            case "RichText":
                result = TextMeasurer.measureRichText(context, paramJson, maxWidth, widthMode, maxHeight, heightMode);
                break;
            case "Image":
                result = ImageMeasurer.measure(paramJson, maxWidth, widthMode, maxHeight, heightMode);
                break;
            case "CheckBox":
                result = CheckBoxMeasurer.measure(context, paramJson, maxWidth, widthMode, maxHeight, heightMode);
                break;
            case "ChoicePicker":
                result = ChoicePickerMeasurer.measure(context, paramJson, maxWidth, widthMode, maxHeight, heightMode);
                break;
            case "DateTimeInput":
                result = DateTimeInputMeasurer.measure(context, paramJson, maxWidth, widthMode, maxHeight, heightMode);
                break;
            case "Slider":
                result = SliderMeasurer.measure(context, paramJson, maxWidth, widthMode, maxHeight, heightMode);
                break;
            case "AudioPlayer":
                result = AudioPlayerMeasurer.measure(context, paramJson, maxWidth, widthMode, maxHeight, heightMode);
                break;
            case "Icon":
                result = IconMeasurer.measure(paramJson, maxWidth, widthMode, maxHeight, heightMode);
                break;
            case "Table":
                result = TableMeasurer.measure(context, paramJson, maxWidth, widthMode, maxHeight, heightMode);
                break;
            default:
                result = MeasureResult.zero();
                break;
        }

        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.w(TAG, "directMeasure result: type=" + type + ", result=" + result);
        }
        return result;
    }
}
