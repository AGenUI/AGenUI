//
//  AGenUIFunctionProtocol.swift
//  AGenUI
//
// Created on 2026/4/22.
//

import Foundation

// MARK: - FunctionConfig

/// SDK-internal function configuration
@objc public class FunctionConfig: NSObject {
    
    /// Function name (unique identifier for registration)
    @objc public let name: String
    
    @objc public init(name: String) {
        self.name = name
    }
    
    /// Serialize config to JSON string for engine registration
    @objc public func toJSON() -> String {
        let dict: [String: Any] = ["name": name]
        guard let data = try? JSONSerialization.data(withJSONObject: dict),
              let json = String(data: data, encoding: .utf8) else {
            return "{}"
        }
        return json
    }
}

// MARK: - AGenUIFunctionResult

/// SDK-internal function execution result
@objc public class FunctionResult: NSObject {
    
    /// Whether the function call succeeded
    @objc public let result: Bool
    
    /// Additional data returned by the function call
    @objc public let value: String

    @objc public init(result: Bool, value: String) {
        self.result = result
        self.value = value
    }
    
    /// Create a success result
    @objc public static func success(value: String) -> FunctionResult {
        return FunctionResult(result: true, value: value)
    }
    
    /// Create a failure result
    @objc public static func failure(value: String) -> FunctionResult {
        return FunctionResult(result: false, value: value)
    }
}

// MARK: - FunctionCallContext

/// Context information for a function call
///
/// Provides the instance and surface context in which the function call is invoked.
@objc public class FunctionCallContext: NSObject {
    
    /// Instance unique identifier
    @objc public let instanceId: Int
    
    /// Surface unique identifier
    @objc public let surfaceId: String
    
    @objc public init(instanceId: Int, surfaceId: String) {
        self.instanceId = instanceId
        self.surfaceId = surfaceId
    }
}

// MARK: - AGenUIFunctionProtocol

/// SDK-internal protocol for platform function implementations
///
/// Bridge layer should adapt external function objects to this protocol
/// before passing them to AGenUI for registration.
@objc public protocol Function: AnyObject {
    
    /// Function configuration
    @objc var functionConfig: FunctionConfig { get }
    
    /// Execute the function synchronously
    /// - Parameters:
    ///   - context: Context information containing instanceId and surfaceId
    ///   - params: JSON string containing the function parameters
    /// - Returns: Execution result with success/failure status and return value
    @objc func execute(context: FunctionCallContext, params: String) -> FunctionResult
}
