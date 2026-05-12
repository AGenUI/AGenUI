#pragma once

#include "agenui_engine.h"
#include "agenui_platform_layout_bridge.h"

namespace agenui {

/**
 * Android-specific platform layout bridge exposed to the C++ Yoga pipeline.
 *
 * The engine only consumes A2UI-space device metrics and component-style JSON from the generic
 * IPlatformLayoutBridge interface. This implementation keeps the latest Android window size and
 * density, then converts them into the A2UI coordinate space expected by Yoga/style resolution.
 */
class AndroidPlatformLayoutBridge final : public IPlatformLayoutBridge {
public:
    AndroidPlatformLayoutBridge() = default;
    ~AndroidPlatformLayoutBridge() override = default;

    int getDeviceWidth() override;
    int getDeviceHeight() override;
    DeviceOrientation getDeviceOrientation() override { return eOrientationuUknown; }
    float getDeviceDensity() override;
    const char* getComponentStyles() override;

    /**
     * Refreshes the cached Android device metrics used by Yoga size conversion.
     */
    void updateDeviceInfo(int widthPx, int heightPx, float density);

private:
    int m_widthPx = 0;
    int m_heightPx = 0;
    float m_density = 1.0f;
};

/**
 * Returns the process-wide AndroidPlatformLayoutBridge singleton.
 */
AndroidPlatformLayoutBridge* getAndroidPlatformLayoutBridge();

/**
 * Injects the Android platform layout bridge into the engine during engine initialization.
 */
void ensureAndroidPlatformLayoutBridge(IAGenUIEngine* engine);

/**
 * Updates the process-wide bridge with the latest Android window size and density.
 */
void updateAndroidPlatformDeviceInfo(int widthPx, int heightPx, float density);

}  // namespace agenui
