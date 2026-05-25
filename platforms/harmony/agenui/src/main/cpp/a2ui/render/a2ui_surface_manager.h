#pragma once

#include <string>
#include <vector>
#include <map>
#include <napi/native_api.h>
#include <arkui/native_node.h>
#include "factory/a2ui_component_registry.h"
#include "a2ui_surface_listener.h"
#include "a2ui/a2ui_component_render_observable.h"
#include "a2ui/a2ui_surface_layout_observable.h"
#include "agenui_surface_manager_interface.h"

namespace a2ui {

class A2UISurface;

/**
 * Surface manager
 * Fully aligned with the cross-platform SurfaceManager interface.
 *
 * Responsibilities:
 * 1. Manage the lifecycle of all surfaces (create, get, destroy)
 * 2. Create an independent ComponentRegistry for each surface by copying factories from the global registry
 * 3. Manage the ISurfaceListener list and dispatch surface/component lifecycle events
 * 4. Forward Harmony-internal observable events (from C++ render-layer components such as
 *    Tabs/Video/Image) to the cross-platform agenui::ISurfaceManager via onRenderFinish /
 *    onSurfaceSizeChanged. This is the only listener registered on the internal observables;
 *    other platforms (iOS/Android) call ISurfaceManager directly without an observable layer.
 */
class A2UISurfaceManager : public agenui::ComponentRenderListener,
                           public agenui::SurfaceLayoutListener {
public:
    /**
     * Constructor
     * @param globalRegistry Global component registry containing all registered factories
     */
    explicit A2UISurfaceManager(ComponentRegistry* globalRegistry);
    ~A2UISurfaceManager() override;

    /**
     * Bind the cross-platform ISurfaceManager that receives forwarded callbacks.
     *
     * Must be called once after construction (before any render-finish / surface-size events
     * are fired) and again with nullptr before the cross-platform SurfaceManager is destroyed,
     * to break the back-reference safely.
     */
    void setCoreSurfaceManager(agenui::ISurfaceManager* coreSurfaceManager);

    // agenui::ComponentRenderListener
    void onRenderFinish(const agenui::ComponentRenderInfo& info) override;
    // agenui::SurfaceLayoutListener
    void onSurfaceSizeChanged(const agenui::SurfaceLayoutInfo& info) override;

    /**
     * Create a surface
     * Matches the cross-platform SurfaceManager.createSurfaceWithoutContainer()
     *
     * Internal flow:
     * 1. Create an independent ComponentRegistry by copying factory mappings from the global registry
     * 2. Create A2UISurface (state = CREATED)
     * 3. Notify ISurfaceListener.onSurfaceCreated
     *
     * @param surfaceId Surface ID
     * @param animated Whether components on this surface may play animations (from CreateSurfaceMessage)
     * @return Newly created surface pointer, or the existing one if it already exists
     */
    A2UISurface* createSurface(const std::string& surfaceId, bool animated = true);

    /**
     * Get the surface
     * @return Surface pointer, or nullptr if it does not exist
     */
    A2UISurface* getSurface(const std::string& surfaceId) const;

    /**
     * Destroy the surface
     * Matches the cross-platform SurfaceManager.destroySurface()
     *
     * Internal flow:
     * 1. Surface.destroy(): recursively destroy the component tree
     * 2. Remove it from the surfaces map
     * 3. Delete the surface and its independent registry
     * 4. Notify ISurfaceListener.onSurfaceDestroyed
     */
    void destroySurface(const std::string& surfaceId);

    /**
     * Clear all surfaces
     */
    void clearAll();

    /**
     * Get the number of managed surfaces
     */
    int getSurfaceCount() const;

    /**
     * Detach all surface root nodes from contentHandle and restore the container to an empty state
     * Do not destroy the surface or contentHandle so sendMockData can render into the same container again later
     */
    void unmountAllRootNodes();

    /**
     * Bind the surface to NodeContent
     * @param surfaceId Surface ID
     * @param env NAPI environment
     * @param nodeContent NodeContent object
     * @return Whether binding succeeded
     */
    bool bindSurface(const std::string& surfaceId, napi_env env, napi_value nodeContent);

    /**
     * Unbind the surface
     * @param surfaceId Surface ID
     * @return Whether unbinding succeeded
     */
    bool unbindSurface(const std::string& surfaceId);

    /**
     * Get the component render completion observer
     */
    agenui::IComponentRenderObservable* getComponentRenderObservable() { return &componentRenderObservable_; }

    /**
     * Get the surface layout observer
     */
    agenui::ISurfaceLayoutObservable* getSurfaceLayoutObservable() { return &surfaceLayoutObservable_; }

private:
    ComponentRegistry* globalRegistry_;                    // Global registry (non-owning)
    std::map<std::string, A2UISurface*> surfaces_;                 // surfaceId -> Surface
    std::map<std::string, ComponentRegistry*> registries_; // surfaceId -> independent registry (owning)
    std::map<std::string, ArkUI_NodeContentHandle> surfaceContentHandles_; // surfaceId -> contentHandle

    agenui::A2UIComponentRenderObservable componentRenderObservable_;  // Component render completion observer (instance)
    agenui::A2UISurfaceLayoutObservable surfaceLayoutObservable_;       // Surface layout observer (instance)

    // Forwarding target. Owned by AGenUIEngine; lifetime managed by napi layer.
    agenui::ISurfaceManager* coreSurfaceManager_ = nullptr;
};

} // namespace a2ui
