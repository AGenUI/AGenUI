package com.amap.agenui.function;

import com.amap.agenui.render.utils.AGenUILogger;

import androidx.annotation.NonNull;

/**
 * Function adapter: adapts IFunction to IPlatformFunction
 *
 * Used to maintain compatibility with the new registerFunction interface.
 * Wraps the Java-layer IFunction and registers it with the C++ layer.
 */
public class PlatformFunction implements IPlatformFunction {
    private static final String TAG = "PlatformFunction";

    private final IFunction function;

    /**
     * Constructor
     *
     * @param function The Function instance to adapt
     */
    public PlatformFunction(@NonNull IFunction function) {
        this.function = function;
    }

    /**
     * Synchronous Function call
     *
     * @param context Call context containing instanceId and surfaceId
     * @param params  Parameters in JSON format
     * @return Execution result in JSON format
     */
    @Override
    public String callSync(FunctionCallContext context, String params) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "callSync: function=" + function.getConfig().getName() + ", params=" + params);
        }
        try {
            FunctionResult result = function.execute(context, params);
            String resultJson = (result != null)
                    ? result.toJsonString()
                    : FunctionResult.createSuccess(null).toJsonString();
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.i(TAG, "callSync result: " + resultJson);
            }
            return resultJson;
        } catch (Exception e) {
            AGenUILogger.e(TAG, "callSync: exception in execute()", e);
            return FunctionResult.createError(e.getMessage()).toJsonString();
        }
    }
}
