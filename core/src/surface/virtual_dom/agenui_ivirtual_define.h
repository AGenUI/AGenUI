#pragma once

#include <string>
#include <stdint.h>
#include "yoga/YGEnums.h"
#include "yoga/Yoga.h"
#include "agenui_platform_layout_bridge.h"

#define PIXEL_TO_DP(value, density) ((value) / density * 2.0)
#define DP_TO_PIXEL(value, density)  ((value) / 2.0 * density)

namespace agenui {

class AGenUIVirtualDefine {
public:
    /**
     * @brief Get the global IPlatformLayoutBridge (from IEngineContext)
     * @return IPlatformLayoutBridge pointer, or nullptr if not set
     */
    static IPlatformLayoutBridge* getPlatformLayoutBridge();

    /**
     * @brief Get the device screen size wrapped as YGSize
     * @return YGSize.width = getDeviceWidth(), YGSize.height = getDeviceHeight();
     *         returns {0, 0} if unavailable
     */
    static YGSize getDeviceScreenSize();

    /**
     * @brief Get the raw device pixel density, cached in a static variable after first call
     * @return Raw getDeviceDensity() value, or 1.0f if unavailable
     */
    static float getDeviceDensity();

    /**
     * @brief Convert a pixel value to dp
     * @param pixel Pixel value
     * @return dp value; returns pixel unchanged if density is invalid
     */
    static float convertPixelToDp(float pixel);

    /**
     * @brief Convert a dp value to pixels
     * @param dp dp value
     * @return Pixel value; returns dp unchanged if density is invalid
     */
    static float convertDpToPixel(float dp);
};

} // namespace agenui
