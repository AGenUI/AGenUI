#include "agenui_component_manager.h"
#include "data_value/agenui_data_value_parser.h"
#include "surface/datamodel/agenui_idata_model.h"
#include "surface/agenui_serializable_data.h"
#include "agenui_engine_context.h"
#include "surface/component_property_spec/agenui_icomponent_property_spec_manager.h"
#include "agenui_logger_internal.h"
#include "nlohmann/json.hpp"

namespace agenui {

ComponentManager::ComponentManager(ISurfaceContext* surfaceContext, IVirtualDOM* virtualDom, const std::string& theme)
    : _surfaceContext(surfaceContext)
    , _virtualDom(virtualDom)
    , _theme(theme)
    , _batchGuard([this] { flushDirtyComponents(); }) {
}

ComponentManager::~ComponentManager() {
    _components.clear();
}

void ComponentManager::updateComponents(const std::vector<std::string>& components) {
    // Batching is the caller's responsibility: every external entry point
    // (see Surface::updateComponents) wraps the call in a BatchScope so
    // attribute updates triggered during parsing/data-binding fan out
    // into a single flush per component.
    for (const auto& componentJson : components) {
        auto component = parseComponent(componentJson);

        if (component) {
            addComponent(component);
            tryUpdateTemplate(component->getId());
        }
    }
}

void ComponentManager::syncBindingValue(const std::string& id, const std::string& attributeName, const std::string& value) {
    auto componentIt = _components.find(id);

    if (componentIt != _components.end()) {
        componentIt->second->syncValue(attributeName, value);
    }
}



std::string ComponentManager::getParentId(const std::string& componentId) {
    if (componentId.empty()) {
        return "";
    }

    for (const auto& pair : _components) {
        if (!pair.second) {
            continue;
        }

        const auto& children = pair.second->getChildren();
        for (const auto& childId : children) {
            if (childId == componentId) {
                return pair.second->getId();
            }
        }
    }

    return "";
}

void ComponentManager::invalidateFunctionCallValues() {
    // Batching is the caller's responsibility:
    // Surface::invalidateFunctionCallValues wraps the call in a BatchScope
    // so the cascading function-call re-evaluations across all components
    // fan out into a single flush per component.
    for (const auto& pair : _components) {
        if (!pair.second) {
            continue;
        }
        pair.second->markDirty(/*attributeName=*/"", /*appendMode=*/false);
        const std::string& id = pair.second->getId();
        if (_dirtyIndex.insert(id).second) {
            _dirtyOrder.emplace_back(id);
        }
    }
    _batchGuard.requestFlush();
}

void ComponentManager::setComponentsDisplayRule(const std::map<std::string, DisplayRule>& displayRules) {
    _displayRules = displayRules;
}

void ComponentManager::executeComponentAction(const std::string& componentId, const std::string& surfaceId, void* dispatcher) {
    auto it = _components.find(componentId);
    if (it == _components.end()) {
        AGENUI_LOG("ComponentManager::executeComponentAction: component not found, id=%s", componentId.c_str());
        return;
    }
    
    auto component = it->second;
    if (component == nullptr) {
        AGENUI_LOG("ComponentManager::executeComponentAction: component is null, id=%s", componentId.c_str());
        return;
    }
    
    component->executeAction(surfaceId, static_cast<agenui::EventDispatcher*>(dispatcher));
}

void ComponentManager::onComponentChanged(const std::string& componentId) {
    if (componentId.empty()) {
        return;
    }

    auto it = _components.find(componentId);
    if (it == _components.end() || !it->second) {
        return;
    }

    // ComponentModel has already accumulated the dirty mark locally via
    // markDirty() before invoking this callback (see ComponentModel::
    // notifyComponentChange). Here we only enqueue the component id.
    //
    // The BatchGuard decides whether to flush immediately (no batch window
    // open) or defer until the outermost batch closes.
    if (_dirtyIndex.insert(componentId).second) {
        _dirtyOrder.emplace_back(componentId);
    }
    _batchGuard.requestFlush();
}

void ComponentManager::flushDirtyComponent(const std::shared_ptr<ComponentModel>& component) {
    if (!component || !_virtualDom) {
        return;
    }
    if (!component->hasDirty()) {
        return;
    }
    const auto& snapshot = component->flushDirty();
    _virtualDom->updateNode(snapshot);
}

void ComponentManager::flushDirtyComponents() {
    // Swap the pending queue out before iterating so any dirty marks raised
    // as a side-effect of flushing (e.g. data binding chains) accumulate
    // into a fresh queue. Those re-entrant marks call requestFlush() on
    // the BatchGuard, which—thanks to its do-while loop—will invoke this
    // method again after we return, draining the newly queued items.
    std::vector<std::string> order;
    std::unordered_set<std::string> index;
    order.swap(_dirtyOrder);
    index.swap(_dirtyIndex);

    for (const auto& id : order) {
        auto it = _components.find(id);
        if (it == _components.end() || !it->second) {
            continue;
        }
        flushDirtyComponent(it->second);
    }
}

void ComponentManager::onComponentDeleted(const std::string& componentId) {
    if (componentId.empty()) {
        return;
    }

    for (auto it = _components.begin(); it != _components.end(); ) {
        if (it->second && it->second->getId() == componentId) {
            it = _components.erase(it);
        } else {
            ++it;
        }
    }

    // Drop any pending dirty mark for the deleted id so the next flush
    // does not waste a lookup on it. _dirtyOrder is a vector — instead of
    // doing an O(n) erase here, we rely on flushDirtyComponents() to skip
    // ids that no longer resolve in _components. Erasing from the index
    // alone keeps re-dirty bookkeeping clean for any later re-insertion
    // with the same id.
    _dirtyIndex.erase(componentId);
}

std::vector<std::shared_ptr<ComponentModel>> ComponentManager::generateListChildren(const std::string& templateId, std::shared_ptr<DataValue> data) {
    std::vector<std::shared_ptr<ComponentModel>> result;

    if (!data) {
        return result;
    }

    std::string rootDataPath = "";
    if (data->getDataType() == DataType::DataBindingData) {
        auto dataBindingData = std::static_pointer_cast<DataBindingDataValue>(data);
        if (dataBindingData) {
            rootDataPath = dataBindingData->getBindingPath();
        }
    }

    SerializableData childrenData = data->getValueData();
    if (!childrenData.isArray()) {
        return result;
    }

    for (size_t index = 0; index < childrenData.size(); ++index) {
        std::string itemPath = rootDataPath + "/" + std::to_string(index);
        auto itemData = std::make_shared<DataBindingDataValue>(static_cast<IDataValueContext*>(nullptr), itemPath);
        auto entity = generateComponentWithTemplate(templateId, std::static_pointer_cast<DataValue>(itemData));
        if (entity) {
            itemData->setContext(static_cast<IDataValueContext*>(entity.get()));
            result.emplace_back(entity);
        }
    }

    return result;
}

std::shared_ptr<ComponentModel> ComponentManager::generateComponentWithTemplate(const std::string& templateId, std::shared_ptr<DataValue> data) {
    auto templateIt = _components.find(templateId);

    if (templateIt == _components.end()) {
        return nullptr;
    }

    auto templateEntity = templateIt->second;
    if (!templateEntity) {
        return nullptr;
    }

    if (!data) {
        return nullptr;
    }

    std::string rootDataPath = "";
    if (data->getDataType() == DataType::DataBindingData) {
        auto dataBindingData = std::static_pointer_cast<DataBindingDataValue>(data);
        if (dataBindingData) {
            rootDataPath = dataBindingData->getBindingPath();
        }
    }

    std::string newId = templateEntity->getId() + "-" + rootDataPath;
    auto newEntity = std::make_shared<ComponentModel>(newId, templateEntity->getRawId(), templateEntity->getComponent(), _surfaceContext, this, this);

    // Apply display rule using the templateId as lookup key
    auto ruleIt = _displayRules.find(templateId);
    if (ruleIt != _displayRules.end()) {
        newEntity->setDisplayRule(ruleIt->second);
    }

    // Clone all attributes from the template
    const auto& templateAttributes = templateEntity->getAllAttributes();
    for (const auto& attrPair : templateAttributes) {
        const std::string& attrKey = attrPair.first;
        const auto& attrValue = attrPair.second;

        if (!attrValue) {
            continue;
        }

        auto clonedValue = attrValue->cloneAsTemplate(newEntity.get(), rootDataPath);
        if (clonedValue) {
            newEntity->setAttribute(attrKey, clonedValue);
        }
    }

    // Apply component spec/styles after all attributes are set
    auto* specManager = getEngineContext()->getComponentPropertySpecManager();
    if (specManager != nullptr) {
        specManager->applySpec(_theme, newEntity.get());
    }

    // Generate non-list child components
    const auto& templateChildren = templateEntity->getChildren();
    if (!templateChildren.empty()) {
        newEntity->setTemplateComponentInfo(data, templateChildren);
        newEntity->generateTemplateChildren("");
    }

    // Generate list child components
    std::string listChildrenTemplateId = templateEntity->getListChildrenTemplateId();
    if (!listChildrenTemplateId.empty()) {
        auto listChildrenData = templateEntity->getListChildrenData();
        if (listChildrenData) {
            auto clonedListChildrenData = listChildrenData->cloneAsTemplate(newEntity.get(), rootDataPath);
            if (clonedListChildrenData) {
                newEntity->setListChildrenData(clonedListChildrenData);
                newEntity->setChildrenTemplateId(listChildrenTemplateId);
                newEntity->generateListChildren();
            }
        }
    }

    addComponent(newEntity);

    return newEntity;
}

std::shared_ptr<ComponentModel> ComponentManager::parseComponent(const std::string& componentJson) {
    auto json = nlohmann::json::parse(componentJson, nullptr, false);
    
    if (json.is_discarded()) {
        return nullptr;
    }
    
    if (!json.contains("id") || !json.contains("component")) {
        return nullptr;
    }
    
    std::string id = json["id"].get<std::string>();
    std::string component = json["component"].get<std::string>();
    
    // Use rawId from JSON if present, otherwise fall back to id
    std::string rawId = id;
    if (json.contains("rawId")) {
        rawId = json["rawId"].get<std::string>();
    }

    auto entity = std::make_shared<ComponentModel>(id, rawId, component, _surfaceContext, this, this);

    auto ruleIt = _displayRules.find(id);
    if (ruleIt != _displayRules.end()) {
        entity->setDisplayRule(ruleIt->second);
    }

    parseChildren(json, component, entity);

    // All fields except id/component/children/child/rawId are treated as attributes
    for (auto it = json.begin(); it != json.end(); ++it) {
        std::string key = it.key();
        if (key != "id" && key != "component" && key != "children" && key != "child" && key != "rawId") {
            std::shared_ptr<DataValue> value;

            // Special handling for action, checks, styles, and tabs
            if (key == "action") {
                std::string actionJson = it.value().dump();
                value = DataValueParser::parseFunctionCallActionDataValue(entity.get(), actionJson);
                if (!value) {
                    value = DataValueParser::parseEventActionDataValue(entity.get(), actionJson);
                }
                if (!value) {
                    value = std::make_shared<StaticDataValue>(actionJson);
                }
            } else if (key == "checks") {
                value = DataValueParser::parseChecksDataValue(entity.get(), it.value().dump());
            } else if (key == "styles") {
                value = DataValueParser::parseStylesDataValue(entity.get(), it.value().dump());
            } else if (key == "tabs" && component == "Tabs") {
                value = DataValueParser::parseTabsDataValue(entity.get(), it.value().dump());
            } else {
                value = DataValueParser::parseDataValue(entity.get(), it.value().dump());
            }
            
            if (value) {
                entity->setAttribute(key, value);
            }
        }
    }
    
    // Apply component spec/styles after all attributes are set
    auto* specManager = getEngineContext()->getComponentPropertySpecManager();
    if (specManager != nullptr) {
        specManager->applySpec(_theme, entity.get());
    }

    return entity;
}

void ComponentManager::parseChildren(const nlohmann::json& json, const std::string &componentType, std::shared_ptr<ComponentModel> entity) {
    if (!entity) {
        return;
    }
    
    std::vector<std::string> children;
    // Parse trigger and content children for Modal components
    if (componentType == "Modal") {
        if (json.contains("trigger")) {
            children.emplace_back(json["trigger"].get<std::string>());
        }
        if (json.contains("content")) {
            children.emplace_back(json["content"].get<std::string>());
        }
    }
    
    if (json.contains("children")) {
        if (json["children"].is_array()) {
            for (const auto& child : json["children"]) {
                if (child.is_string()) {
                    children.emplace_back(child.get<std::string>());
                }
            }
        } else if (json["children"].is_object()) {
            // Object form: parse path (data binding) and componentId (template)
            const auto& childrenObj = json["children"];

            if (childrenObj.contains("path") && childrenObj["path"].is_string()) {
                std::string path = childrenObj["path"].get<std::string>();
                auto dataBindingData = std::make_shared<DataBindingDataValue>(entity.get(), path);
                entity->setListChildrenData(dataBindingData);
            }

            if (childrenObj.contains("componentId") && childrenObj["componentId"].is_string()) {
                std::string componentId = childrenObj["componentId"].get<std::string>();
                entity->setChildrenTemplateId(componentId);
            }

            entity->generateListChildren();
        }
    }

    if (json.contains("child")) {
        if (json["child"].is_string()) {
            children.emplace_back(json["child"].get<std::string>());
        }
    }
    
    if (!children.empty()) {
        entity->setChildren(children);
    }
}


void ComponentManager::addComponent(std::shared_ptr<ComponentModel> component) {
    if (!component || !_virtualDom) {
        return;
    }

    const std::string& id = component->getId();

    // 1. Register: the lookup table owns the only strong reference to the
    //    component, so any later flush / template expansion can find it.
    _components[id] = component;

    // 2. Enqueue: a freshly constructed component is full-dirty by default
    //    (see ComponentModel constructor initial state), so we only need
    //    to enqueue the id; the model's dirty flags are already set.
    if (_dirtyIndex.insert(id).second) {
        _dirtyOrder.emplace_back(id);
    }
    _batchGuard.requestFlush();
}

void ComponentManager::tryUpdateTemplate(const std::string& componentId) {
    for (const auto& pair : _components) {
        if (pair.second) {
            pair.second->tryUpdateChildrenTemplate(componentId);
        }
    }
}

}  // namespace agenui
