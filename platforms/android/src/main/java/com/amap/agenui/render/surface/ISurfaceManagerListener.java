package com.amap.agenui.render.surface;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import java.util.Map;

/**
 * SurfaceManager listener interface.
 * Used to notify external components such as RecyclerView's ChatAdapter.
 * <p>
 * Usage scenarios:
 * - RecyclerView's ChatAdapter implements this interface
 * - Notifies the Adapter to update the list when a Surface is created or destroyed
 * - Notifies the Adapter to refresh the view when components are added or removed
 *
 */
public interface ISurfaceManagerListener {

    /**
     * Callback when a Surface is created
     *
     * @param surface Surface instance (the root View can be obtained via {@link Surface#getContainer()},
     *                the unique identifier via {@link Surface#getSurfaceId()})
     */
    void onCreateSurface(Surface surface);

    /**
     * Callback when a Surface is deleted
     *
     * @param surface The Surface instance that was deleted
     */
    void onDeleteSurface(Surface surface);

    /**
     * Callback when a component's action event is triggered.
     * <p>
     * When a component triggers an interaction event (e.g. button click) and is routed by the SDK,
     * the listener is notified via this method.
     *
     * @param event Event content as a JSON string
     */
    default void onReceiveActionEvent(String event) {
    }

    /**
     * Callback when the root component of a Surface is created or updated.
     * Triggered once the root component (id="root") has been processed.
     * Also triggered on subsequent incremental updates that include the root component.
     *
     * @param surface The Surface whose root component was updated
     * @param props   The root component's properties as String map, may be null
     */
    default void onRootComponentUpdate(Surface surface, Map<String, String> props) {
    }

    /**
     * Callback when a C++ execution error occurs.
     * <p>
     * Invoked for engine-level errors (e.g. DSL parse failures, surface not found).
     * Blank-screen detection results are delivered separately via {@link #onBlankCheckResult}.
     *
     * @param surface The Surface associated with the error; {@code null} if the error cannot
     *                be attributed to a specific Surface (e.g. Surface not yet created)
     * @param code    Error code
     * @param message Error description
     */
    default void onError(Surface surface, int code, String message) {
    }

    /**
     * Callback when a blank-screen check completes for a Surface.
     * <p>
     * Fired for both outcomes: {@code isBlank = true} when the component count is below
     * the configured threshold, {@code isBlank = false} when the check passes.
     *
     * @param surface The Surface on which the blank-screen check was performed;
     *                may be {@code null} if the Surface has already been destroyed
     * @param isBlank {@code true} if the screen is detected as blank;
     *                {@code false} if the check passed
     */
    default void onBlankCheckResult(Surface surface, boolean isBlank) {
    }

    /**
     * Callback when a component has appeared on screen.
     * <p>
     * Fired when a component (e.g. ListComponent whose child was bound by
     * RecyclerView) detects display and reports it through the generic display
     * channel. The host (e.g. AjxA2UIView) typically forwards this to the JS layer.
     *
     * @param surface           The Surface the display occurred on;
     *                          may be {@code null} if the Surface has been destroyed
     * @param parentComponentId ID of the component that detected the display
     * @param parentType        Component type name of the detector ("List" / "Carousel" / "Tabs" ...)
     * @param properties        Display properties as a key-value map
     */
    default void onComponentAppeared(Surface surface, String parentComponentId, String parentType, Map<String, Object> properties) {
    }

    /**
     * Optional pull callback: return the current size of the given surface in vp units.
     * <p>
     * The engine queries this hook synchronously during the first Yoga layout pass when
     * its locally cached surface size has never been initialized (no
     * {@code notifySurfaceSizeChanged} push has arrived yet and no prior pull has
     * succeeded). Once a positive size has been observed via either channel, the cache
     * is authoritative and this method is no longer consulted until the host pushes a
     * fresh size.
     * <p>
     * <b>⚠ THREADING WARNING — invoked on the engine WORKER THREAD, not the UI thread.</b>
     * <ul>
     *   <li>This method is called <i>synchronously</i> on the same engine sub-thread
     *       that delivers {@code onReceiveActionEvent} / {@code onError} and that
     *       processes streaming protocol chunks. It is NOT the Android main thread.</li>
     *   <li>Implementations MUST be thread-safe and MUST NOT block — the engine is
     *       holding up an in-flight layout pass while waiting for the return value.</li>
     *   <li>Implementations MUST NOT call main-thread-only Android APIs
     *       (e.g. {@code View.getMeasuredWidth()}, {@code Activity} APIs,
     *       {@code Resources.getDisplayMetrics()} on a fresh {@code Resources} instance).
     *       Pre-cache the size on the UI thread (under a lock or via a
     *       {@code volatile}/{@link java.util.concurrent.atomic.AtomicReference}) and
     *       just read the cached value here.</li>
     * </ul>
     * <p>
     * Return {@code null} (or a {@link SurfaceSize} with non-positive width/height) to
     * signal "not measurable yet". The engine will then skip the in-flight layout pass
     * and replay it once a positive size arrives via either this provider channel or
     * a subsequent push.
     *
     * @param surfaceId Surface identifier assigned by the engine.
     * @return The current surface size in vp, or {@code null} if not measurable yet.
     */
    @WorkerThread
    @Nullable
    default SurfaceSize surfaceSize(@NonNull String surfaceId) {
        return null;
    }
}
