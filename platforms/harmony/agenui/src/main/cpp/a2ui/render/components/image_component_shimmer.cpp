#include "image_component.h"
#include "a2ui/utils/a2ui_animate_utils.h"
#include "log/a2ui_capi_log.h"
#include <arkui/native_animate.h>
#include <arkui/native_node_napi.h>

namespace a2ui {

void ImageComponent::stopShimmer() {
    if (m_shimmerAnimator) {
        ArkUI_NativeAnimateAPI_1* animateApi = getAnimateApi();
        if (animateApi) {
            OH_ArkUI_Animator_Cancel(m_shimmerAnimator);
            animateApi->disposeAnimator(m_shimmerAnimator);
        }
        m_shimmerAnimator = nullptr;
    }

    if (m_shimmerNode) {
        if (m_nodeHandle) {
            g_nodeAPI->removeChild(m_nodeHandle, m_shimmerNode);
        }
        g_nodeAPI->disposeNode(m_shimmerNode);
        m_shimmerNode = nullptr;
    }

    m_shimmerPending = false;

    HM_LOGI("id=%s", m_id.c_str());
}

void ImageComponent::startShimmer() {
    if (!m_nodeHandle) {
        return;
    }

    stopShimmer();
    m_shimmerPending = true;

    ArkUI_ContextHandle context = OH_ArkUI_GetContextByNode(m_nodeHandle);
    if (context) {
        auto* payloadRef = new std::shared_ptr<ImageCallbackPayload>(m_callbackPayload);
        int32_t postResult = OH_ArkUI_PostFrameCallback(
            context,
            payloadRef,
            [](uint64_t, uint32_t, void* userData) {
                auto* ref = static_cast<std::shared_ptr<ImageCallbackPayload>*>(userData);
                std::shared_ptr<ImageCallbackPayload> payload = *ref;
                delete ref;
                if (payload->component != nullptr) {
                    payload->component->createShimmerLayerIfNeeded();
                }
            });
        if (postResult != ARKUI_ERROR_CODE_NO_ERROR) {
            delete payloadRef;
            createShimmerLayerIfNeeded();
        }
    } else {
        createShimmerLayerIfNeeded();
    }

    HM_LOGI("id=%s", m_id.c_str());
}

void ImageComponent::createShimmerLayerIfNeeded() {
    if (!m_shimmerPending || m_shimmerNode != nullptr || !m_nodeHandle) {
        return;
    }

    float width = getWidth();
    float height = getHeight();

    if (width <= 0.0f || height <= 0.0f) {
        ArkUI_ContextHandle context = OH_ArkUI_GetContextByNode(m_nodeHandle);
        if (context) {
            auto* payloadRef = new std::shared_ptr<ImageCallbackPayload>(m_callbackPayload);
            int32_t postResult = OH_ArkUI_PostFrameCallback(
                context,
                payloadRef,
                [](uint64_t, uint32_t, void* userData) {
                    auto* ref = static_cast<std::shared_ptr<ImageCallbackPayload>*>(userData);
                    std::shared_ptr<ImageCallbackPayload> payload = *ref;
                    delete ref;
                    if (payload->component != nullptr) {
                        payload->component->createShimmerLayerIfNeeded();
                    }
                });
            if (postResult != ARKUI_ERROR_CODE_NO_ERROR) {
                delete payloadRef;
            }
        }
        return;
    }

    m_shimmerPending = false;

    m_shimmerNode = g_nodeAPI->createNode(ARKUI_NODE_STACK);
    if (!m_shimmerNode) {
        HM_LOGE("Failed to create shimmer node, id=%s", m_id.c_str());
        return;
    }

    A2UINode shimmerNodeView(m_shimmerNode);
    shimmerNodeView.setWidth(width);
    shimmerNodeView.setHeight(height);
    shimmerNodeView.setHitTestBehavior(ARKUI_HIT_TEST_MODE_NONE);
    shimmerNodeView.setPosition(0.0f, 0.0f);

    applyShimmerGradient(-0.8f);

    g_nodeAPI->addChild(m_nodeHandle, m_shimmerNode);

    startShimmerAnimation(width, width);

    HM_LOGI("Shimmer layer created, id=%s, width=%f, height=%f",
        m_id.c_str(), width, height);
}

void ImageComponent::applyShimmerGradient(float offset) {
    if (!m_shimmerNode) return;

    constexpr uint32_t kBase = 0xFFE5E5EA;
    constexpr uint32_t kHighlight = 0xFFF8F8FA;
    static const uint32_t colors[5] = {kBase, kBase, kHighlight, kBase, kBase};
    static const float kLoc[5] = {0.0f, 0.35f, 0.50f, 0.65f, 1.0f};
    auto clampF = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    float stops[5];
    for (int i = 0; i < 5; i++) stops[i] = clampF(kLoc[i] + offset);

    ArkUI_ColorStop colorStop;
    colorStop.colors = colors;
    colorStop.stops = stops;
    colorStop.size = 5;

    ArkUI_NumberValue gradientVal[3];
    gradientVal[0].f32 = 135.0f;
    gradientVal[1].i32 = ARKUI_LINEAR_GRADIENT_DIRECTION_CUSTOM;
    gradientVal[2].i32 = 0;
    ArkUI_AttributeItem gradientItem = {gradientVal, 3, nullptr, &colorStop};
    g_nodeAPI->setAttribute(m_shimmerNode, NODE_LINEAR_GRADIENT, &gradientItem);
}

void ImageComponent::startShimmerAnimation(float shimmerWidth, float containerWidth) {
    if (!m_shimmerNode) return;

    ArkUI_ContextHandle context = OH_ArkUI_GetContextByNode(m_nodeHandle);
    ArkUI_NativeAnimateAPI_1* animateApi = getAnimateApi();
    if (!context || !animateApi) {
        HM_LOGW("no context or animateApi, id=%s", m_id.c_str());
        return;
    }

    ArkUI_AnimatorOption* option = OH_ArkUI_AnimatorOption_Create(0);
    if (!option) return;

    OH_ArkUI_AnimatorOption_SetDuration(option, 1200);
    OH_ArkUI_AnimatorOption_SetBegin(option, -0.8f);
    OH_ArkUI_AnimatorOption_SetEnd(option, 0.8f);
    OH_ArkUI_AnimatorOption_SetIterations(option, -1);
    OH_ArkUI_AnimatorOption_SetFill(option, ARKUI_ANIMATION_FILL_MODE_FORWARDS);

    ArkUI_CurveHandle curve = OH_ArkUI_Curve_CreateCubicBezierCurve(0.42f, 0.0f, 0.58f, 1.0f);
    if (curve) OH_ArkUI_AnimatorOption_SetCurve(option, curve);

    auto* animPayloadRef = new std::shared_ptr<ImageCallbackPayload>(m_callbackPayload);

    OH_ArkUI_AnimatorOption_RegisterOnFrameCallback(
        option, animPayloadRef,
        [](ArkUI_AnimatorOnFrameEvent* event) {
            auto* payloadRef = static_cast<std::shared_ptr<ImageCallbackPayload>*>(
                OH_ArkUI_AnimatorOnFrameEvent_GetUserData(event));
            if (!payloadRef) return;
            std::shared_ptr<ImageCallbackPayload> payload = *payloadRef;
            ImageComponent* component = payload->component;
            if (!component || !component->m_shimmerNode) return;

            float offset = OH_ArkUI_AnimatorOnFrameEvent_GetValue(event);
            static const float kLoc[5] = {0.0f, 0.35f, 0.50f, 0.65f, 1.0f};
            constexpr uint32_t kBase = 0xFFE5E5EA;
            constexpr uint32_t kHighlight = 0xFFF8F8FA;
            static const uint32_t colors[5] = {kBase, kBase, kHighlight, kBase, kBase};
            auto clampF = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
            float stops[5];
            for (int i = 0; i < 5; i++) stops[i] = clampF(kLoc[i] + offset);

            ArkUI_ColorStop cs;
            cs.colors = colors;
            cs.stops = stops;
            cs.size = 5;
            ArkUI_NumberValue gv[3];
            gv[0].f32 = 135.0f;
            gv[1].i32 = ARKUI_LINEAR_GRADIENT_DIRECTION_CUSTOM;
            gv[2].i32 = 0;
            ArkUI_AttributeItem gi = {gv, 3, nullptr, &cs};
            g_nodeAPI->setAttribute(component->m_shimmerNode, NODE_LINEAR_GRADIENT, &gi);
        });

    OH_ArkUI_AnimatorOption_RegisterOnFinishCallback(
        option, animPayloadRef,
        [](ArkUI_AnimatorEvent* event) {
            auto* payloadRef = static_cast<std::shared_ptr<ImageCallbackPayload>*>(
                OH_ArkUI_AnimatorEvent_GetUserData(event));
            delete payloadRef;
        });

    OH_ArkUI_AnimatorOption_RegisterOnCancelCallback(
        option, animPayloadRef,
        [](ArkUI_AnimatorEvent* event) {
            auto* payloadRef = static_cast<std::shared_ptr<ImageCallbackPayload>*>(
                OH_ArkUI_AnimatorEvent_GetUserData(event));
            delete payloadRef;
        });

    m_shimmerAnimator = animateApi->createAnimator(context, option);
    if (m_shimmerAnimator) {
        int32_t playResult = OH_ArkUI_Animator_Play(m_shimmerAnimator);
        if (playResult != ARKUI_ERROR_CODE_NO_ERROR) {
            HM_LOGW("Failed to play, result=%d, id=%s", playResult, m_id.c_str());
            animateApi->disposeAnimator(m_shimmerAnimator);
            m_shimmerAnimator = nullptr;
            delete animPayloadRef;
        } else {
            HM_LOGI("Animation started, id=%s", m_id.c_str());
        }
    } else {
        HM_LOGW("createAnimator failed, id=%s", m_id.c_str());
        delete animPayloadRef;
    }

    if (curve) OH_ArkUI_Curve_DisposeCurve(curve);
    OH_ArkUI_AnimatorOption_Dispose(option);
}

} // namespace a2ui
