//
//  AGenUIEdgeInsetsBridgeRaw.mm
//  AGenUI
//

#import "AGenUIEdgeInsetsBridgeRaw.h"
#include "style_parser/agenui_edge_insets_parser.h"

@implementation AGenUIEdgeInsetsBridgeRaw

static NSDictionary *encodeSide(const agenui::EdgeInsetValue &v) {
    NSMutableDictionary *d = [NSMutableDictionary dictionaryWithCapacity:4];
    d[@"value"]  = @(v.value);
    d[@"unit"]   = @((NSInteger)v.unit);
    d[@"isCalc"] = @(v.isCalc);
    if (v.isCalc && !v.calcExpr.empty()) {
        d[@"calcExpr"] = [NSString stringWithUTF8String:v.calcExpr.c_str()];
    }
    return d;
}

+ (nullable NSDictionary *)parseEdgeInsets:(NSString *)cssValue {
    if (cssValue.length == 0) return nil;
    const char *utf8 = [cssValue UTF8String];
    if (utf8 == NULL) return nil;  // unpaired surrogates yield NULL, not std::string UB
    std::string input(utf8);

    agenui::EdgeInsets insets;
    if (!agenui::EdgeInsetsParser::parse(input, insets)) {
        return nil;
    }

    return @{
        @"top":    encodeSide(insets.top),
        @"right":  encodeSide(insets.right),
        @"bottom": encodeSide(insets.bottom),
        @"left":   encodeSide(insets.left),
    };
}

+ (nullable NSDictionary *)parseInlineEdgeInsets:(NSString *)cssValue isRTL:(BOOL)isRTL {
    if (cssValue.length == 0) return nil;
    const char *utf8 = [cssValue UTF8String];
    if (utf8 == NULL) return nil;  // unpaired surrogates yield NULL, not std::string UB
    std::string input(utf8);

    agenui::EdgeInsets insets;
    if (!agenui::EdgeInsetsParser::parseInline(input, isRTL, insets)) {
        return nil;
    }

    return @{
        @"top":    encodeSide(insets.top),
        @"right":  encodeSide(insets.right),
        @"bottom": encodeSide(insets.bottom),
        @"left":   encodeSide(insets.left),
    };
}

@end
