package com.amap.agenui.render.surface;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;
import androidx.annotation.WorkerThread;

/**
 * Internal contract bridging the C++ {@code ISurfaceSizeProvider} engine hook
 * to the Java side. Implemented by {@link SurfaceManager}.
 *
 * <p>The native bridge in
 * {@code core/src/jni/jni_surface_size_provider_bridge.cpp} resolves the single
 * method below by name and signature via JNI reflection — renaming the method,
 * changing its parameters, or changing its return type MUST be mirrored on the
 * C++ side or the binding silently degrades to "no provider".
 *
 * <p>Not part of the public host-facing API — host apps should implement
 * {@link ISurfaceManagerListener#surfaceSize(String)} instead, which the
 * SurfaceManager aggregates through this internal contract.
 */
@Keep
@RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
public interface ISurfaceSizeProviderHost {

    /**
     * Aggregate surface-size pull from registered listeners.
     *
     * <p><b>⚠ THREADING WARNING — invoked synchronously on the engine
     * WORKER THREAD, not the UI thread.</b> See
     * {@link ISurfaceManagerListener#surfaceSize(String)} for the full
     * threading contract that propagates to listener implementations.
     *
     * @param surfaceId Surface identifier passed through from the engine.
     * @return Current surface size in vp, or {@code null} if not measurable yet.
     */
    @Keep
    @WorkerThread
    @Nullable
    SurfaceSize getSurfaceSize(@NonNull String surfaceId);
}
