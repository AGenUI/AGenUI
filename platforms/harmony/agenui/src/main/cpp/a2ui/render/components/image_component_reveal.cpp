#include "image_component.h"
#include "a2ui/utils/a2ui_animate_utils.h"
#include "a2ui/utils/a2ui_color_palette.h"
#include "log/a2ui_capi_log.h"

namespace a2ui {

namespace {

ArkUI_AnimatorHandle createSweepAnimator(
    ArkUI_ContextHandle context,
    ArkUI_NativeAnimateAPI_1* animateApi,
    ArkUI_NodeHandle node,
    float beginOffset,
    float endOffset,
    int32_t durationMs,
    void* userData,
    void (*onFrameCb)(ArkUI_AnimatorOnFrameEvent*),
    void (*onFinishCb)(ArkUI_AnimatorEvent*),
    void (*onCancelCb)(ArkUI_AnimatorEvent*)
) {
    ArkUI_AnimatorOption* option = OH_ArkUI_AnimatorOption_Create(0);
    if (!option) return nullptr;

    OH_ArkUI_AnimatorOption_SetDuration(option, durationMs);
    OH_ArkUI_AnimatorOption_SetBegin(option, beginOffset);
    OH_ArkUI_AnimatorOption_SetEnd(option, endOffset);
    OH_ArkUI_AnimatorOption_SetIterations(option, 1);
    OH_ArkUI_AnimatorOption_SetFill(option, ARKUI_ANIMATION_FILL_MODE_FORWARDS);
    OH_ArkUI_AnimatorOption_SetDirection(option, ARKUI_ANIMATION_DIRECTION_NORMAL);
    ArkUI_CurveHandle curve = OH_ArkUI_Curve_CreateCubicBezierCurve(0.42f, 0.0f, 0.58f, 1.0f);
    if (curve) OH_ArkUI_AnimatorOption_SetCurve(option, curve);

    if (onFrameCb) OH_ArkUI_AnimatorOption_RegisterOnFrameCallback(option, userData, onFrameCb);
    if (onFinishCb) OH_ArkUI_AnimatorOption_RegisterOnFinishCallback(option, userData, onFinishCb);
    if (onCancelCb) OH_ArkUI_AnimatorOption_RegisterOnCancelCallback(option, userData, onCancelCb);

    ArkUI_AnimatorHandle handle = animateApi->createAnimator(context, option);
    if (curve) OH_ArkUI_Curve_DisposeCurve(curve);
    OH_ArkUI_AnimatorOption_Dispose(option);
    return handle;
}

} // namespace

void ImageComponent::prepareFadeInForUrl(const std::string& url) {
    if (!m_nodeHandle) return;
    A2UIImageNode node(m_nodeHandle);
    node.setOpacity(0.0f);
    node.setScale(kImageFadeInStartScale, kImageFadeInStartScale);
    m_pendingFadeIn = true;
    HM_LOGI("opacity=0, scale=%.2f, id=%s", kImageFadeInStartScale, m_id.c_str());
}

void ImageComponent::playFadeInIfNeeded() {
    if (!m_pendingFadeIn || !m_nodeHandle) {
        return;
    }
    m_pendingFadeIn = false;
    playMagicReveal(1500);
    HM_LOGI("scheduled MagicReveal animation, id=%s", m_id.c_str());
}

void ImageComponent::playMagicReveal(int32_t durationMs, float hintW, float hintH) {
    if (!m_nodeHandle) return;

    ArkUI_NativeAnimateAPI_1* animateApi = getAnimateApi();
    ArkUI_NodeHandle imageNode = m_nodeHandle;
    ArkUI_ContextHandle context = OH_ArkUI_GetContextByNode(m_nodeHandle);

    if (!context || !animateApi) {
        HM_LOGW("prerequisites missing, id=%s", m_id.c_str());
        return;
    }

    float nodeW = getWidth();
    float nodeH = getHeight();
    if (nodeW <= 0.0f && hintW > 0.0f) nodeW = hintW;
    if (nodeH <= 0.0f && hintH > 0.0f) nodeH = hintH;
    if (hintW > 0.0f && nodeW > 0.0f && nodeW < hintW * 0.2f) {
        HM_LOGW("nodeW(%.1f) abnormally small vs hint(%.1f), using hint, id=%s",
            nodeW, hintW, m_id.c_str());
        nodeW = hintW;
    }
    if (hintH > 0.0f && nodeH > 0.0f && nodeH < hintH * 0.2f) {
        HM_LOGW("nodeH(%.1f) abnormally small vs hint(%.1f), using hint, id=%s",
            nodeH, hintH, m_id.c_str());
        nodeH = hintH;
    }
    if (nodeW <= 0.0f || nodeH <= 0.0f) {
        HM_LOGW("invalid size(%.1f,%.1f) hint(%.1f,%.1f), id=%s",
            nodeW, nodeH, hintW, hintH, m_id.c_str());
        return;
    }
    HM_LOGI("nodeW=%.1f nodeH=%.1f hint(%.1f,%.1f) id=%s",
        nodeW, nodeH, hintW, hintH, m_id.c_str());

    if (m_revealMaskNode) {
        g_nodeAPI->removeChild(m_nodeHandle, m_revealMaskNode);
        g_nodeAPI->disposeNode(m_revealMaskNode);
        m_revealMaskNode = nullptr;
    }

    A2UIImageNode node(m_nodeHandle);
    node.setOpacity(1.0f);
    node.setScale(kImageFadeInStartScale, kImageFadeInStartScale);

    struct RevealPayload {
        ArkUI_NodeHandle imageNode = nullptr;
        ArkUI_NodeHandle maskNode = nullptr;
        ArkUI_NodeHandle glassNode = nullptr;
        ArkUI_AnimatorHandle maskAnim = nullptr;
        ArkUI_AnimatorHandle glassAnim = nullptr;
        uint32_t maskColors[4] = {
            colors::kColorTransparentWhite,
            colors::kColorTransparentWhite,
            0x22FFFFFF,
            colors::kColorWhite,
        };
        float maskStops[4] = {0.0f, 0.55f, 0.85f, 1.0f};
        uint32_t glassColors[7] = {
            colors::kColorTransparentWhite,
            0x1FFFF5E0, 0x59FFEEDD, 0x80FFFFFF,
            0x4DD9EBFF, 0x1FFFFFFF, colors::kColorTransparentWhite,
        };
        float glassStops[7] = {0.0f, 0.25f, 0.38f, 0.50f, 0.62f, 0.75f, 1.0f};
        int finishCount = 0;
    };
    RevealPayload* rp = new RevealPayload();
    rp->imageNode = m_nodeHandle;

    auto revealCleanup = [](RevealPayload* p) {
        p->finishCount++;
        if (p->finishCount < 2 && p->glassNode != nullptr) return;
        if (p->maskNode && p->imageNode) {
            g_nodeAPI->removeChild(p->imageNode, p->maskNode);
            g_nodeAPI->disposeNode(p->maskNode);
        }
        if (p->glassNode && p->imageNode) {
            g_nodeAPI->removeChild(p->imageNode, p->glassNode);
            g_nodeAPI->disposeNode(p->glassNode);
        }
        ArkUI_NativeAnimateAPI_1* api = getAnimateApi();
        if (api) {
            if (p->maskAnim) api->disposeAnimator(p->maskAnim);
            if (p->glassAnim) api->disposeAnimator(p->glassAnim);
        }
        HM_LOGI("ImageMagicReveal - cleanup done");
        delete p;
    };

    // Mask overlay
    {
        ArkUI_NodeHandle maskNode = g_nodeAPI->createNode(ARKUI_NODE_STACK);
        if (!maskNode) { delete rp; return; }
        rp->maskNode = maskNode;

        A2UINode mv(maskNode);
        mv.setWidth(nodeW);
        mv.setHeight(nodeH);
        mv.setHitTestBehavior(ARKUI_HIT_TEST_MODE_NONE);
        mv.setPosition(0.0f, 0.0f);

        {
            auto clampF = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
            static const float kLoc[4] = {0.0f, 0.55f, 0.85f, 1.0f};
            for (int i = 0; i < 4; i++) rp->maskStops[i] = clampF(kLoc[i] + (-0.8f));
        }
        ArkUI_ColorStop cs;
        cs.colors = rp->maskColors;
        cs.stops = rp->maskStops;
        cs.size = 4;
        ArkUI_NumberValue gv[3];
        gv[0].f32 = 135.0f;
        gv[1].i32 = ARKUI_LINEAR_GRADIENT_DIRECTION_CUSTOM;
        gv[2].i32 = 0;
        ArkUI_AttributeItem gi = {gv, 3, nullptr, &cs};
        g_nodeAPI->setAttribute(maskNode, NODE_LINEAR_GRADIENT, &gi);
        g_nodeAPI->addChild(imageNode, maskNode);
    }

    // Glass overlay
    {
        ArkUI_NodeHandle glassNode = g_nodeAPI->createNode(ARKUI_NODE_STACK);
        if (!glassNode) {
            HM_LOGW("ImageMagicReveal - glassNode create failed, skip glass, id=%s", m_id.c_str());
        } else {
            rp->glassNode = glassNode;
            A2UINode gv(glassNode);
            gv.setWidth(nodeW);
            gv.setHeight(nodeH);
            gv.setHitTestBehavior(ARKUI_HIT_TEST_MODE_NONE);
            gv.setPosition(0.0f, 0.0f);

            {
                auto clampF = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
                static const float kLoc[7] = {0.0f, 0.25f, 0.38f, 0.50f, 0.62f, 0.75f, 1.0f};
                for (int i = 0; i < 7; i++) rp->glassStops[i] = clampF(kLoc[i] + (-0.6f));
            }
            ArkUI_ColorStop gcs;
            gcs.colors = rp->glassColors;
            gcs.stops = rp->glassStops;
            gcs.size = 7;
            ArkUI_NumberValue ggv[3];
            ggv[0].f32 = 135.0f;
            ggv[1].i32 = ARKUI_LINEAR_GRADIENT_DIRECTION_CUSTOM;
            ggv[2].i32 = 0;
            ArkUI_AttributeItem ggi = {ggv, 3, nullptr, &gcs};
            g_nodeAPI->setAttribute(glassNode, NODE_LINEAR_GRADIENT, &ggi);
            g_nodeAPI->addChild(imageNode, glassNode);
        }
    }

    m_revealMaskNode = nullptr;

    // Mask sweep animator
    rp->maskAnim = createSweepAnimator(
        context, animateApi, rp->maskNode,
        0.0f, 1.0f, durationMs, rp,
        [](ArkUI_AnimatorOnFrameEvent* event) {
            auto* p = static_cast<RevealPayload*>(OH_ArkUI_AnimatorOnFrameEvent_GetUserData(event));
            if (!p || !p->maskNode) return;
            float t = OH_ArkUI_AnimatorOnFrameEvent_GetValue(event);
            float sp = -0.8f + t * 1.8f;
            static const float kLoc[4] = {0.0f, 0.55f, 0.85f, 1.0f};
            auto clampF = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
            for (int i = 0; i < 4; i++) p->maskStops[i] = clampF(kLoc[i] + sp);
            ArkUI_ColorStop cs;
            cs.colors = p->maskColors;
            cs.stops = p->maskStops;
            cs.size = 4;
            ArkUI_NumberValue gv[3];
            gv[0].f32 = 135.0f;
            gv[1].i32 = ARKUI_LINEAR_GRADIENT_DIRECTION_CUSTOM;
            gv[2].i32 = 0;
            ArkUI_AttributeItem gi = {gv, 3, nullptr, &cs};
            g_nodeAPI->setAttribute(p->maskNode, NODE_LINEAR_GRADIENT, &gi);
        },
        [](ArkUI_AnimatorEvent* event) {
            auto* p = static_cast<RevealPayload*>(OH_ArkUI_AnimatorEvent_GetUserData(event));
            if (!p) return;
            p->finishCount++;
            if (p->finishCount < 2 && p->glassNode != nullptr) return;
            if (p->maskNode && p->imageNode) { g_nodeAPI->removeChild(p->imageNode, p->maskNode); g_nodeAPI->disposeNode(p->maskNode); }
            if (p->glassNode && p->imageNode) { g_nodeAPI->removeChild(p->imageNode, p->glassNode); g_nodeAPI->disposeNode(p->glassNode); }
            ArkUI_NativeAnimateAPI_1* api = getAnimateApi();
            if (api) { if (p->maskAnim) api->disposeAnimator(p->maskAnim); if (p->glassAnim) api->disposeAnimator(p->glassAnim); }
            HM_LOGI("ImageMagicReveal - cleanup done");
            delete p;
        },
        [](ArkUI_AnimatorEvent* event) {
            auto* p = static_cast<RevealPayload*>(OH_ArkUI_AnimatorEvent_GetUserData(event));
            if (!p) return;
            p->finishCount++;
            if (p->finishCount < 2 && p->glassNode != nullptr) return;
            if (p->maskNode && p->imageNode) { g_nodeAPI->removeChild(p->imageNode, p->maskNode); g_nodeAPI->disposeNode(p->maskNode); }
            if (p->glassNode && p->imageNode) { g_nodeAPI->removeChild(p->imageNode, p->glassNode); g_nodeAPI->disposeNode(p->glassNode); }
            ArkUI_NativeAnimateAPI_1* api = getAnimateApi();
            if (api) { if (p->maskAnim) api->disposeAnimator(p->maskAnim); if (p->glassAnim) api->disposeAnimator(p->glassAnim); }
            delete p;
        }
    );

    // Glass sweep animator
    if (rp->glassNode) {
        rp->glassAnim = createSweepAnimator(
            context, animateApi, rp->glassNode,
            0.0f, 1.0f, durationMs, rp,
            [](ArkUI_AnimatorOnFrameEvent* event) {
                auto* p = static_cast<RevealPayload*>(OH_ArkUI_AnimatorOnFrameEvent_GetUserData(event));
                if (!p || !p->glassNode) return;
                float t = OH_ArkUI_AnimatorOnFrameEvent_GetValue(event);
                float sp = -0.6f + t * 1.8f;
                static const float kLoc[7] = {0.0f, 0.25f, 0.38f, 0.50f, 0.62f, 0.75f, 1.0f};
                auto clampF = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
                for (int i = 0; i < 7; i++) p->glassStops[i] = clampF(kLoc[i] + sp);
                ArkUI_ColorStop gcs;
                gcs.colors = p->glassColors;
                gcs.stops = p->glassStops;
                gcs.size = 7;
                ArkUI_NumberValue ggv[3];
                ggv[0].f32 = 135.0f;
                ggv[1].i32 = ARKUI_LINEAR_GRADIENT_DIRECTION_CUSTOM;
                ggv[2].i32 = 0;
                ArkUI_AttributeItem ggi = {ggv, 3, nullptr, &gcs};
                g_nodeAPI->setAttribute(p->glassNode, NODE_LINEAR_GRADIENT, &ggi);
            },
            [](ArkUI_AnimatorEvent* event) {
                auto* p = static_cast<RevealPayload*>(OH_ArkUI_AnimatorEvent_GetUserData(event));
                if (!p) return;
                p->finishCount++;
                if (p->finishCount < 2 && p->glassNode != nullptr) return;
                if (p->maskNode && p->imageNode) { g_nodeAPI->removeChild(p->imageNode, p->maskNode); g_nodeAPI->disposeNode(p->maskNode); }
                if (p->glassNode && p->imageNode) { g_nodeAPI->removeChild(p->imageNode, p->glassNode); g_nodeAPI->disposeNode(p->glassNode); }
                ArkUI_NativeAnimateAPI_1* api = getAnimateApi();
                if (api) { if (p->maskAnim) api->disposeAnimator(p->maskAnim); if (p->glassAnim) api->disposeAnimator(p->glassAnim); }
                HM_LOGI("ImageMagicReveal - cleanup done");
                delete p;
            },
            [](ArkUI_AnimatorEvent* event) {
                auto* p = static_cast<RevealPayload*>(OH_ArkUI_AnimatorEvent_GetUserData(event));
                if (!p) return;
                p->finishCount++;
                if (p->finishCount < 2 && p->glassNode != nullptr) return;
                if (p->maskNode && p->imageNode) { g_nodeAPI->removeChild(p->imageNode, p->maskNode); g_nodeAPI->disposeNode(p->maskNode); }
                if (p->glassNode && p->imageNode) { g_nodeAPI->removeChild(p->imageNode, p->glassNode); g_nodeAPI->disposeNode(p->glassNode); }
                ArkUI_NativeAnimateAPI_1* api = getAnimateApi();
                if (api) { if (p->maskAnim) api->disposeAnimator(p->maskAnim); if (p->glassAnim) api->disposeAnimator(p->glassAnim); }
                delete p;
            }
        );
    } else {
        rp->finishCount = 1;
    }

    // Scale ease-out animation: kImageFadeInStartScale -> 1.0
    {
        struct ScalePayload { ArkUI_NodeHandle node; ArkUI_AnimatorHandle anim; };
        ScalePayload* sp = new ScalePayload();
        sp->node = m_nodeHandle;

        ArkUI_AnimatorOption* scaleOpt = OH_ArkUI_AnimatorOption_Create(0);
        if (scaleOpt) {
            OH_ArkUI_AnimatorOption_SetDuration(scaleOpt, durationMs);
            OH_ArkUI_AnimatorOption_SetBegin(scaleOpt, 0.0f);
            OH_ArkUI_AnimatorOption_SetEnd(scaleOpt, 1.0f);
            OH_ArkUI_AnimatorOption_SetIterations(scaleOpt, 1);
            OH_ArkUI_AnimatorOption_SetFill(scaleOpt, ARKUI_ANIMATION_FILL_MODE_FORWARDS);
            OH_ArkUI_AnimatorOption_SetDirection(scaleOpt, ARKUI_ANIMATION_DIRECTION_NORMAL);
            ArkUI_CurveHandle easeCurve = OH_ArkUI_Curve_CreateCubicBezierCurve(0.0f, 0.0f, 0.58f, 1.0f);
            if (easeCurve) OH_ArkUI_AnimatorOption_SetCurve(scaleOpt, easeCurve);

            OH_ArkUI_AnimatorOption_RegisterOnFrameCallback(
                scaleOpt, sp,
                [](ArkUI_AnimatorOnFrameEvent* event) {
                    auto* p = static_cast<ScalePayload*>(OH_ArkUI_AnimatorOnFrameEvent_GetUserData(event));
                    if (!p || !p->node) return;
                    float t = OH_ArkUI_AnimatorOnFrameEvent_GetValue(event);
                    float s = kImageFadeInStartScale + (1.0f - kImageFadeInStartScale) * t;
                    A2UINode(p->node).setScale(s, s);
                });
            OH_ArkUI_AnimatorOption_RegisterOnFinishCallback(
                scaleOpt, sp,
                [](ArkUI_AnimatorEvent* event) {
                    auto* p = static_cast<ScalePayload*>(OH_ArkUI_AnimatorEvent_GetUserData(event));
                    if (!p) return;
                    if (p->node) A2UINode(p->node).setScale(1.0f, 1.0f);
                    ArkUI_NativeAnimateAPI_1* api = getAnimateApi();
                    if (api && p->anim) api->disposeAnimator(p->anim);
                    delete p;
                });
            OH_ArkUI_AnimatorOption_RegisterOnCancelCallback(
                scaleOpt, sp,
                [](ArkUI_AnimatorEvent* event) {
                    auto* p = static_cast<ScalePayload*>(OH_ArkUI_AnimatorEvent_GetUserData(event));
                    if (!p) return;
                    if (p->node) A2UINode(p->node).setScale(1.0f, 1.0f);
                    ArkUI_NativeAnimateAPI_1* api = getAnimateApi();
                    if (api && p->anim) api->disposeAnimator(p->anim);
                    delete p;
                });

            ArkUI_AnimatorHandle scaleHandle = animateApi->createAnimator(context, scaleOpt);
            sp->anim = scaleHandle;
            if (easeCurve) OH_ArkUI_Curve_DisposeCurve(easeCurve);
            OH_ArkUI_AnimatorOption_Dispose(scaleOpt);

            if (scaleHandle) {
                OH_ArkUI_Animator_Play(scaleHandle);
                HM_LOGI("ImageMagicReveal - scale animator started (%.2f -> 1.0)",
                    kImageFadeInStartScale);
            } else {
                A2UINode(m_nodeHandle).setScale(1.0f, 1.0f);
                delete sp;
            }
        } else {
            delete sp;
        }
    }

    if (rp->maskAnim) {
        OH_ArkUI_Animator_Play(rp->maskAnim);
        HM_LOGI("ImageMagicReveal - mask animator started, id=%s", m_id.c_str());
    } else {
        if (rp->maskNode && rp->imageNode) { g_nodeAPI->removeChild(rp->imageNode, rp->maskNode); g_nodeAPI->disposeNode(rp->maskNode); }
        if (rp->glassNode && rp->imageNode) { g_nodeAPI->removeChild(rp->imageNode, rp->glassNode); g_nodeAPI->disposeNode(rp->glassNode); }
        if (animateApi && rp->glassAnim) animateApi->disposeAnimator(rp->glassAnim);
        delete rp;
        return;
    }
    if (rp->glassAnim) {
        OH_ArkUI_Animator_Play(rp->glassAnim);
        HM_LOGI("ImageMagicReveal - glass animator started");
    }
}

} // namespace a2ui
