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
    }

    private let dateFormatter: DateFormatter

    // MARK: - LoggerDelegate

    func onLog(level: Logger.Level, tag: String, func: String, line: Int, message: String) {
        let prefix = levelPrefix(for: level)
        let timestamp = dateFormatter.string(from: Date())
        print("[AGenUI/\(prefix)] \(timestamp) [\(tag):\(line)] \(message)")
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
}
