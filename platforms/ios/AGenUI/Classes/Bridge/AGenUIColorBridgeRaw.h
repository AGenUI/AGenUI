//
//  AGenUIColorBridgeRaw.h
//  AGenUI
//
//  Narrow Obj-C++ bridge to the shared C++ CSS color parser
//  (core/src/style_parser/agenui_color_parser). Returns a serialized
//  NSDictionary so that all strongly-typed data models can live in Swift
//  (see AGUIColorModel.swift) and the cross-language ABI surface stays
//  minimal — only this single class is exposed via the AGenUI umbrella header.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Dictionary schema produced by `+parseColor:`.
///
/// {
///   "type":        "solid" | "gradient",
///   "solidColor":  NSNumber<UInt32> ARGB                 // when type=="solid"
///   "gradient":    {                                       // when type=="gradient"
///     "type":         "linear" | "radial" | "conic",
///     "isRepeating":  NSNumber<BOOL>,
///     "stops":        NSArray< NSDictionary >,
///     "linear":       { "angle": NSNumber<float> }                       // optional
///     "radial":       { "shape", "size", "centerX", "centerY",           // optional
///                       "centerXIsPx", "centerYIsPx", "radiusX", "radiusY",
///                       "hasExplicitSize", "radiusXIsPercent", "radiusYIsPercent" }
///     "conic":        { "startAngle", "centerX", "centerY",              // optional
///                       "centerXIsPx", "centerYIsPx" }
///   }
/// }
///
/// Each stop dict contains:
///   { "color": NSNumber<UInt32> ARGB, "position": NSNumber<float>,
///     "unit": NSNumber<NSInteger>, "hasPosition": NSNumber<BOOL>,
///     "isHint": NSNumber<BOOL> }
///
/// Enum values follow the Swift `AGUI*` definitions in AGUIColorModel.swift.
@interface AGenUIColorBridgeRaw : NSObject
+ (nullable NSDictionary *)parseColor:(NSString *)cssValue;
@end

NS_ASSUME_NONNULL_END
