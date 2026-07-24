#include "image_component.h"
#include "log/a2ui_capi_log.h"
#include "a2ui/utils/a2ui_parse_utils.h"
#include "a2ui/utils/a2ui_padding_utils.h"
#include "a2ui/bridge/image_loader_bridge.h"
#include "a2ui/measure/a2ui_platform_layout_bridge.h"
#include "a2ui/utils/a2ui_unit_utils.h"

namespace a2ui {

ImageComponent::ImageComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, "Image") {

    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_IMAGE);

    A2UIImageNode node(m_nodeHandle);
    node.setObjectFitCover();
    node.setTransformCenterPercent(0.5f, 0.5f);

    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
    }

    m_callbackPayload = std::make_shared<ImageCallbackPayload>();
    m_callbackPayload->component = this;
    m_payloadRef = new std::shared_ptr<ImageCallbackPayload>(m_callbackPayload);

    g_nodeAPI->addNodeEventReceiver(m_nodeHandle, onImageCompleteCallback);
    g_nodeAPI->registerNodeEvent(m_nodeHandle, NODE_IMAGE_ON_COMPLETE, 0, m_payloadRef);

    HM_LOGI( "ImageComponent - Created: id=%s, handle=%s",
                id.c_str(), m_nodeHandle ? "valid" : "null");
}

ImageComponent::~ImageComponent() {
    cancelRevealAnimators();

    if (m_callbackPayload) {
        m_callbackPayload->component = nullptr;
    }

    stopShimmer();

    if (m_nodeHandle) {
        g_nodeAPI->unregisterNodeEvent(m_nodeHandle, NODE_IMAGE_ON_COMPLETE);
        g_nodeAPI->removeNodeEventReceiver(m_nodeHandle, onImageCompleteCallback);
    }
    if (m_payloadRef) {
        delete m_payloadRef;
        m_payloadRef = nullptr;
    }

    HM_LOGI("ImageComponent - Destroyed: id=%s", m_id.c_str());
}

void ImageComponent::onDestroy() {
    HM_LOGI("ImageComponent::onDestroy - id=%s", m_id.c_str());

    cancelRevealAnimators();

    if (m_callbackPayload) {
        m_callbackPayload->component = nullptr;
    }

    if (!m_currentRequestId.empty()) {
        ImageLoaderBridge::getInstance().cancel(m_currentRequestId);
        m_currentRequestId.clear();
    }

    stopShimmer();

    if (m_nodeHandle) {
        g_nodeAPI->unregisterNodeEvent(m_nodeHandle, NODE_IMAGE_ON_COMPLETE);
        g_nodeAPI->removeNodeEventReceiver(m_nodeHandle, onImageCompleteCallback);
    }

    if (m_payloadRef) {
        delete m_payloadRef;
        m_payloadRef = nullptr;
    }

}

void ImageComponent::createView() {
    // Delegates to base createView(): sets m_viewCreated, calls onCreateView(),
    // recursively creates children, then applies stored properties via
    // updateProperties() → onUpdateProperties() → applyUrl().
    //
    // In HarmonyOS the ArkUI image node is created in the constructor, so
    // there is no node to create here.  The override exists to participate
    // in the cross-platform createView() lifecycle — when a lazy container
    // (e.g. horizontal List) defers this component, applyUrl() will not fire
    // until createView() is invoked by the adapter.
    // Mirrors iOS ImageComponent.createView() which sets up imageView before
    // loadImage() can execute.
    A2UIComponent::createView();

    HM_LOGI("ImageComponent::createView completed, id=%s, url=%s",
            m_id.c_str(), m_currentUrl.c_str());
}

void ImageComponent::onUpdateProperties(const nlohmann::json& properties) {
    // Defer all property application until createView() has been called.
    // For lazy containers (e.g. horizontal List), updateProperties may be
    // called before createView() — properties are already stored in
    // m_properties by the base class, and will be fully applied when
    // createView() calls updateProperties(m_properties).
    // Mirrors iOS Component.updateProperties() which guards on isViewCreated.
    if (!isViewCreated()) {
        return;
    }

    if (!m_nodeHandle) {
        HM_LOGE( "handle is null, id=%s", m_id.c_str());
        return;
    }

    applyFit(properties);
    applyStyles(properties);
    applyBackgroundColor(properties);

    float yogaWidth = 0.0f;
    float yogaHeight = 0.0f;
    bool urlChanged = false;
    bool sizeChanged = false;

    if (properties.contains("url")) {
        std::string url = extractStringValue(properties["url"]);
        urlChanged = (url != m_currentUrl);
    }

    // styles is sent in full each time, so key-existence checks are always true.
    // Compare actual values to detect real size changes.
    if (properties.contains("styles") && properties["styles"].is_object()) {
        const auto& styles = properties["styles"];

        nlohmann::json newWidth, newHeight;
        if (styles.contains("width")) newWidth = styles["width"];
        if (styles.contains("height")) newHeight = styles["height"];

        if (newWidth != m_currentWidth || newHeight != m_currentHeight) {
            sizeChanged = true;
            m_currentWidth = newWidth;
            m_currentHeight = newHeight;
        }

        if (styles.contains("width") && styles["width"].is_number()) {
            yogaWidth = styles["width"].get<float>();
        }
        if (styles.contains("height") && styles["height"].is_number()) {
            yogaHeight = styles["height"].get<float>();
        }
    }

    // Only reload when URL or size values actually changed.
    if (urlChanged || (sizeChanged && !m_currentUrl.empty())) {
        applyUrl(properties, yogaWidth, yogaHeight);
    }

    HM_LOGI( "Applied properties, id=%s", m_id.c_str());
}


    void ImageComponent::applyUrl(const nlohmann::json &properties, float yogaWidth, float yogaHeight) {
    if (!properties.contains("url")) {
        HM_LOGI("no url field, id=%s", m_id.c_str());
        if (!m_currentUrl.empty()) {
            HM_LOGI("no url in props but m_currentUrl exists, reapplying src, id=%s", m_id.c_str());
            m_lastAnimatedUrl.clear();
            A2UIImageNode(m_nodeHandle).setSrc(m_currentUrl);
        }
        return;
    }

    std::string url = extractStringValue(properties["url"]);
    HM_LOGI("id=%s, newUrl=%s, currentUrl=%s, fadeEnabled=%s",
        m_id.c_str(), url.c_str(), m_currentUrl.c_str(),
        isImageFadeInEnabled() ? "true" : "false");
    if (url.empty()) {
        if (!m_currentRequestId.empty()) {
            ImageLoaderBridge::getInstance().cancel(m_currentRequestId);
            m_currentRequestId.clear();
        }
        m_currentUrl.clear();
        m_pendingFadeIn = false;
        stopShimmer();
        A2UIImageNode node(m_nodeHandle);
        node.resetBackgroundColor();
        node.resetOpacityTransition();
        node.setOpacity(1.0f);
        node.setScale(1.0f, 1.0f);
        HM_LOGW( "url is empty, id=%s", m_id.c_str());
        return;
    }

    bool urlChanged = (url != m_currentUrl);

    if (urlChanged && !m_currentRequestId.empty()) {
        HM_LOGI("url changed, cancel old requestId=%s, id=%s", m_currentRequestId.c_str(), m_id.c_str());
        ImageLoaderBridge::getInstance().cancel(m_currentRequestId);
        m_currentRequestId.clear();
    }

    // Check whether an external loader exists
    if (ImageLoaderBridge::getInstance().hasLoader() && urlChanged) {
        m_currentUrl = url;
        m_lastAnimatedUrl.clear();


        // Convert A2UI design units to physical pixels before passing the size hint
        // to the image loader. The loader contract (ImageLoadOptionsKey width/height)
        // is defined in physical px, matching Android ImageComponent's
        // StyleHelper.standardUnitToPx (px = a2ui / 2 * density).
        float hintW = UnitConverter::a2uiToPx(yogaWidth);
        float hintH = UnitConverter::a2uiToPx(yogaHeight);

        auto payloadRef = new std::shared_ptr<ImageCallbackPayload>(m_callbackPayload);
        std::string requestId = ImageLoaderBridge::getInstance().loadImage({
            url,
            hintW,
            hintH,
            m_id,
            getSurfaceId(),
            m_nodeHandle,
            [payloadRef](const std::string& rid, bool success, bool isCancelled) {
                std::shared_ptr<ImageCallbackPayload> payload = *payloadRef;
                delete payloadRef;
                ImageComponent* component = payload->component;
                if (component == nullptr) {
                    HM_LOGW("image_loader callback: component already destroyed, requestId=%s", rid.c_str());
                    return;
                }
                if (component->m_currentRequestId != rid) {
                    HM_LOGW("image_loader callback: stale requestId=%s, current=%s, skip",
                        rid.c_str(), component->m_currentRequestId.c_str());
                    return;
                }
                component->m_currentRequestId.clear();

                if (isCancelled) {
                    HM_LOGI("image_loader callback: cancelled, id=%s url=%s",
                        component->m_id.c_str(), component->m_currentUrl.c_str());
                    component->stopShimmer();
                    return;
                }

                if (!success) {
                    HM_LOGW("image_loader callback: failed, keep loader failure state, id=%s url=%s",
                        component->m_id.c_str(), component->m_currentUrl.c_str());
                    component->stopShimmer();
                    return;
                }

                HM_LOGI("image_loader callback: success(PixelMap set), id=%s url=%s",
                    component->m_id.c_str(), component->m_currentUrl.c_str());
                component->stopShimmer();
            }
        });

        if (requestId.empty()) {
            HM_LOGW("image_loader: loadImage returned empty requestId, skip ArkUI fallback, id=%s", m_id.c_str());
            stopShimmer();
        } else {
            m_currentRequestId = requestId;
        }
        return;
    }

    A2UIImageNode node(m_nodeHandle);
    node.setSrc(url);
    m_currentUrl = url;
    if (urlChanged) {
        m_lastAnimatedUrl.clear();
    }

    HM_LOGI( "Set image src: %s", url.c_str());

    if (!isImageFadeInEnabled() || !m_surfaceAnimated) {
        HM_LOGI("fadeIn disabled (fadeIn=%s, surfaceAnimated=%s), skip shimmer, id=%s",
            isImageFadeInEnabled() ? "true" : "false", m_surfaceAnimated ? "true" : "false", m_id.c_str());
        return;
    }
}

void ImageComponent::applyFit(const nlohmann::json& properties) {
    if (!properties.contains("fit") || !properties["fit"].is_string()) {
        return;
    }

    A2UIImageNode node(m_nodeHandle);
    node.setObjectFit(static_cast<ArkUI_ObjectFit>(mapObjectFit(properties["fit"].get<std::string>())));
}


void ImageComponent::applyStyles(const nlohmann::json& properties) {
    if (!properties.contains("styles") || !properties["styles"].is_object()) {
        return;
    }

    const auto& styles = properties["styles"];

    if (styles.contains("border-radius")) {
        float radius = 0.0f;

        if (styles["border-radius"].is_number()) {
            radius = styles["border-radius"].get<float>();
        } else if (styles["border-radius"].is_string()) {
            radius = parseFloat(styles["border-radius"].get<std::string>(), 0.0f);
        }

        if (radius > 0.0f) {
            A2UIImageNode node(m_nodeHandle);
            node.setBorderRadius(radius);
        }
    }

    // Border width
    if (styles.contains("border-width")) {
        float width = 0.0f;

        if (styles["border-width"].is_number()) {
            width = styles["border-width"].get<float>();
        } else if (styles["border-width"].is_string()) {
            width = parseFloat(styles["border-width"].get<std::string>(), 0.0f);
        }

        if (width > 0.0f) {
            A2UIImageNode node(m_nodeHandle);
            node.setBorderWidth(width, width, width, width);
            node.setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
            HM_LOGI( "Set border-width: %f", width);
        }
    }

    // Border color
    if (styles.contains("border-color") && styles["border-color"].is_string()) {
        uint32_t color = parseColor(styles["border-color"].get<std::string>());
        A2UIImageNode node(m_nodeHandle);
        node.setBorderColor(color);
        HM_LOGI( "Set border-color: 0x%X", color);
    }

    // CSS padding -> ArkUI Image node setPadding
    //
    // ARKUI_NODE_IMAGE is a leaf node. Yoga has already accounted for padding
    // when computing the leaf's borderBox layout, but the underlying image
    // bitmap is drawn across the whole frame regardless. Translating CSS
    // padding to NODE_PADDING shrinks the bitmap's draw area to the
    // contentBox, mirroring Android `ImageView.setPadding(...)` and iOS
    // anchor-based insets. Per W3C, `<img>` is a replaced element that
    // honours `padding`. This is NOT a double-count with Yoga.
    {
        float pt = 0.0f, pr = 0.0f, pb = 0.0f, pl = 0.0f;
        ::a2ui::padding_utils::resolveUserPadding(styles, pt, pr, pb, pl);
        A2UINode imgBase(m_nodeHandle);
        if (::a2ui::padding_utils::hasAnyPadding(pt, pr, pb, pl)) {
            imgBase.setPadding(pt, pr, pb, pl);
        } else {
            imgBase.resetPadding();
        }
    }
}

int32_t ImageComponent::mapObjectFit(const std::string& fit) {
    if (fit == "contain") {
        return ARKUI_OBJECT_FIT_CONTAIN;
    } else if (fit == "cover") {
        return ARKUI_OBJECT_FIT_COVER;
    } else if (fit == "scaleDown") {
        return ARKUI_OBJECT_FIT_SCALE_DOWN;
    } else if (fit == "fill") {
        return ARKUI_OBJECT_FIT_FILL;
    } else if (fit == "none") {
        // none maps to fill: stretch to fill container
        return ARKUI_OBJECT_FIT_FILL;
    }
    // Default matches Android: FIT_CENTER (contain)
    return ARKUI_OBJECT_FIT_CONTAIN;
}


std::string ImageComponent::extractStringValue(const nlohmann::json& value) {
    return a2ui::extractStringValue(value);
}


void ImageComponent::onImageCompleteCallback(ArkUI_NodeEvent* event) {
    void* userData = OH_ArkUI_NodeEvent_GetUserData(event);
    if (!userData) {
        HM_LOGE( "userData is null");
        return;
    }

    auto* payloadRef = static_cast<std::shared_ptr<ImageCallbackPayload>*>(userData);
    std::shared_ptr<ImageCallbackPayload> payload = *payloadRef;

    if (payload->component == nullptr) {
        HM_LOGW("onImageCompleteCallback: component already destroyed (null), skip");
        return;
    }

    auto* component = payload->component;

    HM_LOGI( "Start, id=%s", component->m_id.c_str());

    ArkUI_NodeComponentEvent* componentEvent = OH_ArkUI_NodeEvent_GetNodeComponentEvent(event);
    ArkUI_NumberValue* data = componentEvent ? componentEvent->data : nullptr;

    // data[0]: loadingStatus (0=success, 1=fail)
    A2UIImageNode node(component->m_nodeHandle);

    if (!data) {
        HM_LOGW("data is null (abnormal event), "
                "fallback to stopShimmer+playMagicReveal, id=%s", component->m_id.c_str());
        component->stopShimmer();
        if (isImageFadeInEnabled() && component->m_surfaceAnimated && component->m_currentUrl != component->m_lastAnimatedUrl) {
            component->m_lastAnimatedUrl = component->m_currentUrl;
            float w = component->getWidth();
            float h = component->getHeight();
            component->playMagicReveal(1500, w, h);
        }
        return;
    }

    int32_t loadingStatus = data[0].i32;
    HM_LOGI("id=%s, status=%d, imageWidth=%f, imageHeight=%f, componentWidth=%f, componentHeight=%f",
        component->m_id.c_str(),
        loadingStatus,
        data[1].f32,
        data[2].f32,
        data[3].f32,
        data[4].f32);

    if (loadingStatus != 0) {
        HM_LOGW("loadingStatus=%d (non-zero), id=%s, imageWidth=%f, imageHeight=%f",
            loadingStatus, component->m_id.c_str(), data[1].f32, data[2].f32);
        component->stopShimmer();
        A2UIImageNode(component->m_nodeHandle).setOpacity(1.0f);
        A2UIImageNode(component->m_nodeHandle).setScale(1.0f, 1.0f);

        if (data[1].f32 > 0.0f && data[2].f32 > 0.0f) {
            if (!component->m_currentUrl.empty() &&
                component->m_currentUrl == component->m_lastAnimatedUrl) {
                HM_LOGW("status=1 duplicate, skip, id=%s", component->m_id.c_str());
                return;
            }
            component->m_lastAnimatedUrl = component->m_currentUrl;
            return;
        }

        if (!component->m_currentUrl.empty() &&
            component->m_currentUrl != component->m_lastAnimatedUrl) {
            HM_LOGW("reapplying src to recover from status=1, id=%s, url=%s",
                component->m_id.c_str(), component->m_currentUrl.c_str());
            component->m_lastAnimatedUrl = component->m_currentUrl;
            A2UIImageNode(component->m_nodeHandle).setSrc(component->m_currentUrl);
        }

        return;
    }

    if (!component->m_currentUrl.empty() && component->m_currentUrl == component->m_lastAnimatedUrl) {
        HM_LOGW("duplicate callback for url=%s, skip, id=%s",
            component->m_currentUrl.c_str(), component->m_id.c_str());
        return;
    }
    component->m_lastAnimatedUrl = component->m_currentUrl;

    component->stopShimmer();

    HM_LOGI("Image loaded successfully, id=%s, imageWidth=%f, imageHeight=%f",
        component->m_id.c_str(), data[1].f32, data[2].f32);

    if (isImageFadeInEnabled() && component->m_surfaceAnimated) {
        float pw = component->getWidth();
        float ph = component->getHeight();
        component->playMagicReveal(1500, pw, ph);
    } else {
        HM_LOGI("fadeIn disabled (fadeIn=%s, surfaceAnimated=%s), skip playMagicReveal, id=%s",
            isImageFadeInEnabled() ? "true" : "false",
            component->m_surfaceAnimated ? "true" : "false",
            component->m_id.c_str());
    }
}

} // namespace a2ui
