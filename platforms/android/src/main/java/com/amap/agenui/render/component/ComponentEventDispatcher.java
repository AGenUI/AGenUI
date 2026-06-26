package com.amap.agenui.render.component;

import androidx.annotation.RestrictTo;

/**
 * Bridge interface between components and the Native layer.
 * <p>
 * Responsible for submitting component Action events and synchronizing UI state data to the
 * Native layer.
 * <p>
 * Implemented by SurfaceManager and injected via Surface → A2UIComponent.
 * <p>
 * <b>For internal SDK use only; not exposed externally.</b>
 *
 */
@RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
public interface ComponentEventDispatcher {

    /**
     * Submits a UI Action to the Native layer
     *
     * @param surfaceId   Surface ID
     * @param componentId ID of the component that triggered the action
     * @param contextJson Context parameters (JSON format)
     */
    void submitUIAction(String surfaceId, String componentId, String contextJson);

    /**
     * Submits a UI data model change to the Native layer
     *
     * @param surfaceId   Surface ID
     * @param componentId Component ID
     * @param changeData  Changed content (JSON format)
     */
    void submitUIDataModel(String surfaceId, String componentId, String changeData);

    /**
     * Reports the result of a blank-screen check to the manager layer.
     * Called for both pass ({@code isBlank = false}) and fail ({@code isBlank = true}) outcomes.
     *
     * @param surfaceId Surface ID
     * @param isBlank   {@code true} if the component count is below threshold; {@code false} if the check passed
     */
    void onBlankCheckResult(String surfaceId, boolean isBlank);

    /**
     * Notifies a component appeared event.
     * <p>
     * Generic appeared channel — any component type (List, Carousel, Tabs, etc.)
     * can call this to report that itself or its children have appeared on screen.
     *
     * @param surfaceId         Surface ID
     * @param parentComponentId ID of the component that detected the appearance
     * @param parentType        Component type name of the detector ("List" / "Carousel" / "Tabs" ...)
     * @param properties        Appeared properties as a key-value map
     */
    void notifyAppearedEvent(String surfaceId, String parentComponentId, String parentType, java.util.Map<String, Object> properties);
}
