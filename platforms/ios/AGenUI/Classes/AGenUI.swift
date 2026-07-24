//
//  AGenUI.swift
//  AGenUI
//
// Created on 2026/3/18.
//

import Foundation

/// AGenUI SDK global entry point
///
/// Singleton responsible for global registration of theme, DesignToken, day/night mode, skills, components, and image loaders.
@objc(AGenUISDK) public class AGenUISDK: NSObject {

    private override init() {
        super.init()
    }

    // MARK: - Engine Bridge

    private static var engineBridge: AGenUIEngineBridge {
        return AGenUIEngineBridge.sharedInstance()
    }

    /// Strong references to registered functions to prevent deallocation
    private static var registeredFunctions: [String: Function] = [:]
    private static let fuctionsLock = NSLock()

    // MARK: - Global Configuration

    // MARK: - Theme Management

    /// Register default theme configuration
    ///
    /// Registers theme and design token configurations to customize UI component appearance.
    ///
    /// - Parameters:
    ///   - theme: Theme configuration JSON string
    ///   - designToken: Design token configuration JSON string
    /// - Returns: AGenUIError with result and message fields
    @objc public static func registerDefaultTheme(_ theme: String, designToken: String) -> AGenUIError {
        Logger.shared.debug("registerDefaultTheme - theme length: \(theme.count), designToken length: \(designToken.count)")
        
        // 1. Wrap theme with "default" key
        guard let themeData = theme.data(using: .utf8),
              let themeObject = try? JSONSerialization.jsonObject(with: themeData) else {
            Logger.shared.error("Failed to parse theme JSON")
            return AGenUIError(result: false, message: "Invalid theme JSON format")
        }
        
        let wrappedTheme: [String: Any] = ["default": themeObject]
        guard let wrappedData = try? JSONSerialization.data(withJSONObject: wrappedTheme),
              let wrappedThemeJson = String(data: wrappedData, encoding: .utf8) else {
            Logger.shared.error("Failed to serialize wrapped theme JSON")
            return AGenUIError(result: false, message: "Failed to wrap theme JSON")
        }
        
        // 2. Register theme
        let themeResult = engineBridge.loadThemeConfig(wrappedThemeJson)
        if !themeResult {
            Logger.shared.error("Theme registration failed")
            return AGenUIError(result: false, message: "Theme registration failed")
        }
        
        // 3. Register DesignToken
        let designTokenResult = engineBridge.loadDesignTokenConfig(designToken)
        if !designTokenResult {
            Logger.shared.error("DesignToken registration failed")
            return AGenUIError(result: false, message: "DesignToken registration failed")
        }
        
        Logger.shared.info("Default theme registered successfully")
        return AGenUIError(result: true, message: "Success")
    }

    /// Set day/night mode
    ///
    /// - Parameter mode: "light" or "dark"
    @objc public static func setDayNightMode(_ mode: String) {
        Logger.shared.debug("setDayNightMode: \(mode)")
        guard mode == "light" || mode == "dark" else {
            Logger.shared.warning("setDayNightMode: invalid mode '\(mode)', expected 'light' or 'dark'")
            return
        }
        engineBridge.setDayNightMode(mode)
        Logger.shared.info("Day/Night mode set to: \(mode)")
    }

    // MARK: - Path Configuration

    /// Set path configuration
    ///
    /// - Parameter configJson: Path configuration JSON string, e.g. {"templateDir": "/path/to/templates"}
    /// - Returns: AGenUIError with result and message fields
    @objc public static func setPathConfig(_ configJson: String) -> AGenUIError {
        Logger.shared.debug("setPathConfig - configJson length: \(configJson.count)")
        let result = engineBridge.setPathConfig(configJson)
        if result {
            Logger.shared.info("Path config set successfully")
            return AGenUIError(result: true, message: "Success")
        } else {
            Logger.shared.error("Failed to set path config")
            return AGenUIError(result: false, message: "Failed to parse path config JSON")
        }
    }

    // MARK: - Component Registration

    /// Register a custom component factory
    ///
    /// - Parameters:
    ///   - type: Component type name
    ///   - creator: Factory closure to create the component
    /// - Note: Overwrites if the component type already exists
    public static func registerComponent<T: Component>(_ type: String, creator: @escaping (String, [String: Any]) -> T) {
        Logger.shared.debug("registerComponent: \(type)")
        ComponentRegister.shared.register(type, creator: creator)
        Logger.shared.info("Custom component registered: \(type)")
    }
    
    /// Register a custom component factory (Objective-C compatible)
    ///
    /// - Parameters:
    ///   - type: Component type name
    ///   - creator: Factory closure to create the component, returns a Component instance
    /// - Note: Overwrites if the component type already exists
    @objc(registerComponent:creator:)
    public static func registerComponentObjC(_ type: String, creator: @escaping (String, [String: Any]) -> Component) {
        Logger.shared.debug("registerComponent (ObjC): \(type)")
        ComponentRegister.shared.register(type, creator: creator)
        Logger.shared.info("Custom component registered (ObjC): \(type)")
    }
    
    /// Register a custom component factory with explicit class name (Objective-C compatible)
    ///
    /// When called from ObjC, Swift generic T is erased to Component.
    /// This method accepts an explicit class name string to resolve the actual
    /// Component subclass for correct measurement dispatch.
    ///
    /// - Parameters:
    ///   - type: Component type name
    ///   - componentClassName: Fully qualified class name string for the Component subclass
    ///   - creator: Factory closure to create the component
    /// - Note: Overwrites if the component type already exists
    @objc(registerComponent:componentClassName:creator:)
    public static func registerComponentObjC(_ type: String, componentClassName: String, creator: @escaping (String, [String: Any]) -> Component) {
        Logger.shared.debug("registerComponent (ObjC): \(type), class: \(componentClassName)")
        ComponentRegister.shared.register(type, componentClassName: componentClassName, creator: creator)
        Logger.shared.info("Custom component registered (ObjC): \(type)")
    }
    
    @objc public static func unRegisterComponent(_ type: String) {
        Logger.shared.debug("unregisterComponent: \(type)")
        ComponentRegister.shared.unregister(type)
        Logger.shared.info("Custom component unregistered: \(type)")
    }

    // MARK: - Image Loader Registration

    /// Register a global image loader
    ///
    /// - Parameter loader: A loader instance implementing the ImageLoader protocol
    @objc(registerImageLoader:)
    public static func registerImageLoader(_ loader: ImageLoader) {
        Logger.shared.debug("registerImageLoader: \(type(of: loader))")
        ImageLoaderConfiguration.shared.loader = loader
        Logger.shared.info("ImageLoader registered: \(type(of: loader))")
    }

    // MARK: - Font Registration

    /// Register a custom font from a file URL.
    ///
    /// The font is loaded via `CTFontManager` and becomes available for
    /// `font-family` CSS property resolution in all text components.
    ///
    /// - Parameters:
    ///   - familyName: the name authors use in `font-family` (case-insensitive)
    ///   - fileURL: URL pointing to a `.ttf` or `.otf` font file
    /// - Returns: `true` if registration succeeds
    @objc(registerFont:fileURL:)
    public static func registerFont(_ familyName: String, fileURL: URL) -> Bool {
        let ok = FontRegistry.shared.registerFont(familyName: familyName, fileURL: fileURL)
        if ok {
            Logger.shared.info("registerFont: '\(familyName)' from \(fileURL.lastPathComponent)")
        } else {
            Logger.shared.error("registerFont: failed for '\(familyName)'")
        }
        return ok
    }

    /// Register a font name already available in the system.
    ///
    /// Use this when the font is declared in `Info.plist` under `UIAppFonts`
    /// and you want to expose it under a custom CSS family name.
    ///
    /// - Parameters:
    ///   - familyName: the name authors use in `font-family` (case-insensitive)
    ///   - fontName: the actual font name (PostScript name or family name)
    /// - Returns: `true` on success
    @objc(registerFont:fontName:)
    public static func registerFont(_ familyName: String, fontName: String) -> Bool {
        let ok = FontRegistry.shared.registerFont(familyName: familyName, fontName: fontName)
        if ok {
            Logger.shared.info("registerFont: '\(familyName)' -> '\(fontName)'")
        }
        return ok
    }

    // MARK: - FunctionCall Management

    /// Register a platform function with the engine
    ///
    /// - Parameter function: Object implementing the AGenUIFunctionProtocol
    @objc(registerFunction:)
    public static func registerFunction(_ function: Function) {
        let config = function.functionConfig
        let name = config.name
        guard !name.isEmpty else {
            Logger.shared.error("registerFunction: function name is empty")
            return
        }
        
        let configJson = config.toJSON()
        Logger.shared.debug("registerFunction - name: \(name)")
        
        // Create bridge callback that delegates to the function instance
        let bridgeCallback: AGenUIFunctionCallCallback = { [weak function] instanceId, surfaceId, argsJson in
            
            guard let function = function else {
                return "{\"status\":\"Error\",\"error\":\"Function deallocated\"}"
            }
            
            let context = FunctionCallContext(instanceId: Int(instanceId), surfaceId: surfaceId ?? "")
            let result = function.execute(context: context, params: argsJson)
            
            let resultDict: [String: Any] = [
                "status": result.result ? "Success" : "Error",
                "data": result.value
            ]
            
            guard let resultData = try? JSONSerialization.data(withJSONObject: resultDict),
                  let resultJson = String(data: resultData, encoding: .utf8) else {
                return "{\"status\":\"Error\",\"error\":\"Failed to serialize result\"}"
            }
            
            return resultJson
        }
        
        // Retain the function object to prevent deallocation
        fuctionsLock.lock()
        registeredFunctions[name] = function
        fuctionsLock.unlock()
        
        engineBridge.registerFunction(name, config: configJson, callback: bridgeCallback)
        Logger.shared.info("FunctionCall registered: \(name)")
    }

    /// Unregister a previously registered platform function
    ///
    /// - Parameter name: Function name to unregister
    @objc(unregisterFunctionWithName:)
    public static func unregisterFunction(_ name: String) {
        guard !name.isEmpty else {
            Logger.shared.error("unregisterFunction: name is empty")
            return
        }
        
        fuctionsLock.lock()
        registeredFunctions.removeValue(forKey: name)
        fuctionsLock.unlock()
        engineBridge.unregisterFunction(name)
        Logger.shared.info("FunctionCall unregistered: \(name)")
    }
    
    // MARK: - Logging
    
    /// Custom Logger
    ///
    /// - Parameter customLogger: Custom Logger
    /// - Note: By setting the delegate, other modules like can receive log output
    @objc public static func setCustomLogger(_ customLogger: LoggerDelegate?) {
        Logger.shared.delegate = customLogger
        // Wire the C++ engine to route runtime logs through the bridge so the
        // IRuntimeLogger extension (getMinLevel + delegate forwarding) actually takes effect.
        engineBridge.setRuntimeLogEnabled(true)
    }

    /// Set the minimum log level for AGenUI SDK.
    ///
    /// Only logs with level >= the specified level will be output.
    /// This controls both the Swift-side filtering and the C++ hot-path
    /// early-exit (via `IRuntimeLogger.getMinLevel()`), so filtered-out
    /// messages skip variadic formatting and cross-language bridging entirely.
    ///
    /// - Parameter level: The minimum log level to output.
    ///   Possible values: `.debug`(0), `.info`(1), `.warning`(2), `.error`(3), `.fatal`(4), `.performance`(5)
    @objc public static func setMinLogLevel(_ level: Logger.Level) {
        Logger.shared.minimumLevel = level
    }

    /// Get the current minimum log level for AGenUI SDK.
    ///
    /// - Returns: The current minimum log level.
    @objc public static func getMinLogLevel() -> Logger.Level {
        return Logger.shared.minimumLevel
    }

    // MARK: - Version Info

    /// Get the AGenUI SDK version
    ///
    /// - Returns: Version string
    @objc public static func getVersion() -> String {
        return AGenUIEngineBridge.sdkVersion()
    }
}

// MARK: - AGenUIError

@objc public class AGenUIError: NSObject {
    /// Whether the operation succeeded
    @objc public let result: Bool
    /// Error message
    @objc public let message: String
    
    @objc public init(result: Bool, message: String) {
        self.result = result
        self.message = message
    }
}


