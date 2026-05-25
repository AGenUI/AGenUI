//
//  AGenUIColorBridgeRaw.mm
//  AGenUI
//

#import "AGenUIColorBridgeRaw.h"
#include "style_parser/agenui_color_parser.h"

@implementation AGenUIColorBridgeRaw

+ (nullable NSDictionary *)parseColor:(NSString *)cssValue {
    if (cssValue.length == 0) return nil;
    std::string input = [cssValue UTF8String];

    agenui::ColorValue cv;
    if (!agenui::ColorParser::parse(input, cv)) {
        return nil;
    }

    if (cv.type != agenui::ColorValueType::Gradient) {
        return @{
            @"type":       @"solid",
            @"solidColor": @(cv.solidColor),
        };
    }

    NSMutableArray *stops = [NSMutableArray arrayWithCapacity:cv.gradient.colorStops.size()];
    for (const auto &s : cv.gradient.colorStops) {
        [stops addObject:@{
            @"color":       @(s.color),
            @"position":    @(s.position),
            @"unit":        @((NSInteger)s.unit),
            @"hasPosition": @(s.hasPosition),
            @"isHint":      @(s.isHint),
        }];
    }

    NSMutableDictionary *gradient = [NSMutableDictionary dictionary];
    gradient[@"isRepeating"] = @(cv.gradient.isRepeating);
    gradient[@"stops"]       = stops;

    switch (cv.gradient.type) {
        case agenui::GradientType::Linear:
            gradient[@"type"]   = @"linear";
            gradient[@"linear"] = @{ @"angle": @(cv.gradient.linear.angle) };
            break;
        case agenui::GradientType::Radial:
            gradient[@"type"] = @"radial";
            gradient[@"radial"] = @{
                @"shape":            @((NSInteger)cv.gradient.radial.shape),
                @"size":             @((NSInteger)cv.gradient.radial.size),
                @"centerX":          @(cv.gradient.radial.centerX),
                @"centerY":          @(cv.gradient.radial.centerY),
                @"centerXIsPx":      @(cv.gradient.radial.centerXIsPx),
                @"centerYIsPx":      @(cv.gradient.radial.centerYIsPx),
                @"radiusX":          @(cv.gradient.radial.radiusX),
                @"radiusY":          @(cv.gradient.radial.radiusY),
                @"hasExplicitSize":  @(cv.gradient.radial.hasExplicitSize),
                @"radiusXIsPercent": @(cv.gradient.radial.radiusXIsPercent),
                @"radiusYIsPercent": @(cv.gradient.radial.radiusYIsPercent),
            };
            break;
        case agenui::GradientType::Conic:
            gradient[@"type"] = @"conic";
            gradient[@"conic"] = @{
                @"startAngle":  @(cv.gradient.conic.startAngle),
                @"centerX":     @(cv.gradient.conic.centerX),
                @"centerY":     @(cv.gradient.conic.centerY),
                @"centerXIsPx": @(cv.gradient.conic.centerXIsPx),
                @"centerYIsPx": @(cv.gradient.conic.centerYIsPx),
            };
            break;
    }

    return @{
        @"type":     @"gradient",
        @"gradient": gradient,
    };
}

@end
