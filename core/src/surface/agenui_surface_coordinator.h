#pragma once

// Standard library headers
#include <map>
#include <memory>
#include <string>

// Project headers
#include "agenui_dispatcher_types.h"
#include "agenui_render_info_types.h"
#include "agenui_surface.h"
#include "agenui_errorcode_define.h"

namespace agenui {

class SurfaceManager;
class EventDispatcher;
class TemplateRegistry;

/**
 * @brief Surface coordinator (formerly AGenUIStateEngine)
 *
 * Manages all surfaces and processes messages from the service layer.
 * Exists as a member of SurfaceManager (multiple instances possible).
 */
class SurfaceCoordinator {
public:
    /**
     * @brief Constructor
     * @param owner Owning SurfaceManager (not owned)
     */
    explicit SurfaceCoordinator(SurfaceManager* owner);
    ~SurfaceCoordinator();

    // Non-copyable and non-movable
    SurfaceCoordinator(const SurfaceCoordinator&) = delete;
    SurfaceCoordinator& operator=(const SurfaceCoordinator&) = delete;

    void invalidateFunctionCallValues();

    // Surface management
    AGenUIExeCode createSurface(const std::string& jsonData);
    AGenUIExeCode deleteSurface(const std::string& jsonData);
    Surface* getSurface(const std::string& surfaceId) const;

    // Component and data model updates
    AGenUIExeCode updateComponents(const std::string& jsonData);
    AGenUIExeCode updateDataModel(const std::string& jsonData);
    AGenUIExeCode appendDataModel(const std::string& jsonData);

    // UI interaction entry points (called by SurfaceManager::submitUIAction / submitUIDataModel)
    void handleAction(const ActionMessage& msg);
    void handleSyncUIToData(const SyncUIToDataMessage& msg);

    // Platform callback handlers (dispatched by SurfaceManager to the message thread)
    void handleRenderFinish(const ComponentRenderInfo& info);
    void handleSurfaceSizeChanged(const SurfaceLayoutInfo& info);

    /**
     * @brief Dispatches an execution error event to all listeners.
     * @param code Error code (AGenUIExeCode enum value)
     * @param surfaceId Surface identifier (empty if not yet parsed from the protocol)
     */
    void reportError(AGenUIExeCode code, const std::string& surfaceId);

private:
    void initFunctionCalls();

private:
    SurfaceManager* _owner = nullptr;
    std::map<std::string, std::unique_ptr<Surface>> _surfaces;
};

}  // namespace agenui
