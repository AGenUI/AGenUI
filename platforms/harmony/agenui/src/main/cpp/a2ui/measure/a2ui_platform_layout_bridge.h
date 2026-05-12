#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "agenui_platform_layout_bridge.h"
#include "text_measurement_utils.h"

namespace a2ui {

/**
 * @brief Set device screen information during ETS-side initialization.
 * @param width Screen width in px
 * @param height Screen height in px
 * @param density Screen density, such as 3.375
 */
void setDeviceInfo(int width, int height, float density);

/**
 * @brief Return the screen density used by rendering components.
 * @return Screen density, default 3.375f
 */
float getScreenDensity();

/**
 * @brief Read component-specific style configuration from g_component_styles.
 * @param componentName Component name such as "CheckBox" or "Tabs"
 * @return Style JSON for the component, or an empty object if missing
 */
const nlohmann::json& getComponentStylesFor(const std::string& componentName);

/**
 * @brief Set the global image fade-in switch.
 * @param enabled True to enable fade-in, false to disable it
 */
void setImageFadeInEnabled(bool enabled);

/**
 * @brief Return whether image fade-in is enabled globally.
 */
bool isImageFadeInEnabled();

/**
 * @brief Return the image fade-in duration.
 * @return Duration in milliseconds
 */
int32_t getImageFadeInDurationMs();

class A2UIPlatformLayoutBridge : public agenui::IPlatformLayoutBridge {
public:
    A2UIPlatformLayoutBridge();
    ~A2UIPlatformLayoutBridge() override;

    int getDeviceWidth() override;
    int getDeviceHeight() override;
    DeviceOrientation getDeviceOrientation() override { return eOrientationuUknown; }
    float getDeviceDensity() override;
    const char* getComponentStyles() override;
};

} // namespace a2ui
