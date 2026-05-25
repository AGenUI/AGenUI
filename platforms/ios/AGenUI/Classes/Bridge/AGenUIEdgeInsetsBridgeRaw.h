//
//  AGenUIEdgeInsetsBridgeRaw.h
//  AGenUI
//
//  Narrow Obj-C++ bridge to the shared C++ CSS edge-insets shorthand parser
//  (core/src/style_parser/agenui_edge_insets_parser). Returns a serialized
//  NSDictionary so the strongly-typed data models can live in Swift
//  (see AGUIEdgeInsetsModel.swift) and the cross-language ABI surface stays
//  minimal — only this single class is exposed via the AGenUI umbrella header.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Dictionary schema produced by `+parseEdgeInsets:`.
///
/// {
///   "top":    { "value": NSNumber<float>, "unit": NSNumber<NSInteger>,
///               "isCalc": NSNumber<BOOL>, "calcExpr": NSString? },
///   "right":  { ... },
///   "bottom": { ... },
///   "left":   { ... }
/// }
///
/// `unit` integer matches the C++ `agenui::EdgeInsetUnit` enum 1:1
/// (Px=0, Percent=1, Em=2, Rem=3, Vw=4, Vh=5, Vmin=6, Vmax=7,
///  Cm=8, Mm=9, In=10, Pt=11, Pc=12, Auto=13). The same numeric contract
/// is shared with the Android JNI (`EdgeInsetsValue.EdgeInsetSide.UNIT_*`).
/// Swift consumers should decode through `AGUIEdgeInsetUnit` defined in
/// `AGUIEdgeInsetsModel.swift`.
@interface AGenUIEdgeInsetsBridgeRaw : NSObject
+ (nullable NSDictionary *)parseEdgeInsets:(NSString *)cssValue;
@end

NS_ASSUME_NONNULL_END
