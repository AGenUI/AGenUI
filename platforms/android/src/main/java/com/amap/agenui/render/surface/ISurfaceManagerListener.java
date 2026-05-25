package com.amap.agenui.render.surface;

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
     *
     * @param type      Error type: "SDKInternal" (engine error) or "BlankScreen" (blank screen detection)
     * @param code      Error reason string (e.g. "DSL_parse_failed", "surfaceNotFound",
     *                  "componentCountInsufficient")
     * @param message
     * @param surfaceId Surface identifier (empty if not yet parsed)
     */
    default void onError(String type, int code, String message, String surfaceId) {
    }
}
