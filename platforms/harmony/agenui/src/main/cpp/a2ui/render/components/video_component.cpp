#include "video_component.h"
#include "../a2ui_node.h"
#include "a2ui/utils/a2ui_color_palette.h"
#include "log/a2ui_capi_log.h"

namespace a2ui {

using colors::kColorBlack;

std::mutex VideoComponent::s_instanceMutex;
std::map<OH_NativeXComponent*, VideoComponent*>& VideoComponent::getInstanceMap() {
    static auto* p = new std::map<OH_NativeXComponent*, VideoComponent*>();
    return *p;
}

VideoComponent::VideoComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, "Video") {

    m_xcomponentId = "video_xcomp_" + id;

    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_STACK);

    {
        A2UINode node(m_nodeHandle);
        node.setPercentWidth(1.0f);
        node.setBackgroundColor(kColorBlack);
        node.setClip(true);
    }

    m_xcomponentHandle = g_nodeAPI->createNode(ARKUI_NODE_XCOMPONENT);

    ArkUI_AttributeItem xcompIdItem = {nullptr, 0, m_xcomponentId.c_str(), nullptr};
    g_nodeAPI->setAttribute(m_xcomponentHandle, NODE_XCOMPONENT_ID, &xcompIdItem);

    ArkUI_NumberValue xcompTypeValue[] = {{.i32 = ARKUI_XCOMPONENT_TYPE_SURFACE}};
    ArkUI_AttributeItem xcompTypeItem = {xcompTypeValue, 1, nullptr, nullptr};
    g_nodeAPI->setAttribute(m_xcomponentHandle, NODE_XCOMPONENT_TYPE, &xcompTypeItem);

    {
        A2UINode xcomp(m_xcomponentHandle);
        xcomp.setPercentWidth(1.0f);
        xcomp.setPercentHeight(1.0f);
    }

    m_nativeXComponent = OH_NativeXComponent_GetNativeXComponent(m_xcomponentHandle);
    if (m_nativeXComponent) {
        m_xcompCallback.OnSurfaceCreated = onSurfaceCreatedCB;
        m_xcompCallback.OnSurfaceChanged = onSurfaceChangedCB;
        m_xcompCallback.OnSurfaceDestroyed = onSurfaceDestroyedCB;
        m_xcompCallback.DispatchTouchEvent = dispatchTouchEventCB;
        OH_NativeXComponent_RegisterCallback(m_nativeXComponent, &m_xcompCallback);
        registerInstance(m_nativeXComponent);
    }

    g_nodeAPI->addChild(m_nodeHandle, m_xcomponentHandle);

    m_avPlayer = OH_AVPlayer_Create();
    if (m_avPlayer) {
        m_playerCallbackValid.store(true);
        OH_AVPlayer_SetOnInfoCallback(m_avPlayer, onPlayerInfoCallback, this);
        OH_AVPlayer_SetOnErrorCallback(m_avPlayer, onPlayerErrorCallback, this);
    }

    createControlsBar();

    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
    }

    HM_LOGI( "VideoComponent - Created: id=%s, xcompId=%s, avPlayer=%s",
                id.c_str(), m_xcomponentId.c_str(), m_avPlayer ? "valid" : "null");
}

VideoComponent::~VideoComponent() {
    HM_LOGI( "VideoComponent - Destroying: id=%s", m_id.c_str());
    releasePlayer();
    unregisterInstance();
    destroyControlsBar();

    if (m_xcomponentHandle && m_nodeHandle) {
        g_nodeAPI->removeChild(m_nodeHandle, m_xcomponentHandle);
        g_nodeAPI->disposeNode(m_xcomponentHandle);
        m_xcomponentHandle = nullptr;
    }
    m_nativeXComponent = nullptr;
    m_window = nullptr;

    HM_LOGI( "VideoComponent - Destroyed: id=%s", m_id.c_str());
}

void VideoComponent::onDestroy() {
    releasePlayer();
    destroyControlsBar();
}


void VideoComponent::registerInstance(OH_NativeXComponent* xcomp) {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    getInstanceMap()[xcomp] = this;
}

void VideoComponent::unregisterInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    for (auto it = getInstanceMap().begin(); it != getInstanceMap().end(); ++it) {
        if (it->second == this) {
            getInstanceMap().erase(it);
            break;
        }
    }
}


void VideoComponent::onUpdateProperties(const nlohmann::json& properties) {
    if (!m_nodeHandle) {
        HM_LOGE( "handle is null, id=%s", m_id.c_str());
        return;
    }

    if (properties.find("autoPlay") != properties.end()) {
        const auto& autoPlayVal = properties["autoPlay"];
        if (autoPlayVal.is_boolean()) {
            m_autoPlay = autoPlayVal.get<bool>();
        } else if (autoPlayVal.is_string()) {
            m_autoPlay = (autoPlayVal.get<std::string>() == "true");
        }
    }

    if (properties.find("controls") != properties.end()) {
        const auto& controlsVal = properties["controls"];
        if (controlsVal.is_boolean()) {
            m_controls = controlsVal.get<bool>();
        } else if (controlsVal.is_string()) {
            m_controls = (controlsVal.get<std::string>() == "true");
        }
    }

    if (properties.find("url") != properties.end()) {
        const auto& urlVal = properties["url"];
        if (urlVal.is_string()) {
            std::string newUrl = urlVal.get<std::string>();
            if (!newUrl.empty()) {
                setVideoUrl(newUrl);
            }
        }
    }

    updateControlsBarLayout();

    HM_LOGI( "url=%s, autoPlay=%d, controls=%d",
                m_videoUrl.c_str(), m_autoPlay ? 1 : 0, m_controls ? 1 : 0);
}


void VideoComponent::onSurfaceCreatedCB(OH_NativeXComponent* component, void* window) {
    if (!component || !window) {
        HM_LOGE( "Invalid params");
        return;
    }

    VideoComponent* self = nullptr;
    std::string videoUrl;
    {
        std::lock_guard<std::mutex> lock(s_instanceMutex);
        auto it = getInstanceMap().find(component);
        if (it != getInstanceMap().end() && it->second) {
            self = it->second;
            self->m_window = reinterpret_cast<OHNativeWindow*>(window);
            videoUrl = self->m_videoUrl;
            HM_LOGI( "id=%s, window=%p",
                        self->m_id.c_str(), window);
        }
    }
    if (self && !videoUrl.empty()) {
        self->handleVideoPrepare(videoUrl);
    }
}

void VideoComponent::onSurfaceChangedCB(OH_NativeXComponent* component, void* window) {
    if (!component || !window) return;

    std::lock_guard<std::mutex> lock(s_instanceMutex);
    auto it = getInstanceMap().find(component);
    if (it != getInstanceMap().end() && it->second) {
        VideoComponent* self = it->second;
        self->m_window = reinterpret_cast<OHNativeWindow*>(window);
        HM_LOGI( "id=%s", self->m_id.c_str());
    }
}

void VideoComponent::onSurfaceDestroyedCB(OH_NativeXComponent* component, void* window) {
    if (!component) return;

    std::lock_guard<std::mutex> lock(s_instanceMutex);
    auto it = getInstanceMap().find(component);
    if (it != getInstanceMap().end() && it->second) {
        VideoComponent* self = it->second;
        HM_LOGI( "id=%s", self->m_id.c_str());
        self->m_window = nullptr;
    }
}

void VideoComponent::dispatchTouchEventCB(OH_NativeXComponent* component, void* window) {
    if (!component) return;

    std::lock_guard<std::mutex> lock(s_instanceMutex);
    auto it = getInstanceMap().find(component);
    if (it == getInstanceMap().end() || !it->second) return;

    VideoComponent* self = it->second;

    OH_NativeXComponent_TouchEvent touchEvent;
    OH_NativeXComponent_GetTouchEvent(component, window, &touchEvent);

    if (touchEvent.type == OH_NativeXComponent_TouchEventType::OH_NATIVEXCOMPONENT_UP) {
        HM_LOGI( "Touch UP, toggling controls, id=%s",
                    self->m_id.c_str());
        self->toggleControlsBar();
    }
}

} // namespace a2ui
