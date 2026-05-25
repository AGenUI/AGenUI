//
//  Surface.swift
//  AGenUI
//
// Created on 2026/2/27.
//  Refactored on 2026/4/1.
//

import UIKit

/// Surface - Independent UI canvas
///
/// Manages component tree, where each Component is a UIView subclass.
/// The root component serves as the Surface's view.
@objc public class Surface: NSObject {
    
    // MARK: - Properties
    
    /// Surface unique identifier
    @objc public let surfaceId: String
    
    /// Surface width constraint (CGFloat.infinity means infinite width)
    private(set) var width: CGFloat
    
    /// Surface height constraint (CGFloat.infinity means infinite height)
    private(set) var height: CGFloat
    
    /// Surface container view
    /// Root component will be added as a subview of this view
    @objc public let view: UIView
    
    /// Root component (tree root)
    private(set) var rootComponent: Component?
    
    /// Component tree (componentId -> Component)
    private var componentTree: [String: Component] = [:]
    
    /// Layout change callback
    @objc public var onLayoutChanged: (() -> Void)?
    
    /// Whether component appear animations are enabled for this surface
    @objc public var animationEnabled: Bool = true

    /// Original raw protocol content (the full JSON string that was parsed to create this surface)
    @objc public var rawProtocolContent: String = ""
    
    /// Associated SurfaceManager (weak reference to avoid retain cycle)
    weak var surfaceManager: SurfaceManager?
    
    /// Flag indicating whether built-in components have been registered
    private static var hasRegisteredBuiltInComponents = false
    
    // MARK: - Blank Check Properties
    
    /// Pending blank-check work item; cancelled on deinit or when re-armed
    /// by another `startBlankCheck(...)` invocation.
    private var blankCheckWorkItem: DispatchWorkItem?
    
    // MARK: - Initialization
    
    /// Initialize Surface
    ///
    /// - Parameters:
    ///   - surfaceId: Surface ID
    @objc public init(surfaceId: String) {
        self.surfaceId = surfaceId
        self.width = CGFloat.infinity
        self.height = CGFloat.infinity
        
        // Create container view
        let containerView = UIView()
        containerView.backgroundColor = .clear
        containerView.translatesAutoresizingMaskIntoConstraints = true
        containerView.clipsToBounds = false
        self.view = containerView
        
        self.view.accessibilityLabel = surfaceId;
        self.view.accessibilityIdentifier = surfaceId;
        
        Logger.shared.debug("Created: \(surfaceId), width: \(width), height: \(height)")
    }
    
    /// Create component using registered factory
    ///
    /// - Parameters:
    ///   - type: Component type
    ///   - id: Component ID
    ///   - properties: Component properties
    /// - Returns: Created component, or nil if factory not found
    private func createComponent(_ type: String, id: String, properties: [String: Any]) -> Component? {
        return ComponentRegister.shared.createComponent(type, id: id, properties: properties)
    }
    
    // MARK: - Notification
    @objc internal func notifyLayoutChangedInternal() {
        notifyLayoutChangedInternalReal()
    }
    
    /// Notify layout change
    @objc internal func notifyLayoutChangedInternalReal() {
        guard let rootComponent = rootComponent else {
            return
        }
        
        // Sync surface.view size with the root component's frame computed by C++ Yoga engine.
        // Each Component's frame is set by applyLayoutFromStyles() when updateProperties() is called.
        // Without this step, surface.view always has zero height and callers cannot read the
        // rendered content height from surface.view.frame in the onLayoutChanged callback.
        let contentWidth  = rootComponent.frame.width
        let contentHeight = rootComponent.frame.maxY  // use maxY to include absolute-positioned children that extend below root
        if contentWidth > 0 || contentHeight > 0 {
            view.frame = CGRect(x: view.frame.origin.x,
                                y: view.frame.origin.y,
                                width: contentWidth,
                                height: contentHeight)
        }
        
        // Layout computed by C++ Engine; iOS only triggers callback
        onLayoutChanged?()
    }
    
    // MARK: - Size Management
    
    /// Update Surface size constraints
    ///
    /// - Parameters:
    ///   - width: New width constraint
    ///   - height: New height constraint
    @objc public func updateSize(width: CGFloat, height: CGFloat) {
        // Normalize size values
        let normalizedWidth = normalizeSize(width)
        let normalizedHeight = normalizeSize(height)
        
        self.width = normalizedWidth
        self.height = normalizedHeight
        
        Logger.shared.debug("Size updated: width=\(width)(normalized: \(normalizedWidth)), height=\(height)(normalized: \(normalizedHeight))")
        
        // Notify C++ Yoga engine that surface size changed
        // a2ui units = pt * 2 (consistent with BS_POINT_SCALE = 0.5)
        let widthCXX = normalizedWidth.isFinite ? Float(normalizedWidth) : 0.0
        let heightCXX = normalizedHeight.isFinite ? Float(normalizedHeight) : 0.0
        surfaceManager?.notifySurfaceSizeChanged(surfaceId: surfaceId, width: widthCXX, height: heightCXX)
        
        // Trigger layout change
        notifyLayoutChangedInternal()
    }
    
    /// Normalize size value - convert CGFloat_MAX to infinity
    private func normalizeSize(_ value: CGFloat) -> CGFloat {
        let maxThreshold: CGFloat = 1e308
        return value >= maxThreshold ? CGFloat.infinity : value
    }
    
    // MARK: - Component Management
    
    /// Add component to tree
    ///
    /// - Parameters:
    ///   - componentId: Component ID
    ///   - componentType: Component type
    ///   - properties: Component properties
    func addComponent(componentId: String, componentType: String, properties: [String: Any], parentId: String?) {
        Logger.shared.debug("[Surface] Adding component: \(componentId) (\(componentType))")
        
        // Check if component already exists
        if componentTree[componentId] != nil {
            Logger.shared.debug("⚠ Component already exists: \(componentId)")
            return
        }
        
        // Create component
        guard let component = createComponent(componentType, id: componentId, properties: properties) else {
            Logger.shared.error("Failed to create component: \(componentType)")
            return
        }
        
        // Set owning Surface
        component.surface = self
        
        // Add to component tree
        componentTree[componentId] = component
        Logger.shared.debug("Component added to componentTree")
        
        // Case 1: componentId is "root" - add to view as root component
        if componentId == "root" {
            rootComponent = component
            Logger.shared.debug("Set as root component (id == 'root')")
            
            // Add root component to view
            view.addSubview(component)
            
            // Set up properties update callback for root component
            component.onPropertiesUpdate = { [weak self] props in
                guard let self = self else { return }
                self.surfaceManager?.notifyRootComponentUpdate(surface: self, props: props)
            }
        }
        
        // Case 2: Check if parent already exists in componentTree
        // (parent arrived before this child) - traverse to find parent
        if componentId != "root" && component.parent == nil && parentId != nil {
            if let parentComponent = getComponent(componentId: parentId!) {
                parentComponent.addChild(component)
                Logger.shared.debug("Added to existing parent: \(parentComponent.componentId)")
            } else {
                Logger.shared.error("Component added to pendingChildren error: \(componentId)")
            }
        }
        
        notifyLayoutChangedInternal()
        Logger.shared.debug("Component added: \(component.componentId)")
    }
    
    /// Remove component from tree
    ///
    /// - Parameter componentId: Component ID
    func removeComponent(componentId: String) {
        guard let component = componentTree[componentId] else {
            Logger.shared.debug("Component not found: \(componentId)")
            return
        }
        
        // Remove from parent
        component.parent?.removeChild(component)
        
        // Destroy component
        component.destroy()
        
        // Remove from tree
        componentTree.removeValue(forKey: componentId)
        
        // If it's the root component
        if rootComponent?.componentId == componentId {
            rootComponent = nil
        }
        
        // Notify layout change
        notifyLayoutChangedInternal()
        
        Logger.shared.debug("Component removed: \(componentId)")
    }
    
    /// Update component properties
    ///
    /// - Parameters:
    ///   - componentId: Component ID
    ///   - properties: New properties
    func updateComponent(componentId: String, properties: [String: Any]) {
        guard let component = componentTree[componentId] else {
            Logger.shared.debug("Component not found: \(componentId)")
            return
        }
        
        // Update properties (component.properties["children"] is now updated)
        component.updateProperties(properties)
        
        // Notify layout change
        notifyLayoutChangedInternal()
        
        Logger.shared.debug("Component updated: \(componentId)")
    }
    
    /// Get component by ID
    ///
    /// - Parameter componentId: Component ID
    /// - Returns: Component instance
    func getComponent(componentId: String) -> Component? {
        return componentTree[componentId]
    }
    
    /// Get all components
    ///
    /// - Returns: All components in tree
    func getAllComponents() -> [Component] {
        return Array(componentTree.values)
    }
    
    // MARK: - Blank Check
    
    /// Start blank-check on this Surface.
    ///
    /// Calling this method **immediately** schedules a single delayed detection.
    /// After `checkDelayMs` elapses, the SDK counts current valid components on
    /// this Surface and emits the result via
    /// `SurfaceManagerListener.onBlankCheckResult(_:isBlank:)`.
    ///
    /// Calling again before the previous detection fires will cancel the previous
    /// schedule and start a new one with the latest parameters.
    ///
    /// - Parameters:
    ///   - checkDelayMs:        Delay (in ms) before detection runs
    ///   - validComponentCount: Threshold; if actual valid component count is lower, treated as blank
    @objc public func startBlankCheck(checkDelayMs: Int, validComponentCount: Int) {
        let delayMs = max(0, checkDelayMs)
        let threshold = max(0, validComponentCount)
        Logger.shared.info("BlankCheck armed: surface=\(surfaceId), delayMs=\(delayMs), threshold=\(threshold)")
        
        // Cancel any previous pending check
        blankCheckWorkItem?.cancel()
        
        let work = DispatchWorkItem { [weak self] in
            guard let self = self else { return }
            // lcpX rule: count components whose view bounds (width & height) are both > 0
            var count = 0
            for component in self.componentTree.values {
                if component.bounds.width > 0 && component.bounds.height > 0 {
                    count += 1
                    if count >= threshold { break }
                }
            }
            let isBlank = count < threshold
            Logger.shared.info("BlankCheck result: surface=\(self.surfaceId), count=\(count), threshold=\(threshold), isBlank=\(isBlank)")
            self.surfaceManager?.notifyBlankCheckResult(surface: self, isBlank: isBlank)
        }
        blankCheckWorkItem = work
        
        DispatchQueue.main.asyncAfter(deadline: .now() + .milliseconds(delayMs), execute: work)
    }
    
    // MARK: - Lifecycle
    
    deinit {
        Logger.shared.debug("Deinit: \(surfaceId)")
        
        // Cancel any pending blank check
        blankCheckWorkItem?.cancel()
        blankCheckWorkItem = nil
        
        // Destroy all components
        for component in componentTree.values {
            component.destroy()
        }
        
        // Clear component tree
        componentTree.removeAll()
        rootComponent = nil
        
        Logger.shared.debug("Deinitialized: \(surfaceId)")
    }
    
    // MARK: - JSON Processing
    
    /// Process single component JSON
    ///
    /// - Parameter componentJson: Component JSON string
    func processAddComponentJson(_ componentJson: String, parentId: String?) {
        // Parse component JSON
        guard var componentData = parseJSON(componentJson) else {
            Logger.shared.error("Failed to parse component JSON")
            return
        }
        
        // Normalize: convert "child" to "children" array
        // This allows components like Button to have a single child
        if let child = componentData["child"] as? String {
            if !child.isEmpty {
                componentData["children"] = [child]
                componentData.removeValue(forKey: "child")
            }
        }
        
        // Extract component information
        guard let componentId = componentData["id"] as? String else {
            Logger.shared.error("Component missing id")
            return
        }
        
        var componentType = componentData["type"] as? String
        if componentType == nil {
            componentType = componentData["component"] as? String
        }
        
        guard let type = componentType else {
            Logger.shared.error("Component missing type: \(componentId)")
            return
        }
                
        Logger.shared.debug("Processing component: id=\(componentId), type=\(type)")
        
        // Extract properties: use entire componentData excluding metadata fields
        var properties = componentData
        properties.removeValue(forKey: "id")
        properties.removeValue(forKey: "type")
        properties.removeValue(forKey: "component")
        properties.removeValue(forKey: "parent")
        
        // Add new component
        addComponent(
            componentId: componentId,
            componentType: type,
            properties: properties,
            parentId: parentId
        )
        Logger.shared.info("Component added: \(componentId)")
    }
    
    /// Process components update messages
    ///
    /// - Parameter messages: Array of update messages, each containing componentId and component JSON
    func processComponentsUpdate(_ messages: [[String: String]]) {
        for message in messages {
            guard let componentId = message["componentId"],
                  let componentJson = message["component"] else {
                Logger.shared.error("Invalid components update message")
                continue
            }
            
            guard var componentData = parseJSON(componentJson) else {
                Logger.shared.error("Failed to parse component JSON for update: \(componentId)")
                continue
            }
            
            componentData.removeValue(forKey: "id")
            componentData.removeValue(forKey: "type")
            componentData.removeValue(forKey: "component")
            componentData.removeValue(forKey: "parent")
            updateComponent(componentId: componentId, properties: componentData)
        }
    }
    
    /// Process components add messages
    ///
    /// - Parameter messages: Array of add messages, each containing parentId, componentId and component JSON
    func processComponentsAdd(_ messages: [[String: String]]) {
        for message in messages {
            guard let componentJson = message["component"] else {
                Logger.shared.error("Invalid components add message")
                continue
            }
            
            let parentId = message["parentId"]
            processAddComponentJson(componentJson, parentId: parentId)
        }
    }
    
    /// Process components remove messages
    ///
    /// - Parameter messages: Array of remove messages, each containing parentId and componentId
    func processComponentsRemove(_ messages: [[String: String]]) {
        for message in messages {
            guard let componentId = message["componentId"] else {
                Logger.shared.error("Invalid components remove message")
                continue
            }
            removeComponent(componentId: componentId)
        }
    }
    
    // MARK: - JSON Helpers
    
    /// Parse JSON string to dictionary
    ///
    /// - Parameter jsonString: JSON string
    /// - Returns: Dictionary, returns nil if parsing fails
    private func parseJSON(_ jsonString: String) -> [String: Any]? {
        guard let jsonData = jsonString.data(using: .utf8),
              let dict = try? JSONSerialization.jsonObject(with: jsonData) as? [String: Any] else {
            return nil
        }
        return dict
    }
}
