#include "jni_agenui_android_platform_layout_bridge.h"

#include <cmath>

namespace agenui {

namespace {

constexpr const char* kComponentStylesJson = "{}";

float pxToA2ui(int px, float density) {
    if (density <= 0.0f) {
        return static_cast<float>(px);
    }
    return (static_cast<float>(px) / density) * 2.0f;
}

AndroidPlatformLayoutBridge g_androidPlatformLayoutBridge;

}  // namespace

int AndroidPlatformLayoutBridge::getDeviceWidth() {
    return static_cast<int>(std::round(pxToA2ui(m_widthPx, m_density)));
}

int AndroidPlatformLayoutBridge::getDeviceHeight() {
    return static_cast<int>(std::round(pxToA2ui(m_heightPx, m_density)));
}

float AndroidPlatformLayoutBridge::getDeviceDensity() {
    return m_density > 0.0f ? m_density : 1.0f;
}

const char* AndroidPlatformLayoutBridge::getComponentStyles() {
    return kComponentStylesJson;
}

void AndroidPlatformLayoutBridge::updateDeviceInfo(int widthPx, int heightPx, float density) {
    if (widthPx > 0) {
        m_widthPx = widthPx;
    }
    if (heightPx > 0) {
        m_heightPx = heightPx;
    }
    if (density > 0.0f) {
        m_density = density;
    }
}

AndroidPlatformLayoutBridge* getAndroidPlatformLayoutBridge() {
    return &g_androidPlatformLayoutBridge;
}

void ensureAndroidPlatformLayoutBridge(IAGenUIEngine* engine) {
    if (!engine) {
        return;
    }
    engine->setPlatformLayoutBridge(&g_androidPlatformLayoutBridge);
}

void updateAndroidPlatformDeviceInfo(int widthPx, int heightPx, float density) {
    g_androidPlatformLayoutBridge.updateDeviceInfo(widthPx, heightPx, density);
}

}  // namespace agenui
