#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace a2ui {

class A2UISurface;

/**
 * Surface lifecycle listener interface.
 * Surface lifecycle listener.
 *
 * Used to notify external observers about surface and component lifecycle events.
 */
class ISurfaceListener {
public:
    virtual ~ISurfaceListener() = default;

    /**
     * Called after surface creation.
     * Called after the surface is created.
     *
     * @param surfaceId Surface ID
     * @param surface Surface instance pointer
     */
    virtual void onSurfaceCreated(const std::string& surfaceId, A2UISurface* surface) = 0;

    /**
     * Called after surface destruction.
     * Called after the surface is destroyed.
     *
     * @param surfaceId Surface ID
     */
    virtual void onSurfaceDestroyed(const std::string& surfaceId) = 0;

    /**
     * Called when a component is added to the surface.
     * Called when a component is added to the surface.
     *
     * @param surfaceId Surface ID
     * @param componentId Component ID
     */
    virtual void onComponentAdded(const std::string& surfaceId, const std::string& componentId) = 0;

    /**
     * Called when a component is removed from the surface.
     * Called when a component is removed from the surface.
     *
     * @param surfaceId Surface ID
     * @param componentId Component ID
     */
    virtual void onComponentRemoved(const std::string& surfaceId, const std::string& componentId) = 0;

    /**
     * Called when a component appears in a container (e.g., List item bound to viewport).
     * Bind == Display: fired when the NodeAdapter binds a child to a viewport slot.
     *
     * @param surfaceId Surface ID
     * @param parentComponentId Container component ID (e.g., the List's id)
     * @param parentType Container type (e.g., "List")
     * @param properties Child's raw properties (JSON pass-through)
     */
    virtual void onComponentAppeared(const std::string& surfaceId,
                                     const std::string& parentComponentId,
                                     const std::string& parentType,
                                     const nlohmann::json& properties) {}
};

} // namespace a2ui
