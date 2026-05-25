#pragma once
#include <memory>
#include <vector>

namespace agenui {
class IPlatformLayoutBridge {

public:
    virtual ~IPlatformLayoutBridge() {}

    enum DeviceOrientation {
        eOrientationuUknown,
        eOrientationPortrait = 1,
        eOrientationPortraitUpsideDown,
        eOrientationLandscapeLeft,
        eOrientationLandscapeRight,
    };

    virtual int getDeviceWidth() = 0;
    virtual int getDeviceHeight() = 0;
    virtual DeviceOrientation getDeviceOrientation() = 0;
    virtual float getDeviceDensity() = 0;
    virtual const char* getComponentStyles() = 0;
};

} // namespace agenui
