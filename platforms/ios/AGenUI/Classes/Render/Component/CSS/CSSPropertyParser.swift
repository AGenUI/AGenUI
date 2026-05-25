//
//  CSSPropertyParser.swift
//  AGenUI
//
// Created on 2026/2/28.
//

import UIKit

/// CSS property parser
/// Responsible for parsing CSS property strings into typed values
public class CSSPropertyParser {
    
    // MARK: - Constant Definitions
    
    /// Base point scale factor (used for px unit conversion)
    private static let BS_POINT_SCALE: CGFloat = 0.5
    
    // MARK: - Precompiled Regular Expressions (compiled once at class load to avoid repeated compilation overhead)
    
    /// drop-shadow regex: drop-shadow(offsetX offsetY blur color)
    private static let dropShadowRegex: NSRegularExpression? = {
        let pattern = "drop-shadow\\(\\s*(-?[\\d.]+)(?:px)?\\s+(-?[\\d.]+)(?:px)?\\s+([\\d.]+)(?:px)?\\s+(.+)\\s*\\)"
        return try? NSRegularExpression(pattern: pattern)
    }()
    
    /// box-shadow regex: offsetX offsetY blur [spread] color
    private static let boxShadowRegex: NSRegularExpression? = {
        let pattern = "(-?[\\d.]+)(?:px)?\\s+(-?[\\d.]+)(?:px)?\\s+([\\d.]+)(?:px)?(?:\\s+(-?[\\d.]+)(?:px)?)?\\s+(.+)"
        return try? NSRegularExpression(pattern: pattern)
    }()
    
    /// url() regex: supports double quotes, single quotes, and no quotes formats
    private static let urlFunctionRegex: NSRegularExpression? = {
        let pattern = #"url\(\s*['"]?([^'"\)]+)['"]?\s*\)"#
        return try? NSRegularExpression(pattern: pattern)
    }()
    
    // MARK: - Main Parsing Methods
    
    /// Parses property value
    /// - Parameters:
    ///   - value: Property value string
    ///   - config: Property configuration
    /// - Returns: Parsed property value
    static func parse(value: String, config: CSSPropertyConfig) -> CSSPropertyValue {
        let trimmedValue = value.trimmingCharacters(in: .whitespacesAndNewlines)
        
        switch config.valueType {
        case .dimension:
            return parseDimension(trimmedValue)
        case .number:
            return parseNumber(trimmedValue)
        case .color:
            return parseColor(trimmedValue)

        case .opacity:
            return parseOpacity(trimmedValue)
        case .shadow:
            // shadow type needs different parsing methods based on property name
            if config.name == "filter" {
                return parseFilter(trimmedValue)
            } else if config.name == "box-shadow" {
                return parseBoxShadow(trimmedValue)
            }
            return .invalid
        case .keyword:
            // keyword type uses valid values list from configuration for validation
            return parseKeyword(trimmedValue, validValues: config.validValues ?? [])
        case .url:
            return parseUrlFunction(trimmedValue)
        }
    }
    
    // MARK: - Dimension Parsing
    
    /// Parses dimension values (supports numbers, percentages, pt and px units, and auto keyword)
    /// - Parameter value: Dimension value string, e.g., "100", "50%", "100px", "20pt" or "auto"
    /// - Returns: Parsed property value
    public static func parseDimension(_ value: String) -> CSSPropertyValue {
        // First check if it's the auto keyword
        if value.lowercased() == "auto" {
            return .keyword("auto")
        }
        
        if value.hasSuffix("%") {
            // Percentage: "50%" -> 0.5
            let numStr = String(value.dropLast())
            if let num = Double(numStr) {
                return .percentage(CGFloat(num / 100.0))
            }
        } else if value.hasSuffix("px") {
            // px unit: apply BS_POINT_SCALE scaling
            // Formula: xpx * BS_POINT_SCALE
            // Example: "100px" = 100 * 0.5 = 50.0
            //       "200px" = 200 * 0.5 = 100.0
            let numStr = String(value.dropLast(2))
            if let num = Double(numStr) {
                let convertedValue = CGFloat(num) * BS_POINT_SCALE
                return .number(convertedValue)
            }
        } else if value.hasSuffix("pt") {
            // pt unit: "20pt" -> 20.0
            let numStr = String(value.dropLast(2))
            if let num = Double(numStr) {
                return .number(CGFloat(num))
            }
        } else {
            // Unitless number: "100" -> 100.0
            // Note: unitless numbers are treated as px by default, which is equivalent to pt in iOS
            // Example: "2" is equivalent to "2px", both parse to 2.0
            if let num = Double(value) {
                return .number(CGFloat(num))
            }
        }
        return .invalid
    }
    
    /// Parses pure numeric values
    /// - Parameter value: Numeric string, e.g., "1", "0.5" or "3 / 2" (ratio expression)
    /// - Returns: Parsed property value
    private static func parseNumber(_ value: String) -> CSSPropertyValue {
        // Check if it's a ratio expression (e.g., "3 / 2" or "16/9")
        if value.contains("/") {
            let components = value.split(separator: "/").map { $0.trimmingCharacters(in: .whitespaces) }
            if components.count == 2,
               let numerator = Double(components[0]),
               let denominator = Double(components[1]),
               denominator != 0 {
                let ratio = CGFloat(numerator / denominator)
                return .number(ratio)
            }
            return .invalid
        }
        
        // Standard numeric parsing
        if let num = Double(value) {
            return .number(CGFloat(num))
        }
        return .invalid
    }
    
    // MARK: - Color Parsing
    
    /// Parses CSS color values via the shared C++ CSS color parser
    /// (`core/src/style_parser/agenui_color_parser.cpp`) — single source of truth
    /// across Android / iOS / Harmony for hex (incl. #RGB shorthand), rgb / rgba,
    /// hsl / hwb, named colors (incl. `transparent`), `currentcolor`, and
    /// linear / radial / conic gradients.
    /// - Parameter value: Color value string
    /// - Returns: `.color(UIColor)`, `.gradient(GradientInfo)`, or `.invalid` on parse failure
    public static func parseColor(_ value: String) -> CSSPropertyValue {
        let css = value.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !css.isEmpty else { return .invalid }

        guard let cv = AGenUIColorBridge.parseColor(css) else {
            Logger.shared.debug("[CSSPropertyParser] parseColor: native parse failed for: \(value)")
            return .invalid
        }

        if cv.type == .gradient, let gi = cv.gradient {
            return .gradient(gi)
        }

        let argb = cv.solidColor
        let a = CGFloat((argb >> 24) & 0xFF) / 255.0
        let r = CGFloat((argb >> 16) & 0xFF) / 255.0
        let g = CGFloat((argb >>  8) & 0xFF) / 255.0
        let b = CGFloat( argb        & 0xFF) / 255.0
        return .color(UIColor(red: r, green: g, blue: b, alpha: a))
    }
    

    
    // MARK: - Opacity Parsing
    
    /// Parses opacity values
    /// - Parameter value: Opacity string, e.g., "0.5" or "1"
    /// - Returns: Parsed property value, automatically clamped to 0-1 range
    private static func parseOpacity(_ value: String) -> CSSPropertyValue {
        if let num = Double(value) {
            // Automatically clamp to 0-1 range
            let opacity = max(0.0, min(1.0, CGFloat(num)))
            return .number(opacity)
        }
        return .invalid
    }
    
    // MARK: - Requirement 9 New Parsing Methods
    
    /// Parses filter property (only supports drop-shadow)
    /// Format: "drop-shadow(offsetX offsetY blur color)"
    /// Example: "drop-shadow(0 2 4 rgba(0,0,0,0.1))" or "drop-shadow(0px 4px 16px rgba(0, 0, 0, 0.08))"
    /// - Parameter value: Filter value string
    /// - Returns: Parsed property value
    static func parseFilter(_ value: String) -> CSSPropertyValue {
        // Use precompiled regex to match drop-shadow function
        guard let regex = dropShadowRegex,
              let match = regex.firstMatch(in: value, range: NSRange(value.startIndex..., in: value)) else {
            Logger.shared.debug("Invalid filter format: \(value)")
            return .invalid
        }
        
        let nsString = value as NSString
        
        // Extract parameters
        let offsetXStr = nsString.substring(with: match.range(at: 1))
        let offsetYStr = nsString.substring(with: match.range(at: 2))
        let blurStr = nsString.substring(with: match.range(at: 3))
        let colorStr = nsString.substring(with: match.range(at: 4)).trimmingCharacters(in: .whitespaces)
        
        guard let offsetX = Double(offsetXStr),
              let offsetY = Double(offsetYStr),
              let blur = Double(blurStr) else {
            return .invalid
        }
        
        // Parse color
        let colorValue = parseColor(colorStr)
        guard case .color(let color) = colorValue else {
            return .invalid
        }
        
        // Create shadow object (drop-shadow does not support spread)
        let shadow = CSSShadow(
            offsetX: CGFloat(offsetX),
            offsetY: CGFloat(offsetY),
            blur: CGFloat(blur),
            color: color
        )
        
        return .shadow(shadow)
    }
    
    /// Parses box-shadow property
    /// Format: "offsetX offsetY blur [spread] color"
    /// Example: "0 2 8 0 rgba(0,0,0,0.15)" or "0px 2px 4px rgba(0, 0, 0, 0.1)"
    /// - Parameter value: box-shadow value string
    /// - Returns: Parsed property value
    static func parseBoxShadow(_ value: String) -> CSSPropertyValue {
        // Use precompiled regex to match box-shadow format
        guard let regex = boxShadowRegex,
              let match = regex.firstMatch(in: value, range: NSRange(value.startIndex..., in: value)) else {
            Logger.shared.debug("Invalid box-shadow format: \(value)")
            return .invalid
        }
        
        let nsString = value as NSString
        
        // Extract parameters
        let offsetXStr = nsString.substring(with: match.range(at: 1))
        let offsetYStr = nsString.substring(with: match.range(at: 2))
        let blurStr = nsString.substring(with: match.range(at: 3))
        
        // spread parameter is optional
        var spreadStr: String? = nil
        if match.range(at: 4).location != NSNotFound {
            spreadStr = nsString.substring(with: match.range(at: 4))
        }
        
        let colorStr = nsString.substring(with: match.range(at: 5)).trimmingCharacters(in: .whitespaces)
        
        guard let offsetX = Double(offsetXStr),
              let offsetY = Double(offsetYStr),
              let blur = Double(blurStr) else {
            return .invalid
        }
        
        // Parse spread if present, otherwise default to 0
        var spread: Double = 0
        if let spreadStr = spreadStr, let spreadValue = Double(spreadStr) {
            spread = spreadValue
        }
        
        // Parse color
        let colorValue = parseColor(colorStr)
        guard case .color(let color) = colorValue else {
            return .invalid
        }
        
        // Create shadow object (box-shadow supports spread, including 0 and negative values)
        let shadow = CSSShadow(
            offsetX: CGFloat(offsetX),
            offsetY: CGFloat(offsetY),
            blur: CGFloat(blur),
            spread: CGFloat(spread),
            color: color
        )
        
        return .shadow(shadow)
    }
    
    /// Parses keywords (used for border-style, overflow, display, visibility)
    /// - Parameters:
    ///   - value: Keyword string
    ///   - validValues: Valid values list
    /// - Returns: Parsed property value
    static func parseKeyword(_ value: String, validValues: [String]) -> CSSPropertyValue {
        let lowercased = value.lowercased()
        if validValues.contains(lowercased) {
            return .keyword(lowercased)
        }
        Logger.shared.log("[CSSPropertyParser] Invalid keyword '\(value)', expected one of: \(validValues.joined(separator: ", "))")
        return .invalid
    }
    
    // MARK: - URL Parsing
    
    /// Parses CSS url() function
    /// Supported formats:
    /// - url("https://example.com/image.png")  double quotes
    /// - url('res://icon')                      single quotes
    /// - url(paper.gif)                         no quotes
    /// - url(file:///path/to/image.png)         unquoted file path
    /// - Parameter value: Property value string
    /// - Returns: Parsed property value
    private static func parseUrlFunction(_ value: String) -> CSSPropertyValue {
        let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
        
        // Use precompiled regex to match url() function
        guard let regex = urlFunctionRegex,
              let match = regex.firstMatch(in: trimmed, range: NSRange(trimmed.startIndex..., in: trimmed)) else {
            #if DEBUG
            Logger.shared.debug("Invalid url() format: \(value)")
            #endif
            return .invalid
        }
        
        let nsString = trimmed as NSString
        let urlString = nsString.substring(with: match.range(at: 1))
        
        return .url(urlString)
    }
    

    
    /// Parses offset property values (used for top, left, bottom, right)
    /// Reuses parseDimension logic to ensure consistent px unit scaling behavior
    /// Supported formats:
    /// - Pixel value: "10px" → .number(5.0) (with BS_POINT_SCALE scaling)
    /// - Percentage: "50%" → .percentage(0.5)
    /// - Keyword: "auto" → .keyword("auto")
    /// - Negative value: "-10px" → .number(-5.0)
    /// - Unitless number: "10" → .number(10)
    /// - Parameter value: offset property value string
    /// - Returns: Parsed property value
    static func parseOffset(_ value: String) -> CSSPropertyValue {
        // First trim whitespace, then reuse parseDimension
        // This ensures position offset uses the same unit handling logic as width/height properties
        let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
        return parseDimension(trimmed)
    }
    
    
    // MARK: - Property Value Extraction Helper Methods
    
    /// Extracts string value
    /// - Parameter value: Value of any type
    /// - Returns: String representation
    public static func extractStringValue(_ value: Any) -> String {
        return String(describing: value)
    }
    
    /// Extracts numeric value
    /// - Parameter value: Value of any type
    /// - Returns: Double value, or nil if conversion fails
    public static func extractNumberValue(_ value: Any) -> Double? {
        if let number = value as? Double {
            return number
        }
        if let number = value as? Int {
            return Double(number)
        }
        return nil
    }
    
    /// Extracts boolean value
    /// - Parameter value: Value of any type
    /// - Returns: Bool value, defaults to false
    public static func extractBooleanValue(_ value: Any) -> Bool {
        if let boolValue = value as? Bool {
            return boolValue
        }
        if let stringValue = value as? String {
            return stringValue.lowercased() == "true"
        }
        return false
    }
}
