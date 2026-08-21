//
//  ColumnComponent.swift
//  AGenUI
//
// Created on 2026/2/27.
//

import UIKit

/// ColumnComponent component implementation (compliant with A2UI v0.9 protocol)
///
/// Supported properties:
/// - children: Child component ID array (Array<String>)
/// - justify: Main axis alignment (String: start, center, end, spaceBetween, spaceAround, spaceEvenly)
/// - align: Cross axis alignment (String: start, center, end, stretch)
/// - spacing: Child component spacing (Double, default 0)
///
/// Design notes:
/// - Uses FlexLayout with .column direction for vertical layout
/// - CSS properties (justify-content, align-items, etc.) are applied automatically via CSSPropertyApplier
class ColumnComponent: Component {
    
    // MARK: - Initialization
    
    init(componentId: String, properties: [String: Any]) {
        super.init(componentId: componentId, componentType: "Column", properties: properties)
        
        // Apply initial properties
        updateProperties(DiffValue.from(properties))
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}
