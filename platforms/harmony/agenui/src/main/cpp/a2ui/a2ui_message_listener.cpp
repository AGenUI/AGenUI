#include "a2ui_message_listener.h"
#include "hilog/log.h"
#include "log/a2ui_capi_log.h"
#include "agenui_logger_internal.h"
#include "render/a2ui_surface.h"
#include "render/a2ui_component_types.h"
#include "render/factory/a2ui_component_creator.h"
#include <nlohmann/json.hpp>
#include "utils/a2ui_log_utils.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "A2UIMessageListener"

extern napi_env a2ui_get_napi_env();

namespace agenui {

// Static member initialization
std::map<std::string, int>& A2UIMessageListener::getSurfaceIdToInstanceIdMap() {
    static auto* p = new std::map<std::string, int>();
    return *p;
}
std::mutex A2UIMessageListener::s_mappingMutex_;

std::map<int, A2UIMessageListener*>& A2UIMessageListener::getInstanceToListenerMap() {
    static auto* p = new std::map<int, A2UIMessageListener*>();
    return *p;
}
std::mutex A2UIMessageListener::s_instanceMapMutex_;

A2UIMessageListener::A2UIMessageListener(int instanceId)
    : instanceId_(instanceId), surfaceManager_(nullptr), tsfn_(nullptr) {
    initGlobalRegistry();
    surfaceManager_ = std::make_unique<a2ui::A2UISurfaceManager>(&globalRegistry_);

    // Register instance for exposure dispatch lookup
    {
        std::lock_guard<std::mutex> lock(s_instanceMapMutex_);
        getInstanceToListenerMap()[instanceId_] = this;
    }

    HM_LOGI("A2UIMessageListener created, instanceId=%d, factories=%d",
            instanceId_, globalRegistry_.getRegisteredFactoryCount());
}

A2UIMessageListener::~A2UIMessageListener() {
    // Unregister instance from exposure dispatch lookup
    {
        std::lock_guard<std::mutex> lock(s_instanceMapMutex_);
        getInstanceToListenerMap().erase(instanceId_);
    }

    // Remove all surfaceId→instanceId entries owned by this instance so the
    // static map doesn't accumulate stale entries across create/destroy cycles.
    {
        std::lock_guard<std::mutex> lock(s_mappingMutex_);
        auto& map = getSurfaceIdToInstanceIdMap();
        for (auto it = map.begin(); it != map.end(); ) {
            if (it->second == instanceId_) {
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    }

    // tsfn_ is owned by napi_init.cpp and must not be released here.
    tsfn_ = nullptr;

    napi_env env = a2ui_get_napi_env();
    for (auto& ref : listeners_) {
        napi_delete_reference(env, ref);
    }
    listeners_.clear();

    surfaceManager_.reset();

    HM_LOGI("A2UIMessageListener destroyed, instanceId=%d", instanceId_);
}

void A2UIMessageListener::setTsfn(napi_threadsafe_function tsfn) {
    tsfn_ = tsfn;
    HM_LOGI("setTsfn: tsfn assigned, instanceId=%d, tsfn=%p", instanceId_, (void*)tsfn);
}

void A2UIMessageListener::postToMainThread(MainThreadTask task) {
    if (!tsfn_) {
        HM_LOGE("postToMainThread: tsfn not initialized, instanceId=%d, dropping task", instanceId_);
        return;
    }
    auto* taskPtr = new MainThreadTask(std::move(task));
    napi_status status = napi_call_threadsafe_function(tsfn_, taskPtr, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        HM_LOGE("postToMainThread: napi_call_threadsafe_function failed, instanceId=%d, status=%d", instanceId_, status);
        delete taskPtr;
    }
}

void A2UIMessageListener::initGlobalRegistry() {
    globalRegistry_.setOwnsFactories(true);

    auto makeCreator = [](const std::string& type) -> a2ui::A2UIComponentCreator* {
        auto* creator = new a2ui::A2UIComponentCreator();
        creator->setType(type);
        return creator;
    };

    globalRegistry_.registerFactory(a2ui::ComponentType::kText,         makeCreator(a2ui::ComponentType::kText));
    globalRegistry_.registerFactory(a2ui::ComponentType::kColumn,       makeCreator(a2ui::ComponentType::kColumn));
    globalRegistry_.registerFactory(a2ui::ComponentType::kIcon,         makeCreator(a2ui::ComponentType::kIcon));
    globalRegistry_.registerFactory(a2ui::ComponentType::kTabs,         makeCreator(a2ui::ComponentType::kTabs));
    globalRegistry_.registerFactory(a2ui::ComponentType::kCard,         makeCreator(a2ui::ComponentType::kCard));
    globalRegistry_.registerFactory(a2ui::ComponentType::kList,         makeCreator(a2ui::ComponentType::kList));
    globalRegistry_.registerFactory(a2ui::ComponentType::kButton,       makeCreator(a2ui::ComponentType::kButton));
    globalRegistry_.registerFactory(a2ui::ComponentType::kImage,        makeCreator(a2ui::ComponentType::kImage));
    globalRegistry_.registerFactory(a2ui::ComponentType::kTextField,    makeCreator(a2ui::ComponentType::kTextField));
    globalRegistry_.registerFactory(a2ui::ComponentType::kRow,          makeCreator(a2ui::ComponentType::kRow));
    globalRegistry_.registerFactory(a2ui::ComponentType::kSlider,       makeCreator(a2ui::ComponentType::kSlider));
    globalRegistry_.registerFactory(a2ui::ComponentType::kCheckBox,     makeCreator(a2ui::ComponentType::kCheckBox));
    globalRegistry_.registerFactory(a2ui::ComponentType::kChoicePicker, makeCreator(a2ui::ComponentType::kChoicePicker));
    globalRegistry_.registerFactory(a2ui::ComponentType::kDateTimeInput,makeCreator(a2ui::ComponentType::kDateTimeInput));
    globalRegistry_.registerFactory(a2ui::ComponentType::kModal,        makeCreator(a2ui::ComponentType::kModal));
    globalRegistry_.registerFactory(a2ui::ComponentType::kRichText,     makeCreator(a2ui::ComponentType::kRichText));
    globalRegistry_.registerFactory(a2ui::ComponentType::kTable,        makeCreator(a2ui::ComponentType::kTable));
    globalRegistry_.registerFactory(a2ui::ComponentType::kVideo,        makeCreator(a2ui::ComponentType::kVideo));
    globalRegistry_.registerFactory(a2ui::ComponentType::kAudioPlayer,  makeCreator(a2ui::ComponentType::kAudioPlayer));
    globalRegistry_.registerFactory(a2ui::ComponentType::kCarousel,     makeCreator(a2ui::ComponentType::kCarousel));
    globalRegistry_.registerFactory(a2ui::ComponentType::kDivider,      makeCreator(a2ui::ComponentType::kDivider));
    globalRegistry_.registerFactory(a2ui::ComponentType::kWeb,          makeCreator(a2ui::ComponentType::kWeb));
}

a2ui::A2UISurfaceManager* A2UIMessageListener::getSurfaceManager() const {
    return surfaceManager_.get();
}

// ==================== surfaceId -> instanceId Mapping ====================

void A2UIMessageListener::registerSurfaceMapping(const std::string& surfaceId) {
    std::lock_guard<std::mutex> lock(s_mappingMutex_);
    getSurfaceIdToInstanceIdMap()[surfaceId] = instanceId_;
    HM_LOGI("Surface mapping registered: surfaceId=%s -> instanceId=%d", surfaceId.c_str(), instanceId_);
}

void A2UIMessageListener::unregisterSurfaceMapping(const std::string& surfaceId) {
    unregisterSurfaceMappingStatic(surfaceId);
}

void A2UIMessageListener::unregisterSurfaceMappingStatic(const std::string& surfaceId) {
    std::lock_guard<std::mutex> lock(s_mappingMutex_);
    getSurfaceIdToInstanceIdMap().erase(surfaceId);
    HM_LOGI("Surface mapping unregistered: surfaceId=%s", surfaceId.c_str());
}

int A2UIMessageListener::findInstanceIdBySurfaceId(const std::string& surfaceId) {
    std::lock_guard<std::mutex> lock(s_mappingMutex_);
    auto it = getSurfaceIdToInstanceIdMap().find(surfaceId);
    if (it != getSurfaceIdToInstanceIdMap().end()) {
        return it->second;
    }
    return 0;
}

A2UIMessageListener* A2UIMessageListener::findListenerBySurfaceId(const std::string& surfaceId) {
    int instanceId = findInstanceIdBySurfaceId(surfaceId);
    if (instanceId == 0) return nullptr;
    std::lock_guard<std::mutex> lock(s_instanceMapMutex_);
    auto it = getInstanceToListenerMap().find(instanceId);
    return (it != getInstanceToListenerMap().end()) ? it->second : nullptr;
}

void A2UIMessageListener::dispatchComponentAppeared(const std::string& surfaceId,
                                                    const std::string& parentComponentId,
                                                    const std::string& parentType,
                                                    const std::string& properties) {
    std::weak_ptr<A2UIMessageListener> weakSelf = weak_from_this();
    postToMainThread([weakSelf, surfaceId, parentComponentId, parentType, properties](napi_env env) {
        auto self = weakSelf.lock();
        if (!self) return;

        for (auto& ref : self->listeners_) {
            napi_value listener = nullptr;
            napi_get_reference_value(env, ref, &listener);
            if (!listener) continue;

            napi_value onComponentAppearedFunc = nullptr;
            napi_get_named_property(env, listener, "onComponentAppeared", &onComponentAppearedFunc);

            napi_valuetype funcType = napi_undefined;
            if (onComponentAppearedFunc) napi_typeof(env, onComponentAppearedFunc, &funcType);
            if (funcType != napi_function) continue;

            napi_value surfaceIdValue = nullptr;
            napi_create_string_utf8(env, surfaceId.c_str(), NAPI_AUTO_LENGTH, &surfaceIdValue);
            napi_value parentComponentIdValue = nullptr;
            napi_create_string_utf8(env, parentComponentId.c_str(), NAPI_AUTO_LENGTH, &parentComponentIdValue);
            napi_value parentTypeValue = nullptr;
            napi_create_string_utf8(env, parentType.c_str(), NAPI_AUTO_LENGTH, &parentTypeValue);
            napi_value propertiesValue = nullptr;
            napi_create_string_utf8(env, properties.c_str(), NAPI_AUTO_LENGTH, &propertiesValue);

            napi_value args[] = { surfaceIdValue, parentComponentIdValue, parentTypeValue, propertiesValue };
            napi_value result = nullptr;
            napi_call_function(env, listener, onComponentAppearedFunc, 4, args, &result);
        }
    });
}

// ==================== IAGenUIMessageListener Implementation ====================

void A2UIMessageListener::onCreateSurface(const CreateSurfaceMessage& msg) {
    HM_LOGI("instanceId=%d, surfaceId=%s, catalogId=%s" , instanceId_, msg.surfaceId.c_str(), msg.catalogId.c_str());
    // listeners_ and surfaceManager_ are only touched on the main thread.
    std::string surfaceId = msg.surfaceId;
    bool animated = msg.animated;
    std::string rawProtocolContent = msg.rawProtocolContent;
    // Capture weak_ptr instead of `this`: the main-thread task may run after
    // the listener has been destroyed (e.g. rapid create/destroy on the main
    // thread keeps the TSFN queue from draining).
    std::weak_ptr<A2UIMessageListener> weakSelf = weak_from_this();
    postToMainThread([weakSelf, surfaceId, animated, rawProtocolContent](napi_env env) {
        auto self = weakSelf.lock();
        if (!self || !self->surfaceManager_) {
            HM_LOGW("onCreateSurface: listener already destroyed, surfaceId=%s", surfaceId.c_str());
            return;
        }
        // Register the surfaceId -> instanceId mapping.
        self->registerSurfaceMapping(surfaceId);
        a2ui::A2UISurface* surface = self->surfaceManager_->createSurface(surfaceId, animated);
        if (!surface) {
            HM_LOGE("Failed to create surface: %s", surfaceId.c_str());
            return;
        }

        // Notify ArkTS listeners after the surface is created.
        for (auto& ref : self->listeners_) {
            napi_value listener = nullptr;
            napi_get_reference_value(env, ref, &listener);
            if (!listener) continue;

            napi_value onCreatedFunc = nullptr;
            napi_get_named_property(env, listener, "onCreateSurface", &onCreatedFunc);

            napi_valuetype funcType = napi_undefined;
            if (onCreatedFunc) napi_typeof(env, onCreatedFunc, &funcType);
            if (funcType == napi_function) {
                napi_value surfaceIdValue = nullptr;
                napi_create_string_utf8(env, surfaceId.c_str(), NAPI_AUTO_LENGTH, &surfaceIdValue);
                napi_value rawProtocolContentValue = nullptr;
                napi_create_string_utf8(env, rawProtocolContent.c_str(), NAPI_AUTO_LENGTH, &rawProtocolContentValue);
                napi_value args[] = { surfaceIdValue, rawProtocolContentValue };
                napi_value result = nullptr;
                napi_call_function(env, listener, onCreatedFunc, 2, args, &result);  // 2 args: surfaceId, rawProtocolContent
            }
        }
    });
}

void A2UIMessageListener::onContentHandleReady() {
    HM_LOGI("Method deprecated, use bindSurface instead");
}

void A2UIMessageListener::onDeleteSurface(const DeleteSurfaceMessage& msg) {
    HM_LOGI("instanceId=%d, surfaceId=%s", instanceId_, msg.surfaceId.c_str());

    // listeners_ and surfaceManager_ are only touched on the main thread.
    std::string surfaceId = msg.surfaceId;
    std::weak_ptr<A2UIMessageListener> weakSelf = weak_from_this();
    postToMainThread([weakSelf, surfaceId](napi_env env) {
        auto self = weakSelf.lock();
        if (!self || !self->surfaceManager_) {
            // Surface was already torn down with the listener; mapping cleanup is still safe.
            A2UIMessageListener::unregisterSurfaceMappingStatic(surfaceId);
            return;
        }
        for (auto& ref : self->listeners_) {
            napi_value listener = nullptr;
            napi_get_reference_value(env, ref, &listener);
            if (!listener) continue;

            napi_value onDestroyedFunc = nullptr;
            napi_get_named_property(env, listener, "onDeleteSurface", &onDestroyedFunc);

            napi_valuetype funcType = napi_undefined;
            if (onDestroyedFunc) napi_typeof(env, onDestroyedFunc, &funcType);
            if (funcType == napi_function) {
                napi_value surfaceIdValue = nullptr;
                napi_create_string_utf8(env, surfaceId.c_str(), NAPI_AUTO_LENGTH, &surfaceIdValue);
                napi_value result = nullptr;
                napi_call_function(env, listener, onDestroyedFunc, 1, &surfaceIdValue, &result);
            }
        }

        self->surfaceManager_->destroySurface(surfaceId);

        // Remove the surfaceId -> instanceId mapping.
        A2UIMessageListener::unregisterSurfaceMappingStatic(surfaceId);

        HM_LOGI("Surface destroyed: %s, remaining: %d", surfaceId.c_str(), self->surfaceManager_->getSurfaceCount());
    });
}

void A2UIMessageListener::onComponentsUpdate(const std::string &surfaceId, const std::vector<ComponentsUpdateMessage> &msg) {
    for (size_t msgIndex = 0; msgIndex < msg.size(); ++msgIndex) {
        std::string brief = A2UILogUtils::formatComponentBrief(msg[msgIndex].component);
        HM_LOGI("->msg%zu: %s", msgIndex + 1, brief.c_str());
    }

    // surfaceManager_ is only accessed on the main thread.
    std::weak_ptr<A2UIMessageListener> weakSelf = weak_from_this();
    postToMainThread([weakSelf, surfaceId, msg](napi_env /*env*/) {
        auto self = weakSelf.lock();
        if (!self || !self->surfaceManager_) return;  // listener torn down before task ran
        a2ui::A2UISurface* surface = self->surfaceManager_->getSurface(surfaceId);
        if (!surface) {
            HM_LOGE("onComponentsUpdate: Surface not found: %s", surfaceId.c_str());
            return;
        }
        surface->handleComponentsUpdate(msg);
        AGENUI_PERFORMANCE_LOG("components_applied", "%s", surfaceId.c_str());
    });
}

void A2UIMessageListener::onComponentsAdd(const std::string &surfaceId, const std::vector<ComponentsAddMessage> &msg) {
    for (size_t msgIndex = 0; msgIndex < msg.size(); ++msgIndex) {
        std::string brief = A2UILogUtils::formatComponentBrief(msg[msgIndex].component);
        HM_LOGI("->msg%zu: %s", msgIndex + 1, brief.c_str());
    }

    // surfaceManager_ is only accessed on the main thread.
    std::weak_ptr<A2UIMessageListener> weakSelf = weak_from_this();
    postToMainThread([weakSelf, surfaceId, msg](napi_env /*env*/) {
        auto self = weakSelf.lock();
        if (!self || !self->surfaceManager_) return;
        a2ui::A2UISurface* surface = self->surfaceManager_->getSurface(surfaceId);
        if (!surface) {
            HM_LOGE("onComponentsAdd: Surface not found: %s", surfaceId.c_str());
            return;
        }
        for (const auto& m : msg) {
            surface->handleComponentAdd(m);
        }
        AGENUI_PERFORMANCE_LOG("components_applied", "%s", surfaceId.c_str());
    });
}

void A2UIMessageListener::onComponentsRemove(const std::string &surfaceId, const std::vector<ComponentsRemoveMessage> &msg) {
    HM_LOGI("surfaceId: %s, count: %zu", surfaceId.c_str(), msg.size());

    // surfaceManager_ is only accessed on the main thread.
    std::weak_ptr<A2UIMessageListener> weakSelf = weak_from_this();
    postToMainThread([weakSelf, surfaceId, msg](napi_env /*env*/) {
        auto self = weakSelf.lock();
        if (!self || !self->surfaceManager_) return;
        a2ui::A2UISurface* surface = self->surfaceManager_->getSurface(surfaceId);
        if (!surface) {
            HM_LOGE("onComponentsRemove: Surface not found: %s", surfaceId.c_str());
            return;
        }
        surface->handleComponentsRemove(msg);
    });
}

void A2UIMessageListener::onActionEventRouted(const std::string &content) {
    HM_LOGI("instanceId=%d, content: %s", instanceId_, content.c_str());

    // listeners_ are only accessed on the main thread.
    std::string contentCopy = content;
    std::weak_ptr<A2UIMessageListener> weakSelf = weak_from_this();
    postToMainThread([weakSelf, contentCopy](napi_env env) {
        auto self = weakSelf.lock();
        if (!self) return;
        for (auto& ref : self->listeners_) {
            napi_value listener = nullptr;
            napi_get_reference_value(env, ref, &listener);
            if (!listener) continue;

            napi_value onActionEventRoutedFunc = nullptr;
            napi_get_named_property(env, listener, "onActionEventRouted", &onActionEventRoutedFunc);

            napi_valuetype funcType = napi_undefined;
            if (onActionEventRoutedFunc) napi_typeof(env, onActionEventRoutedFunc, &funcType);
            if (funcType == napi_function) {
                napi_value contentValue = nullptr;
                napi_create_string_utf8(env, contentCopy.c_str(), NAPI_AUTO_LENGTH, &contentValue);
                napi_value result = nullptr;
                napi_call_function(env, listener, onActionEventRoutedFunc, 1, &contentValue, &result);
            }
        }
    });
}

void A2UIMessageListener::onError(const ErrorMessage& msg) {
    HM_LOGE("instanceId=%d, code=%d, surfaceId=%s, message=%s",
            instanceId_, msg.code, msg.surfaceId.c_str(), msg.message.c_str());

    int32_t code = msg.code;
    std::string surfaceId = msg.surfaceId;
    std::string message = msg.message;
    std::weak_ptr<A2UIMessageListener> weakSelf = weak_from_this();
    postToMainThread([weakSelf, code, surfaceId, message](napi_env env) {
        auto self = weakSelf.lock();
        if (!self) return;
        for (auto& ref : self->listeners_) {
            napi_value listener = nullptr;
            napi_get_reference_value(env, ref, &listener);
            if (!listener) continue;

            napi_value onErrorFunc = nullptr;
            napi_get_named_property(env, listener, "onError", &onErrorFunc);

            napi_valuetype funcType = napi_undefined;
            if (onErrorFunc) napi_typeof(env, onErrorFunc, &funcType);
            if (funcType == napi_function) {
                napi_value codeValue = nullptr;
                napi_create_int32(env, code, &codeValue);
                napi_value surfaceIdValue = nullptr;
                napi_create_string_utf8(env, surfaceId.c_str(), NAPI_AUTO_LENGTH, &surfaceIdValue);
                napi_value messageValue = nullptr;
                napi_create_string_utf8(env, message.c_str(), NAPI_AUTO_LENGTH, &messageValue);
                napi_value args[] = { codeValue, surfaceIdValue, messageValue };
                napi_value result = nullptr;
                napi_call_function(env, listener, onErrorFunc, 3, args, &result);
            }
        }
    });
}

void A2UIMessageListener::registerListener(napi_value listener) {
    napi_env env = a2ui_get_napi_env();
    
    napi_ref ref = nullptr;
    napi_create_reference(env, listener, 1, &ref);
    listeners_.push_back(ref);
    
    HM_LOGI("instanceId=%d, listener registered, total: %zu", instanceId_, listeners_.size());
}

void A2UIMessageListener::unregisterListener(napi_value listener) {
    napi_env env = a2ui_get_napi_env();
    
    if (listeners_.empty()) {
        HM_LOGW("instanceId=%d, no listeners registered", instanceId_);
        return;
    }
    
    for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
        napi_value existingListener = nullptr;
        napi_get_reference_value(env, *it, &existingListener);
        
        bool isEqual = false;
        napi_strict_equals(env, listener, existingListener, &isEqual);
        
        if (isEqual) {
            napi_delete_reference(env, *it);
            listeners_.erase(it);
            HM_LOGI("instanceId=%d, listener unregistered, remaining: %zu", instanceId_, listeners_.size());
            return;
        }
    }
    
    HM_LOGW("instanceId=%d, listener not found", instanceId_);
}

} // namespace agenui
