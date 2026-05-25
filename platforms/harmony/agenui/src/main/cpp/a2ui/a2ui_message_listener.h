#ifndef A2UI_MESSAGE_LISTENER_H
#define A2UI_MESSAGE_LISTENER_H

#include "agenui_message_listener.h"
#include "render/factory/a2ui_component_registry.h"
#include "render/a2ui_surface_manager.h"
#include <map>
#include <memory>
#include <vector>
#include <mutex>
#include <functional>
#include <napi/native_api.h>

namespace agenui {

// ==================== Main-thread Dispatch ====================

/**
 * @brief Task wrapper for NAPI work that must run on the main thread.
 */
using MainThreadTask = std::function<void(napi_env)>;

/**
 * @brief Multi-instance A2UI message listener.
 * @remark Each ISurfaceManager owns one A2UIMessageListener instance used to
 *         receive C++ engine events and render them into the A2UI container.
 *
 * Data flow:
 * - onCreateSurface: create the surface with its own registry and component tree
 * - onDeleteSurface: destroy the surface and recursively tear down the component tree
 */
// Inherits enable_shared_from_this so worker-thread events can capture a
// weak_ptr and safely no-op when the listener has already been destroyed
// on the main thread (see postToMainThread call sites below).
class A2UIMessageListener : public IAGenUIMessageListener,
                            public std::enable_shared_from_this<A2UIMessageListener> {
public:
    /**
     * @brief Constructor
     * @param instanceId Associated ISurfaceManager instance ID
     */
    explicit A2UIMessageListener(int instanceId);
    ~A2UIMessageListener();
    
    // Non-copyable
    A2UIMessageListener(const A2UIMessageListener&) = delete;
    A2UIMessageListener& operator=(const A2UIMessageListener&) = delete;
    
    /**
     * @brief Set the threadsafe function.
     * @param tsfn Global main-thread threadsafe function
     */
    void setTsfn(napi_threadsafe_function tsfn);
    
    /**
     * @brief Return the associated instanceId.
     */
    int getInstanceId() const { return instanceId_; }

    // ==================== IAGenUIMessageListener Implementation ====================

    void onCreateSurface(const CreateSurfaceMessage& msg) override;
    void onDeleteSurface(const DeleteSurfaceMessage& msg) override;
    void onActionEventRouted(const std::string &content) override;
    void onComponentsUpdate(const std::string& surfaceId, const std::vector<ComponentsUpdateMessage>& msg) override;
    void onComponentsAdd(const std::string& surfaceId, const std::vector<ComponentsAddMessage>& msg) override;
    void onComponentsRemove(const std::string& surfaceId, const std::vector<ComponentsRemoveMessage>& msg) override;
    void onError(const ErrorMessage& msg) override;

    // ==================== Render-layer Access ====================

    /**
     * @brief Return the render-layer surface manager.
     */
    a2ui::A2UISurfaceManager* getSurfaceManager();

    /**
     * @brief Deprecated contentHandle-ready callback.
     * @deprecated Use bindSurface instead.
     */
    void onContentHandleReady();

    // ==================== ArkTS Listener Management ====================

    /**
     * @brief Register an A2UI surface listener.
     * @param listener NAPI listener object
     */
    void registerListener(napi_value listener);

    /**
     * @brief Unregister the A2UI surface listener
     * @param listener NAPI listener object
     */
    void unregisterListener(napi_value listener);

    // ==================== surfaceId -> instanceId Mapping ====================

    /**
     * @brief Look up the instanceId associated with a surfaceId.
     * @param surfaceId Unique surface identifier
     * @return Matching instanceId, or 0 when not found
     */
    static int findInstanceIdBySurfaceId(const std::string& surfaceId);

private:
    /**
     * @brief Initialize the global component registry.
     */
    void initGlobalRegistry();

    /**
     * @brief Register a surfaceId -> instanceId mapping.
     */
    void registerSurfaceMapping(const std::string& surfaceId);

    /**
     * @brief Unregister a surfaceId -> instanceId mapping.
     */
    void unregisterSurfaceMapping(const std::string& surfaceId);

    /**
     * @brief Static helper used to unregister a surfaceId -> instanceId mapping.
     */
    static void unregisterSurfaceMappingStatic(const std::string& surfaceId);

    int instanceId_;                                  // Associated ISurfaceManager instance ID
    a2ui::ComponentRegistry globalRegistry_;        // Global registry that owns all factories
    a2ui::A2UISurfaceManager* surfaceManager_;      // Render-layer surface manager
    std::vector<napi_ref> listeners_;               // Registered ArkTS listeners

    // Main-thread threadsafe function owned by napi_init.cpp Init/Stop.
    napi_threadsafe_function tsfn_ = nullptr;

    /**
     * @brief Dispatch a task to the main thread.
     * @param task Operation that must run on the main thread
     */
    void postToMainThread(MainThreadTask task);

    // Shared surfaceId -> instanceId mapping across all instances.
    static std::map<std::string, int> s_surfaceIdToInstanceId_;
    static std::mutex s_mappingMutex_;
};

} // namespace agenui

#endif // A2UI_MESSAGE_LISTENER_H
