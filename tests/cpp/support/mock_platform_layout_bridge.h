// Configurable IPlatformLayoutBridge for tests.

#pragma once

#include <string>

#include "agenui_platform_layout_bridge.h"

namespace agenui {
namespace testing {

class MockPlatformLayoutBridge : public ::agenui::IPlatformLayoutBridge {
public:
    int getDeviceWidth() override { return deviceWidth; }
    int getDeviceHeight() override { return deviceHeight; }
    DeviceOrientation getDeviceOrientation() override { return orientation; }
    float getDeviceDensity() override { return density; }
    const char* getComponentStyles() override { return componentStyles.c_str(); }

    int deviceWidth = 1080;
    int deviceHeight = 1920;
    DeviceOrientation orientation = eOrientationPortrait;
    float density = 2.0f;
    std::string componentStyles = "{}";
};

}  // namespace testing
}  // namespace agenui
