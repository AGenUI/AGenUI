//
//  Component.swift
//  AGenUI
//
// Created on 2026/4/1.
//

import UIKit

// MARK: - Measure Mode

/// Yoga measure mode (corresponds to YGMeasureMode)
///
/// Defines how the parent constrains the child's size during measurement.
/// - Undefined: The parent has not imposed any constraint. The child can be whatever size it wants.
/// - Exactly: The parent has determined an exact size for the child.
/// - AtMost: The child can be as large as it wants up to the specified size.
public enum MeasureMode: Int {
    case undefined = 0
    case exactly = 1
    case atMost = 2
}

/// Component base class - inherits from UIView
///
/// Core design philosophy: Component is View, View is Component
/// - Component itself is a UIView, no additional view property needed
/// - Parent-child relationship is view hierarchy: addChild() automatically calls addSubview()
/// - Tree structure managed via Component's parent/children properties
@objc open class Component: UIView {

    // MARK: - Layer Class

    /// Backing layer is CAGradientLayer; `colors == nil` behaves like CALayer.
    override public class var layerClass: AnyClass {
        return CAGradientLayer.self
    }

    var gradientLayer: CAGradientLayer? {
        return layer as? CAGradientLayer
    }

    // MARK: - Gradient Background

    private var gradientInfo: AGUIGradientInfo?

    /// Set gradient (non-nil) or clear it (nil) on the view's own layer.
    @MainActor func setGradient(_ info: AGUIGradientInfo?) {
        gradientInfo = info
        guard let gl = gradientLayer else { return }
        guard let gInfo = gradientInfo else {
            CATransaction.begin()
            CATransaction.setDisableActions(true)
            gl.colors = nil
            gl.locations = nil
            CATransaction.commit()
            return
        }
        guard bounds.width > 0, bounds.height > 0 else { return }
        CATransaction.begin()
        CATransaction.setDisableActions(true)
        CAGradientLayerFactory.configure(gInfo, on: gl, bounds: bounds)
        CATransaction.commit()
    }

    // MARK: - Core Properties
    
    /// Unique component identifier
    public let componentId: String
    
    /// Component type
    public let componentType: String
    
    /// Component properties
    public var properties: [String: Any] = [:]

    /// Per-key dirty tracking for incremental updates.
    /// Created on the first `updateProperties` call.
    private var state: ComponentState?

    // MARK: - Tree Structure
    
    /// Child components list
    public private(set) var children: [Component] = []
    
    /// Parent component
    public weak var parent: Component?
    
    /// Owning Surface
    public weak var surface: Surface?
    
    // MARK: - Action
    
    /// Action definition, extracted from properties["action"]
    private(set) var actionDef: [String: Any]?
    
    /// Tap gesture recognizer
    private var tapGesture: UITapGestureRecognizer?
    
    // MARK: - Callbacks
    
    /// Called after updateProperties completes
    /// - Parameters: the properties that were applied in this update
    public var onPropertiesUpdate: (([String: Any]) -> Void)?

    /// Called whenever this component's frame is actually changed (post-write).
    /// Container parents (e.g., ListComponent) can set this to be notified about
    /// engine-driven frame mutations and refresh their derived state (such as contentSize).
    /// Single-listener: setting replaces any previous closure.
    public var onFrameChange: ((CGRect) -> Void)?
    
    // MARK: - Initialization
    
    /// Initialize component
    ///
    /// - Parameters:
    ///   - componentId: Unique component identifier
    ///   - componentType: Component type
    ///   - properties: Initial properties
    public init(componentId: String, componentType: String, properties: [String: Any] = [:]) {
        self.componentId = componentId
        self.componentType = componentType
        self.properties = properties
        super.init(frame: .zero)
        #if DEBUG
        accessibilityLabel = "\(componentType) \(componentId)"
        accessibilityIdentifier = "\(componentType) \(componentId)"
        #endif
        
        // Note: Do not call updateProperties in base class init
        // because subclass properties (e.g., label) are not yet initialized
        // Subclasses should call updateProperties(properties) after creating internal views
    }
    
    required public init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Tree Operations
    
    /// Add a child component
    ///
    /// Automatically establishes parent-child relationship and adds to view hierarchy.
    /// Uses relative position insertion to ensure child view order matches properties["children"].
    ///
    /// - Parameter child: Child component
    @MainActor open func addChild(_ child: Component) {
        // Avoid duplicate addition
        if children.contains(where: { $0.componentId == child.componentId }) {
            return
        }
        
        // Set parent-child relationship
        child.parent = self
        child.surface = self.surface
        
        // Get target position from children array
        let childrenIds = getChildrenIdsFromProperties()
        guard let targetIndex = childrenIds.firstIndex(of: child.componentId) else {
            // Not in children array
            return
        }
        
        // Calculate actual insertion position (relative position insertion algorithm)
        // Iterate through siblings before current child in childrenIds, count those already in children array
        var insertPosition = 0
        for (index, siblingId) in childrenIds.enumerated() {
            if index >= targetIndex { break }
            // Check if this sibling is already in children array (by componentId match)
            if children.contains(where: { $0.componentId == siblingId }) {
                insertPosition += 1
            }
        }
        
        // Insert at correct position in children array
        children.insert(child, at: min(insertPosition, children.count))

        attachChildView(child, at: insertPosition)
    }
    
    /// Remove a child component
    ///
    /// - Parameter child: Child component
    @MainActor open func removeChild(_ child: Component) {
        // Remove from view hierarchy
        child.removeFromSuperview()
        
        // Clear parent-child relationship
        child.parent = nil
        child.surface = nil
        
        // Remove from list
        children.removeAll { $0.componentId == child.componentId }
    }
    
    /// Insert child component at specified position
    ///
    /// - Parameters:
    ///   - child: Child component
    ///   - index: Insertion position
    @MainActor open func insertChild(_ child: Component, at index: Int) {
        guard index >= 0 && index <= children.count else { return }
        
        // Avoid duplicate addition
        if children.contains(where: { $0.componentId == child.componentId }) {
            return
        }
        
        children.insert(child, at: index)
        child.parent = self
        child.surface = self.surface

        attachChildView(child, at: index)
    }

    private func attachChildView(_ child: Component, at index: Int) {
        guard shouldCreateChildView() else { return }
        guard canCreateChildViewConsideringParent() || isViewCreated else { return }

        child.createView()
        if child.superview !== self {
            insertSubview(child, at: min(index, subviews.count))
        }
    }
    
    /// Get child component
    ///
    /// - Parameter componentId: Component ID
    /// - Returns: Child component instance, or nil if not found
    @MainActor public func getChild(_ componentId: String) -> Component? {
        return children.first { $0.componentId == componentId }
    }
    
    /// Find child component (recursive)
    ///
    /// - Parameter componentId: Component ID
    /// - Returns: Child component instance, or nil if not found
    @MainActor public func findChild(_ componentId: String) -> Component? {
        // First search direct children
        if let child = getChild(componentId) {
            return child
        }
        
        // Recursive search
        for child in children {
            if let found = child.findChild(componentId) {
                return found
            }
        }
        
        return nil
    }
    
    // MARK: - Children Management
    
    /// Get child component IDs from properties
    ///
    /// Subclasses can override to customize which properties to extract child IDs from.
    /// Default: extracts from properties["children"].
    ///
    /// - Returns: Child component ID array
    @MainActor open func getChildrenIdsFromProperties() -> [String] {
        return properties["children"] as? [String] ?? []
    }
    
    // MARK: - Property Updates
    
    /// Update component properties
    ///
    /// First call: initialises `ComponentState` and runs the full-apply path.
    /// Subsequent calls: uses the incremental path — the diff is compared per-key
    /// against stored values, and layout/CSS is skipped when nothing actually changed.
    ///
    /// Aligned with HarmonyOS `A2UIComponent::updateProperties` and Android
    /// `A2UIComponent.updateProperties`.
    ///
    /// - Parameter properties: diff map from the core engine (only changed keys)
    @MainActor open func updateProperties(_ properties: [String: Any]) {
        if state == nil {
            state = ComponentState()
        }

        // Per-key compare against stored values; only truly changed keys are marked dirty.
        state!.updateProperties(properties)

        // Flatten supported CSS keys out of the styles sub-object and merge into stored properties.
        var allProperties = properties
        if let styles = allProperties["styles"] as? [String: Any] {
            let supportedStyles = filterSupportedProperties(styles)
            allProperties.merge(supportedStyles) { _, new in new }
            // Keep the latest Yoga frame for later lazy createView() replays.
            var storedStyles = self.properties["styles"] as? [String: Any] ?? [:]
            for key in ["x", "y", "width", "height"] {
                if let value = styles[key] {
                    storedStyles[key] = value
                }
            }
            self.properties["styles"] = storedStyles
            allProperties.removeValue(forKey: "styles")
        }
        self.properties.merge(allProperties) { _, new in new }

        // Skip the entire apply cycle when nothing actually changed.
        if !state!.isDirty {
            return
        }

        // Layout + CSS only when styles changed
        if let styles = properties["styles"] as? [String: Any] {
            applyLayoutFromStyles(styles)
            CSSPropertyApplier.apply(properties: allProperties, to: self)
        }

        // Extract and process action
        if let action = allProperties["action"] as? [String: Any] {
            self.actionDef = action
            addTapGesture()
        }

        #if DEBUG
        accessibilityHint = properties.description
        #endif

        // Apply accessibility attributes from DSL
        applyAccessibility()

        // Notify properties update callback
        onPropertiesUpdate?(allProperties)

        state!.clearDirty()
    }

    // MARK: - Accessibility

    /// Apply accessibility attributes from DSL `accessibility` property.
    ///
    /// Maps `label` to `accessibilityLabel` and `description` to `accessibilityHint`.
    /// Only touches accessibility state when the `accessibility` field is present and non-empty;
    /// otherwise resets to system defaults so that removing the field from DSL clears VoiceOver text.
    private func applyAccessibility() {
        guard let a11y = self.properties["accessibility"] as? [String: Any], !a11y.isEmpty else {
            resetAccessibility()
            return
        }

        // label -> accessibilityLabel
        if let label = a11y["label"] as? String, !label.isEmpty {
            self.accessibilityLabel = label
            self.isAccessibilityElement = true
        } else {
            self.accessibilityLabel = nil
        }

        // description -> accessibilityHint
        if let desc = a11y["description"] as? String, !desc.isEmpty {
            self.accessibilityHint = desc
        } else {
            self.accessibilityHint = nil
        }
    }

    /// Reset accessibility properties to their system default state.
    private func resetAccessibility() {
        self.isAccessibilityElement = false
        self.accessibilityLabel = nil
        self.accessibilityHint = nil
    }

    // MARK: - Visual Style Hooks
    
    /// Called when border-radius is applied via CSS.
    ///
    /// Subclasses can override this to propagate the radius to inner subviews
    /// (e.g., ImageComponent mirrors it to imageView, TableComponent to innerTableView).
    /// The base implementation sets self.layer.cornerRadius only.
    ///
    /// - Parameter radius: Corner radius in points
    @MainActor open func setBorderRadius(_ radius: CGFloat) {
        layer.cornerRadius = radius
    }
    
    /// Filter supported CSS properties
    /// - Parameter properties: Original properties dictionary
    /// - Returns: Dictionary containing only supported properties
    private func filterSupportedProperties(_ properties: [String: Any]) -> [String: Any] {
        let supportedKeys = CSSPropertyRegistry.shared.getAllPropertyNames()
        return properties.filter { key, _ in
            supportedKeys.contains(key)
        }
    }
    
    /// Base point scale factor: converts a2ui units to pt (a2ui / 2 = pt)
    public static let BS_POINT_SCALE: CGFloat = 0.5

    /// Apply layout position and size from Engine-computed styles (x, y, width, height)
    ///
    /// The C++ Engine computes layout via Yoga and includes x, y, width, height
    /// in the styles dictionary. iOS applies these directly to self.frame,
    /// matching HarmonyOS's A2UIComponent::updateLayoutProperties() behavior.
    ///
    /// - Parameter styles: The styles sub-dictionary from the component JSON
    private func applyLayoutFromStyles(_ styles: [String: Any]) {
        let x = cgFloatValue(styles["x"]) * Component.BS_POINT_SCALE
        let y = cgFloatValue(styles["y"]) * Component.BS_POINT_SCALE
        let width = max(0, cgFloatValue(styles["width"]) * Component.BS_POINT_SCALE)
        let height = max(0, cgFloatValue(styles["height"]) * Component.BS_POINT_SCALE)

        var newFrame = self.frame
        newFrame.origin.x = x
        newFrame.origin.y = y
        newFrame.size.width  = width
        newFrame.size.height = height
        self.frame = newFrame
    }
    
    /// Convert a numeric value from styles dictionary to CGFloat
    /// Handles Int, Float, Double, and NSNumber types from JSON parsing
    private func cgFloatValue(_ value: Any?) -> CGFloat {
        guard let value = value else { return 0 }
        if let d = value as? Double { return CGFloat(d) }
        if let i = value as? Int { return CGFloat(i) }
        if let f = value as? Float { return CGFloat(f) }
        return 0
    }

    // MARK: - View Lifecycle
    /// view creation to createView(); non-lazy components create views in init.
    
    open func shouldCreateChildView() -> Bool {
        return true
    }
    
    func canCreateChildViewConsideringParent() -> Bool{
        guard shouldCreateChildView() else {return false}
        if let parent = parent,!parent.canCreateChildViewConsideringParent(){
            return false
        }
        return true
    }
    

    public private(set) var isViewCreated: Bool = false


    /// Idempotent lifecycle hook: creates internal views, recursively creates children,
    /// then applies all stored properties.
    open func createView() {
        guard !isViewCreated else { return }
        isViewCreated = true
        createChildViews()
        updateProperties(self.properties)
    }


    /// Recursively create views for all children and add them to the view hierarchy.
    private func createChildViews() {
        for child in children {
            child.createView()
            if child.superview != self {
                addSubview(child)
            }
        }
        
    }

    // MARK: - Layout

    /// Frame setter: notifies `onFrameChange` and re-applies gradient geometry on resize.
    open override var frame: CGRect {
        get { super.frame }
        set {
            let oldFrame = super.frame
            super.frame = newValue
            if oldFrame != newValue {
                onFrameChange?(newValue)
                if oldFrame.size != newValue.size {
                    setGradient(gradientInfo)
                }
            }
        }
    }

    /// Notify C++ engine that this component has finished rendering with its actual size
    ///
    /// The size is converted to a2ui units (pt * 2) before passing to the engine.
    /// - Parameters:
    ///   - width: Rendered width in pt
    ///   - height: Rendered height in pt
    open func notifyLayoutChanged(width: CGFloat, height: CGFloat) {
        guard let surface = surface else { return }
        let widthA2ui = Float(width)   // pt -> a2ui
        let heightA2ui = Float(height)  // pt -> a2ui
        surface.surfaceManager?.notifyComponentRenderFinish(
            surfaceId: surface.surfaceId,
            componentId: componentId,
            type: componentType,
            width: widthA2ui,
            height: heightA2ui
        )
    }

    // MARK: - Gesture Handling
    
    /// Add tap gesture
    private func addTapGesture() {
        guard tapGesture == nil else { return }
        
        let gesture = UITapGestureRecognizer(target: self, action: #selector(handleTap))
        addGestureRecognizer(gesture)
        isUserInteractionEnabled = true
        tapGesture = gesture
    }
    
    /// Remove tap gesture
    private func removeTapGesture() {
        guard let gesture = tapGesture else { return }
        removeGestureRecognizer(gesture)
        tapGesture = nil
    }
    
    /// Trigger UI action to notify SDK of user interaction
    ///
    /// Component instance is already bound to its identifier, no need to pass it when calling.
    @objc public func triggerAction() {
        guard let actionDef = actionDef, let surface = surface else { return }
        surface.surfaceManager?.triggerAction(
            surfaceId: surface.surfaceId,
            componentId: componentId,
            context: ["action": actionDef]
        )
    }

    /// Sync this component's UI state to the data model
    ///
    /// Suitable for UI state changes such as form input, toggle state, etc.
    ///
    /// - Parameter change: State change key-value pair
    @objc public func syncState(_ change: [String: Any]) {
        guard let surface = surface else { return }
        surface.surfaceManager?.syncState(
            surfaceId: surface.surfaceId,
            componentId: componentId,
            context: change
        )
    }

    /// Handle tap event
    @objc open func handleTap() {
        triggerAction()
    }
    
    // MARK: - Appearance Tracking

    /// Notify SurfaceManager that a child component has entered the visible area.
    /// - Parameters:
    ///   - parentType: Container type ("List" / "Carousel" etc.)
    ///   - properties: The appeared child's full properties dictionary
    final func notifyAppeared() {
        guard let surface = surface,
              let parent = parent else { return }

        var properties = properties
        properties.removeValue(forKey: "styles")
        properties["id"] = componentId
        surface.surfaceManager?.notifyComponentAppeared(
            surface: surface,
            parentComponentId: parent.componentId,
            parentType: parent.componentType,
            properties: properties
        )
    }

    // MARK: - Local Style Config
    
    /// Get the component's local style config
    ///
    /// Reads config for current component type from localConfig.json
    /// - Returns: Config dictionary, or nil if no config for current component type
    internal func getLocalStyleConfig() -> [String: Any]? {
        return ComponentStyleConfigManager.shared.getConfig(for: componentType)
    }

    // MARK: - Measurement

    /// Measure the intrinsic size of a component (called by Yoga layout engine)
    ///
    /// Subclasses override this method to provide the component's intrinsic size
    /// under the given constraints.
    /// This method is called on the engine's background thread; implementations must be thread-safe.
    /// Returns zero size by default (does not participate in Yoga measurement).
    open class func measure(type: String,
                       paramJson: String,
                       maxWidth: Float,
                       widthMode: MeasureMode,
                       maxHeight: Float,
                       heightMode: MeasureMode) -> CGSize {
        return .zero
    }

    // MARK: - Measurement Helpers

    /// Parse CGFloat from an attribute value (compatible with NSNumber and "32px" format strings)
    class func parseFloat(_ value: Any?, defaultValue: CGFloat) -> CGFloat {
        if let num = value as? NSNumber { return CGFloat(num.doubleValue) }
        if let str = value as? String {
            let clean = str.replacingOccurrences(of: "px", with: "")
            return CGFloat(Double(clean) ?? Double(defaultValue))
        }
        return defaultValue
    }

    /// Parse Int from an attribute value (compatible with NSNumber and String)
    class func parseInt(_ value: Any?, defaultValue: Int) -> Int {
        if let num = value as? NSNumber { return num.intValue }
        if let str = value as? String { return Int(str) ?? defaultValue }
        return defaultValue
    }


}
