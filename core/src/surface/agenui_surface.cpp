#include "agenui_surface.h"
#include "agenui_template_registry.h"
#include "agenui_engine_context.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_size_provider.h"
#include "agenui_type_define.h"
#include "module/agenui_surface_manager.h"
#include "agenui_measurement.h"
#include "datamodel/agenui_data_model.h"
#include "virtual_dom/agenui_virtual_dom.h"
#include "component_manager/agenui_component_manager.h"
#include "agenui_batch_guard.h"
#include "agenui_logger_internal.h"
#include "module/agenui_event_dispatcher.h"

namespace agenui {

Surface::Surface(const std::string& surfaceId, const std::string& theme, SurfaceManager* surfaceManager)
    : _surfaceId(surfaceId),
      _theme(theme),
      _dataModel(nullptr),
      _virtualDom(nullptr),
      _componentManager(nullptr),
      _surfaceManager(surfaceManager),
      _dispatchGuard([this] { flushPendingDispatches(); }) {
    AGENUI_PERFORMANCE_LOG("surface_create_begin", "%s", surfaceId.c_str());
          
    _dataModel = new DataModel();

    IMeasurementManager* mm = nullptr;
    IAGenUIEngine* engine = getAGenUIEngine();
    if (engine) {
        mm = engine->getMeasurementManager();
    }
    // Pass `this` as the ISurfaceContext so VirtualDOM can pull the surface
    // size on demand (cache-first; falls back to the host-supplied provider
    // on the bootstrap miss before any onSurfaceSizeChanged push has arrived).
    _virtualDom = new VirtualDOM(this, this, mm);
    _componentManager = new ComponentManager(this, _virtualDom, _theme);

    // Wire up cascading batch guards:
    //   _dispatchGuard → VDOM guard → CM guard
    // endBatch unwinds inside-out: CM flushes dirty components (updateNode),
    // then VDOM flushes layout (onNodeAdded/Update/Remove → pending queues),
    // then dispatch guard flushes accumulated platform messages.
    _virtualDom->batchGuard()->setInnerGuards({_componentManager->batchGuard()});
    _dispatchGuard.setInnerGuards({_virtualDom->batchGuard()});

    AGENUI_PERFORMANCE_LOG("surface_create_end", "%s", surfaceId.c_str());
}

Surface::~Surface() {
    _isDestroying = true;
    // Detach cascading guard pointers before destroying the targets they
    // point to, preventing any accidental dereference of dangling pointers
    // if a callback fires during teardown.
    _dispatchGuard.setInnerGuards({});
    _virtualDom->batchGuard()->setInnerGuards({});
    SAFELY_DELETE(_componentManager);
    SAFELY_DELETE(_virtualDom);
    SAFELY_DELETE(_dataModel);
    
    AGENUI_PERFORMANCE_LOG("surface_destroy", "%s", _surfaceId.c_str());
}

int Surface::getInstanceId() const {
    if (_surfaceManager) {
        return _surfaceManager->getInstanceId();
    }
    return -1;
}

std::string Surface::getSurfaceId() const {
    return _surfaceId;
}

IDataModel* Surface::getDataModel() const {
    return _dataModel;
}

void Surface::ensureSurfaceSizeFetched() {
    if (_surfaceSizeFetched) {
        return;
    }
    if (!_surfaceManager) {
        return;
    }
    // Route through SurfaceManager::getSurfaceSize() so the provider call is
    // serialized with setSurfaceSizeProvider() and never dereferences a
    // dangling pointer. nullopt = no provider injected yet, retry later.
    auto size = _surfaceManager->getSurfaceSize(_surfaceId);
    if (!size.has_value()) {
        return;
    }
    _surfaceWidth  = size->width;
    _surfaceHeight = size->height;
    _surfaceSizeFetched = true;
}

float Surface::getSurfaceWidth() {
    ensureSurfaceSizeFetched();
    return _surfaceWidth;
}

float Surface::getSurfaceHeight() {
    ensureSurfaceSizeFetched();
    return _surfaceHeight;
}

void Surface::updateComponentSize(const ComponentRenderInfo& info) {
    if (_virtualDom) {
        BatchScope batchScope(_virtualDom->batchGuard());
        VirtualDOM* virtualDomImpl = static_cast<VirtualDOM*>(_virtualDom);
        virtualDomImpl->updateComponentSize(info);
    } else {
        AGENUI_LOG("failed: virtualDom is null");
    }
}

void Surface::updateTabsSelectedIndex(const ComponentRenderInfo& info) {
    if (_virtualDom) {
        BatchScope batchScope(_virtualDom->batchGuard());
        VirtualDOM* virtualDomImpl = static_cast<VirtualDOM*>(_virtualDom);
        virtualDomImpl->updateTabsSelectedIndex(info.componentId, info.selectedIndex);
    } else {
        AGENUI_LOG("failed: virtualDom is null");
    }
}

void Surface::updateSurfaceSize(const SurfaceLayoutInfo& info) {
    // Always refresh the cache and flip the fetched flag so subsequent
    // bootstrap pulls are short-circuited, even when the size is unchanged.
    const bool firstPush = !_surfaceSizeFetched;
    const bool sizeChanged = firstPush
                          || _surfaceWidth  != info.width
                          || _surfaceHeight != info.height;

    _surfaceWidth  = info.width;
    _surfaceHeight = info.height;
    _surfaceSizeFetched = true;

    // Skip the VDOM notification when the size is unchanged: it would wipe
    // every node's platform-measured size cache and re-layout against an
    // unchanged width, forcing pointless re-measure on Text / Image nodes.
    if (!sizeChanged) {
        return;
    }

    if (_virtualDom) {
        BatchScope batchScope(_virtualDom->batchGuard());
        VirtualDOM* virtualDomImpl = static_cast<VirtualDOM*>(_virtualDom);
        virtualDomImpl->notifySurfaceSizeChanged();
    } else {
        AGENUI_LOG("virtualDom is null");
    }
}

AGenUIExeCode Surface::updateComponents(const nlohmann::json& componentsData) {
    std::vector<std::string> parsedComponents;
    std::map<std::string, DisplayRule> displayRules;

    if (!componentsData.contains("components")) {
        return UpdateComponents_MissingComponentsField;
    }

    if (!componentsData["components"].is_array()) {
        return UpdateComponents_ComponentsNotArray;
    }

    const auto& components = componentsData["components"];
    parsedComponents.reserve(components.size());

    for (const auto& component : components) {
        TemplateRegistry* templateRegistry = getEngineContext()->getTemplateRegistry();

        bool componentField = component.contains("component") && component["component"].is_string();
        std::string componentName;
        if (componentField) {
            componentName = component["component"].get<std::string>();
        } else {
            return UpdateComponents_MissingComponentEntity;
        }
        if (templateRegistry && templateRegistry->isTemplate(componentName)) {
            AGENUI_LOG("template: %s", componentName.c_str());
            std::string componentId = "";
            if (component.contains("id") && component["id"].is_string()) {
                componentId = component["id"].get<std::string>();
            }

            bool isNonRootNode = false;
            if (!componentId.empty() && _componentManager != nullptr) {
                std::string parentId = _componentManager->getParentId(componentId);
                if (!parentId.empty()) {
                    isNonRootNode = true;
                }
            }

            auto expandedResult = templateRegistry->expandTemplate(component, isNonRootNode);
            if (expandedResult.components.empty()) {
                return UpdateComponents_TemplateExpansionFailed;
            }
            for (auto& comp : expandedResult.components) {
                parsedComponents.emplace_back(std::move(comp));
            }
            for (const auto& pair : expandedResult.displayRules) {
                displayRules[pair.first] = pair.second;
            }
        } else {
            AGENUI_LOG("none template: %s", componentName.c_str());
            auto componentStr = component.dump();
            parsedComponents.emplace_back(std::move(componentStr));
        }
    }

    if (parsedComponents.empty()) {
        return UpdateComponents_MissingComponentEntity;
    }

    if (_componentManager != nullptr) {
        BatchScope batchScope(_virtualDom->batchGuard());
        if (!displayRules.empty()) {
            _componentManager->setComponentsDisplayRule(displayRules);
        }
        _componentManager->updateComponents(parsedComponents);
    }

    return Execute_Success;
}

void Surface::updateDataModel(const nlohmann::json& dataModelData) {
    if (_dataModel == nullptr) {
        return;
    }
    AGENUI_PERFORMANCE_LOG("surface_updateDataModel_begin", "%s", _surfaceId.c_str());
    BatchScope batchScope(_virtualDom->batchGuard());

    if (!dataModelData.contains("path") && !dataModelData.contains("value")) {
        _dataModel->updateData("/", dataModelData.dump());
        return;
    }

    std::string path = dataModelData.contains("path") ? dataModelData["path"].get<std::string>() : "/";

    if (!dataModelData.contains("value")) {
        return;
    }

    const auto& value = dataModelData["value"];
    std::string valueStr;
    valueStr = value.dump();
    _dataModel->updateData(path, valueStr);

    AGENUI_PERFORMANCE_LOG("surface_updateDataModel_end", "%s", _surfaceId.c_str());
}

void Surface::appendDataModel(const nlohmann::json& dataModelData) {
    if (_dataModel == nullptr) {
        return;
    }
    AGENUI_PERFORMANCE_LOG("surface_appendDataModel_begin", "%s", _surfaceId.c_str());
    BatchScope batchScope(_virtualDom->batchGuard());

    std::string path = dataModelData.contains("path") ? dataModelData["path"].get<std::string>() : "/";

    if (!dataModelData.contains("value")) {
        return;
    }

    const auto& value = dataModelData["value"];
    std::string valueStr;
    if (value.is_string()) {
        valueStr = value.get<std::string>();
    } else {
        valueStr = value.dump();
    }

    _dataModel->appendData(path, valueStr);

    AGENUI_PERFORMANCE_LOG("surface_appendDataModel_end", "%s", _surfaceId.c_str());
}

void Surface::onNodeUpdate(const std::string& componentId, const std::string& nodeJson) {
    if (!_surfaceManager) return;
    ComponentsUpdateMessage message;
    message.componentId = componentId;
    message.component = nodeJson;
    _pendingDispatches.emplace_back(std::move(message));
    _dispatchGuard.requestFlush();
}

void Surface::onNodeAdded(const std::string& parentId, const std::string& nodeJson) {
    auto json = nlohmann::json::parse(nodeJson, nullptr, false);
    if (json.is_discarded() || !json.contains("id") || !json["id"].is_string()) {
        AGENUI_LOG("failed to parse componentId from nodeJson");
        return;
    }

    AGENUI_LOG("onNodeAdded: %s, parentId:%s", nodeJson.c_str(), parentId.c_str());
    std::string componentId = json["id"].get<std::string>();
    if (!_surfaceManager) return;
    ComponentsAddMessage message;
    message.parentId = parentId;
    message.componentId = componentId;
    message.component = nodeJson;
    _pendingDispatches.emplace_back(std::move(message));
    _dispatchGuard.requestFlush();
}

void Surface::onNodeRemoved(const std::string& parentId, const std::string& id) {
    AGENUI_LOG("onNodeRemoved: %s, parentId:%s", id.c_str(), parentId.c_str());
    if (!_surfaceManager) return;
    ComponentsRemoveMessage message;
    message.parentId = parentId;
    message.componentId = id;
    _pendingDispatches.emplace_back(std::move(message));
    _dispatchGuard.requestFlush();
}

void Surface::flushPendingDispatches() {
    if (!_surfaceManager) return;
    auto* dispatcher = _surfaceManager->getEventDispatcher();
    if (!dispatcher) return;
    if (_pendingDispatches.empty()) return;

    // Take ownership of the queue up front so re-entrant observer callbacks
    // triggered by a dispatch land in a fresh queue rather than the run we
    // are currently iterating.
    std::vector<PendingDispatch> pending;
    pending.swap(_pendingDispatches);

    // Slice the queue into maximal runs of the same alternative and dispatch
    // each run through its typed listener call. This preserves the exact
    // emission order across kinds while keeping the existing per-kind
    // listener signatures (vector<AddMessage> / vector<RemoveMessage> /
    // vector<UpdateMessage>) intact.
    size_t i = 0;
    while (i < pending.size()) {
        const size_t kind = pending[i].index();
        size_t j = i + 1;
        while (j < pending.size() && pending[j].index() == kind) {
            ++j;
        }
        switch (kind) {
            case 0: {  // ComponentsAddMessage
                std::vector<ComponentsAddMessage> run;
                run.reserve(j - i);
                for (size_t k = i; k < j; ++k) {
                    run.emplace_back(std::move(std::get<ComponentsAddMessage>(pending[k])));
                }
                dispatcher->dispatchComponentsAdd(_surfaceId, run);
                break;
            }
            case 1: {  // ComponentsRemoveMessage
                std::vector<ComponentsRemoveMessage> run;
                run.reserve(j - i);
                for (size_t k = i; k < j; ++k) {
                    run.emplace_back(std::move(std::get<ComponentsRemoveMessage>(pending[k])));
                }
                dispatcher->dispatchComponentsRemove(_surfaceId, run);
                break;
            }
            case 2: {  // ComponentsUpdateMessage
                std::vector<ComponentsUpdateMessage> run;
                run.reserve(j - i);
                for (size_t k = i; k < j; ++k) {
                    run.emplace_back(std::move(std::get<ComponentsUpdateMessage>(pending[k])));
                }
                dispatcher->dispatchComponentsUpdate(_surfaceId, run);
                break;
            }
            default:
                break;
        }
        i = j;
    }
}


void Surface::syncUIToData(const std::string& componentId, const std::string& changingData) {
    if (_componentManager == nullptr) {
        AGENUI_LOG("componentManager is null");
        return;
    }

    auto changeJson = nlohmann::json::parse(changingData, nullptr, false);

    if (changeJson.is_discarded()) {
        AGENUI_LOG("failed to parse changingData");
        return;
    }

    if (!changeJson.is_object()) {
        AGENUI_LOG("changeJson is not an object");
        return;
    }

    {
        BatchScope batchScope(_virtualDom->batchGuard());
        for (auto it = changeJson.begin(); it != changeJson.end(); ++it) {
            const std::string& attributeName = it.key();
            const auto& fieldValue = it.value();

            std::string valueStr = fieldValue.dump();
            _componentManager->syncBindingValue(componentId, attributeName, valueStr);
            AGENUI_LOG("synced componentId:%s, attributeName:%s, value:%s", componentId.c_str(), attributeName.c_str(), valueStr.c_str());
        }
    }
}

void Surface::handleUserAction(const std::string& sourceComponentId) {
    AGENUI_PERFORMANCE_LOG("surface_handleUserAction_begin", "%s", sourceComponentId.c_str());
    
    if (_componentManager != nullptr && _surfaceManager) {
        EventDispatcher* dispatcher = _surfaceManager->getEventDispatcher();
        _componentManager->executeComponentAction(sourceComponentId, _surfaceId, dispatcher);
    }
    
    AGENUI_PERFORMANCE_LOG("surface_handleUserAction_end", "%s", sourceComponentId.c_str());
}

void Surface::invalidateFunctionCallValues() {
    if (_componentManager != nullptr) {
        BatchScope batchScope(_virtualDom->batchGuard());
        _componentManager->invalidateFunctionCallValues();
    }
}

}  // namespace agenui
