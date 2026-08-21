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
    /// - Parameters: the raw diff values that were applied (.deleted → NSNull)
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
        // Subclasses should call updateProperties(DiffValue.from(properties)) after creating internal views
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

        // Resolve the view anchor BEFORE attaching: the last logical sibling
        // already mounted ahead of the newcomer.
        // `attachChildView` only mounts, it never recomputes positions.
        let anchorChild = children[..<insertPosition].last { $0.superview === self }
        attachChildView(child, above: anchorChild)
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

    private func attachChildView(_ child: Component, above anchorChild: Component?) {
        guard shouldCreateChildView() else { return }
        guard canCreateChildViewConsideringParent() || isViewCreated else { return }

        child.createView()
        if child.superview !== self {
            // Anchor insertion, never counting subview slots: sibling shadow
            // views sit between child bodies, so a numeric index derived from
            // the logical order can land wrong. Anchoring ABOVE the previous
            // sibling's body also leaves every sibling decoration layer
            // untouched (shadows sit below their bodies), keeping this path
            // free of decoration-layer knowledge. No previous sibling -> bottom.
            if let anchorChild {
                insertSubview(child, aboveSubview: anchorChild)
            } else {
                insertSubview(child, at: 0)
            }
            // The body's didMoveToSuperview re-pins the shadow below the body.
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
    
    // MARK: - Shadow

    /// Dedicated shadow view, mounted in the parent view directly below this body.
    /// The body's own layer never carries a shadow, so self-clipping
    /// (corner-radius / clipsToBounds) can never eat the shadow.
    /// Must stay a plain UIView: `setShadow` writes its frame, and a Component
    /// here could re-enter the frame/replay chain.
    private(set) var shadowView: UIView?

    /// Current shadow spec, kept so geometry changes (frame / corner radius)
    /// and reparenting can reapply via `setShadow(shadowInfo)` — mirrors
    /// `gradientInfo`.
    private var shadowInfo: CSSShadow?

    /// Set shadow (non-nil) or clear it (nil). Mirrors `setGradient`.
    ///
    /// Single cohesive shadow entry point: applies the spec, shapes the shadow
    /// from the body's current geometry, and mounts it directly below the body.
    ///
    /// matecode: deliberate full-replay — every call rewrites the layer props and
    /// allocates a fresh mask layer. Ceiling: measurable churn only under
    /// sustained per-frame geometry changes (animations/scroll). Upgrade path:
    /// split a geometry-only fast path (frame + path + mask) out of this method.
    @MainActor func setShadow(_ shadow: CSSShadow?) {
        shadowInfo = shadow
        guard let shadow else {
            shadowView?.removeFromSuperview()
            shadowView = nil
            return
        }
        let view: UIView
        if let existing = shadowView {
            view = existing
        } else {
            let created = UIView(frame: frame)
            created.backgroundColor = .clear
            created.isUserInteractionEnabled = false   // shadows never intercept touches
            shadowView = created
            view = created
        }
        // CSSShadow arrives pt-ready (the parser scales A2UI units to pt).
        // CSS Y-positive is downward, same as UIKit — offset used as-is.
        view.layer.shadowOffset = CGSize(width: shadow.offsetX, height: shadow.offsetY)
        view.layer.shadowRadius = shadow.blur
        view.layer.shadowColor = shadow.color.cgColor
        // shadowColor carries the alpha.
        view.layer.shadowOpacity = 1.0
        // matecode: deliberately NOT rasterized — rasterizing a masked shadow
        // layer clips the halo asymmetrically (CA offscreen buffer heuristic),
        // and AJX ships without rasterize. Ceiling: per-frame shadow layers
        // draw slightly slower; upgrade path only if profiling demands it.
        // Geometry: the shadow view has no content of its own, so the shadow is
        // shaped ONLY by shadowPath.
        view.frame = frame
        let shapePath = UIBezierPath(roundedRect: bounds, cornerRadius: layer.cornerRadius)
        view.layer.shadowPath = shapePath.cgPath
        // Hollow mask: the shadow only renders in the halo region outside the
        // shape — nothing under the shape itself, so a transparent body shows
        // no shadow bleeding through. Mask = outer rect (blur reach, ~2.5x
        // radius incl. offset) with the shape appended REVERSED, so the
        // non-zero winding rule cuts the interior out.
        let radiusOffset = view.layer.shadowRadius * 2.5   // blur reach ~2.5x radius
        let offset = view.layer.shadowOffset
        let maskPath = UIBezierPath(rect: CGRect(x: offset.width - radiusOffset,
                                                 y: offset.height - radiusOffset,
                                                 width: bounds.width + radiusOffset * 2,
                                                 height: bounds.height + radiusOffset * 2))
        maskPath.append(shapePath.reversing())
        let mask = CAShapeLayer()
        mask.path = maskPath.cgPath
        view.layer.mask = mask
        // Mount directly below the body in the current parent (no-op until the
        // body itself is mounted).
        if let parent = superview, view.superview !== parent {
            view.removeFromSuperview()
            parent.insertSubview(view, belowSubview: self)
        }
    }

    // MARK: - Shadow manual sync
    //
    // The shadow lives OUTSIDE the body, so none of UIKit's hierarchy machinery
    // keeps it in sync — every body state change must drag it along explicitly
    // (frame / hidden / alpha / reparent / removal).

    /// Removal takes the shadow out of the hierarchy with the body. The shadow
    /// view itself stays allocated so a later re-mount can reinsert it without
    /// waiting for the next property update.
    open override func removeFromSuperview() {
        shadowView?.removeFromSuperview()
        super.removeFromSuperview()
    }

    /// Whenever the body lands in a (new) superview, the shadow re-pins itself
    /// directly below the body there.
    open override func didMoveToSuperview() {
        super.didMoveToSuperview()
        setShadow(shadowInfo)
    }

    open override var isHidden: Bool {
        didSet { shadowView?.isHidden = isHidden }
    }

    open override var alpha: CGFloat {
        didSet { shadowView?.alpha = alpha }
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
    @MainActor open func updateProperties(_ diff: [String: DiffValue]) {
        if state == nil {
            state = ComponentState()
        }

        // Per-key compare against stored values; only truly changed keys are marked dirty.
        state!.updateProperties(diff)

        // Update self.properties storage: .value → store value, .deleted → remove key.
        for (key, dv) in diff {
            switch dv {
            case .value(let v):
                self.properties[key] = v
            case .deleted:
                self.properties.removeValue(forKey: key)
            }
        }

        // Skip the entire apply cycle when nothing actually changed.
        if !state!.isDirty {
            return
        }

        // Layout + CSS only when styles changed (styles kept at second level).
        // CSSPropertyApplier reads from the styles sub-dictionary, not flattened.
        if case .value(let stylesValue) = diff["styles"], let styles = stylesValue as? [String: Any] {
            applyLayoutFromStyles(styles)
            CSSPropertyApplier.apply(properties: styles, to: self)
        }

        // Action handling: .value → set action + add tap gesture, .deleted → remove
        switch diff["action"] {
        case .value(let actionValue):
            if let action = actionValue as? [String: Any] {
                self.actionDef = action
                addTapGesture()
            }
        case .deleted:
            removeTapGesture()
            self.actionDef = nil
        default:
            break
        }

        #if DEBUG
        accessibilityHint = diff.description
        #endif

        // Apply accessibility attributes from DSL
        applyAccessibility(diff: diff)

        // Notify properties update callback (convert back to raw [String: Any])
        onPropertiesUpdate?(diff.toRaw())

        state!.clearDirty()
    }

    // MARK: - Accessibility

    /// Apply accessibility attributes from DSL `accessibility` property.
    ///
    /// Maps `label` to `accessibilityLabel` and `description` to `accessibilityHint`.
    /// Only touches accessibility state when the `accessibility` field is present and non-empty;
    /// otherwise resets to system defaults so that removing the field from DSL clears VoiceOver text.
    private func applyAccessibility(diff: [String: DiffValue]) {
        switch diff["accessibility"] {
        case .value(let a11yValue):
            guard let a11y = a11yValue as? [String: Any], !a11y.isEmpty else {
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
        case .deleted:
            resetAccessibility()
        default:
            // No accessibility key in this diff — keep current state
            break
        }
    }

    /// Reset accessibility properties to their system default state.
    private func resetAccessibility() {
        self.isAccessibilityElement = false
        self.accessibilityLabel = nil
        self.accessibilityHint = nil
    }

    // MARK: - Visual Style Hooks
    
    /// Called when border-radius is applied via CSS — the single radius write
    /// seam for components.
    ///
    /// Subclasses can override this to propagate the radius to inner subviews.
    /// The base implementation sets self.layer.cornerRadius and reshapes the
    /// sibling shadow (which follows this radius), so a radius arriving after
    /// the shadow never leaves a stale shadowPath.
    ///
    /// - Parameter radius: Corner radius in points
    @MainActor open func setBorderRadius(_ radius: CGFloat) {
        layer.cornerRadius = radius
        setShadow(shadowInfo)
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
        updateProperties(DiffValue.from(self.properties))
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

    /// Frame setter: notifies `onFrameChange` and replays shadow/gradient.
    ///
    /// `super.frame =` does NOT reliably trigger the `bounds`/`center` overrides
    /// in Swift — UIKit's internal `setFrame:` → `setBounds:` + `setCenter:` chain
    /// does not dispatch through Swift's override machinery in all cases.
    /// So we replay shadow/gradient directly here.
    open override var frame: CGRect {
        get { super.frame }
        set {
            let oldFrame = super.frame
            let oldBounds = super.bounds
            super.frame = newValue
            if oldFrame != newValue {
                if oldBounds.size != super.bounds.size {
                    setGradient(gradientInfo)
                }
                setShadow(shadowInfo)
                onFrameChange?(newValue)
            }
        }
    }

    /// Center setter: replays the sibling shadow so it follows position writes
    /// that bypass `frame` (e.g. ButtonComponent.layoutSubviews recentering).
    open override var center: CGPoint {
        get { super.center }
        set {
            let oldFrame = super.frame
            super.center = newValue
            if oldFrame != super.frame {
                setShadow(shadowInfo)
            }
        }
    }

    /// Bounds setter: replays gradient and sibling shadow on size changes that
    /// bypass `frame` (e.g. direct `bounds =` writes or CALayer-driven resizes).
    open override var bounds: CGRect {
        get { super.bounds }
        set {
            let oldBounds = super.bounds
            super.bounds = newValue
            if oldBounds != newValue {
                if oldBounds.size != newValue.size {
                    setGradient(gradientInfo)
                }
                setShadow(shadowInfo)
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
    ///
    /// Sends an empty context, which tells the native layer to use this component's own
    /// `action` attribute. Kept identical across iOS / Android / HarmonyOS.
    @objc public func triggerAction() {
        guard actionDef != nil else { return }
        dispatchAction(context: [:])
    }

    /// Trigger UI action with a caller-supplied context.
    ///
    /// Use this when the action to execute is not the component's own top-level `action` —
    /// e.g. a custom component whose sub-regions carry their own actions (SpanText).
    ///
    /// The native layer resolves `context["action"]` as a standard A2UI action definition
    /// (`{"functionCall": ...}` or `{"event": ...}`); if it is absent or unparsable, the
    /// component's own `action` attribute is used instead.
    ///
    /// - Parameter context: Context data. Put the action definition under the `action` key.
    @objc public func triggerAction(context: [String: Any]) {
        dispatchAction(context: context)
    }

    private func dispatchAction(context: [String: Any]) {
        guard let surface = surface else { return }
        surface.surfaceManager?.triggerAction(
            surfaceId: surface.surfaceId,
            componentId: componentId,
            context: context
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
