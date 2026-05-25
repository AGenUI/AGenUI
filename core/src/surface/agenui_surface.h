#pragma once

// Standard library headers
#include <map>
#include <memory>
#include <string>
#include <vector>

// Third-party library headers
#include "nlohmann/json.hpp"

// Project headers
#include "agenui_batch_guard.h"
#include "agenui_dispatcher_types.h"
#include "agenui_render_info_types.h"
#include "agenui_isurface_context.h"
#include "component_manager/agenui_icomponent_manager.h"
#include "virtual_dom/agenui_virtual_dom_observer.h"
#include "virtual_dom/agenui_ivirtual_dom.h"
#include "datamodel/agenui_idata_model.h"
#include "agenui_errorcode_define.h"

namespace agenui {

class SurfaceManager;

// Represents a UI surface that manages component tree and data model
class Surface : public IVirtualDOMObserver, public ISurfaceContext {
public:
    // Lifecycle
    Surface(const std::string& surfaceId, const std::string& theme, SurfaceManager* surfaceManager);
    ~Surface();
    
    // Getters
    int getInstanceId() const override;
    std::string getSurfaceId() const override;
    IDataModel* getDataModel() const override;
    
    // Component size update
    void updateComponentSize(const ComponentRenderInfo& info);

    // Tabs selected index update
    void updateTabsSelectedIndex(const ComponentRenderInfo& info);
    
    // Surface size update
    void updateSurfaceSize(const SurfaceLayoutInfo& info);
    
    // Component management
    AGenUIExeCode updateComponents(const nlohmann::json& componentsData);
    void onNodeUpdate(const std::string& componentId, const std::string& nodeJson) override;
    void onNodeAdded(const std::string& parentId, const std::string& nodeJson) override;
    void onNodeRemoved(const std::string& parentId, const std::string& id) override;
    
    // Data model management
    void updateDataModel(const nlohmann::json& dataModelData);
    void appendDataModel(const nlohmann::json& dataModelData);
    void syncUIToData(const std::string& componentId, const std::string& changingData);
    
    // User interaction
    void handleUserAction(const std::string& sourceComponentId);
    
    // FunctionCall value invalidation
    void invalidateFunctionCallValues();

    // Batch guard access — callers use BatchScope(surface->batchGuard())
    // to open a cascading batch window (dispatch → VDOM → CM).
    BatchGuard* batchGuard() { return &_dispatchGuard; }

private:
    void flushPendingDispatches();

    std::string _surfaceId;
    std::string _theme;
    IDataModel* _dataModel;
    IVirtualDOM* _virtualDom;
    IComponentManager* _componentManager;
    SurfaceManager* _surfaceManager = nullptr;
    bool _isDestroying = false;

    // ---- Batched dispatch bookkeeping ----
    // _dispatchGuard: defers platform dispatch calls (onNodeUpdate/Add/Remove)
    //                 until the outermost batch window closes, so the platform
    //                 receives all changes in a single burst per operation type.
    BatchGuard _dispatchGuard;
    std::vector<ComponentsUpdateMessage> _pendingUpdates;
    std::vector<ComponentsAddMessage> _pendingAdds;
    std::vector<ComponentsRemoveMessage> _pendingRemoves;
};

}  // namespace agenui
