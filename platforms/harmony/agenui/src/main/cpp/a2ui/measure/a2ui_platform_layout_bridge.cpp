#include "a2ui_platform_layout_bridge.h"
#include "log/a2ui_capi_log.h"
#include "a2ui/utils/a2ui_unit_utils.h"
#include "nlohmann/json.hpp"
#include <algorithm>

// Global density must stay outside the namespace to match the extern declaration.
float gDensityForUI = 3.0f;

namespace a2ui {

// Cached device metrics. Defaults match the original hard-coded values.
static int s_deviceWidth = 300;
static int s_deviceHeight = 500;
static float s_deviceDensity = 3.375f;
static bool s_hasImageFadeInOverride = false;
static bool s_imageFadeInEnabled = true;
static constexpr int32_t kDefaultImageFadeInDurationMs = 1500;

static const char* g_component_styles = R"JSON({
  "Tabs": {
    "tab-mode": "fixed",
    "indicator-color": "#2273F7",
    "indicator-width": "48px",
    "indicator-height": "8px",
    "indicator-radius": "4px",
    "tab-font-size": "32px",
    "tab-font-size-selected": "32px",
    "tab-font-color": {"call": "token", "args": {"name": "Color_Black"}},
    "tab-font-color-selected": {"call": "token", "args": {"name": "Color_Text_Brand"}},
    "tab-font-weight": "normal",
    "tab-font-weight-selected": "bold"
  },
  "Modal": {
    "show-close-button": "false",
    "close-button-margin": "16px",
    "close-button-size": "72px",
    "overlay-color": "#00000080"
  },
  "Button": {
    "disabled-opacity": "0.4"
  },
  "Image": {
    "fade-in-enabled": true,
    "fade-in-duration": 1500
  },
  "CheckBox": {
    "checkbox-size": "32px",
    "checkbox-background-color-selected": "#2E82FF",
    "checkbox-border-color-selected": "#2E82FF",
    "checkbox-background-color": "#00000000",
    "checkbox-border-color": "#0000001A",
    "checkbox-background-color-disabled": "#EBEBEB",
    "checkbox-border-color-disabled": "#0000001A",
    "checkbox-border-width": "3px",
    "checkbox-border-radius": "12px",
    "text-margin": "16px",
    "text-color": "#000000",
    "text-color-disabled": "#66000000",
    "text-size": "32px"
  },
  "ChoicePicker": {
    "checkbox-size": "32px",
    "checkbox-margin": "8px",
    "checkbox-item-padding": "16px",
    "checkbox-background-color-selected": "#2E82FF",
    "checkbox-border-color-selected": "#2E82FF",
    "checkbox-background-color": "#00000000",
    "checkbox-border-color": "#0000001A",
    "checkbox-border-width": "3px",
    "checkbox-border-radius": "12px",
    "text-margin": "16px",
    "text-color": {"call": "token", "args": {"name": "Color_Black"}},
    "text-size": "32px",
    "choice-gap": "40px",
    "chip-padding-horizontal": "28px",
    "chip-padding-vertical": "16px",
    "chip-gap": "24px",
    "chip-border-radius": "999px",
    "chip-border-width": "2px",
    "chip-indicator-gap": "16px",
    "chip-background-color": "#F0F1F3",
    "chip-background-color-selected": "#2E82FF",
    "chip-border-color": "#E0E3E8",
    "chip-border-color-selected": "#2E82FF",
    "chip-text-color": "#666666",
    "chip-text-color-selected": "#FFFFFF",
    "search-height": "96px",
    "search-padding-horizontal": "24px",
    "search-padding-vertical": "16px",
    "search-border-width": "2px",
    "search-border-radius": "999px",
    "search-margin-bottom": "24px",
    "search-margin-horizontal": "24px",
    "search-text-size": "28px",
    "search-text-color": "#000000",
    "search-background-color": "#FFFFFF",
    "search-border-color": "#0000001A",
    "search-placeholder-color": "#66000000",
    "search-placeholder-text": "Search"
  },
  "Slider": {
    "slider-height": "48px",
    "track-height": "4px",
    "track-corner-radius": "2px",
    "minimum-track-color": "#1A66FF",
    "maximum-track-color": "#EEF0F4",
    "thumb-outer-diameter": "48px",
    "thumb-outer-color": "#FFFFFF",
    "thumb-inner-diameter": "16px",
    "thumb-inner-color": "#1A66FF"
  },
  "AudioPlayer": {
    "size": "80px",
    "play-icon-size": "40px",
    "pause-icon-size": "35px",
    "ring-width": "8px",
    "play-bg-color": "#2273F7",
    "pause-bg-color": "#FFFFFF",
    "ring-color": "#2273F7",
    "play-icon-color": "#FFFFFF",
    "pause-icon-color": "#2273F7",
    "loading-color": "#2273F7",
    "error-bg-color": "#CCCCCC"
  },
  "DateTimeInput": {
    "compact": {
      "height": "56px",
      "font-size": "24px",
      "icon-name": "event",
      "icon-size": "24px",
      "icon-spacing": "6px",
      "selected-background-color": "#2273F714",
      "selected-text-color": "#2273F7",
      "unselected-text-color": "#000000",
      "placeholder-text": "Select Data",
      "padding-vertical": "12px",
      "padding-horizontal": "24px",
      "corner-radius": "8px",
      "popup-mask-color": "#00000066",
      "popup-corner-radius": "12px"
    },
    "wheels-2col": {
      "font-size": "28px",
      "row-spacing": "80px",
      "selected-color": "#000000",
      "unselected-color": "#33000000",
      "selected-background-color": "#FFFFFF",
      "picker-height": "368px",
      "divider-color": "#0F000000",
      "divider-height": "2px",
      "background-color": "#FFFFFF",
      "container-padding": "16px"
    },
    "wheels-3col": {
      "font-size": "36px",
      "row-spacing": "80px",
      "selected-color": "#000000",
      "unselected-color": "#33000000",
      "selected-background-color": "#FFFFFF",
      "picker-height": "428px",
      "divider-color": "#0F000000",
      "divider-height": "2px",
      "background-color": "#FFFFFF",
      "container-padding": "40px"
    },
    "wheels-5col": {
      "font-size": "28px",
      "row-spacing": "80px",
      "selected-color": "#000000",
      "unselected-color": "#33000000",
      "selected-background-color": "#FFFFFF",
      "picker-height": "368px",
      "divider-color": "#0F000000",
      "divider-height": "2px",
      "background-color": "#FFFFFF",
      "container-padding": "15px"
    }
  },
  "Carousel": {
    "indicator-dot-spacing": "8px",
    "indicator-inactive-dot-width": "6px",
    "indicator-active-dot-width": "24px",
    "indicator-container-height": "6px",
    "indicator-bottom-offset": "12px",
    "indicator-background-color": "#00000000",
    "indicator-active-dot-color": "#00000099",
    "indicator-inactive-dot-color": "#0000001a",
    "indicator-active-corner-radius": "3px",
    "image-placeholder-color": "#F2F2F7"
  },
  "Table": {
    "header-bg-color": "#EEEFF2",
    "header-font-color": "#000000",
    "header-font-size": "28px",
    "header-font-weight": "bold",
    "body-bg-color": [
      "#FFFFFF",
      "#F6F7F8"
    ],
    "body-font-color": "#000000",
    "body-font-size": "28px",
    "body-font-weight": "normal",
    "text-align": "left",
    "vertical-align": "center",
    "min-column-width": "100px",
    "max-column-width": "600px",
    "cell-padding-vertical":"20px",
    "cell-padding-horizontal":"32px",
    "horizontal-scroll": "true"
  }
})JSON";

// Static variable used to cache the parsed JSON object
static nlohmann::json s_parsedComponentStyles;
static bool s_stylesInitialized = false;

// Parse component styles once on first use.
static void initializeComponentStyles() {
    if (!s_stylesInitialized) {
        s_parsedComponentStyles = nlohmann::json::parse(g_component_styles, nullptr, false);
        if (s_parsedComponentStyles.is_discarded()) {
            HM_LOGE("initializeComponentStyles - Failed to parse component styles");
            s_parsedComponentStyles = nlohmann::json::object();
        } else {
            HM_LOGD("Component styles initialized successfully");
        }
        s_stylesInitialized = true;
    }
}

void setDeviceInfo(int width, int height, float density) {
    s_deviceWidth = width;
    s_deviceHeight = height;
    s_deviceDensity = density;
    gDensityForUI  = density;
    HM_LOGD("setDeviceInfo: width=%d, height=%d, density=%.3f", width, height, density);
}

float getScreenDensity() {
    return s_deviceDensity;
}

// Empty object singleton for getComponentStylesFor to return reference when component does not exist
static const nlohmann::json s_emptyStyles = nlohmann::json::object();

const nlohmann::json& getComponentStylesFor(const std::string& componentName) {
    initializeComponentStyles();
    if (s_parsedComponentStyles.contains(componentName) &&
        s_parsedComponentStyles[componentName].is_object()) {
        return s_parsedComponentStyles[componentName];
    }
    return s_emptyStyles;
}

void setImageFadeInEnabled(bool enabled) {
    s_hasImageFadeInOverride = true;
    s_imageFadeInEnabled = enabled;
    HM_LOGI("setImageFadeInEnabled: enabled=%s", enabled ? "true" : "false");
}

bool isImageFadeInEnabled() {
    if (s_hasImageFadeInOverride) {
        return s_imageFadeInEnabled;
    }

    const nlohmann::json imageStyles = getComponentStylesFor("Image");
    if (imageStyles.is_object() && imageStyles.contains("fade-in-enabled")) {
        const nlohmann::json& value = imageStyles["fade-in-enabled"];
        if (value.is_boolean()) {
            return value.get<bool>();
        }
        if (value.is_number_integer()) {
            return value.get<int>() != 0;
        }
        if (value.is_string()) {
            const std::string flag = value.get<std::string>();
            if (flag == "true" || flag == "1") {
                return true;
            }
            if (flag == "false" || flag == "0") {
                return false;
            }
        }
    }
    return s_imageFadeInEnabled;
}

int32_t getImageFadeInDurationMs() {
    const nlohmann::json imageStyles = getComponentStylesFor("Image");
    if (imageStyles.is_object() && imageStyles.contains("fade-in-duration")) {
        const nlohmann::json& value = imageStyles["fade-in-duration"];
        if (value.is_number_integer()) {
            return std::max(0, value.get<int32_t>());
        }
        if (value.is_number()) {
            return std::max(0, static_cast<int32_t>(value.get<float>()));
        }
        if (value.is_string()) {
            // Convert string to integer; return 0 if invalid
            const std::string sv = value.get<std::string>();
            if (!sv.empty()) {
                return std::max(0, static_cast<int32_t>(std::atoi(sv.c_str())));
            }
        }
    }
    return kDefaultImageFadeInDurationMs;
}

A2UISurfaceSizeProvider* getSharedSurfaceSizeProvider() {
    static A2UISurfaceSizeProvider sProvider;
    return &sProvider;
}

agenui::SurfaceSize A2UISurfaceSizeProvider::getSurfaceSize(const std::string& /*surfaceId*/) {
    agenui::SurfaceSize size;
    size.width  = UnitConverter::pxToA2ui(static_cast<float>(s_deviceWidth));
    size.height = UnitConverter::pxToA2ui(static_cast<float>(s_deviceHeight));
    return size;
}

const char* getComponentStylesRaw() {
    return g_component_styles;
}

} // namespace a2ui
