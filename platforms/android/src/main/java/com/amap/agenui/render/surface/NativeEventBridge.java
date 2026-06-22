package com.amap.agenui.render.surface;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;

import com.amap.agenui.IAGenUIMessageListener;
import com.amap.agenui.render.component.A2UIComponent;
import com.amap.agenui.render.component.ComponentRegistry;
import com.amap.agenui.render.utils.AGenUILogger;
import com.google.gson.Gson;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/**
 * JNI Bridge - receives events from the C++ EventDispatcher
 * <p>
 * Responsibilities:
 * 1. Receives createSurface, incremental add/update/remove, and destroySurface events from C++
 * 2. Forwards events to SurfaceManager for processing
 * 3. Parses JSON data and updates components
 *
 */
public class NativeEventBridge implements IAGenUIMessageListener {

    private static final String TAG = "NativeEventBridge";

    private final SurfaceManager surfaceManager;
    private final int instanceId;
    private final Gson gson = new Gson();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    public NativeEventBridge(@NonNull SurfaceManager surfaceManager, int instanceId) {
        this.surfaceManager = surfaceManager;
        this.instanceId = instanceId;
    }

    @Override
    public void onCreateSurface(String surfaceId, String catalogId, Map<String, String> theme, boolean sendDataModel, boolean animated, String rawProtocolContent) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "onCreateSurface: surfaceId=" + surfaceId + ", catalogId=" + catalogId);
        }

        mainHandler.post(() -> {
            Surface surface = surfaceManager.createSurface(surfaceId, rawProtocolContent, animated);
            if (surface == null) {
                AGenUILogger.e(TAG, "Surface created failed");
            }
        });
    }

    @Override
    public void onComponentsUpdate(String surfaceId, String[] components) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "onComponentsUpdate: surfaceId=" + surfaceId + ", count=" + (components != null ? components.length : 0));
        }
        mainHandler.post(() -> applyIncrementalComponentUpdates(surfaceId, components));
    }

    @Override
    public void onComponentsAdd(String surfaceId, String[] parentIds, String[] components) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "onComponentsAdd: surfaceId=" + surfaceId
                    + ", parents=" + (parentIds != null ? parentIds.length : 0)
                    + ", components=" + (components != null ? components.length : 0) + ", components=" + (components != null ? Arrays.toString(components) : "null"));
        }
        mainHandler.post(() -> applyIncrementalComponentAdds(surfaceId, parentIds, components));
    }

    @Override
    public void onComponentsRemove(String surfaceId, String[] parentIds, String[] componentIds) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "onComponentsRemove: surfaceId=" + surfaceId
                    + ", parents=" + (parentIds != null ? parentIds.length : 0)
                    + ", componentIds=" + (componentIds != null ? componentIds.length : 0));
        }
        mainHandler.post(() -> applyIncrementalComponentRemovals(surfaceId, parentIds, componentIds));
    }

    @Override
    public void onDeleteSurface(String surfaceId) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "onDeleteSurface: surfaceId=" + surfaceId);
        }
        mainHandler.post(() -> surfaceManager.destroySurface(surfaceId));
    }

    @Override
    public void onActionEventRouted(String content) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "onActionEventRouted: content=" + content);
        }
        mainHandler.post(() -> surfaceManager.notifyActionEvent(content));
    }

    @Override
    public void onError(int code, String surfaceId, String message) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "onError: code=" + code + ", surfaceId=" + surfaceId + ", message=" + message);
        }
        // Map to the SDKInternal type defined in the design doc; message is reported as the reason field
        mainHandler.post(() -> surfaceManager.notifyError(code, message, surfaceId));
    }

    /**
     * Processes the creation and addition of a single component.
     * Shared method reused by incremental add handlers.
     *
     * @param surface          Surface instance
     * @param componentData    Component data (contains id, type, properties, etc.)
     * @param explicitParentId Explicitly specified parent component ID (optional; if null, obtained
     *                         from componentData)
     * @return true if the component was successfully added; false otherwise
     */
    private boolean processComponent(Surface surface, Map<String, Object> componentData, String explicitParentId) {
        try {
            String componentId = extractComponentId(componentData);
            String componentType = extractComponentType(componentData);

            if (componentId == null || componentType == null) {
                AGenUILogger.e(TAG, "Component missing id or type, surfaceId: " + surface.getSurfaceId() + ", explicitParentId: " + explicitParentId);
                return false;
            }

            // If component already exists, update properties only
            A2UIComponent existingComponent = surface.getComponent(componentId);
            if (existingComponent != null) {
                AGenUILogger.e(TAG, "Component already exists, updating: " + componentId);

                Map<String, Object> updateProperties = extractComponentProperties(componentData);
                if (updateProperties != null && !updateProperties.isEmpty()) {
                    existingComponent.updateProperties(updateProperties);
                }
                return true;
            }

            // Determine parent component ID
            String parentId = explicitParentId;
            if (parentId == null) {
                Object parentObj = componentData.get("parent");
                if (parentObj instanceof String) {
                    parentId = (String) parentObj;
                }
            }

            Map<String, Object> properties = extractComponentProperties(componentData);

            // Create component
            Context componentContext = surfaceManager.getContext();
            if (componentContext == null) {
                AGenUILogger.e(TAG, "Cannot create component: Activity context is null");
                return false;
            }
            A2UIComponent component = ComponentRegistry.createComponent(componentContext, componentType, componentId, properties);
            if (component == null) {
                AGenUILogger.e(TAG, "Failed to create component: type=" + componentType + ", id=" + componentId);
                return false;
            }

            surface.addComponent(parentId, component);

            // Update component properties
            if (properties != null && !properties.isEmpty()) {
                component.updateProperties(properties);
            }

            return true;

        } catch (Exception e) {
            AGenUILogger.e(TAG, "Error processing component", e);
            return false;
        }
    }

    private void applyIncrementalComponentUpdates(String surfaceId, String[] components) {
        Surface surface = surfaceManager.getSurface(surfaceId);
        if (surface == null) {
            AGenUILogger.e(TAG, "onComponentsUpdate: Surface not found: " + surfaceId);
            return;
        }

        surface.beginLayoutTransaction();
        boolean rootUpdated = false;
        try {
            if (components == null) {
                return;
            }
            for (String componentJson : components) {
                Map<String, Object> componentData = parseComponentData(componentJson);
                if (componentData == null) {
                    continue;
                }

                String componentId = extractComponentId(componentData);
                if (componentId == null) {
                    AGenUILogger.e(TAG, "onComponentsUpdate: component id missing");
                    continue;
                }

                if ("root".equals(componentId)) {
                    rootUpdated = true;
                }

                A2UIComponent existingComponent = surface.getComponent(componentId);
                if (existingComponent == null) {
                    AGenUILogger.e(TAG, "onComponentsUpdate: component not found, skip id=" + componentId);
                    continue;
                }

                Map<String, Object> updateProperties = extractComponentProperties(componentData);
                if (updateProperties == null || updateProperties.isEmpty()) {
                    if (AGenUILogger.isLoggingEnabled()) {
                        AGenUILogger.d(TAG, "onComponentsUpdate: no properties to update, id=" + componentId);
                    }
                    continue;
                }

                surface.updateComponent(componentId, updateProperties);
            }
        } finally {
            surface.endLayoutTransaction();
        }
        if (rootUpdated) {
            surfaceManager.notifyRootComponentUpdate(surface, extractRootComponentPropsAsStrings(surface));
        }
    }

    private void applyIncrementalComponentAdds(String surfaceId, String[] parentIds, String[] components) {
        Surface surface = surfaceManager.getSurface(surfaceId);
        if (surface == null) {
            AGenUILogger.e(TAG, "onComponentsAdd: Surface not found: " + surfaceId);
            return;
        }

        surface.beginLayoutTransaction();
        boolean rootAdded = false;
        try {
            if (components == null) {
                return;
            }

            for (int i = 0; i < components.length; i++) {
                Map<String, Object> componentData = parseComponentData(components[i]);
                if (componentData == null) {
                    continue;
                }

                String componentId = extractComponentId(componentData);
                if ("root".equals(componentId)) {
                    rootAdded = true;
                }

                String parentId = parentIds != null && i < parentIds.length ? normalizeParentId(parentIds[i]) : null;
                processComponent(surface, componentData, parentId);
            }
        } finally {
            surface.endLayoutTransaction();
        }
        if (rootAdded) {
            surfaceManager.notifyRootComponentUpdate(surface, extractRootComponentPropsAsStrings(surface));
        }
    }

    private void applyIncrementalComponentRemovals(String surfaceId, String[] parentIds, String[] componentIds) {
        Surface surface = surfaceManager.getSurface(surfaceId);
        if (surface == null) {
            AGenUILogger.e(TAG, "onComponentsRemove: Surface not found: " + surfaceId);
            return;
        }

        surface.beginLayoutTransaction();
        try {
            if (componentIds == null) {
                return;
            }

            for (int i = 0; i < componentIds.length; i++) {
                String componentId = componentIds[i];
                String parentId = parentIds != null && i < parentIds.length ? normalizeParentId(parentIds[i]) : null;
                if (AGenUILogger.isLoggingEnabled()) {
                    AGenUILogger.d(TAG, "onComponentsRemove: removing componentId=" + componentId + ", parentId=" + parentId);
                }
                if (componentId == null || componentId.isEmpty()) {
                    continue;
                }
                surface.removeComponent(componentId);
            }
        } finally {
            surface.endLayoutTransaction();
        }
    }

    private Map<String, Object> parseComponentData(String componentJson) {
        if (componentJson == null || componentJson.isEmpty()) {
            AGenUILogger.w(TAG, "parseComponentData: empty component json");
            return null;
        }

        try {
            return gson.fromJson(componentJson, Map.class);
        } catch (Exception e) {
            AGenUILogger.e(TAG, "parseComponentData: failed to parse component json", e);
            return null;
        }
    }

    private String extractComponentId(Map<String, Object> componentData) {
        Object componentId = componentData == null ? null : componentData.get("id");
        return componentId == null ? null : String.valueOf(componentId);
    }

    private String extractComponentType(Map<String, Object> componentData) {
        if (componentData == null) {
            return null;
        }
        Object componentType = componentData.get("type");
        if (componentType == null) {
            componentType = componentData.get("component");
        }
        return componentType == null ? null : String.valueOf(componentType);
    }

    @SuppressWarnings("unchecked")
    private Map<String, Object> extractComponentProperties(Map<String, Object> componentData) {
        if (componentData == null) {
            return null;
        }

        Map<String, Object> properties = (Map<String, Object>) componentData.get("properties");
        if (properties == null) {
            properties = new HashMap<>(componentData);
            properties.remove("id");
            properties.remove("type");
            properties.remove("component");
            properties.remove("parent");
        }
        return properties;
    }

    private String normalizeParentId(String parentId) {
        return (parentId == null || parentId.isEmpty()) ? null : parentId;
    }

    /**
     * Extracts the root component's properties as a String map.
     * Returns null if there is no root component or it has no properties.
     */
    @SuppressWarnings("unchecked")
    private Map<String, String> extractRootComponentPropsAsStrings(Surface surface) {
        A2UIComponent root = surface.getRootComponent();
        if (root == null) return null;
        Map<String, Object> props = root.getProperties();
        if (props == null || props.isEmpty()) return null;
        Map<String, String> result = new HashMap<>();
        for (Map.Entry<String, Object> entry : props.entrySet()) {
            if (entry.getValue() != null) {
                result.put(entry.getKey(), String.valueOf(entry.getValue()));
            }
        }
        return result;
    }

}
