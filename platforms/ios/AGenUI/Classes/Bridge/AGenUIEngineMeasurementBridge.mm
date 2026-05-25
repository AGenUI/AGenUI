//
//  AGenUIEngineMeasurementBridge.mm
//  AGenUI
//

#import "AGenUIEngineMeasurementBridge.h"
#import "AGenUIEngineBridge.h"
#include "agenui_engine_entry.h"
#include "agenui_measurement.h"
#import <os/lock.h>

#pragma mark - C++ IMeasurement Adapter

namespace agenui {

/**
 * @brief C++ IMeasurement adapter
 *
 * No need to hold an ObjC bridge pointer; directly calls
 * AGenUIEngineMeasurementBridge class methods to access
 * the static measureCallback.
 */
class IOSBridgeMeasurement final : public IMeasurement {
public:
    explicit IOSBridgeMeasurement(std::string type)
        : _type(std::move(type)) {}

    MeasureResult measure(const std::string& paramJson,
                          const MeasureModes& modes) override {
        AGenUIMeasureCallback callback = [AGenUIEngineMeasurementBridge measureCallback];
        if (!callback) {
            return MeasureResult{CalcType::Sync, 0.0f, 0.0f, 0};
        }

        NSString *nsType = [NSString stringWithUTF8String:_type.c_str()];
        NSString *nsParamJson = [NSString stringWithUTF8String:paramJson.c_str()];

        CGSize size = callback(
            nsType, nsParamJson,
            modes.width.maxValue / 2., modes.width.mode,
            modes.height.maxValue / 2., modes.height.mode);

        // Convert pt -> a2ui units (1pt = 2 a2ui units, same as @2x logical pixel scale)
        return MeasureResult{
            CalcType::Sync,
            (float)size.width * 2.0f,
            (float)size.height * 2.0f,
            0
        };
    }

private:
    std::string _type;
};

}  // namespace agenui

#pragma mark - AGenUIEngineMeasurementBridge

@interface AGenUIEngineMeasurementBridge ()

@end

static AGenUIMeasureCallback s_measureCallback;
static NSMutableSet<NSString *> *s_registeredTypes;
static os_unfair_lock s_callbackLock = OS_UNFAIR_LOCK_INIT;

@implementation AGenUIEngineMeasurementBridge

#pragma mark - Class Property measureCallback

+ (AGenUIMeasureCallback)measureCallback {
    os_unfair_lock_lock(&s_callbackLock);
    AGenUIMeasureCallback cb = s_measureCallback;
    os_unfair_lock_unlock(&s_callbackLock);
    return cb;
}

+ (void)setMeasureCallback:(AGenUIMeasureCallback)measureCallback {
    os_unfair_lock_lock(&s_callbackLock);
    s_measureCallback = [measureCallback copy];
    os_unfair_lock_unlock(&s_callbackLock);
}

#pragma mark - Class Methods

+ (void)initialize {
    if (self == [AGenUIEngineMeasurementBridge class]) {
        s_registeredTypes = [NSMutableSet set];
    }
}

+ (void)registerMeasurementForType:(NSString *)type {
    agenui::IAGenUIEngine* engine = agenui::getAGenUIEngine();
    if (!engine) return;

    agenui::IMeasurementManager* mm = engine->getMeasurementManager();
    if (!mm) return;

    std::string typeStr = type.UTF8String;
    mm->registerMeasurement(typeStr, std::make_shared<agenui::IOSBridgeMeasurement>(typeStr));
    @synchronized (s_registeredTypes) {
        [s_registeredTypes addObject:type];
    }
}

+ (void)unregisterMeasurementForType:(NSString *)type {
    agenui::IAGenUIEngine* engine = agenui::getAGenUIEngine();
    if (!engine) return;

    agenui::IMeasurementManager* mm = engine->getMeasurementManager();
    if (!mm) return;

    mm->unregisterMeasurement(type.UTF8String);
    @synchronized (s_registeredTypes) {
        [s_registeredTypes removeObject:type];
    }
}

+ (void)teardown {
    NSSet *typesCopy;
    @synchronized (s_registeredTypes) {
        typesCopy = [s_registeredTypes copy];
    }
    for (NSString *type in typesCopy) {
        [self unregisterMeasurementForType:type];
    }
}

@end
