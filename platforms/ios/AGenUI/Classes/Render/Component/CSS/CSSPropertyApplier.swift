//
//  CSSPropertyApplier.swift
//  AGenUI
//
// Created on 2026/2/28.
//

import UIKit

/// CSS property applier
/// Responsible for applying parsed CSS properties to components and views
class CSSPropertyApplier {
    // MARK: - Main Application Methods

    /// Applies CSS properties to a UIView (Component as View mode)
    static func apply(properties: [String: Any], to view: UIView) {
        let sortedProperties = sortByPriority(properties)
        for (key, value) in sortedProperties {
            let valueStr: String
            if let str = value as? String {
                valueStr = str
            } else {
                valueStr = "\(value)"
            }
            applyProperty(key: key, value: valueStr, to: view)
        }
    }

    /// Applies CSS properties to a component (BaseA2UIComponent mode)
    static func apply(properties: [String: Any], to component: Component, view: UIView) {
        let sortedProperties = sortByPriority(properties)
        for (key, value) in sortedProperties {
            let valueStr: String
            if let str = value as? String {
                valueStr = str
            } else {
                valueStr = "\(value)"
            }
            applyProperty(key: key, value: valueStr, to: component, view: view)
        }
    }

    // MARK: - Property Sorting
    
    /// Sort properties by priority
    /// - Parameter properties: Property dictionary
    /// - Returns: Sorted property array
    private static func sortByPriority(_ properties: [String: Any]) -> [(String, Any)] {
        // Pre-compute priorities for all properties to avoid repeated lookups during sort comparison
        // Sort complexity O(n log n), before optimization each comparison calls config(for:) twice, totaling 2n log n calls
        // After optimization only n lookups needed, performance improved by approximately 2 log n times
        var priorityCache: [String: Int] = [:]
        priorityCache.reserveCapacity(properties.count)
        
        for key in properties.keys {
            let config = CSSPropertyRegistry.config(for: key)
            priorityCache[key] = config?.priority ?? 0
        }
        
        // Sort using cached priorities
        return properties.sorted { prop1, prop2 in
            let priority1 = priorityCache[prop1.key] ?? 0
            let priority2 = priorityCache[prop2.key] ?? 0
            return priority1 > priority2
        }
    }
    
    // MARK: - Single Property Application
    
    /// Applies a single property (Component as View mode)
    /// - Parameters:
    ///   - key: Property name
    ///   - value: Property value string
    ///   - view: Target view
    private static func applyProperty(key: String, value: String, to view: UIView) {
        // Get property configuration
        guard let config = CSSPropertyRegistry.config(for: key) else {
            #if DEBUG
            Logger.shared.debug("Unknown property: \(key)")
            #endif
            return
        }
        
        // Parse property value
        let parsedValue = CSSPropertyParser.parse(value: value, config: config)
        
        // Validate property value
        if !parsedValue.isValid {
            #if DEBUG
            Logger.shared.debug("Invalid value for property \(key): \(value)")
            #endif
            return
        }
        
        // Apply based on property name
        applyPropertyByKey(key, parsedValue: parsedValue, to: view)
    }
    
    /// Applies a single property (BaseA2UIComponent mode)
    /// - Parameters:
    ///   - key: Property name
    ///   - value: Property value string
    ///   - component: Target component
    ///   - view: Target view
    private static func applyProperty(key: String, value: String, to component: Component, view: UIView) {
        // Get property configuration
        guard let config = CSSPropertyRegistry.config(for: key) else {
            #if DEBUG
            Logger.shared.debug("Unknown property: \(key)")
            #endif
            return
        }
        
        // Parse property value
        let parsedValue = CSSPropertyParser.parse(value: value, config: config)
        
        // Validate property value
        if !parsedValue.isValid {
            #if DEBUG
            Logger.shared.debug("Invalid value for property \(key): \(value)")
            #endif
            return
        }
        
        // Apply based on property name
        applyPropertyByKey(key, parsedValue: parsedValue, to: view)
    }
    
    /// Applies property value based on property name
    /// - Parameters:
    ///   - key: Property name
    ///   - parsedValue: Parsed property value
    ///   - view: Target view
    private static func applyPropertyByKey(_ key: String, parsedValue: CSSPropertyValue, to view: UIView) {
        switch key {

        // Style properties
        case "background":
            applyBackgroundColor(parsedValue, to: view)
        case "background-color":
            applyBackgroundColor(parsedValue, to: view)
        case "background-image":
            applyBackgroundImage(parsedValue, to: view)
        case "border-radius":
            applyBorderRadius(parsedValue, to: view)
        case "opacity":
            applyOpacity(parsedValue, to: view)
            
        // P1: Border properties
        case "border-color":
            applyBorderColor(parsedValue, to: view)
        case "border-width":
            applyBorderWidth(parsedValue, to: view)
            
        // Requirement 9: Display control and visual effects properties
        case "border-style":
            applyBorderStyle(parsedValue, to: view)
        case "overflow":
            applyOverflow(parsedValue, to: view)
        case "display":
            applyDisplay(parsedValue, to: view)
        case "visibility":
            applyVisibility(parsedValue, to: view)
        case "filter":
            applyFilter(parsedValue, to: view)
        case "box-shadow":
            applyBoxShadow(parsedValue, to: view)
            
        default:
            #if DEBUG
            Logger.shared.debug("Unhandled property: \(key)")
            #endif
        }
    }
    
    // MARK: - Style Properties
    
    /// Applies background color
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyBackgroundColor(_ value: CSSPropertyValue, to view: UIView) {
        // Tear down any gradient installed by a previous update so the new value
        // is the sole source of truth (idempotent across re-applies).
        BackgroundGradientHolder.remove(from: view)

        switch value {
        case .color(let color):
            view.backgroundColor = color
        case .gradient(let info):
            // CAGradientLayer paints the background; clear the UIView color so
            // the two don't double-stack.
            view.backgroundColor = .clear
            BackgroundGradientHolder.install(info, on: view)
        default:
            return
        }
    }
    
    /// Applies background image
    /// Supports URL formats:
    /// - Network URL: url("https://example.com/image.png")
    /// - Local resource: url("res://icon") or url(paper.gif)
    /// - Local file: url("file:///path/to/image.png")
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyBackgroundImage(_ value: CSSPropertyValue, to view: UIView) {
        guard case .url(let urlString) = value else { return }
        
        if urlString.isEmpty {
            // Clear background image
            view.layer.contents = nil
            return
        }
        
        // Determine URL type and load
        if urlString.hasPrefix("http://") || urlString.hasPrefix("https://") {
            // Network image - async load
            loadBackgroundImage(from: urlString, for: view)
        } else if urlString.hasPrefix("res://") {
            // Local resource
            let resName = String(urlString.dropFirst(6))
            if let image = UIImage(named: resName) {
                setBackgroundImage(image, for: view)
            }
        } else if urlString.hasPrefix("file://") {
            // Local file
            let filePath = String(urlString.dropFirst(7))
            if let image = UIImage(contentsOfFile: filePath) {
                setBackgroundImage(image, for: view)
            }
        } else {
            // Load as resource name
            if let image = UIImage(named: urlString) {
                setBackgroundImage(image, for: view)
            }
        }
    }
    
    /// Sets background image to view layer
    /// - Parameters:
    ///   - image: Image
    ///   - view: Target view
    private static func setBackgroundImage(_ image: UIImage, for view: UIView) {
        view.layer.contents = image.cgImage
        view.layer.contentsGravity = .resizeAspectFill
    }
    
    /// Asynchronously loads network background image
    /// - Parameters:
    ///   - urlString: Image URL
    ///   - view: Target view
    private static func loadBackgroundImage(from urlString: String, for view: UIView) {
        guard let url = URL(string: urlString) else { return }
        
        URLSession.shared.dataTask(with: url) { data, _, error in
            guard let data = data,
                  let image = UIImage(data: data),
                  error == nil else {
                return
            }
            DispatchQueue.main.async {
                setBackgroundImage(image, for: view)
            }
        }.resume()
    }
    
    /// Applies border radius
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyBorderRadius(_ value: CSSPropertyValue, to view: UIView) {
        guard case .number(let radius) = value else { return }
        // Dispatch to Component.setBorderRadius if available, allowing subclasses to propagate
        // the radius to inner subviews (e.g., imageView, innerTableView) via override.
        // For plain UIViews, fall back to setting layer.cornerRadius directly.
        if let component = view as? Component {
            component.setBorderRadius(radius)
        } else {
            view.layer.cornerRadius = radius
        }
    }
    
    /// Applies opacity
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyOpacity(_ value: CSSPropertyValue, to view: UIView) {
        guard case .number(let opacity) = value else { return }
        // Ensure opacity is between 0.0 and 1.0
        view.alpha = max(0.0, min(1.0, opacity))
    }
    

    
    // MARK: - P1 Border Properties
    
    /// Applies border color
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyBorderColor(_ value: CSSPropertyValue, to view: UIView) {
        guard case .color(let color) = value else { return }
        view.layer.borderColor = color.cgColor
    }
    
    /// Applies border width
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyBorderWidth(_ value: CSSPropertyValue, to view: UIView) {
        guard case .number(let width) = value else { return }
        view.layer.borderWidth = width
    }
    

    
    // MARK: - Requirement 9: Display Control and Visual Effects Properties
    
    /// Applies border style (iOS only supports solid)
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyBorderStyle(_ value: CSSPropertyValue, to view: UIView) {
        guard case .keyword(let style) = value else {
            // Parsing failed, reset border width
            view.layer.borderWidth = 0
            return
        }
        
        // iOS only supports solid border style
        if style != "solid" {
            // Invalid border-style value, reset border width
            view.layer.borderWidth = 0
            #if DEBUG
            Logger.shared.debug("Warning: border-style '\(style)' not supported, only 'solid' is supported on iOS")
            #endif
        }
        // If solid, no additional action needed (iOS default is solid)
    }
    
    /// Applies overflow control
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyOverflow(_ value: CSSPropertyValue, to view: UIView) {
        guard case .keyword(let overflow) = value else { return }
        
        switch overflow {
        case "hidden":
            view.clipsToBounds = true
        case "visible":
            view.clipsToBounds = false
        default:
            #if DEBUG
            Logger.shared.debug("Unknown overflow value: \(overflow)")
            #endif
        }
    }
    
    /// Applies display control
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyDisplay(_ value: CSSPropertyValue, to view: UIView) {
        guard case .keyword(let display) = value else { return }
        
        switch display {
        case "none":
            view.isHidden = true
        default:
            view.isHidden = false
        }
    }
    
    /// Applies visibility control
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyVisibility(_ value: CSSPropertyValue, to view: UIView) {
        guard case .keyword(let visibility) = value else { return }
        
        switch visibility {
        case "hidden":
            view.isHidden = true
            // visibility:hidden still takes space, does not affect FlexLayout
        case "visible":
            view.isHidden = false
        default:
            #if DEBUG
            Logger.shared.debug("Unknown visibility value: \(visibility)")
            #endif
        }
    }
    
    /// Applies filter property (only supports drop-shadow)
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyFilter(_ value: CSSPropertyValue, to view: UIView) {
        guard case .shadow(let shadow) = value else { return }
        applyShadow(shadow, to: view, includeSpread: false)
    }
    
    /// Applies box-shadow property
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    private static func applyBoxShadow(_ value: CSSPropertyValue, to view: UIView) {
        guard case .shadow(let shadow) = value else { return }
        applyShadow(shadow, to: view, includeSpread: true)
    }
    
    /// Applies shadow effect
    /// - Parameters:
    ///   - shadow: Shadow configuration
    ///   - view: Target view
    ///   - includeSpread: Whether to include spread parameter (used by box-shadow)
    private static func applyShadow(_ shadow: CSSShadow, to view: UIView, includeSpread: Bool) {
        // Set shadow offset
        // CSS coordinate system: Y-axis positive direction is downward
        // iOS shadowOffset: Y-axis positive direction is also downward (consistent with UIKit coordinate system)
        // Therefore use original value directly, no need to negate
        view.layer.shadowOffset = CGSize(width: shadow.offsetX/2.0, height: shadow.offsetY/2.0)
        
        // Set shadow blur radius
        // iOS shadowRadius is blur radius, CSS blur is diameter, so divide by 2
        view.layer.shadowRadius = shadow.blur / 2.0
        
        // Set shadow color
        view.layer.shadowColor = shadow.color.cgColor
        
        // Set shadow opacity from color's alpha channel
        var alpha: CGFloat = 0
        if shadow.color.getRed(nil, green: nil, blue: nil, alpha: &alpha) {
            view.layer.shadowOpacity = Float(alpha)
        } else {
            // Fallback for grayscale colors
            var white: CGFloat = 0
            if shadow.color.getWhite(&white, alpha: &alpha) {
                view.layer.shadowOpacity = Float(alpha)
            } else {
                // Default fully opaque
                view.layer.shadowOpacity = 1.0
            }
        }
        
        // Handle spread parameter (only used by box-shadow)
        if includeSpread, let spread = shadow.spread {
            // Implement spread effect via shadowPath (including spread = 0 case)
            let rect = view.bounds.insetBy(dx: -spread, dy: -spread)
            view.layer.shadowPath = UIBezierPath(
                roundedRect: rect,
                cornerRadius: view.layer.cornerRadius
            ).cgPath
        } else {
            view.layer.shadowPath = nil
        }
        
        // Performance optimization: enable rasterization
        view.layer.shouldRasterize = true
        view.layer.rasterizationScale = UIScreen.main.scale
    }
    
    /// Clears shadow effect
    /// - Parameter view: Target view
    private static func clearShadow(from view: UIView) {
        view.layer.shadowOpacity = 0
        view.layer.shadowPath = nil
        view.layer.shouldRasterize = false
    }
        


}

// MARK: - Background Gradient Lifecycle

/// Owns the CAGradientLayer that backs a CSS `background: linear-gradient(...)`
/// (or radial / conic) value. Lives on the host UIView via an associated object so
/// repeated applies stay idempotent and so we can tear the layer down cleanly when
/// the background switches back to a solid color.
///
/// Geometry is bounds-dependent, so we watch `view.layer.bounds` via
/// NSKeyValueObservation and rebuild on each change. The engine also re-invokes
/// `applyBackgroundColor` whenever it pushes a layout update (since layout x/y/w/h
/// arrive inside the same styles dict), so the KVO path is mostly belt-and-suspenders
/// for non-engine-driven frame changes.
fileprivate enum BackgroundGradientHolder {

    private static var key: UInt8 = 0

    static func install(_ info: AGUIGradientInfo, on view: UIView) {
        let state = State(view: view, info: info)
        objc_setAssociatedObject(view, &key, state, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
    }

    static func remove(from view: UIView) {
        if let state = objc_getAssociatedObject(view, &key) as? State {
            state.dispose()
        }
        objc_setAssociatedObject(view, &key, nil, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
    }

    /// One per (view, gradient) installation. Holds the CAGradientLayer + KVO token.
    private final class State {
        private weak var view: UIView?
        private let info: AGUIGradientInfo
        private var gradientLayer: CAGradientLayer?
        private var observation: NSKeyValueObservation?

        init(view: UIView, info: AGUIGradientInfo) {
            self.view = view
            self.info = info
            rebuild()
            // CALayer.bounds is KVO-compliant (unlike most UIView properties),
            // so we observe at the layer level for reliable updates.
            self.observation = view.layer.observe(\.bounds, options: [.new]) { [weak self] _, _ in
                self?.rebuild()
            }
        }

        func dispose() {
            observation?.invalidate()
            observation = nil
            gradientLayer?.removeFromSuperlayer()
            gradientLayer = nil
        }

        private func rebuild() {
            guard let view = view else { return }
            let bounds = view.bounds
            // Defer until the view actually has a non-zero size; KVO will fire again.
            guard bounds.width > 0, bounds.height > 0 else { return }

            guard let fresh = CAGradientLayerFactory.build(info, bounds: bounds) else {
                return
            }

            // Mirror the host view's cornerRadius so the gradient is clipped by
            // border-radius even when the host has not opted into masksToBounds
            // (UIView's solid backgroundColor is rounded for free; sublayers are
            // not unless we set their own cornerRadius + masksToBounds).
            let hostRadius = view.layer.cornerRadius
            fresh.cornerRadius = hostRadius
            fresh.masksToBounds = hostRadius > 0

            if let existing = gradientLayer {
                // Reuse the existing layer to avoid sublayer churn / flicker.
                CATransaction.begin()
                CATransaction.setDisableActions(true)
                existing.frame = bounds
                existing.type = fresh.type
                existing.colors = fresh.colors
                existing.locations = fresh.locations
                existing.startPoint = fresh.startPoint
                existing.endPoint = fresh.endPoint
                existing.cornerRadius = fresh.cornerRadius
                existing.masksToBounds = fresh.masksToBounds
                CATransaction.commit()
            } else {
                CATransaction.begin()
                CATransaction.setDisableActions(true)
                view.layer.insertSublayer(fresh, at: 0)
                CATransaction.commit()
                gradientLayer = fresh
            }
        }
    }
}
