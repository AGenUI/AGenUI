#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "agenui_surface_size_provider.h"
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
 * @brief Return the raw component styles JSON as a C string.
 */
const char* getComponentStylesRaw();

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

/**
 * @brief ISurfaceSizeProvider implementation backed by the device metrics
 * cached via setDeviceInfo(). Returns the device window size (in a2ui units)
 * for every surfaceId, serving as a sensible default until the platform
 * pushes a per-surface size via ISurfaceManager::onSurfaceSizeChanged.
 */
class A2UISurfaceSizeProvider : public agenui::ISurfaceSizeProvider {
public:
    agenui::SurfaceSize getSurfaceSize(const std::string& surfaceId) override;
};

/**
 * @brief Process-wide A2UISurfaceSizeProvider singleton; inject it into
 * every ISurfaceManager via setSurfaceSizeProvider() after creation.
 */
A2UISurfaceSizeProvider* getSharedSurfaceSizeProvider();

} // namespace a2ui
