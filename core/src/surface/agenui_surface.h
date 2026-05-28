#pragma once

// Standard library headers
#include <map>
#include <memory>
#include <string>
#include <variant>
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

    /**
     * @brief Return the cached surface width in vp, lazily fetching from
     *        the host-supplied ISurfaceSizeProvider on first call when the
     *        cache has never been initialized.
     */
    float getSurfaceWidth() override;

    /**
     * @brief Return the cached surface height in vp. Same lazy-fetch
     *        semantics as getSurfaceWidth().
     */
    float getSurfaceHeight() override;
    
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

    /**
     * @brief Issue a one-shot bootstrap fetch of the surface size from the
     *        host-supplied ISurfaceSizeProvider on the first call. Whatever
     *        the provider returns (or absence thereof) is written verbatim
     *        and never retried; subsequent real size changes only arrive
     *        through the push channel (Surface::updateSurfaceSize).
     */
    void ensureSurfaceSizeFetched();

    std::string _surfaceId;
    std::string _theme;
    IDataModel* _dataModel;
    IVirtualDOM* _virtualDom;
    IComponentManager* _componentManager;
    SurfaceManager* _surfaceManager = nullptr;
    bool _isDestroying = false;

    // Surface size (vp). Whatever the host supplies — via either an
    // onSurfaceSizeChanged push (Surface::updateSurfaceSize) or a one-shot
    // bootstrap pull from the ISurfaceSizeProvider — is written verbatim;
    // neither path applies any positivity guard. _surfaceSizeFetched flips
    // to true on the first push or the first pull attempt (regardless of
    // result), so subsequent getSurfaceWidth()/getSurfaceHeight() calls
    // short-circuit to the cached value. Real size changes after bootstrap
    // arrive exclusively through the push channel.
    float _surfaceWidth  = 0.0f;
    float _surfaceHeight = 0.0f;
    bool  _surfaceSizeFetched = false;

    // ---- Batched dispatch bookkeeping ----
    // _dispatchGuard: defers platform dispatch calls (onNodeUpdate/Add/Remove)
    //                 until the outermost batch window closes, so the platform
    //                 receives all changes in a single burst per operation type.
    //
    // _pendingDispatches preserves the original observer-callback order across
    // all three message kinds. On flush we slice the queue into maximal runs
    // of the same kind and dispatch each run via its typed listener call, so
    // the platform sees changes in the exact order VirtualDOM emitted them
    // while keeping the existing per-kind listener signatures intact.
    BatchGuard _dispatchGuard;
    using PendingDispatch = std::variant<ComponentsAddMessage,
                                         ComponentsRemoveMessage,
                                         ComponentsUpdateMessage>;
    std::vector<PendingDispatch> _pendingDispatches;
};

}  // namespace agenui
