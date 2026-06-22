package com.amap.agenui.render.surface;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;
import androidx.annotation.WorkerThread;

import com.amap.agenui.AGenUI;
import com.amap.agenui.IAGenUIMessageListener;
import com.amap.agenui.render.component.ComponentEventDispatcher;
import com.amap.agenui.render.utils.AGenUILogger;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Surface manager
 * <p>
 * Responsibilities:
 * 1. Manages multiple Surface instances
 * 2. Provides interfaces for creating, retrieving, and destroying Surfaces
 * 3. Provides container bind/unbind interfaces (for RecyclerView optimization)
 * 4. Notifies SurfaceListeners (e.g. RecyclerView's ChatAdapter)
 * <p>
 * Design notes:
 * - Surfaces can be created without a container (supports pre-rendering)
 * - Container bind/unbind is independent of the Surface lifecycle
 * - Supports RecyclerView ViewHolder recycling optimization
 *
 */
public class SurfaceManager implements ISurfaceSizeProviderHost {

    private static final String TAG = "SurfaceManager";

    private final WeakReference<Context> contextRef;
    private final NativeEventBridge nativeEventBridge;

    private final Map<String, Surface> surfaces = new ConcurrentHashMap<>();
    private final List<ISurfaceManagerListener> listeners = new CopyOnWriteArrayList<>();
    private final int instanceId;

    /**
     * Constructor
     *
     * @param activity Android Activity
     */
    @SuppressWarnings("RestrictedApi")
    public SurfaceManager(@NonNull Activity activity) throws IllegalStateException {
        this.contextRef = new WeakReference<>(activity);
        this.instanceId = AGenUI.getInstance().createSurfaceManager();
        this.nativeEventBridge = new NativeEventBridge(this, instanceId);
        addMessageListener(nativeEventBridge);
        registerSurfaceSizeProvider();
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "SurfaceManager created with instanceId=" + instanceId);
        }
    }

    /**
     * Begins a round of streaming data reception.
     * <p>
     * Clears the buffer and resets the parsing state. Recommended to call at the start of
     * each new session.
     */
    public void beginTextStream() {
        try {
            nativeBeginTextStream(instanceId);
        } catch (RuntimeException e) {
            AGenUILogger.e(TAG, "Failed to beginTextStream", e);
        }
    }

    /**
     * Transmits data (supports streaming fragments).
     * <p>
     * Supports transmitting a complete JSON data packet or a streaming fragment:
     * - Complete JSON: e.g. createSurface, updateComponents, updateDataModel, and other
     *   complete protocol data
     * - Streaming fragment: supports incremental data transmitted in segments
     *
     * @param dataString Data JSON string; format must conform to the AGenUI protocol spec
     */
    public void receiveTextChunk(String dataString) {
        try {
            nativeReceiveTextChunk(instanceId, dataString);
        } catch (RuntimeException e) {
            AGenUILogger.e(TAG, "Failed to receiveTextChunk", e);
        }
    }

    /**
     * Ends a round of streaming data reception.
     * <p>
     * Resets the parsing state. Should be called after the SSE stream closes normally,
     * the HTTP response ends, the user actively cancels the conversation, or a network
     * disconnect occurs.
     */
    public void endTextStream() {
        try {
            nativeEndTextStream(instanceId);
        } catch (RuntimeException e) {
            AGenUILogger.e(TAG, "Failed to endTextStream", e);
        }
    }

    /**
     * Re-evaluates every component's attributes and styles across all surfaces managed by
     * this SurfaceManager, then emits field-level diffs to the native renderer for any value
     * that actually changed.
     * <p>
     * Call this when host-owned external state has changed in ways the SDK cannot observe
     * (theme, locale, orientation, etc.) and registered FunctionCalls that read from that
     * state need to be re-run. Action handlers are not in scope.
     */
    public void invalidateFunctionCallValues() {
        try {
            nativeInvalidateFunctionCallValues(instanceId);
        } catch (RuntimeException e) {
            AGenUILogger.e(TAG, "Failed to invalidateFunctionCallValues", e);
        }
    }

    /**
     * Adds a Surface listener
     *
     * @param listener ISurfaceManagerListener instance
     */
    public void addListener(ISurfaceManagerListener listener) {
        if (listener != null && !listeners.contains(listener)) {
            listeners.add(listener);
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.d(TAG, "Listener added: " + listener.getClass().getSimpleName());
            }
        }
    }

    /**
     * Removes a Surface listener
     *
     * @param listener ISurfaceManagerListener instance
     */
    public void removeListener(ISurfaceManagerListener listener) {
        if (listener != null) {
            listeners.remove(listener);
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.d(TAG, "Listener removed: " + listener.getClass().getSimpleName());
            }
        }
    }


    /**
     * Registers a UIMessage listener
     *
     * @param listener Event listener
     */
    void addMessageListener(IAGenUIMessageListener listener) {
        if (listener == null) {
            AGenUILogger.w(TAG, "addMessageListener: listener is null");
            return;
        }
        try {
            nativeAddEventListener(instanceId, listener);
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.i(TAG, "UIMessage listener registered: instanceId=" + instanceId);
            }
        } catch (Exception e) {
            AGenUILogger.e(TAG, "Failed to register UIMessage listener", e);
        }
    }

    /**
     * Removes a UIMessage listener
     *
     * @param listener The event listener to remove
     */
    void removeMessageListener(IAGenUIMessageListener listener) {
        if (listener == null) {
            AGenUILogger.w(TAG, "removeMessageListener: listener is null");
            return;
        }
        try {
            nativeRemoveEventListener(instanceId, listener);
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.i(TAG, "UIMessage listener unregistered: instanceId=" + instanceId);
            }
        } catch (Exception e) {
            AGenUILogger.e(TAG, "Failed to unregister UIMessage listener", e);
        }
    }


    /**
     * Destroys all resources held by this SurfaceManager.
     * <p>
     * Destroys all Surfaces, removes all listeners, and cleans up NativeEventBridge.
     */
    public void destroy() {
        clearAll();
        listeners.clear();
        removeMessageListener(nativeEventBridge);
        unregisterSurfaceSizeProvider();
        AGenUI.getInstance().destroySurfaceManager(instanceId);
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "SurfaceManager destroyed, instanceId=" + instanceId);
        }
    }

    /**
     * Registers this SurfaceManager as the C++ engine's surface-size provider.
     * <p>
     * Called once during construction, AFTER the native SurfaceManager has been
     * created and BEFORE any Surface is created on the C++ side, so the very
     * first Yoga bootstrap layout already sees the provider. The C++ bridge
     * will hold a JNI global ref to this object until
     * {@link #unregisterSurfaceSizeProvider()} is called.
     */
    private void registerSurfaceSizeProvider() {
        try {
            nativeSetSurfaceSizeProvider(instanceId, this);
        } catch (Exception e) {
            AGenUILogger.e(TAG, "Failed to register surface size provider", e);
        }
    }

    /**
     * Detaches this SurfaceManager from the C++ engine's surface-size provider
     * slot and releases the JNI global ref held by the bridge.
     * <p>
     * Must run BEFORE the native SurfaceManager is torn down so (a) the JNI
     * bridge can still talk to the live engine to clear the pointer and
     * (b) the worker thread does not call back into a soon-to-be-destroyed
     * Java side. Order matters: provider clear → {@code destroySurfaceManager}.
     */
    private void unregisterSurfaceSizeProvider() {
        try {
            nativeClearSurfaceSizeProvider(instanceId);
        } catch (Exception e) {
            AGenUILogger.e(TAG, "Failed to clear surface size provider", e);
        }
    }


    /**
     * Returns the native instance id assigned by the engine on creation.
     */
    public int getInstanceId() {
        return instanceId;
    }

    /**
     * Returns the Activity Context (weak reference; may return null after the Activity is destroyed).
     * Callers must perform a null check.
     */
    Context getContext() {
        Context ctx = contextRef.get();
        if (ctx == null) {
            AGenUILogger.w(TAG, "getContext: Activity has been GC'd; release() may have been missed");
        }
        return ctx;
    }

    /**
     * Creates a Surface.
     * <p>
     * Called by NativeEventBridge.onCreateSurface().
     * The Surface internally creates a root container; callers obtain it via surface.getContainer().
     *
     * @param surfaceId          Unique Surface identifier
     * @param rawProtocolContent Original raw protocol content
     * @param animated           Initial animation toggle from the native protocol; applied before
     *                           listeners are notified so listener overrides take final effect.
     * @return Created Surface instance
     * @hide
     */
    @RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
    public Surface createSurface(String surfaceId, String rawProtocolContent, boolean animated) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "createSurface: surfaceId=" + surfaceId);
        }
        Context context = getContext();
        if (context == null) {
            AGenUILogger.e(TAG, "createSurface: Cannot create surface, Activity context is null (Activity may have been destroyed)");
            return null;
        }

        Surface surface = new Surface(
                surfaceId,
                getContext(),
                new ComponentEventDispatcher() {
                    @Override
                    public void submitUIAction(String sid, String componentId, String contextJson) {
                        try {
                            nativeSubmitUIAction(instanceId, sid, componentId, contextJson);
                        } catch (Exception e) {
                            AGenUILogger.e(TAG, "Failed to submitUIAction", e);
                        }
                    }

                    @Override
                    public void submitUIDataModel(String sid, String componentId, String changeData) {
                        try {
                            nativeSubmitUIDataModel(instanceId, sid, componentId, changeData);
                        } catch (Exception e) {
                            AGenUILogger.e(TAG, "Failed to submitUIDataModel", e);
                        }
                    }

                    @Override
                    public void onBlankCheckResult(String sid, boolean isBlank) {
                        notifyBlankCheckResult(getSurface(sid), isBlank);
                    }
                },
                new SurfaceLayoutDispatcher(
                        surfaceId,
                        new SurfaceLayoutDispatcher.Callback() {
                            @Override
                            public void onRenderFinish(String callbackSurfaceId,
                                                       String componentId,
                                                       String type,
                                                       float width,
                                                       float height,
                                                       int selectedIndex) {
                                notifyRenderFinish(
                                        callbackSurfaceId,
                                        componentId,
                                        type,
                                        width,
                                        height,
                                        selectedIndex);
                            }

                            @Override
                            public void onSurfaceSizeChanged(String callbackSurfaceId,
                                                             float width,
                                                             float height) {
                                notifySurfaceSizeChanged(callbackSurfaceId, width, height);
                            }
                        })
        );

        surfaces.put(surfaceId, surface);

        surface.setRawProtocolContent(rawProtocolContent);
        surface.setAnimationEnabled(animated);

        // Notify listeners
        notifyListenersOnCreate(surface);

        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "✓ Surface created: " + surfaceId);
        }
        return surface;
    }

    /**
     * Returns a Surface
     *
     * @param surfaceId Unique Surface identifier
     * @return Surface instance, or null if not found
     * @hide
     */
    public Surface getSurface(String surfaceId) {
        return surfaces.get(surfaceId);
    }

    /**
     * Destroys a Surface
     *
     * @param surfaceId Unique Surface identifier
     * @hide
     */
    void destroySurface(String surfaceId) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "destroySurface: surfaceId=" + surfaceId);
        }

        Surface surface = surfaces.remove(surfaceId);
        if (surface != null) {
            surface.destroy();

            // Notify listeners (each listener is independently guarded; a single exception
            // does not affect the remaining listeners)
            for (ISurfaceManagerListener listener : listeners) {
                try {
                    listener.onDeleteSurface(surface);
                } catch (Exception e) {
                    AGenUILogger.e(TAG, "destroySurface: listener threw exception", e);
                }
            }
        }
    }

    /**
     * Clears all Surfaces
     */
    private void clearAll() {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "clearAll: clearing " + surfaces.size() + " surfaces");
        }

        List<String> surfaceIds = new ArrayList<>(surfaces.keySet());
        for (String surfaceId : surfaceIds) {
            destroySurface(surfaceId);
        }
    }

    /**
     * Notifies all listeners that a Surface has been created
     */
    private void notifyListenersOnCreate(Surface surface) {
        for (ISurfaceManagerListener listener : listeners) {
            try {
                listener.onCreateSurface(surface);
            } catch (Exception e) {
                AGenUILogger.e(TAG, "notifyListenersOnCreate: listener threw exception", e);
            }
        }
    }

    /**
     * Notifies all listeners of an Action event.
     * <p>
     * Called by NativeEventBridge when it receives an Action event routed from the C++ layer.
     *
     * @param event Event content as a JSON string
     */
    void notifyActionEvent(String event) {
        for (ISurfaceManagerListener listener : listeners) {
            try {
                listener.onReceiveActionEvent(event);
            } catch (Exception e) {
                AGenUILogger.e(TAG, "notifyActionEvent: listener threw exception", e);
            }
        }
    }

    void notifyRootComponentUpdate(Surface surface, Map<String, String> props) {
        for (ISurfaceManagerListener listener : listeners) {
            try {
                listener.onRootComponentUpdate(surface, props);
            } catch (Exception e) {
                AGenUILogger.e(TAG, "notifyRootComponentUpdate: listener threw exception", e);
            }
        }
    }

    void notifyError(int code, String message, String surfaceId) {
        Surface surface = getSurface(surfaceId);
        for (ISurfaceManagerListener listener : listeners) {
            try {
                listener.onError(surface, code, message);
            } catch (Exception e) {
                AGenUILogger.e(TAG, "notifyError: listener threw exception", e);
            }
        }
    }

    void notifyBlankCheckResult(Surface surface, boolean isBlank) {
        for (ISurfaceManagerListener listener : listeners) {
            try {
                listener.onBlankCheckResult(surface, isBlank);
            } catch (Exception e) {
                AGenUILogger.e(TAG, "notifyBlankCheckResult: listener threw exception", e);
            }
        }
    }

    /**
     * Bridges one async component render-finish notification back into native Yoga.
     */
    void notifyRenderFinish(String surfaceId, String componentId, String type,
                            float width, float height, int selectedIndex) {
        try {
            nativeNotifyRenderFinish(
                    instanceId,
                    surfaceId,
                    componentId,
                    type,
                    width,
                    height,
                    selectedIndex);
        } catch (Exception e) {
            AGenUILogger.e(TAG, "Failed to notifyRenderFinish", e);
        }
    }

    /**
     * Reports the latest stable surface size to native so Yoga can use it as the canvas width.
     */
    void notifySurfaceSizeChanged(String surfaceId, float width, float height) {
        try {
            nativeNotifySurfaceSizeChanged(instanceId, surfaceId, width, height);
        } catch (Exception e) {
            AGenUILogger.e(TAG, "Failed to notifySurfaceSizeChanged", e);
        }
    }

    /**
     * Surface-size pull entry point invoked by the C++ engine via JNI reflection
     * (see {@code core/src/jni/jni_surface_size_provider_bridge.cpp}). Walks the
     * registered listeners in registration order and returns the first non-null
     * {@link SurfaceSize}; {@code null} signals "no listener could measure yet"
     * and is converted to {@code {0, 0}} by the C++ bridge.
     *
     * <p><b>⚠ THREADING WARNING — runs on the engine WORKER THREAD, not the UI
     * thread.</b> See {@link ISurfaceManagerListener#surfaceSize(String)} for the
     * full contract.
     *
     * <p><b>Concurrency note on {@link #listeners}</b>: the field is a
     * {@link CopyOnWriteArrayList}, so this enhanced for-loop walks an immutable
     * snapshot taken at iterator creation. Concurrent {@link #addListener} /
     * {@link #removeListener} calls from the UI thread mutate the underlying
     * array atomically (copy-on-write) and never affect the in-flight snapshot
     * — no {@code ConcurrentModificationException}, no torn reads. The trade-off
     * is the well-known one: a listener added during this iteration won't be
     * consulted until the next pull, and a removed listener may still be
     * consulted exactly once after removal returns. Both are acceptable for a
     * size-pull whose return value is treated as a best-effort snapshot anyway.
     * Each listener is independently guarded so a single exception does not
     * affect the remaining listeners.
     *
     * <p>Method name and signature are part of the JNI binary contract — do not
     * rename or change types without updating the C++ bridge.
     *
     * @param surfaceId Surface identifier passed through from the engine.
     * @return The first non-null size reported by any listener, or {@code null}.
     */
    @Keep
    @Override
    @WorkerThread
    @Nullable
    public SurfaceSize getSurfaceSize(@NonNull String surfaceId) {
        for (ISurfaceManagerListener listener : listeners) {
            try {
                SurfaceSize size = listener.surfaceSize(surfaceId);
                if (size != null) {
                    return size;
                }
            } catch (Throwable t) {
                AGenUILogger.e(TAG, "getSurfaceSize: listener threw", t);
            }
        }
        return null;
    }


    private static native void nativeAddEventListener(int instanceId, IAGenUIMessageListener listener);
    private static native void nativeRemoveEventListener(int instanceId, IAGenUIMessageListener listener);

    private static native void nativeSubmitUIAction(int instanceId, String surfaceId, String sourceComponentId, String contextJson);
    private static native void nativeSubmitUIDataModel(int instanceId, String surfaceId, String componentId, String change);

    private static native void nativeBeginTextStream(int instanceId);
    private static native void nativeReceiveTextChunk(int instanceId, String content);
    private static native void nativeEndTextStream(int instanceId);
    private static native void nativeNotifyRenderFinish(int instanceId,
                                                        String surfaceId,
                                                        String componentId,
                                                        String type,
                                                        float width,
                                                        float height,
                                                        int selectedIndex);
    private static native void nativeNotifySurfaceSizeChanged(int instanceId, String surfaceId, float width, float height);

    private static native void nativeInvalidateFunctionCallValues(int engineId);

    // Surface size pull-channel: register / detach the C++ bridge that proxies
    // ISurfaceSizeProvider calls back into this Java SurfaceManager. The bridge
    // is owned on the C++ side keyed by instanceId; the Java object is held via
    // a global ref while registered, so callers must call clear before letting
    // the SurfaceManager become unreachable.
    private static native void nativeSetSurfaceSizeProvider(int instanceId, ISurfaceSizeProviderHost host);
    private static native void nativeClearSurfaceSizeProvider(int instanceId);

}
