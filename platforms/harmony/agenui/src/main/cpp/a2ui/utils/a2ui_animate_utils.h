#pragma once

#include <arkui/native_animate.h>
#include <arkui/native_node.h>
#include <arkui/native_node_napi.h>
#include <cstdint>
#include <dlfcn.h>

namespace a2ui {

/**
 * Runtime-resolved wrapper for OH_ArkUI_PostFrameCallback (API 18+).
 *
 * Uses dlsym to avoid a hard link-time dependency on the symbol, which
 * would prevent liba2ui-capi.so from loading on API 17 devices where
 * the symbol does not exist in the system libraries.
 */
inline int32_t postFrameCallbackCompat(ArkUI_ContextHandle uiContext, void* userData,
    void (*callback)(uint64_t nanoTimestamp, uint32_t frameCount, void* userData)) {
    using FuncType = int32_t (*)(ArkUI_ContextHandle, void*,
        void (*)(uint64_t, uint32_t, void*));
    static FuncType fn = reinterpret_cast<FuncType>(
        dlsym(RTLD_DEFAULT, "OH_ArkUI_PostFrameCallback"));
    if (fn) {
        return fn(uiContext, userData, callback);
    }
    return ARKUI_ERROR_CODE_CAPI_INIT_ERROR;
}

/**
 * Payload used by opacity animators.  The base fields (nodeHandle,
 * targetOpacity, durationMs, animatorHandle) are used by the shared
 * animateNodeOpacityNow() implementation; the scale fields are only used
 * by image-specific fade-in animations (animateImageFadeIn).
 */
struct OpacityAnimatePayload {
    ArkUI_NodeHandle     nodeHandle      = nullptr;
    float                targetOpacity   = 1.0f;
    int32_t              durationMs      = 0;
    ArkUI_AnimatorHandle animatorHandle  = nullptr;
    bool                 destroyed       = false;
    /// Pointer-to-pointer back to the caller's tracking variable.
    /// The finish/cancel callback nulls *backPtr before deleting this payload,
    /// so the caller never holds a dangling pointer after natural completion.
    OpacityAnimatePayload** backPtr      = nullptr;
    // Image-specific scale animation fields (unused in the generic path).
    float                startScale      = 1.0f;
    float                targetScale     = 1.0f;
};

/**
 * Return the process-wide singleton ArkUI NativeAnimateAPI_1 handle.
 *
 * The handle is loaded lazily on the first call and cached in a function-local
 * static.  Calling this from multiple threads is safe because the ArkUI module
 * interface is idempotent and the static initialisation is guarded by the C++11
 * function-local-static guarantee.
 */
ArkUI_NativeAnimateAPI_1* getAnimateApi();

/**
 * Animate the opacity of @p nodeHandle to @p targetOpacity over @p durationMs.
 *
 * If @p durationMs <= 0 the opacity is applied immediately without animation.
 * Falls back to a direct opacity set when the ArkUI animate API or the node
 * context is unavailable.
 *
 * @param nodeHandle   Target ArkUI node; silently ignored when null.
 * @param targetOpacity Destination opacity value, clamped to [0, 1].
 * @param durationMs    Animation duration in milliseconds; <= 0 means instant.
 * @param outPayload    Optional out-parameter: receives the payload pointer
 *                      so the caller can later cancel via cancelOpacityAnimator().
 */
void animateNodeOpacityNow(ArkUI_NodeHandle nodeHandle, float targetOpacity, int32_t durationMs,
                           OpacityAnimatePayload** outPayload = nullptr);

/**
 * Schedule an opacity animation to run on the next rendered frame.
 *
 * Posts a per-frame callback via OH_ArkUI_PostFrameCallback so the animation
 * starts only after the node has been fully laid out and mounted.  Falls back
 * to animateNodeOpacityNow() when the context is unavailable or the callback
 * registration fails.
 *
 * The payload is allocated and written to @p outPayload immediately (before
 * the frame callback fires), so the caller can cancel via
 * cancelOpacityAnimator() even during the one-frame scheduling window.
 *
 * @param nodeHandle    Target ArkUI node; silently ignored when null.
 * @param targetOpacity Destination opacity value, clamped to [0, 1].
 * @param durationMs    Animation duration in milliseconds.
 * @param outPayload    Optional out-parameter: receives the payload pointer
 *                      so the caller can later cancel via cancelOpacityAnimator().
 */
void animateNodeOpacityAfterMount(ArkUI_NodeHandle nodeHandle, float targetOpacity, int32_t durationMs,
                                  OpacityAnimatePayload** outPayload = nullptr);

/**
 * Cancel a running (or still pending) opacity animator.
 *
 * Sets the destroyed flag and detaches the node handle / back-pointer so any
 * in-flight callbacks skip node access, cancels and disposes the animator if
 * it already started, and sets @p payload to null.  The payload itself is
 * freed by the animator's onCancel/onFinish callback, or by the post-frame
 * callback when the animation had not started yet.
 * Safe to call with a null payload (no-op).
 *
 * @param payload  Reference to the payload pointer; set to nullptr on return.
 */
void cancelOpacityAnimator(OpacityAnimatePayload*& payload);

} // namespace a2ui
