//
//  AGenUIEngineMeasurementBridge.h
//  AGenUI
//

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

NS_ASSUME_NONNULL_BEGIN

/// Measurement callback type
///
/// @param componentType Component type (e.g. "Text", "Image")
/// @param paramJson     Component attribute JSON string
/// @param maxWidth      Maximum available width (pt)
/// @param widthMode     Width mode (0=Undefined 1=Exactly 2=AtMost)
/// @param maxHeight     Maximum available height (pt)
/// @param heightMode    Height mode (0=Undefined 1=Exactly 2=AtMost)
/// @return Measurement result CGSize (width/height, pt)
typedef CGSize (^AGenUIMeasureCallback)(
    NSString *componentType,
    NSString *paramJson,
    float maxWidth,
    int widthMode,
    float maxHeight,
    int heightMode);

/// AGenUI engine measurement bridge (static methods)
///
/// Responsible for:
/// 1. Register IMeasurement implementations for each component type with the C++ engine
/// 2. Receive C++ measurement callbacks and forward to Swift measurement methods
///
/// All methods are class methods; no instance creation needed.
@interface AGenUIEngineMeasurementBridge : NSObject

/// Component measurement callback (set by Swift side, class property)
///
/// Swift side provides actual measurement implementation by setting this block.
/// Called when C++ engine's Yoga layout needs to calculate intrinsic component dimensions.
@property (class, nonatomic, copy, nullable) AGenUIMeasureCallback measureCallback;

/// Register measurement implementation for the specified component type
/// @param type Component type identifier (e.g. "Text")
+ (void)registerMeasurementForType:(NSString *)type;

/// Unregister measurement implementation for the specified component type
/// @param type Component type identifier
+ (void)unregisterMeasurementForType:(NSString *)type;

/// Disable instance creation
- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
