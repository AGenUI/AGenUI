package com.amap.agenui.function;

import androidx.annotation.RestrictTo;

/**
 * JNI bridge interface, for internal SDK use only.
 *
 * The C++ layer calls the implementation class ({@link PlatformFunction}) of this interface
 * directly via JNI. External code should not implement this interface — implement the public
 * {@link IFunction} interface instead.
 *
 * {@code callbackPtr} is a callback handle passed in from the C++ layer. It is returned to
 * the native side internally by the SDK via
 * {@link com.amap.agenui.AGenUI#nativeOnAsyncCallbackResult}. External implementors should
 * not hold or manipulate this value directly.
 */
@RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
interface IPlatformFunction {

    /**
     * Synchronous function call, invoked by the C++ layer via JNI.
     *
     * @param context Call context containing instanceId and surfaceId
     * @param params  Parameters in JSON format
     * @return Return result in JSON format
     */
    String callSync(FunctionCallContext context, String params);
}
