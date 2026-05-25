//
//  SurfaceManager.swift
//  AGenUI
//
// Created on 2026/3/18.
//

import Foundation
import UIKit

// MARK: - SurfaceManagerListener Protocol

/// Surface Manager Listener Protocol
////// Implement this protocol to receive Surface lifecycle events
@objc public protocol SurfaceManagerListener: AnyObject {
    /// Surface created callback
    ///
    /// Called when a Surface has been created
    /// - Parameter surface: The created Surface object
    @objc optional func onCreateSurface(_ surface: Surface)
    
    /// Surface deleted callback
    ///
    /// Called when a Surface has been deleted
    /// - Parameter surfaceId: The ID of the deleted Surface
    @objc optional func onDeleteSurface(_ surface: Surface)
    
    /// Action event routed callback
    ///
    /// Called when C++ routes an action event back after processing
    /// - Parameter event: Action event context JSON string
    @objc optional func onReceiveActionEvent(_ event: String)
    
    /// Root component update callback
    ///
    /// Called when the root component's properties are updated
    /// - Parameters:
    ///   - surface: The Surface whose root component was updated
    ///   - props: The new properties applied to the root component
    @objc optional func onRootComponentUpdate(_ surface: Surface, props: [String: Any])
    
    /// SDK error callback
    ///
    /// - Parameters:
    ///   - surface: The Surface associated with the error, nil if not associated with a specific Surface
    ///   - code: Error code
    ///   - message: Error description
    @objc optional func onError(_ surface: Surface?, code: Int, message: String)
    
    /// Blank check result callback
    ///
    /// Triggered only after `Surface.startBlankCheck(checkDelayMs:validComponentCount:)` is called.
    /// - Parameters:
    ///   - surface: The detected Surface
    ///   - isBlank: true means determined as blank
    @objc optional func onBlankCheckResult(_ surface: Surface, isBlank: Bool)
}

/// AGenUI Surface Manager
///
/// Manages Surface creation, binding, and destruction
/// Also serves as the main SDK entry point
@objc public class SurfaceManager: NSObject {
    
    // MARK: - Properties

    /// Surface dictionary (surfaceId -> Surface)
    private var surfaces: [String: Surface] = [:]

    /// Per-instance SurfaceManager bridge (owns an independent C++ ISurfaceManager)
    private let surfaceBridge = AGenUIEngineSurfaceManagerBridge()

    /// Measurement bridge (registers C++ IMeasurement, forwards to Swift callbacks)
    /// Now uses static methods on AGenUIEngineMeasurementBridge

    /// Listener container (weak references)
    private let listeners = NSHashTable<SurfaceManagerListener>.weakObjects()

    // MARK: - Initialization
    
    public override init() {
        super.init()
        
        _ = ComponentRegister.shared
        
        // Register for notifications from this instance's surfaceBridge
        setupNotificationObservers()

        // Initialize measurement bridge and register Swift measurement callbacks.
        //
        // IMPORTANT: do NOT capture `self` here (weak or strong). `measureCallback` is a
        // single static slot on the bridge, and any later SurfaceManager will overwrite
        // it with its own closure. If that owner SurfaceManager is later deallocated,
        // a `[weak self]` capture flips to nil and every subsequent measure returns
        // .zero — Yoga then produces height=0 frames (e.g. "Deep Thinking" text collapsing
        // mid-layout when its surface is re-streamed). Component lookup goes through
        // `ComponentRegister.shared` (a process-wide singleton) and `Component.measure`
        // is a class method, so SurfaceManager instance state is not needed here.
        AGenUIEngineMeasurementBridge.measureCallback = { (componentType: String, paramJson: String, maxWidth: Float, widthMode: Int32, maxHeight: Float, heightMode: Int32) -> CGSize in
            return SurfaceManager.measureComponent(
                type: componentType,
                paramJson: paramJson,
                maxWidth: maxWidth,
                widthMode: Int(widthMode),
                maxHeight: maxHeight,
                heightMode: Int(heightMode))
        }
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
        surfaceBridge.teardown()
    }
    
    // MARK: - Notification Setup
    
    private func setupNotificationObservers() {
        let notificationCenter = NotificationCenter.default
        let instanceId = surfaceBridge.instanceId
        
        notificationCenter.addObserver(
            self,
            selector: #selector(handleCreateSurfaceNotification(_:)),
            name: NSNotification.Name(rawValue: "AGenUICreateSurfaceNotification_\(instanceId)"),
            object: surfaceBridge
        )
        
        notificationCenter.addObserver(
            self,
            selector: #selector(handleComponentsUpdateNotification(_:)),
            name: NSNotification.Name(rawValue: "AGenUIComponentsUpdateNotification_\(instanceId)"),
            object: surfaceBridge
        )
        
        notificationCenter.addObserver(
            self,
            selector: #selector(handleComponentsAddNotification(_:)),
            name: NSNotification.Name(rawValue: "AGenUIComponentsAddNotification_\(instanceId)"),
            object: surfaceBridge
        )
        
        notificationCenter.addObserver(
            self,
            selector: #selector(handleComponentsRemoveNotification(_:)),
            name: NSNotification.Name(rawValue: "AGenUIComponentsRemoveNotification_\(instanceId)"),
            object: surfaceBridge
        )
        
        notificationCenter.addObserver(
            self,
            selector: #selector(handleDeleteSurfaceNotification(_:)),
            name: NSNotification.Name(rawValue: "AGenUIDeleteSurfaceNotification_\(instanceId)"),
            object: surfaceBridge
        )
        
        notificationCenter.addObserver(
            self,
            selector: #selector(handleActionEventRoutedNotification(_:)),
            name: NSNotification.Name(rawValue: "AGenUIActionEventRoutedNotification_\(instanceId)"),
            object: surfaceBridge
        )
        
        notificationCenter.addObserver(
            self,
            selector: #selector(handleErrorNotification(_:)),
            name: NSNotification.Name(rawValue: "AGenUIErrorNotification_\(instanceId)"),
            object: surfaceBridge
        )
    }
    
    // MARK: - Notification Handlers
    
    @objc private func handleCreateSurfaceNotification(_ notification: Notification) {
        guard let userInfo = notification.userInfo,
              let surfaceId = userInfo["surfaceId"] as? String,
              let catalogId = userInfo["catalogId"] as? String,
              let theme = userInfo["theme"] as? [String: String],
              let sendDataModelValue = userInfo["sendDataModel"] as? NSNumber,
              let animatedValue = userInfo["animated"] as? NSNumber else {
            Logger.shared.error("Invalid create surface notification userInfo")
            return
        }
        
        let rawProtocolContent = userInfo["rawProtocolContent"] as? String ?? ""
                
        onCreateSurface(withSurfaceId: surfaceId,
                       catalogId: catalogId,
                       theme: theme,
                       sendDataModel: sendDataModelValue.boolValue,
                       animated: animatedValue.boolValue,
                       rawProtocolContent: rawProtocolContent)
    }
    
    @objc private func handleComponentsUpdateNotification(_ notification: Notification) {
        guard let userInfo = notification.userInfo,
              let surfaceId = userInfo["surfaceId"] as? String,
              let messages = userInfo["componentsUpdate"] as? [[String: String]] else {
            Logger.shared.error("Invalid components update notification userInfo")
            return
        }
        
        onComponentsUpdate(withSurfaceId: surfaceId, messages: messages)
    }
    
    @objc private func handleComponentsAddNotification(_ notification: Notification) {
        guard let userInfo = notification.userInfo,
              let surfaceId = userInfo["surfaceId"] as? String,
              let messages = userInfo["componentsAdd"] as? [[String: String]] else {
            Logger.shared.error("Invalid components add notification userInfo")
            return
        }
        
        onComponentsAdd(withSurfaceId: surfaceId, messages: messages)
    }
    
    @objc private func handleComponentsRemoveNotification(_ notification: Notification) {
        guard let userInfo = notification.userInfo,
              let surfaceId = userInfo["surfaceId"] as? String,
              let messages = userInfo["componentsRemove"] as? [[String: String]] else {
            Logger.shared.error("Invalid components remove notification userInfo")
            return
        }
        
        onComponentsRemove(withSurfaceId: surfaceId, messages: messages)
    }
    
    @objc private func handleDeleteSurfaceNotification(_ notification: Notification) {
        guard let userInfo = notification.userInfo,
              let surfaceId = userInfo["surfaceId"] as? String else {
            Logger.shared.error("Invalid delete surface notification userInfo")
            return
        }
        
        onDeleteSurface(withSurfaceId: surfaceId)
    }
    
    @objc private func handleActionEventRoutedNotification(_ notification: Notification) {
        guard let userInfo = notification.userInfo,
              let context = userInfo["context"] as? String else {
            Logger.shared.error("Invalid action event routed notification userInfo")
            return
        }
        
        for listener in listeners.allObjects.compactMap({ $0 }) {
            listener.onReceiveActionEvent?(context)
        }
    }
    
    @objc private func handleErrorNotification(_ notification: Notification) {
        guard let userInfo = notification.userInfo else {
            Logger.shared.error("Invalid error notification userInfo")
            return
        }
        
        let code = (userInfo["code"] as? NSNumber)?.intValue ?? 0
        let message = (userInfo["message"] as? String) ?? ""
        let surfaceId = userInfo["surfaceId"] as? String ?? ""
        
        // Look up the related Surface; nil if not associated to any specific Surface
        let surface: Surface? = surfaceId.isEmpty ? nil : surfaces[surfaceId]
        
        for listener in listeners.allObjects.compactMap({ $0 }) {
            listener.onError?(surface, code: code, message: message)
        }
    }
    
    // MARK: - Listener Management
    
    /// Add Surface lifecycle listener
    ///
    /// - Parameter listener: Object implementing SurfaceManagerListener protocol
    @objc public func addListener(_ listener: SurfaceManagerListener) {
        listeners.add(listener)
    }
    
    /// Remove Surface lifecycle listener
    ///
    /// - Parameter listener: Listener object to remove
    @objc public func removeListener(_ listener: SurfaceManagerListener) {
        listeners.remove(listener)
    }
    
    /// Remove all listeners
    @objc public func removeAllListeners() {
        listeners.removeAllObjects()
    }

    /// Returns the native instance id assigned by the engine on creation.
    @objc public func getInstanceId() -> Int {
        return surfaceBridge.instanceId
    }

    // MARK: - Data Interaction

    /// Start a new streaming data session
    ///
    /// Clears the buffer and resets parsing state. Should be called before each streaming session.
    @objc public func beginTextStream() {
        Logger.shared.debug("beginTextStream")
        surfaceBridge.beginTextStream()
    }

    /// End the streaming data session
    ///
    /// Resets parsing state. Should be called after SSE stream closes, HTTP response ends, user aborts, or network disconnects.
    @objc public func endTextStream() {
        Logger.shared.debug("endTextStream")
        surfaceBridge.endTextStream()
    }

    /// Preset the size of an upcoming surface BEFORE its components are laid out.
    ///
    /// Background: the engine processes JSON on a dedicated background thread
    /// in FIFO order, while `onCreateSurface` is delivered to the main thread
    /// asynchronously (via `dispatch_async`). When a host streams
    /// `createSurface` and `updateComponents` back-to-back, the engine may
    /// finish laying out components with the default surface size (0) BEFORE
    /// the main thread gets a chance to call `surface.updateSize(...)`,
    /// causing a brief "narrow-width" flash on first paint.
    ///
    /// Calling this method between the two `receiveTextChunk` calls enqueues
    /// the size on the same FIFO queue, so the engine has the correct surface
    /// width when computing the very first Yoga layout pass — eliminating the
    /// race without blocking either thread.
    ///
    /// Safe to call multiple times; the subsequent `surface.updateSize(...)`
    /// from `onCreateSurface` remains the source of truth and will simply be a
    /// no-op when the value matches.
    ///
    /// - Parameters:
    ///   - surfaceId: Surface ID (must match the one in `createSurface` JSON)
    ///   - width: Width in pt; pass `.infinity` for unconstrained
    ///   - height: Height in pt; pass `.infinity` for unconstrained
    @objc public func presetSurfaceSize(surfaceId: String, width: CGFloat, height: CGFloat) {
        guard !surfaceId.isEmpty else { return }
        let widthCXX  = (width.isFinite  && width  > 0) ? Float(width)  : 0.0
        let heightCXX = (height.isFinite && height > 0) ? Float(height) : 0.0
        surfaceBridge.notifySurfaceSizeChanged(surfaceId, width: widthCXX, height: heightCXX)
    }

    /// Receive text chunk from external source
    ///
    /// Receives JSON data for processing. This is the primary method for sending
    /// component updates, data model changes, and other instructions to the rendering engine.
    ///
    /// - Parameter dataString: JSON string containing the data to process
    ///
    /// Usage example:
    /// ```swift
    /// let jsonData = """
    /// {
    ///   "version": "v0.9",
    ///   "updateComponents": {
    ///     "surfaceId": "main",
    ///     "components": [...]
    ///   }
    /// }
    /// """
    /// surfaceManager.receiveTextChunk(jsonData)
    /// ```
    @objc public func receiveTextChunk(_ dataString: String) {
        surfaceBridge.receiveTextChunk(dataString)
    }

    /// Re-evaluate every component's attributes and styles across all surfaces managed by
    /// this SurfaceManager, then emit field-level diffs to the native renderer for any value
    /// that actually changed.
    ///
    /// Call this when host-owned external state has changed in ways the SDK cannot observe
    /// (theme, locale, orientation, etc.) and registered FunctionCalls that read from that
    /// state need to be re-run. Action handlers are not in scope.
    @objc public func invalidateFunctionCallValues() {
        Logger.shared.debug("invalidateFunctionCallValues")
        surfaceBridge.invalidateFunctionCallValues()
    }

    /// Send user interaction event (internal)
    ///
    /// Notifies the SDK when a user interacts with a component (e.g., button tap, text input).
    /// The SDK will process this event and trigger appropriate data updates or callbacks.
    ///
    /// - Parameters:
    ///   - surfaceId: Surface unique identifier
    ///   - componentId: Component ID that received the interaction
    ///   - context: Additional context data for the interaction
    func triggerAction(surfaceId: String, componentId: String, context: [String: Any]) {
        guard let contextJson = convertToJSON(context) else {
            Logger.shared.error("Failed to convert context to JSON")
            return
        }
        surfaceBridge.triggerAction(surfaceId, componentId: componentId, context: contextJson)
    }
    
    /// Synchronize UI state to data model (internal)
    ///
    /// Updates the underlying data model with the current UI state. Use this when you need to
    /// persist UI changes back to the data layer, such as form input values or toggle states.
    ///
    /// - Parameters:
    ///   - surfaceId: Surface unique identifier
    ///   - componentId: Component ID whose state should be synced
    ///   - context: Current state data to sync
    func syncState(surfaceId: String, componentId: String, context: [String: Any]) {
        guard let contextJson = convertToJSON(context) else {
            Logger.shared.error("Failed to convert context to JSON")
            return
        }
        surfaceBridge.syncState(surfaceId, componentId: componentId, context: contextJson)
    }
    
    /// Notify C++ engine that surface size changed
    ///
    /// - Parameters:
    ///   - surfaceId: Surface ID
    ///   - widthA2ui: New width in a2ui units (pt * 2)
    ///   - heightA2ui: New height in a2ui units (pt * 2)
    func notifySurfaceSizeChanged(surfaceId: String, width: Float, height: Float) {
        surfaceBridge.notifySurfaceSizeChanged(surfaceId, width: width, height: height)
    }
    
    /// Notify C++ engine that a component has finished rendering with its actual size
    ///
    /// - Parameters:
    ///   - surfaceId: Surface ID
    ///   - componentId: Component ID
    ///   - type: Component type
    ///   - widthA2ui: Rendered width in a2ui units (pt * 2)
    ///   - heightA2ui: Rendered height in a2ui units (pt * 2)
    func notifyComponentRenderFinish(surfaceId: String, componentId: String, type: String, width: Float, height: Float) {
        surfaceBridge.notifyComponentRenderFinish(surfaceId, componentId: componentId, type: type, width: width * 2.0, height: height * 2.0)
    }

    func notifyTabSelection(surfaceId: String, componentId: String, type: String, selectedIndex: Int) {
        surfaceBridge.notifyTabSelection(surfaceId, componentId: componentId, type: type, selectedIndex: Int32(selectedIndex))
    }

    // MARK: - Helper Methods
    
    /// Convert dictionary to JSON string
    ///
    /// - Parameter dict: Dictionary to convert
    /// - Returns: JSON string, returns nil if conversion fails
    private func convertToJSON(_ dict: [String: Any]) -> String? {
        guard let jsonData = try? JSONSerialization.data(withJSONObject: dict, options: []),
              let jsonString = String(data: jsonData, encoding: .utf8) else {
            return nil
        }
        return jsonString
    }

    // MARK: - Surface Event Handlers

    /// Surface creation handler (internal)
    func onCreateSurface(withSurfaceId surfaceId: String,
                                      catalogId: String,
                                      theme: [String: String],
                                      sendDataModel: Bool,
                                      animated: Bool = true,
                                      rawProtocolContent: String = "") {
        Logger.shared.info("Surface will create: \(surfaceId), catalogId: \(catalogId)")

        // If already exists, return
        if surfaces[surfaceId] != nil {
            Logger.shared.warning("Surface already exists: \(surfaceId)")
            return
        }

        // Create new Surface with provided size
        let surface = Surface(surfaceId: surfaceId)
        surface.animationEnabled = animated
        surface.rawProtocolContent = rawProtocolContent
        surface.surfaceManager = self
        surfaces[surfaceId] = surface
        
        Logger.shared.info("Surface created: \(surfaceId), width: \(surface.width), height: \(surface.height)")

        // Notify all listeners
        for listener in listeners.allObjects.compactMap({ $0 }) {
            listener.onCreateSurface?(surface)
        }
    }
    
    /// Components update handler (internal)
    func onComponentsUpdate(withSurfaceId surfaceId: String, messages: [[String: String]]) {
        Logger.shared.info("Surface components update: \(surfaceId), messages count: \(messages.count)")
        
        guard let surface = surfaces[surfaceId] else {
            Logger.shared.warning("Surface not found: \(surfaceId)")
            return
        }
        
        surface.processComponentsUpdate(messages)
    }
    
    /// Components add handler (internal)
    func onComponentsAdd(withSurfaceId surfaceId: String, messages: [[String: String]]) {
        Logger.shared.info("Surface components add: \(surfaceId), messages count: \(messages.count)")
        
        guard let surface = surfaces[surfaceId] else {
            Logger.shared.warning("Surface not found: \(surfaceId)")
            return
        }
        
        surface.processComponentsAdd(messages)
    }
    
    /// Components remove handler (internal)
    func onComponentsRemove(withSurfaceId surfaceId: String, messages: [[String: String]]) {
        Logger.shared.info("Surface components remove: \(surfaceId), messages count: \(messages.count)")
        
        guard let surface = surfaces[surfaceId] else {
            Logger.shared.warning("Surface not found: \(surfaceId)")
            return
        }
        
        surface.processComponentsRemove(messages)
    }
    
    /// Notify listeners that root component properties were updated (internal)
    func notifyRootComponentUpdate(surface: Surface, props: [String: Any]) {
        for listener in listeners.allObjects.compactMap({ $0 }) {
            listener.onRootComponentUpdate?(surface, props: props)
        }
    }
    
    /// Notify listeners about a blank-check result (internal)
    func notifyBlankCheckResult(surface: Surface, isBlank: Bool) {
        for listener in listeners.allObjects.compactMap({ $0 }) {
            listener.onBlankCheckResult?(surface, isBlank: isBlank)
        }
    }
    
    /// Delete Surface handler (internal)
    func onDeleteSurface(withSurfaceId surfaceId: String) {
        Logger.shared.info("Surface deleted: \(surfaceId)")

        // Remove and destroy Surface
        guard let surface = surfaces.removeValue(forKey: surfaceId) else {
            Logger.shared.warning("Surface not found: \(surfaceId)")
            return
        }

        // Notify all listeners
        for listener in listeners.allObjects.compactMap({ $0 }) {
            listener.onDeleteSurface?(surface)
        }

        Logger.shared.info("Surface destroyed: \(surfaceId)")
    }
    
    // MARK: - Component Measurement

    /// Measure the intrinsic size of a component
    ///
    /// Called back by the C++ Yoga layout engine, forwarding to the
    /// corresponding component class's class measure method.
    /// This method is called on the engine's background thread.
    /// Measure dispatch entry — must remain `static` so the bridge's measure callback
    /// can call it without capturing any SurfaceManager instance. See the comment at
    /// the callback registration site (init) for why instance capture is unsafe here.
    private static func measureComponent(type: String,
                                          paramJson: String,
                                          maxWidth: Float,
                                          widthMode: Int,
                                          maxHeight: Float,
                                          heightMode: Int) -> CGSize {
        guard let componentClass = ComponentRegister.shared.classForType(type) else {
            return .zero
        }
        let wMode = MeasureMode(rawValue: widthMode) ?? .undefined
        let hMode = MeasureMode(rawValue: heightMode) ?? .undefined
        return componentClass.measure(type: type,
                                      paramJson: paramJson,
                                      maxWidth: maxWidth,
                                      widthMode: wMode,
                                      maxHeight: maxHeight,
                                      heightMode: hMode)
    }

}

