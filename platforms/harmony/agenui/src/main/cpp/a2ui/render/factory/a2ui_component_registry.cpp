#include "a2ui_component_registry.h"

#include <memory>

#include "../a2ui_component.h"
#include "../a2ui_component_state.h"
#include "../hybrid/a2ui_hybrid_factory.h"
#include "../hybrid/a2ui_hybrid_view.h"
#include "log/a2ui_capi_log.h"

namespace a2ui {

namespace {

/**
 * Create a hybrid component through the ArkTS channel.
 *
 * Use unique_ptr so that state is automatically deleted if createHybridView
 * returns nullptr (e.g. ArkTS function not registered, or no node handle).
 * Ownership is released to the hybrid view on the success path.
 */
A2UIComponent* createHybridComponent(const std::string& surfaceId,
                                     int instanceId,
                                     const std::string& type,
                                     const std::string& id,
                                     const nlohmann::json& properties) {
    std::unique_ptr<ComponentState> state(new ComponentState(id, type, properties));
    state->setSurfaceId(surfaceId);
    // Carry the owning manager's instanceId into the ArkTS bridge. Without it the
    // bridge's view registry would key this view by componentId alone, and a second
    // surface reusing the same component id would silently hijack all updates.
    state->setInstanceId(instanceId);
    state->markDirty();
    auto* component = static_cast<A2UIComponent*>(A2UIHybridFactory::createHybridView(state.get()));
    if (component) {
        // The hybrid view now owns state via m_state; relinquish unique_ptr.
        state.release();
        return component;
    }
    // state is deleted here by unique_ptr – no leak on failure path.
    return new A2UIComponent(id, type);
}

} // namespace

ComponentRegistry& ComponentRegistry::global() {
    static ComponentRegistry* instance = [] {
        auto* r = new ComponentRegistry();
        r->registerBuiltInComponents();
        return r;
    }();
    return *instance;
}

void ComponentRegistry::registerBuiltInComponents() {
    for (const auto& descriptor : getComponentDescriptors()) {
        components_[descriptor.type] = descriptor;
    }
    HM_LOGI("Built-in components registered: %zu", components_.size());
}

void ComponentRegistry::registerComponent(const std::string& type, const ComponentDescriptor& descriptor) {
    if (type.empty()) {
        HM_LOGE("registerComponent: empty type");
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    components_[type] = descriptor;
    HM_LOGI("Registered component: %s (hybrid=%d)", type.c_str(), descriptor.isHybrid ? 1 : 0);
}

void ComponentRegistry::unregisterComponent(const std::string& type) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = components_.find(type);
    if (it == components_.end()) {
        HM_LOGW("unregisterComponent: type not registered: %s", type.c_str());
        return;
    }
    components_.erase(it);
    HM_LOGI("Unregistered component: %s", type.c_str());
}

A2UIComponent* ComponentRegistry::createComponent(const std::string& surfaceId,
                                                  int instanceId,
                                                  const std::string& type,
                                                  const std::string& id,
                                                  const nlohmann::json& properties) {
    ComponentDescriptor descriptor;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = components_.find(type);
        if (it == components_.end()) {
            HM_LOGW("No component registered for type: %s (id: %s, surfaceId: %s)",
                    type.c_str(), id.c_str(), surfaceId.c_str());
            return nullptr;
        }
        descriptor = it->second;
    }

    if (descriptor.isHybrid) {
        HM_LOGI("Creating hybrid component: type=%s, id=%s, surfaceId=%s, instanceId=%d",
                type.c_str(), id.c_str(), surfaceId.c_str(), instanceId);
        return createHybridComponent(surfaceId, instanceId, type, id, properties);
    }

    if (!descriptor.create) {
        HM_LOGE("Component entry has no native creator: type=%s, id=%s, surfaceId=%s",
                type.c_str(), id.c_str(), surfaceId.c_str());
        return nullptr;
    }

    A2UIComponent* component = descriptor.create(id, properties);
    if (component) {
        HM_LOGI("Created component: surfaceId=%s, type=%s, id=%s",
                surfaceId.c_str(), type.c_str(), id.c_str());
    } else {
        HM_LOGE("Creator returned null: surfaceId=%s, type=%s, id=%s",
                surfaceId.c_str(), type.c_str(), id.c_str());
    }
    return component;
}

int ComponentRegistry::getRegisteredComponentCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(components_.size());
}

} // namespace a2ui
