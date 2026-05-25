//
//  PlaygroundRuntimeLogger.swift
//  Playground
//
//  Created by yinglong.zyl on 2026/5/8.
//

import Foundation
import AGenUI

/// Playground custom RuntimeLogger
///
/// Implements the LoggerDelegate protocol, receives runtime logs from the AGenUI C++ engine,
/// and formats output to the Xcode console for development debugging.
///
/// Usage: Call AGenUISDK.setLoggerDelegate(PlaygroundRuntimeLogger.shared) in AppDelegate
final class PlaygroundRuntimeLogger: NSObject, LoggerDelegate {

    static let shared = PlaygroundRuntimeLogger()

    private override init() {
        dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "HH:mm:ss.SSS"
        
        fileDateFormatter = DateFormatter()
        fileDateFormatter.dateFormat = "yyyyMMdd_HHmmss_SSS"

        super.init()
        
        // Set up log directory and session log file
        setupLogDirectory()
        createNewLogFile()
    }

    private let dateFormatter: DateFormatter
    private let fileDateFormatter: DateFormatter
    private let logQueue = DispatchQueue(label: "com.agenui.playground.log.queue", qos: .background)
    private var logDirectoryURL: URL?
    private var currentLogFileURL: URL?

    // MARK: - LoggerDelegate

    func onLog(level: Logger.Level, tag: String, func: String, line: Int, message: String) {
        let prefix = levelPrefix(for: level)
        let timestamp = dateFormatter.string(from: Date())
        print("[AGenUI/\(prefix)] \(timestamp) [\(tag):\(line)] \(message)")
        // Persist log to file asynchronously
        saveLogToFile(level: level, timestamp: timestamp, tag: tag, function: `func`, line: line, message: message)
    }

    // MARK: - Private

    private func levelPrefix(for level: Logger.Level) -> String {
        switch level {
        case .debug:       return "DEBUG"
        case .info:        return "INFO"
        case .warning:     return "WARN"
        case .error:       return "ERROR"
        case .fatal:       return "FATAL"
        case .performance: return "PERF"
        }
    }
    
    // MARK: - Log File Management
    
    private func setupLogDirectory() {
        guard let documentsDirectory = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first else {
            print("[PlaygroundRuntimeLogger] Failed to get documents directory")
            return
        }
        
        logDirectoryURL = documentsDirectory.appendingPathComponent("agenui_log")
        
        do {
            try FileManager.default.createDirectory(at: logDirectoryURL!, withIntermediateDirectories: true, attributes: nil)
        } catch {
            print("[PlaygroundRuntimeLogger] Failed to create log directory: \(error)")
        }
    }
    
    private func createNewLogFile() {
        guard let logDirectoryURL = logDirectoryURL else {
            return
        }
        
        // Create a new log file for this session with timestamp
        let fileName = fileDateFormatter.string(from: Date()) + ".log"
        currentLogFileURL = logDirectoryURL.appendingPathComponent(fileName)
        
        print("[PlaygroundRuntimeLogger] Created new log file: \(currentLogFileURL!.path)")
    }
    
    private func saveLogToFile(level: Logger.Level, timestamp: String, tag: String, function: String, line: Int, message: String) {
        guard let logFileURL = currentLogFileURL else {
            return
        }
        
        logQueue.async {
            let logContent = "[AGenUI/\(self.levelPrefix(for: level))] \(timestamp) [\(tag):\(line)] \(function) - \(message)\n"
            
            do {
                // Check if file exists
                if FileManager.default.fileExists(atPath: logFileURL.path) {
                    // Open file handle for appending
                    if let fileHandle = try? FileHandle(forWritingTo: logFileURL) {
                        fileHandle.seekToEndOfFile()
                        if let data = logContent.data(using: .utf8) {
                            fileHandle.write(data)
                            fileHandle.closeFile()
                        }
                    }
                } else {
                    // Create new file
                    try logContent.write(to: logFileURL, atomically: true, encoding: .utf8)
                }
            } catch {
                print("[PlaygroundRuntimeLogger] Failed to save log file: \(error)")
            }
        }
    }
}
