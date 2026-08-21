//
//  CSSValueType.swift
//  AGenUI
//
// Created on 2026/2/28.
//

import Foundation

/// CSS value type enum
/// Defines type classification for CSS property values
enum CSSValueType {
    /// Dimension value type, supports numbers and percentages
    /// Example: "100", "50%"
    case dimension

    /// Color value type
    /// Example: "#FF0000", "rgb(255, 0, 0)", "rgba(255, 0, 0, 0.5)"
    case color
    
    
    /// Opacity type, numeric range 0-1
    /// Example: "0.5", "1.0"
    case opacity
    
    /// Shadow value type
    /// Used for the filter (drop-shadow) property
    /// Example: "drop-shadow(0 2 4 rgba(0,0,0,0.1))"
    case shadow
    
    /// Keyword type
    /// Used for enumerated value properties, such as border-style, overflow, display, visibility
    /// Example: "solid", "hidden", "none", "visible"
    case keyword
    
    /// URL type (CSS url() function)
    /// Used for background-image and similar properties
    /// Example: url("https://example.com/image.png"), url(paper.gif)
    case url
}
