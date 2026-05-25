//
//  CSSPropertyRegistry.swift
//  AGenUI
//
// Created on 2026/2/28.
//

import UIKit

/// CSS property registry
/// Manages configuration information for all CSS properties
class CSSPropertyRegistry {
    
    // MARK: - Singleton
    
    /// Shared instance
    static let shared: CSSPropertyRegistry = {
        let instance = CSSPropertyRegistry()
        // Automatically initialize all properties when singleton is created
        initialize()
        return instance
    }()
    
    // MARK: - Property Storage
    
    /// Property configuration dictionary
    /// Key: property name, Value: property configuration
    private static var configs: [String: CSSPropertyConfig] = [:]
    
    /// Whether initialization is complete
    private static var isInitialized = false
    
    // MARK: - Public Methods
    
    /// Registers property configuration
    /// - Parameter config: Property configuration to register
    static func register(config: CSSPropertyConfig) {
        configs[config.name] = config
    }
    
    /// Gets property configuration
    /// - Parameter property: Property name
    /// - Returns: Property configuration, or nil if not found
    static func config(for property: String) -> CSSPropertyConfig? {
        return configs[property]
    }
    
    /// Gets all registered property names
    /// - Returns: Set of property names
    func getAllPropertyNames() -> Set<String> {
        return Set(CSSPropertyRegistry.configs.keys)
    }
    
    /// Initializes all property configurations
    /// Registers all supported CSS properties
    static func initialize() {
        guard !isInitialized else { return }
        isInitialized = true
        
        // register
        registerVisualProperties()
    }
    
    //Registers all visual style properties.
    private static func registerVisualProperties() {
        
        
        // overflow (priority: 45)
        // Controls content overflow behavior
        register(config: CSSPropertyConfig(
            name: "overflow",
            valueType: .keyword,
            defaultValue: .keyword("visible"),
            validator: nil,
            priority: 45,
            validValues: ["visible", "hidden"]
        ))
        
        // visibility (priority: 45)
        // Controls element visibility (still takes up space)
        register(config: CSSPropertyConfig(
            name: "visibility",
            valueType: .keyword,
            defaultValue: .keyword("visible"),
            validator: nil,
            priority: 45,
            validValues: ["visible", "hidden"]
        ))
        
        // display (priority: 44)
        // Controls element display state (takes no space)
        // Lower priority than visibility, ensuring display is applied last and overrides visibility settings
        register(config: CSSPropertyConfig(
            name: "display",
            valueType: .keyword,
            defaultValue: nil,
            validator: nil,
            priority: 44,
            validValues: ["none", "flex"]
        ))
        
        // background (priority: 40)
        // Simplified version: only supports color values
        register(config: CSSPropertyConfig(
            name: "background",
            valueType: .color,
            defaultValue: nil,
            validator: nil,
            priority: 40
        ))
        
        // background-color (priority: 40)
        register(config: CSSPropertyConfig(
            name: "background-color",
            valueType: .color,
            defaultValue: nil,
            validator: nil,
            priority: 40
        ))
        
        // border-radius (priority: 40)
        register(config: CSSPropertyConfig(
            name: "border-radius",
            valueType: .dimension,
            defaultValue: .number(0),
            validator: nil,
            priority: 40
        ))
        
        // opacity (priority: 40)
        register(config: CSSPropertyConfig(
            name: "opacity",
            valueType: .opacity,
            defaultValue: .number(1.0),
            validator: nil,
            priority: 40
        ))
        
        // background-image (priority: 39, lower than background-color)
        // Supports CSS url() function format
        register(config: CSSPropertyConfig(
            name: "background-image",
            valueType: .url,
            defaultValue: nil,
            validator: nil,
            priority: 39
        ))
        
        // border-color (priority: 40)
        register(config: CSSPropertyConfig(
            name: "border-color",
            valueType: .color,
            defaultValue: .color(.black),
            validator: nil,
            priority: 40
        ))
        
        // border-width (priority: 40)
        register(config: CSSPropertyConfig(
            name: "border-width",
            valueType: .dimension,
            defaultValue: .number(0),
            validator: nil,
            priority: 40
        ))
        
        // border-style (priority: 40)
        // iOS only supports solid, this property is mainly used for validation
        register(config: CSSPropertyConfig(
            name: "border-style",
            valueType: .keyword,
            defaultValue: .keyword("solid"),
            validator: nil,
            priority: 40
        ))
        
        
        // filter (priority: 40)
        // Only supports drop-shadow filter effect
        register(config: CSSPropertyConfig(
            name: "filter",
            valueType: .shadow,
            defaultValue: nil,
            validator: nil,
            priority: 40
        ))
        
        // box-shadow (priority: 40)
        // Full box shadow support (including spread parameter)
        register(config: CSSPropertyConfig(
            name: "box-shadow",
            valueType: .shadow,
            defaultValue: nil,
            validator: nil,
            priority: 40
        ))
        
      }
    
}
