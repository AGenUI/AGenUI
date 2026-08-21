//
//  CSSPropertyValue.swift
//  AGenUI
//
// Created on 2026/2/28.
//

import UIKit

/// CSS property value type
/// Represents parsed CSS property values, supports multiple data types
public enum CSSPropertyValue: Equatable {
    /// Numeric type, e.g., "100" -> 100.0
    case number(CGFloat)
    
    /// Percentage type, e.g., "50%" -> 0.5
    case percentage(CGFloat)
    
    /// Color type, e.g., "#FF0000" -> UIColor.red
    case color(UIColor)
    
    /// Keyword type, e.g., "center", "start", "transparent"
    case keyword(String)
    
    /// Shadow type, used for the filter (drop-shadow) property
    case shadow(CSSShadow)

    /// Gradient type, produced when CSS color value is `linear-gradient(...)`,
    /// `radial-gradient(...)`, `conic-gradient(...)` (or their `repeating-` variants).
    /// The payload is parsed by the shared C++ ColorParser via `AGenUIColorBridge`.
    case gradient(AGUIGradientInfo)

    /// URL type, stores parsed URL string
    /// url("https://example.com/image.png") -> .url("https://example.com/image.png")
    case url(String)
    
    /// Invalid value, indicates parsing failure
    case invalid
    
    // MARK: - Equatable
    
    public static func == (lhs: CSSPropertyValue, rhs: CSSPropertyValue) -> Bool {
        switch (lhs, rhs) {
        case (.number(let lValue), .number(let rValue)):
            return lValue == rValue
        case (.percentage(let lValue), .percentage(let rValue)):
            return lValue == rValue
        case (.color(let lColor), .color(let rColor)):
            return lColor == rColor
        case (.keyword(let lKeyword), .keyword(let rKeyword)):
            return lKeyword == rKeyword
        case (.shadow(let lShadow), .shadow(let rShadow)):
            return lShadow == rShadow
        case (.gradient(let lInfo), .gradient(let rInfo)):
            // AGUIGradientInfo is an immutable snapshot; reference equality is sufficient.
            return lInfo === rInfo
        case (.url(let lUrl), .url(let rUrl)):
            return lUrl == rUrl
        case (.invalid, .invalid):
            return true
        default:
            return false
        }
    }
}

// MARK: - CSSShadow

/// CSS shadow value
/// Shadow spec for the filter (drop-shadow) property
public struct CSSShadow: Equatable {
    /// Horizontal offset (positive = right, negative = left)
    public let offsetX: CGFloat

    /// Vertical offset (positive = down, negative = up)
    public let offsetY: CGFloat

    /// Blur radius (larger value = more blur)
    public let blur: CGFloat

    /// Shadow color
    public let color: UIColor
    
    /// Creates from filter drop-shadow
    /// - Parameters:
    ///   - offsetX: Horizontal offset
    ///   - offsetY: Vertical offset
    ///   - blur: Blur radius
    ///   - color: Shadow color
    init(offsetX: CGFloat, offsetY: CGFloat, blur: CGFloat, color: UIColor) {
        self.offsetX = offsetX
        self.offsetY = offsetY
        self.blur = blur
        self.color = color
    }
    
    // MARK: - Equatable
    
    public static func == (lhs: CSSShadow, rhs: CSSShadow) -> Bool {
        return lhs.offsetX == rhs.offsetX &&
               lhs.offsetY == rhs.offsetY &&
               lhs.blur == rhs.blur &&
               lhs.color == rhs.color
    }
}
