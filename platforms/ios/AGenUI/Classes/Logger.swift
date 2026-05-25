//
//  Logger.swift
//  AGenUI
//
// Created on 2026/3/18.
//

import Foundation

// MARK: - Logger Delegate Protocol

/// Logger delegate protocol for receiving log output events
/// Interface design is consistent with C++ ILogObserver
@objc public protocol LoggerDelegate: AnyObject {
    /// Log output callback
    ///
    /// - Parameters:
    ///   - level: Log level
    ///   - tag: Log tag (typically filename)
    ///   - func: Function name
    ///   - line: Line number
    ///   - message: Log message
    func onLog(level: Logger.Level, tag: String, func: String, line: Int, message: String)
}

/// AGenUI SDK Logger
///
/// Provides unified log output interface with log switch control and level filtering
@objc public class Logger: NSObject {
    
    // MARK: - Log Level
    
    /// Log level
    @objc public enum Level: Int {
        case debug = 0          // Debug log
        case info = 1           // Info log
        case warning = 2        // Warning log
        case error = 3          // Error log
        case fatal = 4          // Fatal log
        case performance = 5    // performance log
    }
    
    // MARK: - Singleton
    
    /// Singleton instance
    @objc public static let shared = Logger()
    
    // MARK: - Properties
    
    /// Whether log output is enabled
    /// Default is enabled, aligned with Android/Harmony.
    /// When no `delegate` is set, logs fall back to NSLog so the SDK is never silent by default.
    @objc public var isEnabled: Bool = {
        return true
    }()
    
    /// Minimum log level
    /// Only logs with level greater than or equal to this will be output
    @objc public var minimumLevel: Level = .debug
    
    /// Whether to show file name and line number
    @objc public var showFileInfo: Bool = true
    
    /// Logger delegate for receiving log events
    @objc public weak var delegate: LoggerDelegate?
    
    // MARK: - Initialization
    
    private override init() {}
    
    // MARK: - Public Methods
    
    /// Output log
    ///
    /// - Parameters:
    ///   - message: Log message
    ///   - level: Log level
    ///   - file: File name (auto captured)
    ///   - function: Function name (auto captured)
    ///   - line: Line number (auto captured)
    @objc public func log(_ message: String, 
                   level: Level = .debug,
                   file: String = #file, 
                   function: String = #function, 
                   line: Int = #line) {
        guard isEnabled, level.rawValue >= minimumLevel.rawValue else { return }
        
        let prefix = levelPrefix(level)
        let fileName = (file as NSString).lastPathComponent
        
        if let delegate = delegate {
            // Output to delegate
            delegate.onLog(level: level, tag: fileName, func: function, line: line, message: message)
        } else {
            // Fallback to NSLog so logs land in the system log / Console.app instead of being silent.
            if showFileInfo {
                NSLog("[%@] [%@:%d] %@", prefix, fileName, line, message)
            } else {
                NSLog("[%@] %@", prefix, message)
            }
        }
    }
    
    // MARK: - Private Methods
    
    private func levelPrefix(_ level: Level) -> String {
        switch level {
        case .debug: return "DEBUG"
        case .info: return "INFO"
        case .warning: return "WARNING"
        case .error: return "ERROR"
        case .fatal: return "FATAL"
        case .performance: return "PERFORMANCE"
        }
    }
}

// MARK: - Convenience Methods

extension Logger {
    
    /// Output debug log
    @objc public func debug(_ message: String, 
                     file: String = #file, 
                     function: String = #function, 
                     line: Int = #line) {
        log(message, level: .debug, file: file, function: function, line: line)
    }
    
    /// Output info log
    @objc public func info(_ message: String, 
                    file: String = #file, 
                    function: String = #function, 
                    line: Int = #line) {
        log(message, level: .info, file: file, function: function, line: line)
    }
    
    /// Output warning log
    @objc public func warning(_ message: String, 
                       file: String = #file, 
                       function: String = #function, 
                       line: Int = #line) {
        log(message, level: .warning, file: file, function: function, line: line)
    }
    
    /// Output error log
    @objc public func error(_ message: String, 
                     file: String = #file, 
                     function: String = #function, 
                     line: Int = #line) {
        log(message, level: .error, file: file, function: function, line: line)
    }
    
    /// Output fatal log
    @objc public func fatal(_ message: String,
                     file: String = #file,
                     function: String = #function,
                     line: Int = #line) {
        log(message, level: .fatal, file: file, function: function, line: line)
    }
    
    /// Output performance log
    @objc public func performance(_ message: String,
                       file: String = #file,
                       function: String = #function,
                       line: Int = #line) {
        log(message, level: .performance, file: file, function: function, line: line)
    }
}


