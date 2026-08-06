#include "a2ui_animate_utils.h"

#include <arkui/native_interface.h>
#include <arkui/native_animate.h>
#include "a2ui/render/a2ui_node.h"
#include "log/a2ui_capi_log.h"

namespace a2ui {

namespace {

float clampOpacity(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

/// Free a payload that never got a running animator: null the caller's
/// tracking pointer first so it never dangles, then delete.
void releasePayload(OpacityAnimatePayload* payload) {
    if (payload->backPtr != nullptr) {
        *(payload->backPtr) = nullptr;
    }
    delete payload;
}

void startOpacityAnimator(OpacityAnimatePayload* payload);

/// Post-frame trampoline used by animateNodeOpacityAfterMount.  The payload is
/// created *before* the frame callback is posted so the owning component can
/// mark it destroyed (via cancelOpacityAnimator) during the one-frame window
/// between scheduling and this callback firing.  Without that, a component
/// destroyed within that window would leave us holding a dangling node handle
/// and animate a freed node (SIGSEGV in setOpacity).
void onAppearAnimatePostFrame(uint64_t /*nanoTimestamp*/, uint32_t /*frameCount*/, void* userData) {
    auto* payload = static_cast<OpacityAnimatePayload*>(userData);
    if (payload == nullptr) {
        return;
    }
    if (payload->destroyed || payload->nodeHandle == nullptr) {
        // Component was destroyed while waiting for this frame — the node is
        // gone and backPtr was already nulled by cancelOpacityAnimator.
        delete payload;
        return;
    }
    startOpacityAnimator(payload);
}

/// Create and play the opacity animator for an already-allocated payload.
/// On every failure/instant path the payload is released (backPtr nulled,
/// payload deleted); on success ownership passes to the finish/cancel callback.
void startOpacityAnimator(OpacityAnimatePayload* payload) {
    ArkUI_NodeHandle nodeHandle = payload->nodeHandle;
    float targetOpacity = payload->targetOpacity;

    if (payload->durationMs <= 0) {
        A2UINode(nodeHandle).setOpacity(targetOpacity);
        releasePayload(payload);
        return;
    }

    ArkUI_ContextHandle context = OH_ArkUI_GetContextByNode(nodeHandle);
    ArkUI_NativeAnimateAPI_1* animateApi = getAnimateApi();
    if (context == nullptr || animateApi == nullptr) {
        A2UINode(nodeHandle).setOpacity(targetOpacity);
        releasePayload(payload);
        return;
    }

    ArkUI_AnimatorOption* option = OH_ArkUI_AnimatorOption_Create(0);
    if (option == nullptr) {
        A2UINode(nodeHandle).setOpacity(targetOpacity);
        releasePayload(payload);
        return;
    }

    ArkUI_CurveHandle curve = OH_ArkUI_Curve_CreateCubicBezierCurve(0.42f, 0.0f, 0.58f, 1.0f);
    OH_ArkUI_AnimatorOption_SetDuration(option, payload->durationMs);
    OH_ArkUI_AnimatorOption_SetBegin(option, 0.0f);
    OH_ArkUI_AnimatorOption_SetEnd(option, targetOpacity);
    OH_ArkUI_AnimatorOption_SetIterations(option, 1);
    OH_ArkUI_AnimatorOption_SetFill(option, ARKUI_ANIMATION_FILL_MODE_FORWARDS);
    OH_ArkUI_AnimatorOption_SetDirection(option, ARKUI_ANIMATION_DIRECTION_NORMAL);
    if (curve != nullptr) {
        OH_ArkUI_AnimatorOption_SetCurve(option, curve);
    }

    OH_ArkUI_AnimatorOption_RegisterOnFrameCallback(
        option,
        payload,
        [](ArkUI_AnimatorOnFrameEvent* event) {
            auto* p = static_cast<OpacityAnimatePayload*>(
                OH_ArkUI_AnimatorOnFrameEvent_GetUserData(event));
            if (p == nullptr || p->destroyed || p->nodeHandle == nullptr) {
                return;
            }
            A2UINode(p->nodeHandle).setOpacity(OH_ArkUI_AnimatorOnFrameEvent_GetValue(event));
        });

    auto finish = [](ArkUI_AnimatorEvent* event) {
        auto* p = static_cast<OpacityAnimatePayload*>(OH_ArkUI_AnimatorEvent_GetUserData(event));
        if (p == nullptr) {
            return;
        }
        if (!p->destroyed && p->nodeHandle != nullptr) {
            A2UINode(p->nodeHandle).setOpacity(p->targetOpacity);
        }
        ArkUI_NativeAnimateAPI_1* api = getAnimateApi();
        if (api != nullptr && p->animatorHandle != nullptr) {
            api->disposeAnimator(p->animatorHandle);
        }
        // Null the caller's tracking pointer before deleting, so the caller
        // never holds a dangling pointer after natural completion.
        if (p->backPtr != nullptr) {
            *(p->backPtr) = nullptr;
        }
        delete p;
    };

    OH_ArkUI_AnimatorOption_RegisterOnFinishCallback(option, payload, finish);
    OH_ArkUI_AnimatorOption_RegisterOnCancelCallback(option, payload, finish);

    ArkUI_AnimatorHandle animatorHandle = animateApi->createAnimator(context, option);
    payload->animatorHandle = animatorHandle;
    if (animatorHandle == nullptr) {
        A2UINode(nodeHandle).setOpacity(targetOpacity);
        releasePayload(payload);
    } else if (OH_ArkUI_Animator_Play(animatorHandle) != ARKUI_ERROR_CODE_NO_ERROR) {
        animateApi->disposeAnimator(animatorHandle);
        A2UINode(nodeHandle).setOpacity(targetOpacity);
        releasePayload(payload);
    }

    if (curve != nullptr) {
        OH_ArkUI_Curve_DisposeCurve(curve);
    }
    OH_ArkUI_AnimatorOption_Dispose(option);
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

ArkUI_NativeAnimateAPI_1* getAnimateApi() {
    static ArkUI_NativeAnimateAPI_1* animateApi = [] {
        ArkUI_NativeAnimateAPI_1* api = nullptr;
        OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_ANIMATE, ArkUI_NativeAnimateAPI_1, api);
        if (api == nullptr) {
            HM_LOGE("Fatal: Failed to get ArkUI NativeAnimateAPI_1");
        }
        return api;
    }();
    return animateApi;
}

void animateNodeOpacityNow(ArkUI_NodeHandle nodeHandle, float targetOpacity, int32_t durationMs,
                           OpacityAnimatePayload** outPayload) {
    if (outPayload) *outPayload = nullptr;
    if (nodeHandle == nullptr) {
        return;
    }

    auto* payload = new OpacityAnimatePayload();
    payload->nodeHandle     = nodeHandle;
    payload->targetOpacity  = clampOpacity(targetOpacity);
    payload->durationMs     = durationMs;
    payload->backPtr        = outPayload;
    if (outPayload) *outPayload = payload;

    startOpacityAnimator(payload);
}

void animateNodeOpacityAfterMount(ArkUI_NodeHandle nodeHandle, float targetOpacity, int32_t durationMs,
                                  OpacityAnimatePayload** outPayload) {
    if (nodeHandle == nullptr) {
        return;
    }

    ArkUI_ContextHandle context = OH_ArkUI_GetContextByNode(nodeHandle);
    if (context == nullptr) {
        animateNodeOpacityNow(nodeHandle, targetOpacity, durationMs, outPayload);
        return;
    }

    // Create the payload BEFORE posting the frame callback and hand it to the
    // caller immediately.  If the owning component is destroyed during the
    // one-frame wait, cancelOpacityAnimator() marks it destroyed and
    // onAppearAnimatePostFrame drops the animation instead of touching the
    // freed node (previously this window caused a use-after-free crash in
    // A2UINode::setOpacity on the vsync thread).
    auto* payload = new OpacityAnimatePayload();
    payload->nodeHandle     = nodeHandle;
    payload->targetOpacity  = clampOpacity(targetOpacity);
    payload->durationMs     = durationMs;
    payload->backPtr        = outPayload;
    if (outPayload) *outPayload = payload;

    if (postFrameCallbackCompat(context, payload, onAppearAnimatePostFrame) != ARKUI_ERROR_CODE_NO_ERROR) {
        releasePayload(payload);
        animateNodeOpacityNow(nodeHandle, targetOpacity, durationMs, outPayload);
    }
}

void cancelOpacityAnimator(OpacityAnimatePayload*& payload) {
    if (payload == nullptr) {
        return;
    }
    // Mark as destroyed so onFrame callbacks bail out immediately.
    payload->destroyed  = true;
    payload->nodeHandle = nullptr;
    // Detach the back-pointer: it targets a member of the component being
    // destroyed, so a late onFinish/onCancel callback must not write through it.
    payload->backPtr    = nullptr;
    ArkUI_AnimatorHandle handle = payload->animatorHandle;
    payload->animatorHandle = nullptr;

    // Null the caller's pointer now — the onCancel callback would otherwise
    // have done it via backPtr, which has just been detached above.
    payload = nullptr;

    if (handle != nullptr) {
        OH_ArkUI_Animator_Cancel(handle);
        ArkUI_NativeAnimateAPI_1* api = getAnimateApi();
        if (api != nullptr) {
            api->disposeAnimator(handle);
        }
    }
    // The onCancel callback (fired synchronously or on next frame) will:
    //   1. See destroyed=true → skip node access
    //   2. Null *(p->backPtr) → no-op (already nulled above)
    //   3. disposeAnimator   → no-op (already disposed)
    //   4. delete p          → frees the payload
    // So we do NOT delete the payload here — the callback owns deletion.
    //
    // If handle == nullptr the animator has not started yet (the payload is
    // waiting in the post-frame queue).  onAppearAnimatePostFrame will see
    // destroyed=true and delete the payload itself.
}

} // namespace a2ui
