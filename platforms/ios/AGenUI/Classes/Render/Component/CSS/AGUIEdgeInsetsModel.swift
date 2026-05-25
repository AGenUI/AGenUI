//
//  AGUIEdgeInsetsModel.swift
//  AGenUI
//
//  Pure-Swift data models for CSS edge-insets shorthand values produced by
//  the shared C++ `EdgeInsetsParser`. Mirrors `EdgeInsetsValue` on Android.
//  The thin Obj-C++ bridge `AGenUIEdgeInsetsBridgeRaw` (NSDictionary in / out)
//  is the only symbol exposed via the AGenUI umbrella header; everything
//  below is Swift-internal.
//

import Foundation

// MARK: - Enums

/// Mirrors `agenui::EdgeInsetUnit` 1:1 and shares the same numeric contract
/// with Android's `EdgeInsetsValue.EdgeInsetSide.UNIT_*` constants.
enum AGUIEdgeInsetUnit: Int {
    case px      = 0
    case percent = 1
    case em      = 2
    case rem     = 3
    case vw      = 4
    case vh      = 5
    case vmin    = 6
    case vmax    = 7
    case cm      = 8
    case mm      = 9
    case `in`    = 10
    case pt      = 11
    case pc      = 12
    case auto    = 13
}

// MARK: - Models

struct AGUIEdgeInsetSide {
    let value: Float
    let unit: AGUIEdgeInsetUnit
    let isCalc: Bool
    let calcExpr: String?
}

struct AGUIEdgeInsets {
    let top: AGUIEdgeInsetSide
    let right: AGUIEdgeInsetSide
    let bottom: AGUIEdgeInsetSide
    let left: AGUIEdgeInsetSide
}

// MARK: - Bridge facade

/// Swift-side facade over `AGenUIEdgeInsetsBridgeRaw`. Hides the dictionary
/// ABI and returns strongly-typed Swift models. This is the only entry point
/// that AGenUI's CSS parsers use.
enum AGenUIEdgeInsetsBridge {

    static func parse(_ cssValue: String) -> AGUIEdgeInsets? {
        guard let dict = AGenUIEdgeInsetsBridgeRaw.parseEdgeInsets(cssValue) as? [String: Any] else {
            return nil
        }
        guard let top = decodeSide(dict["top"]),
              let right = decodeSide(dict["right"]),
              let bottom = decodeSide(dict["bottom"]),
              let left = decodeSide(dict["left"]) else {
            return nil
        }
        return AGUIEdgeInsets(top: top, right: right, bottom: bottom, left: left)
    }

    private static func decodeSide(_ raw: Any?) -> AGUIEdgeInsetSide? {
        guard let d = raw as? [String: Any] else { return nil }
        let value = (d["value"] as? NSNumber)?.floatValue ?? 0
        let unitRaw = (d["unit"] as? NSNumber)?.intValue ?? 0
        let unit = AGUIEdgeInsetUnit(rawValue: unitRaw) ?? .px
        let isCalc = (d["isCalc"] as? NSNumber)?.boolValue ?? false
        let calcExpr = d["calcExpr"] as? String
        return AGUIEdgeInsetSide(value: value, unit: unit, isCalc: isCalc, calcExpr: calcExpr)
    }
}
