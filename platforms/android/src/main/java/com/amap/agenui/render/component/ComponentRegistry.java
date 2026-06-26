package com.amap.agenui.render.component;

import android.content.Context;

import com.amap.agenui.render.measurement.IMeasurer;
import com.amap.agenui.render.measurement.MeasurementBridge;
import com.amap.agenui.render.utils.AGenUILogger;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Component registry (global static)
 * <p>
 * Responsibilities:
 * 1. Manages the registration of all component factories
 * 2. Provides a component creation interface
 * 3. Supports custom component registration
 * <p>
 * Component factories are stateless and all Surfaces share the same factory registry.
 * Built-in components are registered once via {@link #registerBuiltInComponents()};
 * custom components can be appended at any time via {@link #registerFactory(String, ComponentFactory)}.
 *
 */
public class ComponentRegistry {

    private static final String TAG = "ComponentRegistry";

    private static final Map<String, IComponentFactory> factories = new ConcurrentHashMap<>();
    private static volatile boolean initialized = false;

    private ComponentRegistry() {
    } // Prevent instantiation
    /**
     * Registers a component factory
     *
     * @param componentType Component type
     * @param factory       Component factory instance
     */
    public static void registerComponent(String componentType, IComponentFactory factory) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "registerComponent: componentType=" + componentType);
        }
        factories.put(componentType, factory);
        IMeasurer measurer = factory.getMeasurer();
        if (measurer != null) {
            MeasurementBridge.registerMeasurer(componentType, measurer);
        }
    }

    /**
     * Unregisters a component factory
     *
     * @param componentType Component type
     */
    public static void unregisterComponent(String componentType) {
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "unregisterComponent: componentType=" + componentType);
        }
        IComponentFactory removed = factories.remove(componentType);
        if (removed != null && removed.getMeasurer() != null) {
            MeasurementBridge.unregisterMeasurer(componentType);
        }
    }

    /**
     * Returns the component factory for the given type
     *
     * @param componentType Component type
     * @return Component factory instance, or null if not found
     */
    public static IComponentFactory getFactory(String componentType) {
        return factories.get(componentType);
    }

    /**
     * Creates a component
     *
     * @param context       Android Context
     * @param componentType Component type
     * @param id            Component ID
     * @param properties    Component properties
     * @return Created component instance, or null if no factory is registered
     */
    public static A2UIComponent createComponent(
            Context context,
            String componentType,
            String id,
            Map<String, Object> properties) {
        IComponentFactory factory = factories.get(componentType);
        if (factory != null) {
            A2UIComponent component = factory.createComponent(context, id, properties);
            if (component == null) {
                AGenUILogger.e(TAG, "Factory returned null for: " + componentType);
            }
            return component;
        }
        AGenUILogger.e(TAG, "No factory found for componentType: " + componentType);
        return null;
    }

    /**
     * Registers all built-in components (executed only once).
     * Includes: basic components, layout components, and interaction components.
     */
    public static synchronized void registerBuiltInComponents() {
        if (initialized) {
            AGenUILogger.d(TAG, "registerBuiltInComponents: already initialized, skip");
            return;
        }
        initialized = true;
        AGenUILogger.d(TAG, "registerBuiltInComponents");

        BuiltInComponentRegistrar.registerAll();

        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.d(TAG, "Built-in components registered: " + factories.size() + " ✅ All done");
        }
    }

    /**
     * Returns the number of registered component types
     *
     * @return Number of registered component types
     */
    public static int getRegisteredComponentCount() {
        return factories.size();
    }

}
