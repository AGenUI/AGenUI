//
//  RowComponent.swift
//  AGenUI
//
// Created on 2026/2/27.
//

import UIKit

/// RowComponent component implementation (compliant with A2UI v0.9 protocol)
///
/// Supported properties:
/// - children: Child component ID array (Array<String>)
/// - justify: Main axis alignment (String: start, center, end, spaceBetween, spaceAround, spaceEvenly)
/// - align: Cross axis alignment (String: start, center, end, stretch)
/// - spacing: Child component spacing (Double, default 0)
///
/// Design notes:
/// - Uses FlexLayout with .row direction for horizontal layout
/// - CSS properties (justify-content, align-items, etc.) are applied automatically via CSSPropertyApplier
/// - Gap/spacing is implemented via margin on child components (FlexLayout does not directly support gap)
class RowComponent: Component {
    
    // MARK: - Initialization
    
    init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "Row", properties: properties)
        
        // Apply initial properties
        updateProperties(DiffValue.from(properties))
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}
