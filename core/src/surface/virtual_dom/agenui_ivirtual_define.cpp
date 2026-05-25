#include "agenui_ivirtual_define.h"
#include "agenui_engine_context.h"

namespace agenui {

using namespace agenui;

IPlatformLayoutBridge* AGenUIVirtualDefine::getPlatformLayoutBridge() {
    return getEngineContext()->getPlatformLayoutBridge();
}

YGSize AGenUIVirtualDefine::getDeviceScreenSize() {
    IPlatformLayoutBridge* svc = getPlatformLayoutBridge();
    if (svc != nullptr) {
        YGSize size;
        size.width  = static_cast<float>(svc->getDeviceWidth());
        size.height = static_cast<float>(svc->getDeviceHeight());
        return size;
    }
    YGSize zero;
    zero.width  = 0;
    zero.height = 0;
    return zero;
}

float AGenUIVirtualDefine::getDeviceDensity() {
    static float sDensity = 0.0f;
    if (sDensity <= 0.0f) {
        IPlatformLayoutBridge* svc = getPlatformLayoutBridge();
        if (svc != nullptr) {
            sDensity = svc->getDeviceDensity();
        }
    }
    return (sDensity > 0.0f) ? sDensity : 1.0f;
}

float AGenUIVirtualDefine::convertPixelToDp(float pixel) {
    float density = getDeviceDensity();
    return static_cast<float>(PIXEL_TO_DP(pixel, density));
}

float AGenUIVirtualDefine::convertDpToPixel(float dp) {
    float density = getDeviceDensity();
    return static_cast<float>(DP_TO_PIXEL(dp, density));
}
} // namespace agenui
