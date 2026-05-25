package com.amap.agenui;

import androidx.annotation.Keep;
import androidx.annotation.RestrictTo;

import java.util.Map;

/**
 * AGenUI message listener interface
 * Used to receive event callbacks from the C++ layer
 *
 * <p>Note: This interface interacts with the C++ layer via JNI; method signatures must strictly
 * match the C++ layer</p>
 * <p>Corresponding C++ implementation: jni_message_listener_bridge.cpp</p>
 */
@Keep
@RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
public interface IAGenUIMessageListener {
    /**
     * Callback when a Surface is created
     *
     * @param surfaceId            Unique identifier of the surface
     * @param catalogId            Component catalog identifier
     * @param theme                Theme parameters
     * @param sendDataModel        Whether to send back the data model
     * @param animated             Whether to enable animation
     * @param rawProtocolContent   Original raw protocol content (the full JSON string that was parsed to create this surface)
     */
    void onCreateSurface(String surfaceId, String catalogId, Map<String, String> theme, boolean sendDataModel, boolean animated, String rawProtocolContent);

    /**
     * Callback when existing components are updated incrementally.
     *
     * @param surfaceId  Surface identifier
     * @param components String array of component JSON payloads
     */
    void onComponentsUpdate(String surfaceId, String[] components);

    /**
     * Callback when components are added incrementally.
     *
     * @param surfaceId  Surface identifier
     * @param parentIds  Parent component IDs aligned with {@code components}
     * @param components String array of component JSON payloads
     */
    void onComponentsAdd(String surfaceId, String[] parentIds, String[] components);

    /**
     * Callback when components are removed incrementally.
     *
     * @param surfaceId    Surface identifier
     * @param parentIds    Parent component IDs aligned with {@code componentIds}
     * @param componentIds Component IDs to remove
     */
    void onComponentsRemove(String surfaceId, String[] parentIds, String[] componentIds);

    /**
     * Callback when a Surface is deleted
     *
     * @param surfaceId Surface identifier
     */
    void onDeleteSurface(String surfaceId);

    void onActionEventRouted(String content);

    /**
     * Callback when a C++ execution error occurs
     *
     * @param code      Error code (AGenUIExeCode enum value)
     * @param surfaceId Surface identifier (empty if not yet parsed)
     * @param message   Error description (human-readable)
     */
    void onError(int code, String surfaceId, String message);
}
