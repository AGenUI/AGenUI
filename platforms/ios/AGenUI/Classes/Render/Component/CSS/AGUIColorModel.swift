//
//  AGUIColorModel.swift
//  AGenUI
//
//  Pure-Swift data models for CSS color values produced by the shared C++
//  ColorParser. Mirrors com.amap.agenui.ColorValue on Android. Only the
//  thin Obj-C++ bridge `AGenUIColorBridgeRaw` (NSDictionary in / out) is
//  exposed via the AGenUI umbrella header; everything below is Swift-internal.
//

import Foundation

// MARK: - Enums

public enum AGUIColorType: Int {
    case solid    = 0
    case gradient = 1
}

public enum AGUIGradientType: Int {
    case linear = 0
    case radial = 1
    case conic  = 2
}

/// Mirrors the C++ `agenui::StopUnit` ordering exactly.
public enum AGUIStopUnit: Int {
    case percent = 0
    case px      = 1
    case deg     = 2
    case grad    = 3
    case rad     = 4
    case turn    = 5
}

public enum AGUIRadialShape: Int {
    case circle  = 0
    case ellipse = 1
}

public enum AGUIRadialSize: Int {
    case closestSide    = 0
    case closestCorner  = 1
    case farthestSide   = 2
    case farthestCorner = 3
}

// MARK: - Models
//
// Reference types are used (rather than structs) because callers rely on
// reference equality for `CSSPropertyValue.gradient` payloads (an immutable
// snapshot returned from the parser); see `CSSPropertyValue.==`.

public final class AGUIColorStop {
    public let color: UInt32          // ARGB
    public let position: Float
    public let unit: AGUIStopUnit
    public let hasPosition: Bool
    public let isHint: Bool

    init(color: UInt32, position: Float, unit: AGUIStopUnit,
         hasPosition: Bool, isHint: Bool) {
        self.color = color
        self.position = position
        self.unit = unit
        self.hasPosition = hasPosition
        self.isHint = isHint
    }
}

public final class AGUILinearParams {
    public let angle: Float           // degrees: 0=to top, 90=to right
    init(angle: Float) { self.angle = angle }
}

public final class AGUIRadialParams {
    public let shape: AGUIRadialShape
    public let size: AGUIRadialSize
    public let centerX: Float
    public let centerY: Float
    public let centerXIsPx: Bool
    public let centerYIsPx: Bool
    public let radiusX: Float
    public let radiusY: Float
    public let hasExplicitSize: Bool
    public let radiusXIsPercent: Bool
    public let radiusYIsPercent: Bool

    init(shape: AGUIRadialShape, size: AGUIRadialSize,
         centerX: Float, centerY: Float,
         centerXIsPx: Bool, centerYIsPx: Bool,
         radiusX: Float, radiusY: Float,
         hasExplicitSize: Bool,
         radiusXIsPercent: Bool, radiusYIsPercent: Bool) {
        self.shape = shape
        self.size = size
        self.centerX = centerX
        self.centerY = centerY
        self.centerXIsPx = centerXIsPx
        self.centerYIsPx = centerYIsPx
        self.radiusX = radiusX
        self.radiusY = radiusY
        self.hasExplicitSize = hasExplicitSize
        self.radiusXIsPercent = radiusXIsPercent
        self.radiusYIsPercent = radiusYIsPercent
    }
}

public final class AGUIConicParams {
    public let startAngle: Float
    public let centerX: Float
    public let centerY: Float
    public let centerXIsPx: Bool
    public let centerYIsPx: Bool

    init(startAngle: Float, centerX: Float, centerY: Float,
         centerXIsPx: Bool, centerYIsPx: Bool) {
        self.startAngle = startAngle
        self.centerX = centerX
        self.centerY = centerY
        self.centerXIsPx = centerXIsPx
        self.centerYIsPx = centerYIsPx
    }
}

public final class AGUIGradientInfo {
    public let gradientType: AGUIGradientType
    public let isRepeating: Bool
    public let colorStops: [AGUIColorStop]
    public let linear: AGUILinearParams?
    public let radial: AGUIRadialParams?
    public let conic: AGUIConicParams?

    init(gradientType: AGUIGradientType, isRepeating: Bool,
         colorStops: [AGUIColorStop],
         linear: AGUILinearParams?,
         radial: AGUIRadialParams?,
         conic: AGUIConicParams?) {
        self.gradientType = gradientType
        self.isRepeating = isRepeating
        self.colorStops = colorStops
        self.linear = linear
        self.radial = radial
        self.conic = conic
    }
}

public final class AGUIColorValue {
    public let type: AGUIColorType
    public let solidColor: UInt32     // ARGB
    public let gradient: AGUIGradientInfo?

    init(type: AGUIColorType, solidColor: UInt32, gradient: AGUIGradientInfo?) {
        self.type = type
        self.solidColor = solidColor
        self.gradient = gradient
    }
}

// MARK: - Bridge facade

/// Swift-side facade over `AGenUIColorBridgeRaw`. Hides the dictionary ABI
/// and returns strongly-typed Swift models. This is the only entry point
/// that AGenUI's CSS parser uses.
public enum AGenUIColorBridge {

    public static func parseColor(_ cssValue: String) -> AGUIColorValue? {
        guard let dict = AGenUIColorBridgeRaw.parseColor(cssValue) as? [String: Any],
              let type = dict["type"] as? String else {
            return nil
        }

        if type == "solid" {
            let argb = (dict["solidColor"] as? NSNumber)?.uint32Value ?? 0
            return AGUIColorValue(type: .solid, solidColor: argb, gradient: nil)
        }

        guard type == "gradient",
              let g = dict["gradient"] as? [String: Any],
              let gType = g["type"] as? String else {
            return nil
        }

        let gradientType: AGUIGradientType
        switch gType {
        case "linear": gradientType = .linear
        case "radial": gradientType = .radial
        case "conic":  gradientType = .conic
        default: return nil
        }

        let isRepeating = (g["isRepeating"] as? NSNumber)?.boolValue ?? false
        let stops = (g["stops"] as? [[String: Any]] ?? []).compactMap { decodeStop($0) }

        let linear: AGUILinearParams? = (g["linear"] as? [String: Any]).map {
            AGUILinearParams(angle: ($0["angle"] as? NSNumber)?.floatValue ?? 0)
        }
        let radial: AGUIRadialParams? = (g["radial"] as? [String: Any]).flatMap(decodeRadial)
        let conic: AGUIConicParams? = (g["conic"] as? [String: Any]).map(decodeConic)

        let info = AGUIGradientInfo(
            gradientType: gradientType,
            isRepeating: isRepeating,
            colorStops: stops,
            linear: linear,
            radial: radial,
            conic: conic
        )
        return AGUIColorValue(type: .gradient, solidColor: 0, gradient: info)
    }

    private static func decodeStop(_ d: [String: Any]) -> AGUIColorStop? {
        let color    = (d["color"] as? NSNumber)?.uint32Value ?? 0
        let position = (d["position"] as? NSNumber)?.floatValue ?? 0
        let unitRaw  = (d["unit"] as? NSNumber)?.intValue ?? 0
        let unit     = AGUIStopUnit(rawValue: unitRaw) ?? .percent
        let hasPos   = (d["hasPosition"] as? NSNumber)?.boolValue ?? false
        let isHint   = (d["isHint"] as? NSNumber)?.boolValue ?? false
        return AGUIColorStop(color: color, position: position, unit: unit,
                             hasPosition: hasPos, isHint: isHint)
    }

    private static func decodeRadial(_ d: [String: Any]) -> AGUIRadialParams? {
        let shapeRaw = (d["shape"] as? NSNumber)?.intValue ?? 0
        let sizeRaw  = (d["size"]  as? NSNumber)?.intValue ?? 3
        return AGUIRadialParams(
            shape: AGUIRadialShape(rawValue: shapeRaw) ?? .ellipse,
            size: AGUIRadialSize(rawValue: sizeRaw) ?? .farthestCorner,
            centerX:          (d["centerX"]          as? NSNumber)?.floatValue ?? 0,
            centerY:          (d["centerY"]          as? NSNumber)?.floatValue ?? 0,
            centerXIsPx:      (d["centerXIsPx"]      as? NSNumber)?.boolValue  ?? false,
            centerYIsPx:      (d["centerYIsPx"]      as? NSNumber)?.boolValue  ?? false,
            radiusX:          (d["radiusX"]          as? NSNumber)?.floatValue ?? 0,
            radiusY:          (d["radiusY"]          as? NSNumber)?.floatValue ?? 0,
            hasExplicitSize:  (d["hasExplicitSize"]  as? NSNumber)?.boolValue  ?? false,
            radiusXIsPercent: (d["radiusXIsPercent"] as? NSNumber)?.boolValue  ?? false,
            radiusYIsPercent: (d["radiusYIsPercent"] as? NSNumber)?.boolValue  ?? false
        )
    }

    private static func decodeConic(_ d: [String: Any]) -> AGUIConicParams {
        return AGUIConicParams(
            startAngle:  (d["startAngle"]  as? NSNumber)?.floatValue ?? 0,
            centerX:     (d["centerX"]     as? NSNumber)?.floatValue ?? 0,
            centerY:     (d["centerY"]     as? NSNumber)?.floatValue ?? 0,
            centerXIsPx: (d["centerXIsPx"] as? NSNumber)?.boolValue  ?? false,
            centerYIsPx: (d["centerYIsPx"] as? NSNumber)?.boolValue  ?? false
        )
    }
}
