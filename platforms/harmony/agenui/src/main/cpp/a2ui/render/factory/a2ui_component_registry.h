#pragma once

#include <map>
#include <mutex>
#include <string>
#include <nlohmann/json.hpp>

#include "a2ui_component_descriptors.h"

namespace a2ui {

class A2UIComponent;

/**
 * Global component registry — the single source of truth for which component
 * types can be rendered, aligned with the cross-platform ComponentRegistry.
 *
 * Entries come from two registration paths and live in one table:
 * 1. Built-in components, seeded once from the static descriptor table
 *    (a2ui_component_descriptors.h) on first access.
 * 2. Host custom components, appended at runtime through the NAPI
 *    registerComponent/unregisterComponent exports. These are always hybrid:
 *    creation is delegated to the ArkTS side via A2UIHybridFactory.
 *
 * Component factories are stateless and all Surfaces share this registry.
 * Component instances are owned by their Surface, not by this registry.
 */
class ComponentRegistry {
public:
    /**
     * Process-wide registry. Seeds the built-in components on first access.
     */
    static ComponentRegistry& global();

    /**
     * Register (or replace) a component type.
     * @param type Component type name such as "Text" or "AmapText"
     * @param descriptor Entry describing how to create/measure the type
     */
    void registerComponent(const std::string& type, const ComponentDescriptor& descriptor);

    /**
     * Remove a previously registered component type.
     */
    void unregisterComponent(const std::string& type);

    /**
     * Create a component through its registered entry.
     *
     * @param surfaceId Surface ID used to identify the owning surface
     * @param instanceId Owning SurfaceManager's instance ID. Hybrid components
     *        forward it to ArkTS so the ArkTS-side view registry can key entries
     *        by (instanceId, componentId) — componentId alone collides when two
     *        surfaces (even across SurfaceManagers) use the same DSL component id.
     * @param type Component type
     * @param id Component ID
     * @param properties Component properties
     * @return Newly created component instance (caller owns the lifetime),
     *         or nullptr when the type is unknown or creation fails
     */
    A2UIComponent* createComponent(const std::string& surfaceId,
                                   int instanceId,
                                   const std::string& type,
                                   const std::string& id,
                                   const nlohmann::json& properties);

    /**
     * Return the number of registered component types.
     */
    int getRegisteredComponentCount() const;

private:
    ComponentRegistry() = default;

    // Seed built-in components from the static descriptor table.
    void registerBuiltInComponents();

    // type -> descriptor. Written by both registration paths; read on creation.
    std::map<std::string, ComponentDescriptor> components_;
    mutable std::mutex mutex_;
};

} // namespace a2ui
